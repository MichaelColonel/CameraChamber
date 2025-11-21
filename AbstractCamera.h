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

#include <QtSerialPort/QSerialPort>
#include <QByteArray>
#include <QObject>

#include <utility>

#include "typedefs.h"

class QTimer;
class QSettings;

class TGraph;
class TGraphErrors;
class TH1;

class AbstractCamera : public QObject {
  Q_OBJECT
public:
  static constexpr int RESOLUTION_16BIT = (1 << AdcResolutionType::ADC_16_BIT);
  static constexpr int RESOLUTION_18BIT = (1 << AdcResolutionType::ADC_18_BIT);
  static constexpr int RESOLUTION_20BIT = (1 << AdcResolutionType::ADC_20_BIT);

  static constexpr int ADC_WORDS_PER_CHANNEL = 2;
  static constexpr int ADC_CHANNELS_PER_CHUNK = ADC_WORDS_PER_CHANNEL * CHANNELS_PER_CHIP;
  static constexpr int BITS = CHAR_BIT * 2;
  static constexpr int BUFFER_SIZE = 3;

  AbstractCamera(const QString& commandPortDeviceName, const QString& dataPortDeviceName, QObject *parent = nullptr);
  virtual ~AbstractCamera();
  bool connect();
  void disconnect();
  bool isDeviceAlreadyConnected(const QString& otherPort);

public slots:
  void onCommandPortDataReady();
  void onDataPortDataReady();

signals:
  void acquisitionStarted();
  void acquisitionFinished();
  void commandWritten(QByteArray* command);

protected:
  void processRawData();
  virtual void processDataCounts() = 0;
  bool loadCalibration(const QString& cameraDirectory); // directory must contain ChipsPositions.json file and chips JSON files
  bool loadCalibration(QSettings* settings);
  bool saveCalibration(QSettings* settings);

  void updateGraph(int chip = 1, int strip = 1); // update raw counts time graph
  void updateProfiles(); // update profiles and histograms

  QString commandPortName;
  QString dataPortName;

  QSerialPort* commandPort{ nullptr };
  QSerialPort* dataPort{ nullptr };
  QTimer* timeoutTimer{ nullptr }; /// extraction (spill) or initiation timeout

  QByteArray commandResponseBuffer;
  QByteArray dataResponseBuffer;

  ChipChannelsCountsMap channelsCountsA; // Side-A
  ChipChannelsCountsMap channelsCountsB; // Side-B

  std::vector< int > chipsVerticalStripsVector;
  std::vector< int > chipsHorizontalStripsVector;

  std::pair< int, int > refCalibChipStrip = {1, 1};
  std::pair< int, int > refCalibAmplChipStripVertical = { 1, 1 };
  std::pair< int, int > refCalibAmplChipStripHorizontal = { 1, 1 };
  std::array< int, 4 > pedestalSignalGateArray = { 1, 100, 101, 200 };

  struct CameraResponse {
    int ChipsEnabled{ -1 };
    unsigned short ChipsEnabledCode{ 0x0FFF };
    int IntegrationTimeCode{ 1 }; // Integration time code(0...15)
    int AdcMode{ 16 }; // 16-bit or 20-bit (16 or 20)
    int CapacityCode{ 7 }; // Capacity code (0...7)
    bool ExternalStartState{ false };
  } cameraResponse;

  struct ChannelInfo {
    double pedMeanA{ -1. };
    double pedMeanB{ -1. };
    double pedMom2A{ -1. };
    double pedMom2B{ -1. };
    double sigMeanA{ -1. };
    double sigMeanB{ -1. };
    double sigMom2A{ -1. };
    double sigMom2B{ -1. };
    double sigCountA{ -1. };
    double sigCountB{ -1. };
    double sigSumA{ -1. };
    double sigSumB{ -1. };
  };

  TGraph* channelGraph{ nullptr };
  TH1* channelSideHist[2]{ nullptr, nullptr };
  TGraphErrors* profileGraph[2]{ nullptr, nullptr };

private:
  Q_DISABLE_COPY(AbstractCamera);
};
