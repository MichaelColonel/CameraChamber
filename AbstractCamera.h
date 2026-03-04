/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#pragma once

#include <QObject>
#include <QByteArray>
#include <QColor>

#include <QtSerialPort/QSerialPort>

#include <bitset>

#include "typedefs.h"

QT_BEGIN_NAMESPACE

class QTimer;
class QSettings;

class TGraph;
class TGraphErrors;
class TH1;
class TH2;
class TPad;
class TDirectory;
class TTree;

class AbstractCameraPrivate;

class AbstractCamera : public QObject {
  Q_OBJECT
public:
  struct CameraDeviceData {
    QString ID;
    QString DataDirectory;
    QString CommandDeviceName;
    QString DataDeviceName;
  };
  typedef QList< CameraDeviceData > CameraDataList;

  static constexpr int RESOLUTION_16BIT = (1 << AdcResolutionType::ADC_16_BIT);
  static constexpr int RESOLUTION_18BIT = (1 << AdcResolutionType::ADC_18_BIT);
  static constexpr int RESOLUTION_20BIT = (1 << AdcResolutionType::ADC_20_BIT);

  static constexpr int ADC_WORDS_PER_CHANNEL = 2;
  static constexpr int ADC_CHANNELS_PER_CHUNK = ADC_WORDS_PER_CHANNEL * CHANNELS_PER_CHIP;
  static constexpr int BUFFER_SIZE = 3;

  static constexpr std::array< double, CHAR_BIT > CHARGE_RANGE{ 12.5, 50., 100., 150., 200., 250., 300., 350. };
  static constexpr std::array< double, CHAR_BIT > CAPACITY_RANGE{ 3.0, 12.5, 25., 37.5, 50., 62.5, 75., 87.5 };

  explicit AbstractCamera();
  explicit AbstractCamera(const CameraDeviceData& data, QObject *parent = nullptr);
  virtual ~AbstractCamera();
  bool connect();
  void disconnect();
  bool isDeviceAlreadyConnected(const QString& otherPort);
  bool isDeviceAlreadyConnected();

  bool isInitiationListEmpty() const;
  QString getCommandPortError() const;
  QString getDataPortError() const;
  CameraDeviceData getCameraData() const;
  CameraResponse getCameraResponse() const;
  AcquisitionParameters getAcquisitionParameters() const;

  // set pedestal and signal gate and update channel info map
  void setPedestalSignalGate(int pedMin, int pedMax, int sigMin, int sigMax);
  // set ROOT directory to save spill data trees
  void setRootDirectory(TDirectory* dir);

  // get adc data to update raw counts time graph
  bool getAdcData(int chip, int channel, std::vector< int >& adcData,
    AdcTimeType dataType = AdcTimeType::INTEGRATOR_AB);

  virtual TGraph* createProfile(CameraProfileType profileType, bool withErrors = false);
  virtual void updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors) = 0; // update profiles
  virtual TH2* createProfile2D(bool integral = false) = 0;
  virtual void updateProfiles2D(TH2* pseudo2D, TH2* integPseudo2D) = 0; // update profile histograms

  bool processExternalData(TTree* rootFileTree);
  int getChipsEnabledCode() const;
  int getIntegrationTimeMs() const;
  int getCapacityCode() const;
  int getAdcResolutionMode() const;
  int getAdcSamples() const;
  bool getChipChannelInfo(int chip, int channel, ChannelInfoPair& info);
  void getChipChannelInfo(std::map< ChipChannelPair, ChannelInfoPair >& infoMap);
  bool getOnceTimeExternalStartFlag() const;

  ChipChannelPair getReferenceChipChannel(bool adcAmp = false, CameraProfileType profileType = CameraProfileType::PROFILE_VERTICAL) const;
  bool setReferenceChipChannel(const ChipChannelPair& pair, bool adcAmp = false, CameraProfileType profileType = CameraProfileType::PROFILE_VERTICAL);

  QByteArray getSetIntegrationTimeCommand(int integrationTimeMs = 2) const;
  QByteArray getSetCapacityCommand(int capacityCode = 3) const;
  QByteArray getResetChipCommand() const;
  QByteArray getResetAlteraCommand() const;
  QByteArray getStartAcquisitionCommand() const;
  QByteArray getSetAdcResolutionCommand(bool adc20Bit = false) const;
  QByteArray getSetChipsEnabledCommand(int chipsEnabledCode = 0x0FFF) const;
  QByteArray getSetNumberOfChipsCommand() const;
  QByteArray getSetSamplesCommand(int numberOfSamples = 100) const;
  QByteArray getSetExternalStartCommand(bool externalStart = false) const;
  QByteArray getWriteChipsCapacitiesCommand() const;
  QByteArray getFirstContactCommand() const;
  QByteArray getListChipsEnabledCommand() const;
  QByteArray getOnceTimeExternalStartCommand() const;

  QString getChipsAddresses() const;
  void setInitiationList(const QByteArrayList& initList);
  virtual void processDataCounts(bool splitData = false,
    IntegratorType integType = IntegratorType::A,
    ProfileRepresentationType profileType = ProfileRepresentationType::CHARGE);
  bool loadSettings(QSettings* settings);
  bool saveSettings(QSettings* settings);

public slots:
  void onCommandPortDataReady();
  void onCommandPortBytesWritten(qint64);
  void onCommandPortError(QSerialPort::SerialPortError);
  void onDataPortDataReady();
  void onDataPortBytesWritten(qint64);
  void onDataPortError(QSerialPort::SerialPortError);
  void writeCommand(const QByteArray& com);
  void writeNextCommandFromInitiationList();
  void writeLastCommandOnceAgain();

signals:
  void firmwareCommandBufferIsReset();
  void acquisitionStarted();
  void acquisitionFinished();
  void commandWritten(const QByteArray& command);
  void initiationStarted();
  void initiationProgress(int prog);
  void initiationFinished();
  void logMessage(const QString& msg, const QString& context, QColor color);

protected:
  void processRawData();
  // if capacityCode and timeCode are not equal -1 => load chips data
  // for the specific capacity code and integration time code
  bool loadCameraData(const QString& cameraDirectory, int capacityCode = -1, int timeCode = -1); // directory must contain ChipsPositions.json file and chips JSON files
  bool loadChipData(const QString& chipFile, int chipPosition, int capacityCode = -1, int timeCode = -1); // chip file in camera directory

//  void setRefAdcCalibrationChipChannel(int chip, int channel);
//  void setRefAmpChipChannelVerticalProfile(int chip, int channel);
//  void setRefAmpChipChannelHorizontalProfile(int chip, int channel);

  const std::array< int, 4 >& getPedestalSignalGate() const;

  const std::vector< int >& getChipsAddressesVector() const;
  const std::vector< int >& getVerticalProfileChips() const;
  const std::vector< int >& getHorizontalProfileChips() const;
  const std::vector< double >& getVerticalProfile() const;
  const std::vector< double >& getHorizontalProfile() const;
  std::vector< double >& getVerticalProfile();
  std::vector< double >& getHorizontalProfile();
  ChipChannelPair& getReferenceAdcVerticalChipChannel();
  ChipChannelPair& getReferenceAdcHorizontalChipChannel();
  ChipChannelPair& getReferenceAmplitudeVerticalChipChannel();
  ChipChannelPair& getReferenceAmplitudeHorizontalChipChannel();

  const std::vector< double >& getVerticalProfileStripsNumbers() const;
  const std::vector< double >& getHorizontalProfileStripsNumbers() const;
  const std::vector< ChipChannelPair >& getVerticalProfileChipChannelStrips() const;
  const std::vector< ChipChannelPair >& getHorizontalProfileChipChannelStrips() const;
  const std::map< ChipChannelPair, ChannelInfoPair >& getChipChannelInfoMap() const;
  CameraProfileType getProfileBrokenChipChannelsStrips(int chip, std::vector< int >& strips) const;
  std::vector< std::vector< double > >& getProfileFramesVector();

public:
  template< size_t N > static void ReverseBits(std::bitset< N >& b);
  template< size_t N > static std::vector< double > GenerateStripsNumbers(int init = 1);
  static std::vector< double > GenerateStripsNumbers(size_t n, int init = 1);
  static std::vector< double > GenerateFullProfileStripsBinsBorders(size_t n);

protected:
  QScopedPointer< AbstractCameraPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(AbstractCamera);
  Q_DISABLE_COPY(AbstractCamera);
};

QT_END_NAMESPACE
