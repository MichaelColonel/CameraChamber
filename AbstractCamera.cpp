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

// RapidJSON includes
#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/filereadstream.h>

#include "AbstractCamera.h"
#include "AdcAmplitudeCalibrationData.h"

#include <TH1.h>
#include <TH2.h>
#include <TTree.h>
#include <TGraphErrors.h>
#include <TFile.h>

#include <QDebug>
#include <QDir>
#include <QScopedPointer>
#include <QSettings>
#include <QDateTime>

#include <utility>
#include <cstring>

QT_BEGIN_NAMESPACE

class AbstractCameraPrivate
{
  Q_DECLARE_PUBLIC(AbstractCamera);
protected:
  AbstractCamera* const q_ptr;
public:
  AbstractCameraPrivate(AbstractCamera &object);
  virtual ~AbstractCameraPrivate();
  CameraProfileType getProfileForChip(int chip) const;
  bool checkLastCommandWrittenAndRespose();
  bool checkLastCommandWrittenAndRespose(const QByteArray& responseBuffer);
  bool saveData();

  AbstractCamera::CameraDeviceData cameraData;

  QScopedPointer< QSerialPort > commandPort;
  QScopedPointer< QSerialPort > dataPort;

  QByteArray commandResponseBuffer; // command response
  QByteArray adcDataBuffer; // data response
  QList< QByteArray > initiateCommandsList;
  QByteArray lastCommandWritten;
  bool initiationFlag{ false };
  bool acquisitionFlag{ false };
  std::vector< int > chipsAddressesVector; // acquired chips addresses
  struct CameraResponse cameraResponse;

  // members for time and profile reconstruction
  // used by derived classes
  ChipChannelsCountsMap channelsCountsA; // Side-A
  ChipChannelsCountsMap channelsCountsB; // Side-B

  std::vector< int > verticalProfileChipsVector; // chips for horizontal profile
  std::vector< int > horizontalProfileChipsVector; // chips for vertical profile

  std::vector< double > verticalProfileVector; // vertical profile amplitude vector
  std::vector< double > horizontalProfileVector; // horizontal profile amplitude vector

  std::vector< double > verticalProfileStripsNumbersVector; // strips numbers for vertical profile amplitude vector
  std::vector< double > horizontalProfileStripsNumbersVector; // strips numbers for horizontal profile amplitude vector

  std::vector< ChipChannelPair > verticalProfileChipChannelStripsVector;
  std::vector< ChipChannelPair > horizontalProfileChipChannelStripsVector;

  // Data from JSON parameters file of the camera
  // ADC-Offset tangent chip calibration A, B. Amplitude sensitivity calibration
  ChipCapacityIntegrationTimeCalibrationMap chipCalibrationMap;
  ChipBrokenChannelsMap chipChannelsStripsBrokenMap; // chip = key, channels (ir strips) = value

  // Data stored in Qt settings file for each camera
  ChipChannelPair refAdcChipChannelVertical = { 2, 1 };
  ChipChannelPair refAdcChipChannelHorizontal = { 2, 1 };
  ChipChannelPair refAmpChipChannelVertical = { 2, 1 };
  ChipChannelPair refAmpChipChannelHorizontal = { 2, 1 };
  std::array< int, 4 > pedestalSignalGateArray = { 1, 100, 101, 200 };

  std::map< ChipChannelPair, ChannelInfoPair > chipChannelInfoMap;
  std::vector< std::vector< double > > profileFramesVector;

  int dataNumber{ 1 };
  bool onceTimeExternalStartFlag{ false };
  QDateTime dataDateTime;
  TDirectory* rootDir{ nullptr };

  const QByteArray FirstContactCommand = QByteArray(1, 'A');
  const QByteArray OnceTimeExternalStartCommand = QByteArray("S\0\0", AbstractCamera::BUFFER_SIZE);
  const QByteArray AcquisitionCommand = QByteArray("B\0\0", AbstractCamera::BUFFER_SIZE);
  const QByteArray ChipResetCommand = QByteArray("H\0\0", AbstractCamera::BUFFER_SIZE);
  const QByteArray AlteraResetCommand = QByteArray("R\0\0", AbstractCamera::BUFFER_SIZE);
  const QByteArray WriteCapacitiesCommand = QByteArray("G\0\0", AbstractCamera::BUFFER_SIZE); // write DDC232 configuration register
  const QByteArray ListChipsEnabledCommand = QByteArray("L\0\0", AbstractCamera::BUFFER_SIZE); // list of chips enabled
};

AbstractCameraPrivate::AbstractCameraPrivate(AbstractCamera &object)
  :
  q_ptr(&object),
  commandPort(new QSerialPort(&object)),
  dataPort(new QSerialPort(&object))
{
}

AbstractCameraPrivate::~AbstractCameraPrivate()
{
}

bool AbstractCameraPrivate::checkLastCommandWrittenAndRespose()
{
  return this->checkLastCommandWrittenAndRespose(this->commandResponseBuffer);
}

bool AbstractCameraPrivate::checkLastCommandWrittenAndRespose(const QByteArray& responseBuffer)
{
  if (responseBuffer.isEmpty() || this->lastCommandWritten.isEmpty())
  {
    return false;
  }

  char responseHeader = responseBuffer.at(0);
  char lastWrittenHeader = this->lastCommandWritten.at(0);

  // Check response command header
  auto CheckRespHeader = [responseHeader](char headerCharacter) -> bool { return responseHeader == headerCharacter; };
  // Check last written command header
  auto CheckLastWritHeader = [lastWrittenHeader](char headerCharacter) -> bool { return lastWrittenHeader == headerCharacter; };

  const std::string commandHeadersOK("GMRHAOCIDSELP");
  const std::string commandHeadersNO("OMIEP");

  if (responseBuffer.endsWith("OK") || responseBuffer.endsWith("Welcome"))
  {
    // Check response command header is OK
    auto resComHeader = std::find_if(std::begin(commandHeadersOK), std::end(commandHeadersOK), CheckRespHeader);
    // Check last written command header is OK
    auto writtenComHeader = std::find_if(std::begin(commandHeadersOK), std::end(commandHeadersOK), CheckLastWritHeader);
    bool resComHeaderOK = resComHeader != std::end(commandHeadersOK);
    bool writtenComHeaderOK = writtenComHeader != std::end(commandHeadersOK);
    if (resComHeaderOK && writtenComHeaderOK && *resComHeader == *writtenComHeader)
    {
      return true;
    }
  }

  if (responseBuffer.endsWith("NO"))
  {
    // Check response command header is NO
    auto resComHeader = std::find_if(std::begin(commandHeadersNO), std::end(commandHeadersNO), CheckRespHeader);
    // Check last written command header is NO
    auto writtenComHeader = std::find_if(std::begin(commandHeadersNO), std::end(commandHeadersNO), CheckLastWritHeader);
    bool resComHeaderNO = resComHeader != std::end(commandHeadersNO);
    bool writtenComHeaderNO = writtenComHeader != std::end(commandHeadersNO);
    if (resComHeaderNO && writtenComHeaderNO && *resComHeader == *writtenComHeader)
    {
      qCritical() << Q_FUNC_INFO << ": Last command with header \"" << lastWrittenHeader << "\" : parameter isn't set";
      return false;
    }
  }
  return false;
}

CameraProfileType AbstractCameraPrivate::getProfileForChip(int chip) const
{
  const std::vector< int >& vProf = this->verticalProfileChipsVector;
  const std::vector< int >& hProf = this->horizontalProfileChipsVector;

  if (std::find(vProf.begin(), vProf.end(), chip) != vProf.end())
  {
    return CameraProfileType::PROFILE_VERTICAL;
  }
  else if (std::find(hProf.begin(), hProf.end(), chip) != hProf.end())
  {
    return CameraProfileType::PROFILE_HORIZONTAL;
  }
  return CameraProfileType::CameraProfileType_Last;
}

bool AbstractCameraPrivate::saveData()
{
  if (!this->rootDir)
  {
    return false;
  }
  const int& capacityCode = this->cameraResponse.CapacityCode;
  const int& integrationTimeCode = this->cameraResponse.IntegrationTimeCode;

  unsigned int mode = (capacityCode << 4) | integrationTimeCode;

  std::vector< char > buffer(this->adcDataBuffer.size(), 0);
  std::copy(this->adcDataBuffer.begin(), this->adcDataBuffer.end(), buffer.begin());

  std::vector< char >::size_type bufferSize = buffer.size();

  QString spillDataString = QObject::tr("%1_%2").arg(this->dataNumber++).arg(this->dataDateTime.toString("ddMMyyyy_hhmmss"));

  QByteArray spillString = spillDataString.toLatin1();
  const char* titleString = "Camera events at date and time";
  std::unique_ptr< TTree > spillTree(new TTree(spillString.data(), titleString));
  int dataBits = this->cameraResponse.AdcMode;

  spillTree->Branch("respChipsEnabled", &this->cameraResponse.ChipsEnabled, "respChipsEnabled/I"); // not used
  spillTree->Branch("respChipsEnabledCode", &this->cameraResponse.ChipsEnabledCode, "respChipsEnabledCode/s");
  spillTree->Branch("respIntTime", &this->cameraResponse.IntegrationTimeCode, "respIntTime/I");
  spillTree->Branch("respCapacity", &this->cameraResponse.CapacityCode, "respCapacity/I");
  spillTree->Branch("respExtStart", &this->cameraResponse.ExternalStartState, "respExtStart/O");
  spillTree->Branch("respAdcMode", &this->cameraResponse.AdcMode, "respAdcMode/I");

  spillTree->Branch("mode", &mode, "mode/i"); // not used
  spillTree->Branch("bufferSize", &bufferSize, "bufferSize/l");
  spillTree->Branch("adcMode", &dataBits, "adcMode/I"); // not used

  const std::vector< char >* bufferPtr = &buffer;
  spillTree->Branch("bufferVector", "std::vector< char >", &bufferPtr);

  std::vector< int >::size_type chipsAcquired = this->chipsAddressesVector.size();
  std::vector< int >* chipsAcquiredPtr = &this->chipsAddressesVector;
  spillTree->Branch("chipsAcquired", &chipsAcquired, "chipsAcquired/l");
  spillTree->Branch("chipsAcquiredVector", "std::vector< int >", &chipsAcquiredPtr);
  spillTree->Fill();
  this->rootDir->WriteTObject(spillTree.get());
  return true;
}

AbstractCamera::AbstractCamera()
  :
  QObject(nullptr),
  d_ptr(new AbstractCameraPrivate(*this))
{
}

AbstractCamera::AbstractCamera(const CameraDeviceData& data, QObject *parent)
  :
  QObject(parent),
  d_ptr(new AbstractCameraPrivate(*this))
{
  Q_D(AbstractCamera);

  d->cameraData = data;
//  int capacityCode = -1;
//  int timeCode = -1;
//  if (d->cameraData.ID == "Camera4")
//  {
//    capacityCode = 0;
//    timeCode = 1;
//  }
  if (this->loadCameraData(d->cameraData.DataDirectory))
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": \" << d->cameraData.ID << \" data successfully loaded";
#endif
    d->verticalProfileVector.resize(d->verticalProfileChipsVector.size() * CHANNELS_PER_CHIP);
    std::fill(std::begin(d->verticalProfileVector), std::end(d->verticalProfileVector), 0.);

    d->horizontalProfileVector.resize(d->horizontalProfileChipsVector.size() * CHANNELS_PER_CHIP);
    std::fill(std::begin(d->horizontalProfileVector), std::end(d->horizontalProfileVector), 0.);

    d->verticalProfileStripsNumbersVector = GenerateStripsNumbers(d->verticalProfileVector.size());
    d->horizontalProfileStripsNumbersVector = GenerateStripsNumbers(d->horizontalProfileVector.size());
  }

  if (d->chipCalibrationMap.size())
  {
    ChipCapacityCalibrationData checkCapTimeAdcData(d->chipCalibrationMap);
    ChipChannelCalibrationMap linA = checkCapTimeAdcData.getFirstAdcLinearCalibrationA();
    ChipChannelCalibrationMap linB = checkCapTimeAdcData.getFirstAdcLinearCalibrationB();
    ChipChannelCalibrationMap ampl = checkCapTimeAdcData.getFirstAmpUniformCalibration();
    if (!linA.empty() && !linB.empty() && !ampl.empty())
    {
      emit logMessage("Good, some calibration data is present", d->cameraData.ID, Qt::green);
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": \" << d->cameraData.ID << \" some calibration data is present.";
#endif
    }
  }
}

AbstractCamera::~AbstractCamera()
{
}

bool AbstractCamera::connect()
{
  Q_D(AbstractCamera);
  bool connected = false;
  if (d->commandPort && !d->commandPort->isOpen())
  {
    d->lastCommandWritten.clear();
    d->adcDataBuffer.clear();

    d->commandPort->setPortName(d->cameraData.CommandDeviceName);
    d->commandPort->setBaudRate(QSerialPort::Baud57600);
    d->commandPort->setDataBits(QSerialPort::Data8);
    d->commandPort->setParity(QSerialPort::NoParity);
    d->commandPort->setStopBits(QSerialPort::OneStop);
    d->commandPort->setFlowControl(QSerialPort::NoFlowControl);

    QObject::connect(d->commandPort.data(), SIGNAL(readyRead()),
      this, SLOT(onCommandPortDataReady()));
    QObject::connect(d->commandPort.data(), SIGNAL(bytesWritten(qint64)),
      this, SLOT(onCommandPortBytesWritten(qint64)));
    QObject::connect(d->commandPort.data(), SIGNAL(errorOccurred(QSerialPort::SerialPortError)),
      this, SLOT(onCommandPortError(QSerialPort::SerialPortError)));

    connected = d->commandPort->open(QIODevice::ReadWrite);
  }

  if (d->dataPort && !d->dataPort->isOpen())
  {
    d->dataPort->setPortName(d->cameraData.DataDeviceName);
    d->dataPort->setBaudRate(QSerialPort::Baud57600);
    d->dataPort->setDataBits(QSerialPort::Data8);
    d->dataPort->setParity(QSerialPort::NoParity);
    d->dataPort->setStopBits(QSerialPort::OneStop);
    d->dataPort->setFlowControl(QSerialPort::NoFlowControl);

    QObject::connect(d->dataPort.data(), SIGNAL(readyRead()),
      this, SLOT(onDataPortDataReady()));
    // read-only port
//    QObject::connect(d->dataPort.data(), SIGNAL(bytesWritten(qint64)),
//      this, SLOT(onDataPortBytesWritten(qint64)));
    QObject::connect(d->dataPort.data(), SIGNAL(errorOccurred(QSerialPort::SerialPortError)),
      this, SLOT(onDataPortError(QSerialPort::SerialPortError)));

    connected &= d->dataPort->open(QIODevice::ReadOnly);
  }

  return connected;
}

void AbstractCamera::disconnect()
{
  Q_D(AbstractCamera);
  bool connected = true;

  if (d->commandPort && d->commandPort->isOpen())
  {
    d->commandPort->flush();
    d->commandPort->close();
  }
  else if (d->commandPort && !d->commandPort->isOpen())
  {
  }

  if (d->dataPort && d->dataPort->isOpen())
  {
    d->dataPort->flush();
    d->dataPort->close();
  }
  else if (d->dataPort && !d->dataPort->isOpen())
  {
  }
  d->dataNumber = 1;
  d->cameraData = AbstractCamera::CameraDeviceData();
  connected = false;
  Q_UNUSED(connected);
}

bool AbstractCamera::isDeviceAlreadyConnected()
{
  Q_D(AbstractCamera);
  bool com = false, data = false;
  if (d->commandPort && d->commandPort->isOpen())
  {
    com = true;
  }
  if (d->dataPort && d->dataPort->isOpen())
  {
    data = true;
  }
  return (com && data);
}

bool AbstractCamera::isDeviceAlreadyConnected(const QString &otherPort)
{
  Q_D(AbstractCamera);
  if (d->commandPort && d->commandPort->isOpen())
  {
    return d->cameraData.CommandDeviceName == otherPort;
  }
  if (d->dataPort && d->dataPort->isOpen())
  {
    return d->cameraData.DataDeviceName == otherPort;
  }
  return false;
}

bool AbstractCamera::isInitiationListEmpty() const
{
  Q_D(const AbstractCamera);
  return d->initiateCommandsList.isEmpty();
}

void AbstractCamera::onCommandPortDataReady()
{
  Q_D(AbstractCamera);
  QByteArray portData = d->commandPort->readAll();

#if !QT_NO_DEBUG
  qDebug() << Q_FUNC_INFO << ": Data recieved size: " << portData.size();
#endif

  if (d->initiationFlag && d->initiateCommandsList.isEmpty())
  {
    d->initiationFlag = false;
    emit initiationProgress(0);
    emit initiationFinished();
  }

  d->commandResponseBuffer.push_back(portData);
  const quint8* data = reinterpret_cast< quint8* >(d->commandResponseBuffer.data());

  if (d->commandResponseBuffer.size() == 4 && d->checkLastCommandWrittenAndRespose())
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << d->commandResponseBuffer;
#endif
    switch (data[0])
    {
    case 'A':
      d->cameraResponse.ExternalStartState = bool(data[1] - '0');
      if (d->lastCommandWritten.size() == 1 && d->lastCommandWritten.startsWith('A')) // firmware response
      {
        emit firmwareCommandBufferIsReset();
        d->commandResponseBuffer.clear();
        d->lastCommandWritten.clear();
        d->initiateCommandsList.clear();
        return;
      }
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set external interrupt command response: " << d->cameraResponse.ExternalStartState;
#endif
      emit logMessage(tr("External start flag: %1").arg(d->cameraResponse.ExternalStartState), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'C':
      d->cameraResponse.CapacityCode = data[1] - '0';
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set chip capacity command response: " << d->cameraResponse.CapacityCode;
#endif
      emit logMessage(tr("Chip capacity code: %1").arg(d->cameraResponse.CapacityCode), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'D':
      d->cameraResponse.AdcMode = (data[1] - '0') ? ADC_20_BIT : ADC_16_BIT;
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": ADC resolution command response: " << d->cameraResponse.AdcMode;
#endif
      emit logMessage(tr("ADC resolution: %1").arg(d->cameraResponse.AdcMode), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'G':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Capasities have been written into chip";
#endif
      emit logMessage(tr("Capasities have been written into chip"), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'I':
      d->cameraResponse.IntegrationTimeCode = (data[1] - 'A');
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set integration time command response code: " << char(data[1]);
      qDebug() << Q_FUNC_INFO << ": Set integration time is: " << (d->cameraResponse.IntegrationTimeCode + 1) * 2 << " ms";
#endif
      emit logMessage(tr("Integration time: %1 ms").arg((d->cameraResponse.IntegrationTimeCode + 1) * 2), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'M':
      d->cameraResponse.ChipsEnabled = data[1] - 'A';
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Number of chips enabled: " << d->cameraResponse.ChipsEnabled;
#endif
      emit logMessage(tr("Number of chips enabled: %1").arg(d->cameraResponse.ChipsEnabled), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'O':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set number of chips command response code: " << char(data[1]);
      qDebug() << Q_FUNC_INFO << ": Number of chips (MUST be 12) are: " << data[1] - 'A';
#endif
      emit logMessage(tr("Number of chips (MUST be 12): %1").arg(data[1] - 'A'), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    default:
      break;
    }
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    return;
  }
  else if (d->commandResponseBuffer.size() == 4 && !d->checkLastCommandWrittenAndRespose())
  {
    qCritical() << Q_FUNC_INFO << ": Wrong or strange command response: " << d->commandResponseBuffer;
    emit logMessage(tr("Wrong or strange command response"), d->cameraData.ID, Qt::yellow);
    emit commandWritten(d->lastCommandWritten);
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    return;
  }
  if (d->commandResponseBuffer.size() == 3 && d->checkLastCommandWrittenAndRespose())
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << d->commandResponseBuffer;
#endif
    switch (data[0])
    {
    case 'G':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": DDC232 configuration register is set";
#endif
      emit logMessage(tr("DDC232 configuration register is set"), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'R':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": ALTERA reset";
#endif
      emit logMessage(tr("ALTERA reset"), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'H':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Chip reset";
#endif
      emit logMessage(tr("Chip reset"), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'S':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Single shot acquisition with external start";
#endif
      emit logMessage(tr("Single shot acquisition..."), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'E':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Devices enabled are set";
#endif
      emit logMessage(tr("Chips are enabled"), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'P':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": New number of samples are set";
#endif
      {
        const unsigned char* comPtr = reinterpret_cast< unsigned char * >(d->lastCommandWritten.data());
        d->cameraResponse.AdcSamples = (comPtr[2] << CHAR_BIT) | comPtr[1];
      }
      emit logMessage(tr("New number of samples is set: %1").arg(d->cameraResponse.AdcSamples), d->cameraData.ID, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    default:
      break;
    }
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    return;
  }
  else if (d->commandResponseBuffer.size() == 3 && !d->checkLastCommandWrittenAndRespose())
  {
    qCritical() << Q_FUNC_INFO << ": Wrong or strange command response: " << d->commandResponseBuffer;
    emit logMessage(tr("Wrong or strange command response"), d->cameraData.ID, Qt::yellow);
    emit commandWritten(d->lastCommandWritten);
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    return;
  }

  if (d->commandResponseBuffer.startsWith("Start") && d->commandResponseBuffer.size() == 5)
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": Acquisition begins";
#endif
    d->acquisitionFlag = true;
    d->dataDateTime = QDateTime::currentDateTime();
    emit acquisitionStarted();
  }
  if (d->commandResponseBuffer.startsWith("Start") && d->commandResponseBuffer.endsWith("Finish"))
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": Acquisition ends: " << d->commandResponseBuffer.size() << " bytes of data";
#endif
    /// Save raw data
    this->processRawData();
    // store acquisition data into a file
    d->saveData();
    /// Data is safe => clear acquisition buffer
    emit commandWritten(d->lastCommandWritten);
    d->adcDataBuffer.clear();
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    emit acquisitionFinished();
    d->acquisitionFlag = false;
    d->onceTimeExternalStartFlag = false;
    return;
  }
  if (d->commandResponseBuffer.startsWith('L')
    && d->commandResponseBuffer.size() == 15
    && d->checkLastCommandWrittenAndRespose())
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": List of enabled chips is recieved: " << d->commandResponseBuffer;
#endif

    std::bitset< CHIPS_PER_CAMERA > enabledChips;
    for (int i = 0; i < CHIPS_PER_CAMERA; ++i)
    {
      enabledChips.set(i, data[i + 1] - '0');
    }
    d->cameraResponse.ChipsEnabledCode = enabledChips.to_ulong();
    d->cameraResponse.ChipsEnabled = enabledChips.count();

    emit logMessage(tr("List of enabled chips is recieved"), d->cameraData.ID, Qt::green);
    emit commandWritten(d->lastCommandWritten);
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    return;
  }

  if (d->commandResponseBuffer.startsWith("Start") && d->commandResponseBuffer.contains("Abort"))
  {
    int restIndexOf = d->commandResponseBuffer.indexOf("Abort") + std::strlen("Abort");
    QByteArray restResponse = d->commandResponseBuffer.mid(restIndexOf);
    qCritical() << Q_FUNC_INFO << ": Abort found! Rest response: " << restResponse;
    emit logMessage(tr("Abort received!"), d->cameraData.ID, Qt::yellow);
    // Parse rest response
    // if response is OK
    if (d->checkLastCommandWrittenAndRespose(restResponse))
    {
      d->commandResponseBuffer.clear();
      d->lastCommandWritten.clear();
    }
    else
    {
      d->commandResponseBuffer = restResponse;
    }
    d->onceTimeExternalStartFlag = false;
    return;
  }
  if (d->commandResponseBuffer.startsWith("Welcome") && d->commandResponseBuffer.size() == std::strlen("Welcome"))
  {
    emit logMessage(tr("Firmware command buffer reset is finished!"), d->cameraData.ID, Qt::green);
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    d->initiateCommandsList.clear();
    return;
  }
}

void AbstractCamera::onCommandPortBytesWritten(qint64 bytes)
{
  Q_D(AbstractCamera);
  if (bytes == 1)
  {
    emit logMessage(tr("Firmware command buffer reset..."), d->cameraData.ID, Qt::yellow);
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": one byte has been written.";
#endif
  }
  else if (bytes > 1)
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": " << bytes << " bytes have been written.";
#endif
  }
  if (d->initiateCommandsList.size() && d->lastCommandWritten.size() == bytes)
  {
    emit initiationProgress(static_cast< int >(d->initiateCommandsList.size()));
    d->initiateCommandsList.pop_front();
  }
  if (d->lastCommandWritten.size() == bytes && d->lastCommandWritten == d->AcquisitionCommand)
  {
    d->onceTimeExternalStartFlag = false;
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": Acquisition command written";
#endif
  }
  else if (d->lastCommandWritten.size() == bytes && d->lastCommandWritten == d->OnceTimeExternalStartCommand)
  {
    d->onceTimeExternalStartFlag = true;
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": Waiting for the external start...";
#endif
  }
}

void AbstractCamera::onCommandPortError(QSerialPort::SerialPortError)
{
}


void AbstractCamera::writeNextCommandFromInitiationList()
{
  Q_D(AbstractCamera);
  if (d->initiateCommandsList.isEmpty())
  {
    return;
  }

  d->lastCommandWritten = d->initiateCommandsList.front();
  if (d->commandPort && d->commandPort->isOpen())
  {
    emit initiationProgress(static_cast< int >(d->initiateCommandsList.size()));
    this->writeCommand(d->lastCommandWritten);
  }
}

void AbstractCamera::writeLastCommandOnceAgain()
{
  Q_D(AbstractCamera);
  if (d->commandPort && d->commandPort->isOpen())
  {
    this->writeCommand(d->lastCommandWritten);
  }
}

void AbstractCamera::onDataPortDataReady()
{
  Q_D(AbstractCamera);
  if (d->dataPort)
  {
    QByteArray portData = d->dataPort->readAll();
    d->adcDataBuffer.push_back(portData);
  }
}

void AbstractCamera::onDataPortBytesWritten(qint64)
{
  // Read-only port
}

void AbstractCamera::onDataPortError(QSerialPort::SerialPortError)
{
}

void AbstractCamera::processRawData()
{
  Q_D(AbstractCamera);
  d->channelsCountsA.clear();
  d->channelsCountsB.clear();

  QString dataFileName = QDir::tempPath() + QString(QDir::separator()) + "raw_data_bits.txt";
  QScopedPointer< QFile > dataFile(new QFile(dataFileName));
  bool res = dataFile->open(QIODevice::WriteOnly | QIODevice::Text);
  QTextStream dataFileStream(dataFile.get()); // dump bits to temp file for debug

  const QByteArray& arr = d->adcDataBuffer;
  constexpr int BYTES_CHUNK = 4; // bytes per adc data chunk (channel, chip, ADC count, sync bits)
  using DataIntegrity = std::array< int, BYTES_CHUNK >;
  DataIntegrity findData{ 0, 0, 0, 0 };
  for (int i = 0; i < arr.size() - BYTES_CHUNK; i += BYTES_CHUNK)
  {
    std::bitset< CHAR_BIT > d0(arr.at(i)), d1(arr.at(i + 1)), d2(arr.at(i + 2)), d3(arr.at(i + 3));
    if (!d0.test(7) && d0.test(0))
    {
      findData[0] += 1;
    }
    if (!d1.test(7) && d1.test(0))
    {
      findData[1] += 1;
    }
    if (!d2.test(7) && d2.test(0))
    {
      findData[2] += 1;
    }
    if (!d3.test(7) && d3.test(0))
    {
      findData[3] += 1;
    }
  }
  DataIntegrity::iterator findDataIter = std::max_element(findData.begin(), findData.end());
  int offset = findDataIter - findData.begin();

#if !QT_NO_DEBUG
  qDebug() << Q_FUNC_INFO << ": Found data " << findData[0] << ' ' << findData[1] << ' ' << findData[2] << ' ' << findData[3] << ' ' << offset;
#endif

  d->chipsAddressesVector.clear();
  if (offset >= arr.size() - BYTES_CHUNK)
  {
    qWarning() << Q_FUNC_INFO << ": Raw data offset is way too high, no correct data left";
    return;
  }

  // dump raw data bits for debug
  if (res)
  {
    for (int i = offset; i < arr.size() - BYTES_CHUNK; i += BYTES_CHUNK)
    {
      std::bitset< CHAR_BIT > d0(arr.at(i)), d1(arr.at(i + 1)), d2(arr.at(i + 2)), d3(arr.at(i + 3));

      QString string_d0 = QString::fromStdString(d0.to_string());
      QString string_d1 = QString::fromStdString(d1.to_string());
      QString string_d2 = QString::fromStdString(d2.to_string());
      QString string_d3 = QString::fromStdString(d3.to_string());
      dataFileStream << qSetFieldWidth(10) << string_d0 << string_d1 << string_d2 << string_d3 << '\n';
    }
  }

  constexpr int SIDE_BIT = 6;
  constexpr int CHIP_BITS = 4;
  std::map< int, bool > chipPresenceMap;
  for (int i = offset; i < arr.size() - BYTES_CHUNK; i += BYTES_CHUNK)
  {
    std::bitset< CHAR_BIT > d0(arr.at(i)), d1(arr.at(i + 1)), d2(arr.at(i + 2)), d3(arr.at(i + 3));
    std::bitset< CHIP_BITS > chipBits((d1 >> CHIP_BITS).to_ulong());

    this->ReverseBits(d0);
    this->ReverseBits(d2);
    this->ReverseBits(d3);
    this->ReverseBits(chipBits);

    bool sideFlag = d0.test(SIDE_BIT);
    int channel = static_cast<int>(((d0 >> 1) & std::bitset< CHAR_BIT >(0x1F)).to_ulong());
    int chip = chipBits.to_ulong() - CHIP_BITS;
    int count = static_cast< int >((d3.to_ulong() << CHAR_BIT) | d2.to_ulong());
    chipPresenceMap[chip] = true;
    std::array< std::vector< int >, CHANNELS_PER_CHIP >& channelsA = d->channelsCountsA[chip];
    std::array< std::vector< int >, CHANNELS_PER_CHIP >& channelsB = d->channelsCountsB[chip];
    std::vector< int >& channelVectorA = channelsA[channel];
    std::vector< int >& channelVectorB = channelsB[channel];
    if (sideFlag)
    {
      channelVectorA.push_back(count);
    }
    else
    {
      channelVectorB.push_back(count);
    }
  }
  dataFileStream.flush();
  res = dataFile->flush();
  if (res)
  {
    dataFile->close();
  }

  d->chipsAddressesVector.reserve(CHIPS_PER_CAMERA);
  for (const std::pair< int, bool >& chipFlagPair : chipPresenceMap)
  {
    int chipAddress = chipFlagPair.first;
    d->chipsAddressesVector.push_back(chipAddress);
  }
  std::sort(d->chipsAddressesVector.begin(), d->chipsAddressesVector.end());
}

template< size_t N >
void AbstractCamera::ReverseBits(std::bitset< N >& b)
{
  for (size_t i = 0; i < N / 2; ++i)
  {
    bool t = b[i];
    b[i] = b[N - i - 1];
    b[N - i - 1] = t;
  }
}

template< size_t N >
std::vector< double > AbstractCamera::GenerateStripsNumbers(int init)
{
  std::vector< double > v(N, 0.);
  std::iota(std::begin(v), std::end(v), init);
  return v;
}

std::vector< double > AbstractCamera::GenerateStripsNumbers(size_t n, int init)
{
  std::vector< double > v(n, 0.);
  std::iota(std::begin(v), std::end(v), init);
  return v;
}

std::vector< double > AbstractCamera::GenerateFullProfileStripsBinsBorders(size_t n)
{
  std::vector< double > v(n, 0.);
  v[0] = 0.0;
  constexpr double step1 = STRIP_STEP_PER_SIDE_MM / 2.; // 1. mm
  for (size_t i = 1; i < v.size(); ++i)
  {
    v[i] = v[i - 1] + step1;
  }
  return v;
}

bool AbstractCamera::loadCameraData(const QString &cameraDirectory, int capacityCode, int timeCode)
{
  Q_D(AbstractCamera);
//  d->capacityTimeAdcAmpCalibrationMap.clear();
  d->chipCalibrationMap.clear();
//  d->chipChannelCalibrationA.clear();
//  d->chipChannelCalibrationB.clear();
//  d->chipChannelCalibrationAmplitude.clear();

  // Load JSON descriptor file
  QString path = QDir::currentPath() + QDir::separator() \
    + cameraDirectory + QDir::separator() + "ChipsPosition.json";

  std::string jsonFileName = path.toStdString();

  FILE *fp = fopen(jsonFileName.c_str(), "r");
  if (!fp)
  {
    qCritical() << Q_FUNC_INFO << ": Can't open JSON file: " << jsonFileName.c_str();
    return false;
  }
  const size_t size = 100000;
  std::unique_ptr< char[] > buffer(new char[size]);
  rapidjson::FileReadStream fs(fp, buffer.get(), size);

  rapidjson::Document doc;
  if (doc.ParseStream(fs).HasParseError())
  {
    qCritical() << Q_FUNC_INFO << ": Can't parse JSON file: " << jsonFileName.c_str();
    fclose(fp);
    return false;
  }
  fclose(fp);

  const rapidjson::Value& chipChannels = doc["ChipChannels"];
  const rapidjson::Value& verticalChips = doc["VerticalChips"]; // chips for horizontal profile
  const rapidjson::Value& horizontalChips = doc["HorizontalChips"]; // chips for vertical profile
  const rapidjson::Value& chips = doc["Chips"]; // vector chips objects (calibration data)
  const rapidjson::Value& channelSpacingPerSideMM = doc["ChannelStepPerSideMM"]; // control value
  const rapidjson::Value& brokenStrips = doc["BrokenChipChannelsStrips"]; // vector of broken strips objects

  if (channelSpacingPerSideMM.IsDouble())
  {
    double stripSpacingPerSide = channelSpacingPerSideMM.GetDouble();
    if (std::abs(STRIP_STEP_PER_SIDE_MM - stripSpacingPerSide) > 0.0001)
    {
      return false;
    }
  }

  std::array< int, CHANNELS_PER_CHIP > channels;
  std::bitset< CHIPS_PER_CAMERA > chipChannelsReverse;
  if (chipChannels.IsArray() && chipChannels.Size() == CHANNELS_PER_CHIP)
  {
    for (rapidjson::SizeType i = 0; i < chipChannels.Size(); i++) // Uses SizeType instead of size_t
    {
      int pos = chipChannels[i].GetInt();
      channels[i] = pos;
    }
  }
  // vertical chips for horizontal profile
  if (verticalChips.IsArray())
  {
    d->horizontalProfileChipsVector.resize(verticalChips.Size());
    for (rapidjson::SizeType i = 0; i < verticalChips.Size(); i++) // Uses SizeType instead of size_t
    {
      d->horizontalProfileChipsVector[i] = verticalChips[i].GetInt();
    }
  }
  // horizontal chips for vertical profile
  if (horizontalChips.IsArray())
  {
    d->verticalProfileChipsVector.resize(horizontalChips.Size());
    for (rapidjson::SizeType i = 0; i < horizontalChips.Size(); i++) // Uses SizeType instead of size_t
    {
      d->verticalProfileChipsVector[i] = horizontalChips[i].GetInt();
    }
  }
  if (chips.IsArray())
  {
    for (rapidjson::SizeType i = 0; i < chips.Size(); i++) // Uses SizeType instead of size_t
    {
      int position = -1;
      int reverseChannelsFlag = -1;
      std::string calibrationFileName;
      for (rapidjson::Value::ConstMemberIterator layerIter = chips[i].MemberBegin();
        layerIter != chips[i].MemberEnd(); ++layerIter)
      {
        const rapidjson::Value& name = layerIter->name;
        if (name.GetString() == std::string("Position"))
        {
          const rapidjson::Value& positionValue = layerIter->value;
          position = positionValue.GetInt();
        }
        if (name.GetString() == std::string("Name"))
        {
          const rapidjson::Value& nameValue = layerIter->value;
          calibrationFileName = std::string("Chip") + std::string(nameValue.GetString()) + ".json";
        }
        if (name.GetString() == std::string("ReverseChannels"))
        {
          const rapidjson::Value& nameValue = layerIter->value;
          reverseChannelsFlag = nameValue.GetInt();
        }
      }

      if (position != -1 && calibrationFileName.size())
      {
        QString filePath = QDir::currentPath() + QDir::separator() \
          + cameraDirectory + QDir::separator() \
          + QString::fromStdString(calibrationFileName);
        if (this->loadChipData(filePath, position, capacityCode, timeCode))
        {
          /// message here
        }
        else
        {
          /// error message here
        }
      }
      if (position != -1 && reverseChannelsFlag != -1)
      {
        chipChannelsReverse.set(position - 1, static_cast< bool >(reverseChannelsFlag));
      }
    }
  }

  if (brokenStrips.IsArray())
  {
    for (rapidjson::SizeType i = 0; i < brokenStrips.Size(); i++)
    {
      int chip = -1;
      std::vector< int > channelsStrips;
      for (rapidjson::Value::ConstMemberIterator iter = brokenStrips[i].MemberBegin();
        iter != brokenStrips[i].MemberEnd(); ++iter)
      {
        const rapidjson::Value& name = iter->name;
        if (name.GetString() == std::string("Chip"))
        {
          const rapidjson::Value& brokenChipValue = iter->value;
          chip = brokenChipValue.GetInt();
        }
        if (name.GetString() == std::string("ChannelsStrips"))
        {
          const rapidjson::Value& channelsStripsValue = iter->value;
          if (channelsStripsValue.IsArray())
          {
            channelsStrips.reserve(channelsStripsValue.Size());
            for (rapidjson::SizeType j = 0; j < channelsStripsValue.Size(); j++)
            {
              channelsStrips.push_back(channelsStripsValue[j].GetInt());
            }
          }
        }
      }
      if (chip > 1 && channelsStrips.size())
      {
        d->chipChannelsStripsBrokenMap.insert({chip, channelsStrips});
      }
    }
  }

  // Create vertical chips channels array depending on channels order (reverse or not)
  // for horizontal profile
  int i = 0;
  d->horizontalProfileChipChannelStripsVector.resize(d->horizontalProfileChipsVector.size() * CHANNELS_PER_CHIP);
  for (int verticalChip : d->horizontalProfileChipsVector)
  {
    if (chipChannelsReverse.test(verticalChip - 1))
    {
      for (auto iter = channels.rbegin(); iter != channels.rend(); ++iter)
      {
        d->horizontalProfileChipChannelStripsVector[i++] = std::make_pair(verticalChip, *iter);
      }
    }
    else
    {
      for (auto iter = channels.begin(); iter != channels.end(); ++iter)
      {
        d->horizontalProfileChipChannelStripsVector[i++] = std::make_pair(verticalChip, *iter);
      }
    }
  }
  // Create horizontal chips channels array depending on channels order (reverse or not)
  i = 0;
  d->verticalProfileChipChannelStripsVector.resize(d->verticalProfileChipsVector.size() * CHANNELS_PER_CHIP);
  for (int horizontalChip : d->verticalProfileChipsVector)
  {
    if (chipChannelsReverse.test(horizontalChip - 1))
    {
      for (auto iter = channels.rbegin(); iter != channels.rend(); ++iter)
      {
        d->verticalProfileChipChannelStripsVector[i++] = std::make_pair(horizontalChip, *iter);
      }
    }
    else
    {
      for (auto iter = channels.begin(); iter != channels.end(); ++iter)
      {
        d->verticalProfileChipChannelStripsVector[i++] = std::make_pair(horizontalChip, *iter);
      }
    }
  }
  return true;
}

bool AbstractCamera::loadChipData(const QString& filePath, int position, int capacityCode, int timeCode)
{
  Q_D(AbstractCamera);
  // Load JSON descriptor file
  std::string jsonFileName = filePath.toStdString();
  if ((position < 1) || (position > CHIPS_PER_CAMERA))
  {
    return false;
  }
  // Load JSON descriptor file
  FILE *fp = fopen(jsonFileName.c_str(), "r");
  if (!fp)
  {
    qCritical() << Q_FUNC_INFO << ": Can't open JSON file: " << jsonFileName.c_str();
    return false;
  }
  const size_t size = 100000;
  std::unique_ptr< char[] > buffer(new char[size]);
  rapidjson::FileReadStream fs(fp, buffer.get(), size);

  rapidjson::Document d1;
  if (d1.ParseStream(fs).HasParseError())
  {
    qCritical() << Q_FUNC_INFO << ": Can't parse JSON file: " << jsonFileName.c_str();
    fclose(fp);
    return false;
  }
  fclose(fp);

  const rapidjson::Value& chipParams = d1["ChipParameters"]; // vector chip calibration data

  if (chipParams.IsArray())
  {
    for (rapidjson::SizeType i = 0; i < chipParams.Size(); i++) // Uses SizeType instead of size_t
    {
      int chipTimeCode = -1;
      int chipCapacityCode = -1;
      std::vector< double > linA, linB, amp;
      double chipIntegTime = -1.;
      double chipCapacity = -1.;

      for (rapidjson::Value::ConstMemberIterator iter = chipParams[i].MemberBegin();
        iter != chipParams[i].MemberEnd(); ++iter)
      {
        const rapidjson::Value& name = iter->name;
        if (name.GetString() == std::string("IntegrationTimeCode"))
        {
          const rapidjson::Value& timeCodeValue = iter->value;
          chipTimeCode = timeCodeValue.GetInt();
        }
        if (name.GetString() == std::string("CapacityCode"))
        {
          const rapidjson::Value& capacityCodeValue = iter->value;
          chipCapacityCode = capacityCodeValue.GetInt();
        }
        if (name.GetString() == std::string("CountPerMilliVoltCalibration"))
        {
          const rapidjson::Value& calib = iter->value;
          if (calib.IsArray() && (calib.Capacity() == 2))
          {
            const rapidjson::Value& sideA = calib[0];
            const rapidjson::Value& sideB = calib[1];
            // side-A calibration
            for (rapidjson::SizeType pos = 0; pos < sideA.Size(); pos++)
            {
              linA.push_back(sideA[pos].GetDouble());
            }
            // side-B calibration
            for (rapidjson::SizeType pos = 0; pos < sideB.Size(); pos++)
            {
              linB.push_back(sideB[pos].GetDouble());
            }
          }
        }
        if (name.GetString() == std::string("AmplitudeCalibration"))
        {
          const rapidjson::Value& amplitude = iter->value;
          if (amplitude.IsArray())
          {
            // Amplitude calibration
            for (rapidjson::SizeType pos = 0; pos < amplitude.Size(); pos++)
            {
              amp.push_back(amplitude[pos].GetDouble());
            }
          }
        }
        if (name.GetString() == std::string("IntegrationTime"))
        {
          const rapidjson::Value& intTime = iter->value;
          chipIntegTime = intTime.GetDouble();
        }
        if (name.GetString() == std::string("Capacity"))
        {
          const rapidjson::Value& intTime = iter->value;
          chipCapacity = intTime.GetDouble();
        }
      }
      if (linA.size() == linB.size() && linA.size() == amp.size() && linA.size() == CHANNELS_PER_CHIP)
      {
        CapacityIntegrationTimeCodesPair capTimePair(chipCapacityCode, chipTimeCode);
        ChipCalibrationData adcCalibrationData;
        CapacityIntegrationTimeCalibrationMap& capTimeIntMap = d->chipCalibrationMap[position];
        adcCalibrationData.linearAdcCalibrationA = linA;
        adcCalibrationData.linearAdcCalibrationB = linB;
        adcCalibrationData.uniformAmplitudeCalibration = amp;
        capTimeIntMap[capTimePair] = adcCalibrationData;
      }
    }
  }
  return true;
}

QString AbstractCamera::getChipsAddresses() const
{
  Q_D(const AbstractCamera);
  QString str;
  if (!d->chipsAddressesVector.size())
  {
    return str;
  }
  else if (d->chipsAddressesVector.size() == 1)
  {
    return QObject::tr("%1").arg(d->chipsAddressesVector[0] + 1);
  }
  else
  {
    size_t s;
    for (s = 0; s < d->chipsAddressesVector.size() - 1; s++)
    {
      str += QObject::tr("%1, ").arg(d->chipsAddressesVector[s] + 1);
    }
    str += QObject::tr("%1").arg(d->chipsAddressesVector[s] + 1);
  }
  return str;
}

QByteArray AbstractCamera::getSetCapacityCommand(int capacityCode) const
{
  const int& max = AcquisitionParameters::MAX_CAPACITY_CODE;
  if ((capacityCode < 0) || (capacityCode > max))
  {
    return QByteArray();
  }
  std::bitset< 8 > chipIndex(0);
  std::bitset< 3 > capacity(capacityCode);
  chipIndex.set(5, capacity.test(0));
  chipIndex.set(6, capacity.test(1));
  chipIndex.set(7, capacity.test(2));

  unsigned char buf[BUFFER_SIZE] = { 'C' };
  buf[1] = static_cast<unsigned char>(chipIndex.to_ulong()); // data
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetIntegrationTimeCommand(int timeMs /* from 2...32 ms step 2 ms */) const
{
  const int& min = AcquisitionParameters::MIN_INTEGRATION_TIME_MS;
  const int& max = AcquisitionParameters::MAX_INTEGRATION_TIME_MS;

  if ((timeMs < min) || (timeMs > max))
  {
    return QByteArray();
  }
  unsigned char buf[BUFFER_SIZE] = { 'I' };
  buf[1] = (timeMs / 2) - 1; // data
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetAdcResolutionCommand(bool adc20Bit) const
{
  unsigned char buf[BUFFER_SIZE] = { 'D', adc20Bit }; // data
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetSamplesCommand(int samples) const
{
  const int& min = AcquisitionParameters::MIN_SAMPLES;
  const int& max = AcquisitionParameters::MAX_SAMPLES;
  if ((samples < min) || (samples > max))
  {
    return QByteArray();
  }
  unsigned char buf[BUFFER_SIZE] = { 'P' };
  buf[1] = samples & 0xFF; // low byte
  buf[2] = (samples >> CHAR_BIT) & 0xFF; // high byte
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetChipsEnabledCommand(int chipsEnabledCode) const
{
  const int& max = AcquisitionParameters::DEFAULT_CHIPS_ENABLED_CODE;
  if ((chipsEnabledCode < 1) || (chipsEnabledCode > max))
  {
    return QByteArray();
  }

  unsigned char buf[BUFFER_SIZE] = { 'E' };
  buf[1] = chipsEnabledCode & 0xFF; // low bite
  buf[2] = (chipsEnabledCode >> CHAR_BIT) & 0xFF; // high bite
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetNumberOfChipsCommand() const
{
  unsigned char buf[BUFFER_SIZE] = { 'O', CHIPS_PER_CAMERA };
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetExternalStartCommand(bool extStartFlag) const
{
  unsigned char buf[BUFFER_SIZE] = { 'A', extStartFlag };
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getStartAcquisitionCommand() const
{
  Q_D(const AbstractCamera);
  return d->AcquisitionCommand;
}

QByteArray AbstractCamera::getResetAlteraCommand() const
{
  Q_D(const AbstractCamera);
  return d->AlteraResetCommand;
}

QByteArray AbstractCamera::getResetChipCommand() const
{
  Q_D(const AbstractCamera);
  return d->ChipResetCommand;
}

QByteArray AbstractCamera::getWriteChipsCapacitiesCommand() const
{
  Q_D(const AbstractCamera);
  return d->WriteCapacitiesCommand;
}

QByteArray AbstractCamera::getListChipsEnabledCommand() const
{
  Q_D(const AbstractCamera);
  return d->ListChipsEnabledCommand;
}

QByteArray AbstractCamera::getFirstContactCommand() const
{
  Q_D(const AbstractCamera);
  return d->FirstContactCommand;
}

QByteArray AbstractCamera::getOnceTimeExternalStartCommand() const
{
  Q_D(const AbstractCamera);
  return d->OnceTimeExternalStartCommand;
}

bool AbstractCamera::getOnceTimeExternalStartFlag() const
{
  Q_D(const AbstractCamera);
  return d->onceTimeExternalStartFlag;
}

bool AbstractCamera::getChipChannelInfo(int chip, int channel, ChannelInfoPair &info)
{
  Q_D(const AbstractCamera);
  auto iter = std::find(std::begin(d->chipsAddressesVector), std::end(d->chipsAddressesVector), (chip - 1));
  if (iter == d->chipsAddressesVector.end())
  {
    return false;
  }
  if (channel < 1 && channel >= CHANNELS_PER_CHIP)
  {
    return false;
  }
  try
  {
    info = d->chipChannelInfoMap.at(std::make_pair(chip, channel));
  }
  catch (const std::out_of_range& ex)
  {
    qWarning() << Q_FUNC_INFO << ": chip channel info map except: " << QString(ex.what());
    return false;
  }
  return true;
}

void AbstractCamera::getChipChannelInfo(std::map< ChipChannelPair, ChannelInfoPair >& infoMap)
{
  Q_D(const AbstractCamera);
  infoMap.clear();
  for (const auto& chipChannelInfoPair : d->chipChannelInfoMap)
  {
    infoMap.insert(chipChannelInfoPair);
  }
}

void AbstractCamera::setInitiationList(const QByteArrayList &initList)
{
  Q_D(AbstractCamera);
  d->initiateCommandsList.clear();
  for (QByteArray command : initList)
  {
    d->initiateCommandsList.push_back(command);
  }
  d->initiationFlag = true;
  emit initiationStarted();
}

void AbstractCamera::writeCommand(const QByteArray &com)
{
  Q_D(AbstractCamera);
  if (com.isEmpty())
  {
    emit logMessage(tr("Input command is empty"), d->cameraData.ID, Qt::yellow);
    return;
  }
  d->lastCommandWritten = com;
  if (d->commandPort && d->commandPort->isOpen())
  {
    d->commandPort->write(d->lastCommandWritten);
  }
}

QString AbstractCamera::getCommandPortError() const
{
  Q_D(const AbstractCamera);
  if (d->commandPort)
  {
    return d->commandPort->errorString();
  }
  return QString();
}

QString AbstractCamera::getDataPortError() const
{
  Q_D(const AbstractCamera);
  if (d->dataPort)
  {
    return d->dataPort->errorString();
  }
  return QString();
}

AbstractCamera::CameraDeviceData AbstractCamera::getCameraData() const
{
  Q_D(const AbstractCamera);
  return d->cameraData;
}

struct CameraResponse AbstractCamera::getCameraResponse() const
{
  Q_D(const AbstractCamera);
  return d->cameraResponse;
}

int AbstractCamera::getChipsEnabledCode() const
{
  Q_D(const AbstractCamera);
  return int(d->cameraResponse.ChipsEnabledCode);
}

int AbstractCamera::getIntegrationTimeMs() const
{
  Q_D(const AbstractCamera);
  return int((d->cameraResponse.IntegrationTimeCode + 1) * 2);
}

int AbstractCamera::getCapacityCode() const
{
  Q_D(const AbstractCamera);
  return d->cameraResponse.CapacityCode;
}

int AbstractCamera::getAdcResolutionMode() const
{
  Q_D(const AbstractCamera);
  return d->cameraResponse.AdcMode;
}

int AbstractCamera::getAdcSamples() const
{
  Q_D(const AbstractCamera);
  return d->cameraResponse.AdcSamples;
}

const std::vector< int >& AbstractCamera::getChipsAddressesVector() const
{
  Q_D(const AbstractCamera);
  return d->chipsAddressesVector;
}

const std::array< int, 4 >& AbstractCamera::getPedestalSignalGate() const
{
  Q_D(const AbstractCamera);
  return d->pedestalSignalGateArray;
}

const std::vector< int >& AbstractCamera::getVerticalProfileChips() const
{
  Q_D(const AbstractCamera);
  return d->verticalProfileChipsVector;
}

const std::vector< int >& AbstractCamera::getHorizontalProfileChips() const
{
  Q_D(const AbstractCamera);
  return d->horizontalProfileChipsVector;
}

const std::vector< double >& AbstractCamera::getVerticalProfile() const
{
  Q_D(const AbstractCamera);
  return d->verticalProfileVector;
}

const std::vector< double >& AbstractCamera::getHorizontalProfile() const
{
  Q_D(const AbstractCamera);
  return d->horizontalProfileVector;
}

std::vector< double >& AbstractCamera::getVerticalProfile()
{
  Q_D(AbstractCamera);
  return d->verticalProfileVector;
}

std::vector< double >& AbstractCamera::getHorizontalProfile()
{
  Q_D(AbstractCamera);
  return d->horizontalProfileVector;
}

const std::vector< double >& AbstractCamera::getVerticalProfileStripsNumbers() const
{
  Q_D(const AbstractCamera);
  return d->verticalProfileStripsNumbersVector;
}

const std::vector< double >& AbstractCamera::getHorizontalProfileStripsNumbers() const
{
  Q_D(const AbstractCamera);
  return d->horizontalProfileStripsNumbersVector;
}

std::vector< std::vector< double > >& AbstractCamera::getProfileFramesVector()
{
  Q_D(AbstractCamera);
  return d->profileFramesVector;
}

const std::map< ChipChannelPair, ChannelInfoPair >& AbstractCamera::getChipChannelInfoMap() const
{
  Q_D(const AbstractCamera);
  return d->chipChannelInfoMap;
}

const std::vector< ChipChannelPair >& AbstractCamera::getVerticalProfileChipChannelStrips() const
{
  Q_D(const AbstractCamera);
  return d->verticalProfileChipChannelStripsVector;
}

const std::vector< ChipChannelPair >& AbstractCamera::getHorizontalProfileChipChannelStrips() const
{
  Q_D(const AbstractCamera);
  return d->horizontalProfileChipChannelStripsVector;
}

ChipChannelPair& AbstractCamera::getReferenceAdcHorizontalChipChannel()
{
  Q_D(AbstractCamera);
  return d->refAdcChipChannelHorizontal;
}

ChipChannelPair& AbstractCamera::getReferenceAdcVerticalChipChannel()
{
  Q_D(AbstractCamera);
  return d->refAdcChipChannelVertical;
}

ChipChannelPair& AbstractCamera::getReferenceAmplitudeHorizontalChipChannel()
{
  Q_D(AbstractCamera);
  return d->refAmpChipChannelHorizontal;
}

ChipChannelPair& AbstractCamera::getReferenceAmplitudeVerticalChipChannel()
{
  Q_D(AbstractCamera);
  return d->refAmpChipChannelVertical;
}

bool AbstractCamera::getAdcData(int chipIndex, int channelIndex, std::vector< int >& adcData, AdcTimeType dataType)
{
  Q_D(AbstractCamera);
  const std::vector<int>& sideA = d->channelsCountsA[chipIndex][channelIndex];
  const std::vector<int>& sideB = d->channelsCountsB[chipIndex][channelIndex];
  size_t sizeA = sideA.size();
  size_t sizeB = sideB.size();
  if (!sizeA || !sizeB)
  {
    qCritical() << Q_FUNC_INFO << ": Chip data size is empty for a chip address: chip number is " << chipIndex;
    qCritical() << Q_FUNC_INFO << tr(": Empty data for a chip %1, channel %2.").arg(chipIndex + 1).arg(channelIndex + 1);
    return false;
  }

  size_t sizeHalfPos = std::min< size_t >(sizeA, sizeB);

  bool res = true;
  switch (dataType)
  {
  case AdcTimeType::INTEGRATOR_AB:
    {
      adcData.clear();
      adcData.reserve(sizeHalfPos * 2);
      for (size_t i = 0; i < sizeHalfPos; i++)
      {
        adcData.push_back(sideA[i]);
        adcData.push_back(sideB[i]);
      }
    }
    break;
  case AdcTimeType::INTEGRATOR_A:
    adcData.resize(sideA.size());
    std::copy(std::begin(sideA), std::end(sideA), std::begin(adcData));
    break;
  case AdcTimeType::INTEGRATOR_B:
    adcData.resize(sideB.size());
    std::copy(std::begin(sideB), std::end(sideB), std::begin(adcData));
    break;
  default:
    res = false;
    break;
  }
  return res;
}

void AbstractCamera::setPedestalSignalGate(int pedMin, int pedMax, int sigMin, int sigMax)
{
  Q_D(AbstractCamera);
  d->pedestalSignalGateArray[0] = pedMin;
  d->pedestalSignalGateArray[1] = pedMax;
  d->pedestalSignalGateArray[2] = sigMin;
  d->pedestalSignalGateArray[3] = sigMax;
}

void AbstractCamera::processDataCounts(bool splitData,
  IntegratorType integType, ProfileRepresentationType profileType)
{
  Q_D(AbstractCamera);
  Q_UNUSED(splitData);
  Q_UNUSED(integType);
  Q_UNUSED(profileType);

  std::bitset< CHIPS_PER_CAMERA > chips(this->getChipsEnabledCode());

  int nofChips = static_cast< int >(chips.count());
  if (nofChips <= 0)
  {
    return;
  }
#if !QT_NO_DEBUG
  qDebug() << Q_FUNC_INFO << "Number of chips: " << nofChips;
#endif

  int intTimeMs = this->getIntegrationTimeMs();
#if !QT_NO_DEBUG
  qDebug() << Q_FUNC_INFO << "Integration time (ms): " << intTimeMs;
#endif
  int pedBegin = d->pedestalSignalGateArray[0] / intTimeMs;
  int pedEnd = d->pedestalSignalGateArray[1] / intTimeMs;
  int sigBegin = d->pedestalSignalGateArray[2] / intTimeMs;
  int sigEnd = d->pedestalSignalGateArray[3] / intTimeMs;

  if (pedBegin > pedEnd)
  {
    std::swap< int >(pedBegin, pedEnd);
  }
  if (sigBegin > sigEnd)
  {
    std::swap< int >(sigBegin, sigEnd);
  }
#if !QT_NO_DEBUG
  qDebug() << Q_FUNC_INFO << ": Gate time (ms): " << pedBegin << ' ' << pedEnd << ' ' << sigBegin << ' ' << sigEnd;
#endif

  if (d->chipsAddressesVector.size() != static_cast< size_t >(nofChips))
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of chips for reconsruction.\nChips enabled: %1, chips acquired: %2").arg(nofChips).arg(d->chipsAddressesVector.size());
    qWarning() << Q_FUNC_INFO << tr(": Chips enabled code: %1").arg(chips.to_ulong());
  }

  ChipCapacityCalibrationData adcData(d->chipCalibrationMap);
  bool foundLinA = false;
  bool foundLinB = false;
  bool foundAmpl = false;
//  bool integrationTimeIsFound = false;
  const int& capCode = d->cameraResponse.CapacityCode;
//  int& timeCode = d->cameraResponse.IntegrationTimeCode;

  ChipChannelCalibrationMap chipChannelCalibrationA;
  ChipChannelCalibrationMap chipChannelCalibrationB;
  ChipChannelCalibrationMap chipChannelCalibrationAmplitude;
  chipChannelCalibrationA = adcData.getAdcLinearCalibrationA(capCode, foundLinA);
  chipChannelCalibrationB = adcData.getAdcLinearCalibrationB(capCode, foundLinB);
  chipChannelCalibrationAmplitude = adcData.getAmpUniformCalibration(capCode, foundAmpl);
  if (foundLinA && foundLinB && foundAmpl)
  {
//    qDebug() << Q_FUNC_INFO << ": Calibration data for chip capacity is found";
    emit logMessage(tr("Calibration data for chip capacity is found"), d->cameraData.ID, Qt::green);
  }
  else
  {
//    qDebug() << Q_FUNC_INFO << ": Calibration data for capacity isn't found";
    emit logMessage(tr("Calibration data for capacity isn't found, load first data available"), d->cameraData.ID, Qt::yellow);
    chipChannelCalibrationA = adcData.getFirstAdcLinearCalibrationA();
    chipChannelCalibrationB = adcData.getFirstAdcLinearCalibrationB();
    chipChannelCalibrationAmplitude = adcData.getFirstAmpUniformCalibration();
  }
  if (chipChannelCalibrationA.empty() || chipChannelCalibrationB.empty() || chipChannelCalibrationAmplitude.empty())
  {
//    qDebug() << Q_FUNC_INFO << ": Calibration data is empty, unity data must be loaded";
    emit logMessage(tr("Calibration data is empty, unity data must be loaded"), d->cameraData.ID, Qt::yellow);
  }
  const ChipChannelPair& refAdcV = d->refAdcChipChannelVertical;
  const ChipChannelPair& refAdcH = d->refAdcChipChannelHorizontal;
  const ChipChannelPair& refAmpV = d->refAmpChipChannelVertical;
  const ChipChannelPair& refAmpH = d->refAmpChipChannelHorizontal;

  double refHA = chipChannelCalibrationA[refAdcH]; // reference horizontal value side-A
  double refHB = chipChannelCalibrationB[refAdcH]; // reference horizontal value side-B
  double refVA = chipChannelCalibrationA[refAdcV]; // reference vertical value side-A
  double refVB = chipChannelCalibrationB[refAdcV]; // reference vertical value side-B
  double refAmplH = chipChannelCalibrationAmplitude[refAmpH]; // reference horizontal amplitude value
  double refAmplV = chipChannelCalibrationAmplitude[refAmpV]; // reference vertical amplitude value
  d->chipChannelInfoMap.clear();

  for (auto iter = d->chipsAddressesVector.begin(); iter != d->chipsAddressesVector.end(); ++iter)
  {
    int chipAddress = *iter;
    std::bitset< CHIPS_PER_CAMERA > tmp = chips;
    ReverseBits(tmp);
    if ((chipAddress < 0) || (chipAddress > CHIPS_PER_CAMERA) || !tmp.test(chipAddress))
    {
      // Skip data for wrong chip address
      continue;
    }
    const std::vector< int >& vertProfChips = d->verticalProfileChipsVector;
    const std::vector< int >& horizProfChips = d->horizontalProfileChipsVector;
    for (int i = 0; i < CHANNELS_PER_CHIP; ++i)
    {
      double refAmpl = 0.;
      double refA = 0., refB = 0.;
      if (std::find(vertProfChips.begin(), vertProfChips.end(), chipAddress + 1) != vertProfChips.end())
      {
        refAmpl = refAmplV;
        refA = refVA;
        refB = refVB;
      }
      else if (std::find(horizProfChips.begin(), horizProfChips.end(), chipAddress + 1) != horizProfChips.end())
      {
        refAmpl = refAmplH;
        refA = refHA;
        refB = refHB;
      }
      else
      {
        refAmpl = -1.;
        refA = -1.;
        refB = -1.;
      }
      ChipChannelPair curr(chipAddress + 1, i + 1); // current chip, strip
      double currA = chipChannelCalibrationA[curr]; // current value side-A
      double currB = chipChannelCalibrationB[curr]; // current value side-B
      double currAmpl = chipChannelCalibrationAmplitude[curr]; // current amplitude value

      std::vector<int>& sideA = d->channelsCountsA[chipAddress][i];
      std::vector<int>& sideB = d->channelsCountsB[chipAddress][i];

      MeanDispAccumSet statPedB, statCalibPedB;
      MeanDispAccumSet statSigB, statCalibSigB;
      MeanDispAccumSet statPedA, statCalibPedA;
      MeanDispAccumSet statSigA, statCalibSigA;

      for (int j = pedBegin; j < pedEnd; ++j)
      {
        statCalibPedA(double(sideA[j]) * refA / currA);
        statCalibPedB(double(sideB[j]) * refB / currB);
        statPedA(sideA[j]);
        statPedB(sideB[j]);
      }
      for (int j = sigBegin; j < sigEnd; ++j)
      {
        statCalibSigA(double(sideA[j]) * refA / currA);
        statCalibSigB(double(sideB[j]) * refB / currB);
        statSigA(sideA[j]);
        statSigB(sideB[j]);
      }

      struct ChannelInfo info, infoCalib;
      info.PedMeanA = boost::accumulators::mean(statPedA);
      info.PedMeanB = boost::accumulators::mean(statPedB);
      info.PedMom2A = boost::accumulators::moment< 2 >(statPedA);
      info.PedMom2B = boost::accumulators::moment< 2 >(statPedB);
      info.SigMeanA = boost::accumulators::mean(statSigA);
      info.SigMeanB = boost::accumulators::mean(statSigB);
      info.SigMom2A = boost::accumulators::moment< 2 >(statSigA);
      info.SigMom2B = boost::accumulators::moment< 2 >(statSigB);
      info.SigCountA = boost::accumulators::count(statSigA);
      info.SigCountB = boost::accumulators::count(statSigB);
      info.SigSumA = boost::accumulators::sum(statSigA);
      info.SigSumB = boost::accumulators::sum(statSigB);

      double signalA = (info.SigSumA - info.SigCountA * info.PedMeanA);
      double signalB = (info.SigSumB - info.SigCountB * info.PedMeanB);
      double signalNoAmp = (signalA + signalB) / 2.;
      double signal = refAmpl * signalNoAmp / currAmpl;

      info.Signal = signal;
      info.SignalNoAmp = signalNoAmp;

      infoCalib.PedMeanA = boost::accumulators::mean(statCalibPedA);
      infoCalib.PedMeanB = boost::accumulators::mean(statCalibPedB);
      infoCalib.PedMom2A = boost::accumulators::moment< 2 >(statCalibPedA);
      infoCalib.PedMom2B = boost::accumulators::moment< 2 >(statCalibPedB);
      infoCalib.SigMeanA = boost::accumulators::mean(statCalibSigA);
      infoCalib.SigMeanB = boost::accumulators::mean(statCalibSigB);
      infoCalib.SigCountA = boost::accumulators::count(statCalibSigA);
      infoCalib.SigCountB = boost::accumulators::count(statCalibSigB);
      infoCalib.SigSumA = boost::accumulators::sum(statCalibSigA);
      infoCalib.SigSumB = boost::accumulators::sum(statCalibSigB);
      infoCalib.SigMom2A = boost::accumulators::moment< 2 >(statCalibSigA);
      infoCalib.SigMom2B = boost::accumulators::moment< 2 >(statCalibSigB);

      signalA = (infoCalib.SigSumA - infoCalib.SigCountA * infoCalib.PedMeanA);
      signalB = (infoCalib.SigSumB - infoCalib.SigCountB * infoCalib.PedMeanB);
      signalNoAmp = (signalA + signalB) / 2.;
      signal = refAmpl * signalNoAmp / currAmpl;

      infoCalib.Signal = signal;
      infoCalib.SignalNoAmp = signalNoAmp;
      d->chipChannelInfoMap[curr] = std::make_pair(info, infoCalib);
    }
  }
}

TGraph* AbstractCamera::createProfile(CameraProfileType profileType, bool withErrors)
{
  Q_D(AbstractCamera);

  const std::vector< double >& vertProfStrips = d->verticalProfileStripsNumbersVector;
  const std::vector< double >& horizProfStrips = d->horizontalProfileStripsNumbersVector;
  const std::vector< double >& vertProfData = d->verticalProfileVector;
  const std::vector< double >& horizProfData = d->horizontalProfileVector;

  TGraph* profile = nullptr;
  switch (profileType)
  {
  case CameraProfileType::PROFILE_VERTICAL:
    {
      if (withErrors)
      {
        profile = new TGraphErrors(vertProfData.size());
      }
      else
      {
        profile = new TGraph(vertProfData.size());
      }
      for (Int_t i = 0; i < vertProfData.size(); ++i)
      {
        profile->SetPoint(i, Double_t(vertProfStrips[i]), Double_t(vertProfData[i]));
      }
    }
    break;
  case CameraProfileType::PROFILE_HORIZONTAL:
    {
      if (withErrors)
      {
        profile = new TGraphErrors(horizProfData.size());
      }
      else
      {
        profile = new TGraph(horizProfData.size());
      }
      for (Int_t i = 0; i < horizProfData.size(); ++i)
      {
        profile->SetPoint(i, Double_t(horizProfStrips[i]), Double_t(horizProfData[i]));
      }
    }
    break;
  default:
    break;
  }
  return profile;
}

CameraProfileType AbstractCamera::getProfileBrokenChipChannelsStrips(int chip, std::vector< int >& strips) const
{
  Q_D(const AbstractCamera);
  CameraProfileType prof = d->getProfileForChip(chip);
  if (prof != CameraProfileType_Last)
  {
    for (const auto& brokenChipChannelsPair : d->chipChannelsStripsBrokenMap)
    {
      int brokenChip = brokenChipChannelsPair.first;
      const std::vector< int >& brokenChannels = brokenChipChannelsPair.second;
      if (brokenChip == chip && brokenChannels.size())
      {
        strips.resize(brokenChannels.size());
        std::copy(brokenChannels.begin(), brokenChannels.end(), strips.begin());
      }
    }
  }
  return prof;
}

bool AbstractCamera::processExternalData(TTree *rootFileTree)
{
  Q_D(AbstractCamera);

  if (!rootFileTree)
  {
    return false;
  }
  CameraResponse response;

  ULong64_t bufferSize = 0;
  std::vector< char > buffer;
  std::vector< char >* bufferPtr = &buffer;

  ULong64_t chipsAcquiredSize = 0;
  std::vector< int > chipsAcquired;
  std::vector< int >* chipsAcquiredPtr = &chipsAcquired;
  unsigned int mode = 0; // depricated
  int adcMode = 0; // depricated
  rootFileTree->SetBranchAddress("respChipsEnabled", &response.ChipsEnabled); // not used
  rootFileTree->SetBranchAddress("respChipsEnabledCode", &response.ChipsEnabledCode);
  rootFileTree->SetBranchAddress("respIntTime", &response.IntegrationTimeCode);
  rootFileTree->SetBranchAddress("respCapacity", &response.CapacityCode);
  rootFileTree->SetBranchAddress("respExtStart", &response.ExternalStartState);
  rootFileTree->SetBranchAddress("respAdcMode", &response.AdcMode);
  rootFileTree->SetBranchAddress("mode", &mode); // depricated
  rootFileTree->SetBranchAddress("adcMode", &adcMode); // depricated
  rootFileTree->SetBranchAddress("bufferSize", &bufferSize);
  rootFileTree->SetBranchAddress("bufferVector", &bufferPtr);
  rootFileTree->SetBranchAddress("chipsAcquired", &chipsAcquiredSize);
  rootFileTree->SetBranchAddress("chipsAcquiredVector", &chipsAcquiredPtr);

  Int_t nentries = static_cast<Int_t>(rootFileTree->GetEntries());
  bool res = true;
  for (Int_t i = 0; i < nentries; i++)
  {
    rootFileTree->GetEntry(i);
    if (bufferSize && buffer.size())
    {
      size_t res = std::min(size_t(bufferSize), buffer.size());
      d->adcDataBuffer.resize(res);
      std::copy(buffer.begin(), buffer.end(), d->adcDataBuffer.begin());
    }
    else
    {
      res = false;
      break;
    }
    d->cameraResponse = response;
    d->chipsAddressesVector = chipsAcquired;
    this->processRawData();
    d->adcDataBuffer.clear();
  }
  return res;
}

bool AbstractCamera::loadSettings(QSettings* settings)
{
  Q_D(AbstractCamera);

  if (!settings)
  {
    return false;
  }
  settings->beginGroup(d->cameraData.ID);
  int refAdcChipVertical = settings->value("ref-adc-chip-vertical", d->refAdcChipChannelVertical.first).toInt();
  int refAdcChannelVertical = settings->value("ref-adc-channel-vertical", d->refAdcChipChannelVertical.second).toInt();
  int refAdcChipHorizontal = settings->value("ref-adc-chip-horizontal", d->refAdcChipChannelHorizontal.first).toInt();
  int refAdcChannelHorizontal = settings->value("ref-adc-channel-horizontal", d->refAdcChipChannelHorizontal.second).toInt();
  int refAmpChipVertical = settings->value("ref-amp-chip-vertical", d->refAmpChipChannelVertical.first).toInt();
  int refAmpChannelVertical = settings->value("ref-amp-channel-vertical", d->refAmpChipChannelVertical.second).toInt();
  int refAmpChipHorizontal = settings->value("ref-amp-chip-horizontal", d->refAmpChipChannelHorizontal.first).toInt();
  int refAmpChannelHorizontal = settings->value("ref-amp-channel-horizontal", d->refAmpChipChannelHorizontal.second).toInt();  
  d->refAdcChipChannelVertical.first = refAdcChipVertical;
  d->refAdcChipChannelVertical.second = refAdcChannelVertical;
  d->refAdcChipChannelHorizontal.first = refAdcChipHorizontal;
  d->refAdcChipChannelHorizontal.second = refAdcChannelHorizontal;
  d->refAmpChipChannelVertical.first = refAmpChipVertical;
  d->refAmpChipChannelVertical.second = refAmpChannelVertical;
  d->refAmpChipChannelHorizontal.first = refAmpChipHorizontal;
  d->refAmpChipChannelHorizontal.second = refAmpChannelHorizontal;

  // load acquisition parameters for the camera
  AcquisitionParameters params;
  unsigned short code = settings->value("chips-enabled-code",
    AcquisitionParameters::DEFAULT_CHIPS_ENABLED_CODE).toUInt();
  if (code <= AcquisitionParameters::DEFAULT_CHIPS_ENABLED_CODE)
  {
    std::bitset< CHIPS_PER_CAMERA > chipsEnabled(code);
    params.ChipsEnabled = chipsEnabled;
  }
  unsigned int time = settings->value("integration-time-ms",
    AcquisitionParameters::DEFAULT_INTEGRATION_TIME_MS).toUInt();
  if (time <= AcquisitionParameters::MAX_INTEGRATION_TIME_MS)
  {
    params.IntegrationTimeMs = time;
  }
  unsigned int capCode = settings->value("capacity-code",
    AcquisitionParameters::DEFAULT_CAPACITY_CODE).toUInt();
  if (capCode <= AcquisitionParameters::MAX_CAPACITY_CODE)
  {
    params.CapacityCode = capCode;
  }
  params.IntegrationSamples = settings->value("nof-adc-samples",
    AcquisitionParameters::DEFAULT_SAMPLES).toInt();
  unsigned int mode = settings->value("adc-mode", AcquisitionParameters::DEFAULT_ADC_MODE).toUInt();
  if (mode == AdcResolutionType::ADC_20_BIT)
  {
    params.AdcMode = AdcResolutionType::ADC_20_BIT;
  }
  else
  {
    params.AdcMode = AdcResolutionType::ADC_16_BIT;
  }
  params.ExternalStartFlag = settings->value("external-start", false).toBool();

  settings->endGroup();

  d->cameraResponse = params.getCameraResponse();

  return true;
}

bool AbstractCamera::saveSettings(QSettings* settings)
{
  Q_D(AbstractCamera);

  if (!settings)
  {
    return false;
  }
  settings->beginGroup(d->cameraData.ID);
  settings->setValue("ref-adc-chip-vertical", d->refAdcChipChannelVertical.first);
  settings->setValue("ref-adc-channel-vertical", d->refAdcChipChannelVertical.second);
  settings->setValue("ref-adc-chip-horizontal", d->refAdcChipChannelHorizontal.first);
  settings->setValue("ref-adc-channel-horizontal", d->refAdcChipChannelHorizontal.second);
  settings->setValue("ref-amp-chip-vertical", d->refAmpChipChannelVertical.first);
  settings->setValue("ref-amp-channel-vertical", d->refAmpChipChannelVertical.second);
  settings->setValue("ref-amp-chip-horizontal", d->refAmpChipChannelHorizontal.first);
  settings->setValue("ref-amp-channel-horizontal", d->refAmpChipChannelHorizontal.second);
  AcquisitionParameters params = this->getAcquisitionParameters();
  settings->setValue("chips-enabled-code", static_cast< int >(params.ChipsEnabled.to_ulong()));
  settings->setValue("integration-time-ms", params.IntegrationTimeMs);
  settings->setValue("capacity-code", params.CapacityCode);
  settings->setValue("nof-adc-samples", params.IntegrationSamples);
  settings->setValue("adc-mode", params.AdcMode);
  settings->setValue("external-start", params.ExternalStartFlag);
  settings->endGroup();
  return true;
}

AcquisitionParameters AbstractCamera::getAcquisitionParameters() const
{
  Q_D(const AbstractCamera);
  AcquisitionParameters params;
  params.setCameraResponse(d->cameraResponse);
  return params;
}

void AbstractCamera::setRootDirectory(TDirectory* dir)
{
  Q_D(AbstractCamera);
  d->rootDir = dir;
}

ChipChannelPair AbstractCamera::getReferenceChipChannel(bool adcAmp, CameraProfileType profileType) const
{
  Q_D(const AbstractCamera);
  ChipChannelPair dummy{ -1, -1 };
  switch (profileType)
  {
  case CameraProfileType::PROFILE_VERTICAL:
    dummy = (adcAmp) ? d->refAmpChipChannelVertical : d->refAdcChipChannelVertical;
    break;
  case CameraProfileType::PROFILE_HORIZONTAL:
    dummy = (adcAmp) ? d->refAmpChipChannelHorizontal : d->refAdcChipChannelHorizontal;
    break;
  default:
    break;
  }
  return dummy;
}

bool AbstractCamera::setReferenceChipChannel(const ChipChannelPair& pair, bool adcAmp, CameraProfileType profileType)
{
  Q_D(AbstractCamera);
  bool res = false;
  if (adcAmp)
  {
    switch (profileType)
    {
    case CameraProfileType::PROFILE_VERTICAL:
      d->refAmpChipChannelVertical = pair;
      res = true;
      break;
    case CameraProfileType::PROFILE_HORIZONTAL:
      d->refAmpChipChannelHorizontal = pair;
      res = true;
      break;
    default:
      break;
    }
  }
  else
  {
    switch (profileType)
    {
    case CameraProfileType::PROFILE_VERTICAL:
      d->refAdcChipChannelVertical = pair;
      res = true;
      break;
    case CameraProfileType::PROFILE_HORIZONTAL:
      d->refAdcChipChannelHorizontal = pair;
      res = true;
      break;
    default:
      break;
    }
  }
  return res;
}
QT_END_NAMESPACE
