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

#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>

#include <QDebug>
#include <QDir>
#include <QScopedPointer>

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
  bool checkLastCommandWrittenAndRespose();
  bool checkLastCommandWrittenAndRespose(const QByteArray& responseBuffer);

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
  std::map< ChipChannelPair, double > chipChannelCalibrationA; // ADC-Offset tangent chip calibration A
  std::map< ChipChannelPair, double > chipChannelCalibrationB; // ADC-Offset tangent chip calibration B
  std::map< ChipChannelPair, double > chipChannelCalibrationAmplitude; // amplitude sensitivity calibration

  ChipChannelPair refAdcChipChannel = { 1, 1 };
  ChipChannelPair refAmpChipChannelVertical = { 1, 1 };
  ChipChannelPair refAmpChipChannelHorizontal = { 1, 1 };
  std::array< int, 4 > pedestalSignalGateArray = { 1, 100, 101, 200 };
  std::map< ChipChannelPair, ChannelInfoPair > chipChannelInfoMap;

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
  if (this->commandResponseBuffer.isEmpty() || this->lastCommandWritten.isEmpty())
  {
    return false;
  }

  const char* responseData = this->commandResponseBuffer.data();
  const char* lastWrittenData = this->lastCommandWritten.data();

  if (this->commandResponseBuffer.endsWith("OK") || this->commandResponseBuffer.endsWith("Welcome"))
  {
    const std::string commandHeaders("GMRHAOCIDSELP");
    char comHeader = responseData[0];
    auto CheckHeaderOK = [comHeader](char headerCharacter) -> bool { return comHeader == headerCharacter; };
    auto resComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderOK);
    comHeader = lastWrittenData[0];
    auto writtenComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderOK);
    if (resComHeader != std::end(commandHeaders) && writtenComHeader != std::end(commandHeaders) && *resComHeader == *writtenComHeader)
    {
      return true;
    }
  }

  if (this->commandResponseBuffer.endsWith("NO"))
  {
    const std::string commandHeaders("OMIEP");
    char comHeader = responseData[0];
    auto CheckHeaderNO = [comHeader](char headerCharacter) -> bool { return comHeader == headerCharacter; };

    auto resComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderNO);
    comHeader = lastWrittenData[0];
    auto writtenComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderNO);
    if (resComHeader != std::end(commandHeaders) && writtenComHeader != std::end(commandHeaders) && *resComHeader == *writtenComHeader)
    {
      qCritical() << Q_FUNC_INFO << ": Last command with header \"" << comHeader << "\" : parameter isn't set";
      return false;
    }
  }
  return false;
}

bool AbstractCameraPrivate::checkLastCommandWrittenAndRespose(const QByteArray& responseBuffer)
{
  if (responseBuffer.isEmpty() || this->lastCommandWritten.isEmpty())
  {
    return false;
  }

  const char* responseData = responseBuffer.data();
  const char* lastWrittenData = this->lastCommandWritten.data();

  if (responseBuffer.endsWith("OK") || responseBuffer.endsWith("Welcome"))
  {
    const std::string commandHeaders("GMRHAOCIDSELP");
    char comHeader = responseData[0];
    auto CheckHeaderOK = [comHeader](char headerCharacter) -> bool { return comHeader == headerCharacter; };
    auto resComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderOK);
    comHeader = lastWrittenData[0];
    auto writtenComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderOK);
    if (resComHeader != std::end(commandHeaders) && writtenComHeader != std::end(commandHeaders) && *resComHeader == *writtenComHeader)
    {
      return true;
    }
  }

  if (responseBuffer.endsWith("NO"))
  {
    const std::string commandHeaders("OMIEP");
    char comHeader = responseData[0];
    auto CheckHeaderNO = [comHeader](char headerCharacter) -> bool { return comHeader == headerCharacter; };

    auto resComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderNO);
    comHeader = lastWrittenData[0];
    auto writtenComHeader = std::find_if(std::begin(commandHeaders), std::end(commandHeaders), CheckHeaderNO);
    if (resComHeader != std::end(commandHeaders) && writtenComHeader != std::end(commandHeaders) && *resComHeader == *writtenComHeader)
    {
      qCritical() << Q_FUNC_INFO << ": Last command with header \"" << comHeader << "\" : parameter isn't set";
      return false;
    }
  }
  return false;
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
  if (this->loadCameraData(d->cameraData.dataDirectory))
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": \"" << d->cameraData.id << "\" data successfully loaded";
#endif
    d->verticalProfileVector.resize(d->verticalProfileChipsVector.size() * CHANNELS_PER_CHIP);
    std::fill(std::begin(d->verticalProfileVector), std::end(d->verticalProfileVector), 0.);

    d->horizontalProfileVector.resize(d->horizontalProfileChipsVector.size() * CHANNELS_PER_CHIP);
    std::fill(std::begin(d->horizontalProfileVector), std::end(d->horizontalProfileVector), 0.);

    d->verticalProfileStripsNumbersVector = GenerateStripsNumbers(d->verticalProfileVector.size());
    d->horizontalProfileStripsNumbersVector = GenerateStripsNumbers(d->horizontalProfileVector.size());
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

    d->commandPort->setPortName(d->cameraData.commandDeviceName);
    d->commandPort->setBaudRate(QSerialPort::Baud57600);
    d->commandPort->setDataBits(QSerialPort::Data8);
    d->commandPort->setParity(QSerialPort::NoParity);
    d->commandPort->setStopBits(QSerialPort::OneStop);
    d->commandPort->setFlowControl(QSerialPort::NoFlowControl);

    QObject::connect(d->commandPort.data(), SIGNAL(readyRead()),
      this, SLOT(onCommandPortDataReady()));
    QObject::connect(d->commandPort.data(), SIGNAL(bytesWritten(qint64)),
      this, SLOT(onCommandPortBytesWritten(qint64)));
    QObject::connect(d->commandPort.data(), SIGNAL(error(QSerialPort::SerialPortError)),
      this, SLOT(onCommandPortError(QSerialPort::SerialPortError)));

    connected = d->commandPort->open(QIODevice::ReadWrite);
  }

  if (d->dataPort && !d->dataPort->isOpen())
  {
    d->dataPort->setPortName(d->cameraData.dataDeviceName);
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
    QObject::connect(d->dataPort.data(), SIGNAL(error(QSerialPort::SerialPortError)),
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
    return d->cameraData.commandDeviceName == otherPort;
  }
  if (d->dataPort && d->dataPort->isOpen())
  {
    return d->cameraData.dataDeviceName == otherPort;
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
      emit logMessage(tr("External start flag: %1").arg(d->cameraResponse.ExternalStartState), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'C':
      d->cameraResponse.CapacityCode = data[1] - '0';
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set chip capacity command response: " << d->cameraResponse.CapacityCode;
#endif
      emit logMessage(tr("Chip capacity code: %1").arg(d->cameraResponse.CapacityCode), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'D':
      d->cameraResponse.AdcMode = (data[1] - '0') ? ADC_20_BIT : ADC_16_BIT;
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": ADC resolution command response: " << d->cameraResponse.AdcMode;
#endif
      emit logMessage(tr("ADC resolution: %1").arg(d->cameraResponse.AdcMode), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'G':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Capasities have been written into chip";
#endif
      emit logMessage(tr("Capasities have been written into chip"), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'I':
      d->cameraResponse.IntegrationTimeCode = (data[1] - 'A');
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set integration time command response code: " << char(data[1]);
      qDebug() << Q_FUNC_INFO << ": Set integration time is: " << (d->cameraResponse.IntegrationTimeCode + 1) * 2 << " ms";
#endif
      emit logMessage(tr("Integration time: %1 ms").arg((d->cameraResponse.IntegrationTimeCode + 1) * 2), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'M':
      d->cameraResponse.ChipsEnabled = data[1] - 'A';
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Number of chips enabled: " << d->cameraResponse.ChipsEnabled;
#endif
      emit logMessage(tr("Number of chips enabled: %1").arg(d->cameraResponse.ChipsEnabled), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'O':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Set number of chips command response code: " << char(data[1]);
      qDebug() << Q_FUNC_INFO << ": Number of chips (MUST be 12) are: " << data[1] - 'A';
#endif
      emit logMessage(tr("Number of chips (MUST be 12): %1").arg(data[1] - 'A'), d->cameraData.id, Qt::green);
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
    emit logMessage(tr("Wrong or strange command response"), d->cameraData.id, Qt::yellow);
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
      emit logMessage(tr("DDC232 configuration register is set"), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'R':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": ALTERA reset";
#endif
      emit logMessage(tr("ALTERA reset"), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'H':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Chip reset";
#endif
      emit logMessage(tr("Chip reset"), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'S':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Single shot acquisition with external start";
#endif
      emit logMessage(tr("Single shot acquisition..."), d->cameraData.id, Qt::green);
      emit commandWritten(d->lastCommandWritten);
      break;
    case 'E':
#if !QT_NO_DEBUG
      qDebug() << Q_FUNC_INFO << ": Devices enabled are set";
#endif
      emit logMessage(tr("Chips are enabled"), d->cameraData.id, Qt::green);
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
      emit logMessage(tr("New number of samples is set: %1").arg(d->cameraResponse.AdcSamples), d->cameraData.id, Qt::green);
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
    emit logMessage(tr("Wrong or strange command response"), d->cameraData.id, Qt::yellow);
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
    /// Data is safe => clear acquisition buffer
    emit commandWritten(d->lastCommandWritten);
    d->adcDataBuffer.clear();
    d->commandResponseBuffer.clear();
    d->lastCommandWritten.clear();
    emit acquisitionFinished();
    d->acquisitionFlag = false;
    return;
  }
  if (d->commandResponseBuffer.startsWith('L') && d->commandResponseBuffer.size() == 15 && d->checkLastCommandWrittenAndRespose())
  {
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": List of enabled chips is recieved: " << d->commandResponseBuffer;
#endif
    {
      std::bitset< CHIPS_PER_CAMERA > enabledChips;
      for (int i = 0; i < CHIPS_PER_CAMERA; ++i)
      {
        enabledChips.set(i, data[i + 1] - '0');
      }
      d->cameraResponse.ChipsEnabledCode = enabledChips.to_ulong();
    }
    emit logMessage(tr("List of enabled chips is recieved"), d->cameraData.id, Qt::green);
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
    emit logMessage(tr("Abort received!"), d->cameraData.id, Qt::yellow);
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
    return;
  }
  if (d->commandResponseBuffer.startsWith("Welcome") && d->commandResponseBuffer.size() == std::strlen("Welcome"))
  {
    emit logMessage(tr("Firmware command buffer reset is finished!"), d->cameraData.id, Qt::green);
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
    emit logMessage(tr("Firmware command buffer reset..."), d->cameraData.id, Qt::yellow);
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
#if !QT_NO_DEBUG
    qDebug() << Q_FUNC_INFO << ": Acquisition command written";
#endif
  }
  else if (d->lastCommandWritten.size() == bytes && d->lastCommandWritten == d->OnceTimeExternalStartCommand)
  {
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
  dataFile->open(QFile::WriteOnly | QIODevice::Text);
  QTextStream dataFileStream(dataFile.get()); // dump bits to temp file for debug

  const QByteArray& arr = d->adcDataBuffer;
  constexpr int BYTES_CHUNK = 4; // bytes per adc data chunk (channel, chip, ADC count, sync bits)
  std::array< int, BYTES_CHUNK > findData{ 0, 0, 0, 0 };
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
  std::array< int, 4 >::iterator findDataIter = std::max_element(findData.begin(), findData.end());
  int offset = findDataIter - findData.begin();

#if !QT_NO_DEBUG
  qDebug() << Q_FUNC_INFO << ": Found data " << findData[0] << ' ' << findData[1] << ' ' << findData[2] << ' ' << findData[3] << ' ' << offset;
#endif

  for (int i = offset; i < arr.size() - BYTES_CHUNK; i += BYTES_CHUNK)
  {
    std::bitset< CHAR_BIT > d0(arr.at(i)), d1(arr.at(i + 1)), d2(arr.at(i + 2)), d3(arr.at(i + 3));
    QString string_d0 = QString::fromStdString(d0.to_string());
    QString string_d1 = QString::fromStdString(d1.to_string());
    QString string_d2 = QString::fromStdString(d2.to_string());
    QString string_d3 = QString::fromStdString(d3.to_string());

    dataFileStream << string_d0 << ' ' << string_d1 << ' ' << string_d2 << ' ' << string_d3 << '\n';
  }

  constexpr int SIDE_BIT = 6;
  constexpr int CHIP_BITS = 4;
  d->chipsAddressesVector.clear();
  std::map< int, bool > chipPresenceMap;
  for (int i = offset; i < arr.size(); i += BYTES_CHUNK)
  {
    std::bitset< CHAR_BIT > d0(arr.at(i)), d1(arr.at(i + 1)), d2(arr.at(i + 2)), d3(arr.at(i + 3));
    this->ReverseBits< CHAR_BIT >(d0);
    bool sideFlag = d0.test(SIDE_BIT);
    int channel = static_cast<int>(((d0 >> 1) & std::bitset< CHAR_BIT >(0x1F)).to_ulong());
    std::bitset< CHIP_BITS > chipBits((d1 >> CHIP_BITS).to_ulong());
    this->ReverseBits< CHIP_BITS >(chipBits);
    int chip = chipBits.to_ulong() - CHIP_BITS;
    chipPresenceMap[chip] = true;
    std::array< std::vector< int >, CHANNELS_PER_CHIP >& channelsA = d->channelsCountsA[chip];
    std::array< std::vector< int >, CHANNELS_PER_CHIP >& channelsB = d->channelsCountsB[chip];
    std::vector< int >& channelVectorA = channelsA[channel];
    std::vector< int >& channelVectorB = channelsB[channel];
    this->ReverseBits< CHAR_BIT >(d2);
    this->ReverseBits< CHAR_BIT >(d3);
    int count = static_cast< int >((d3.to_ulong() << CHAR_BIT) | d2.to_ulong());
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
  if (dataFile->flush())
  {
    dataFile->close();
  }

  for (const auto& [chipAddress, chipFlag] : chipPresenceMap)
  {
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

bool AbstractCamera::loadCameraData(const QString &cameraDirectory)
{
  Q_D(AbstractCamera);
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

  if (channelSpacingPerSideMM.IsDouble())
  {
    double stripSpacingPerSide = channelSpacingPerSideMM.GetDouble();
    if (std::abs(STRIP_STEP_PER_SIDE_MM - stripSpacingPerSide) > 0.0001)
    {
      return false;
    }
  }

  std::array< int, CHANNELS_PER_CHIP > channels;
  std::bitset< CHIPS_PER_PLANE * 2 > chipChannelsReverse;
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
        if (this->loadChipData(filePath, position))
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

bool AbstractCamera::loadChipData(const QString& filePath, int position)
{
  Q_D(AbstractCamera);
  // Load JSON descriptor file
  std::string jsonFileName = filePath.toStdString();
  if ((position < 1) || (position > CHIPS_PER_PLANE * 2))
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
  const rapidjson::Value& calib = d1["CountPerMilliVoltCalibration"];
  if (calib.IsArray() && (calib.Capacity() == 2))
  {
    const rapidjson::Value& sideA = calib[0];
    const rapidjson::Value& sideB = calib[1];
    // side-A calibration
    for (rapidjson::SizeType pos = 0; pos < sideA.Size(); pos++)
    {
      double value = sideA[pos].GetDouble();
      std::pair< int, int > chipChannelPair( position, pos + 1);
      d->chipChannelCalibrationA[chipChannelPair] = value;
    }
    // side-B calibration
    for (rapidjson::SizeType pos = 0; pos < sideB.Size(); pos++)
    {
      double value = sideB[pos].GetDouble();
      std::pair< int, int > chipChannelPair( position, pos + 1);
      d->chipChannelCalibrationB[chipChannelPair] = value;
    }
  }
  const rapidjson::Value& amplitude = d1["AmplitudeCalibration"];
  if (amplitude.IsArray())
  {
    // Amplitude calibration
    for (rapidjson::SizeType pos = 0; pos < amplitude.Size(); pos++)
    {
      double value = amplitude[pos].GetDouble();
      std::pair< int, int > chipChannelPair( position, pos + 1);
      d->chipChannelCalibrationAmplitude[chipChannelPair] = value;
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
  if ((capacityCode < 0) || (capacityCode > 7))
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
  if ((timeMs < 2) || (timeMs > 32))
  {
    return QByteArray();
  }
  unsigned char buf[BUFFER_SIZE] = { 'I' };
  buf[1] = (timeMs / 2) - 1; // data
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetAdcResolutionCommand(bool adc20Bit) const
{
  unsigned char buf[BUFFER_SIZE] = { 'D' };
  buf[1] = adc20Bit; // data
  return QByteArray(reinterpret_cast<char*>(buf), BUFFER_SIZE);
}

QByteArray AbstractCamera::getSetSamplesCommand(int samples) const
{
  if ((samples < 10) || (samples > 1000))
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
  if ((chipsEnabledCode < 1) || (chipsEnabledCode > 0x0FFF))
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
  unsigned char buf[BUFFER_SIZE] = { 'O' };
  buf[1] = CHIPS_PER_PLANE * 2;
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
  for (const auto& [chipChannel, info] : d->chipChannelInfoMap)
  {
    infoMap.insert(std::make_pair(chipChannel, info));
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
    emit logMessage(tr("Input command is empty"), d->cameraData.id, Qt::yellow);
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

int AbstractCamera::getCapasityCode() const
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
  ChipChannelPair refV(2, 15); // reference vertical profile strip chip == 2, strip == 15
  ChipChannelPair refH(1, 15); // reference horizontal profile strip chip == 1, strip == 15
  ChipChannelPair refAmplitudeV(2, 15); // amplitude reference vertical profile strip chip == 2, strip == 15
  ChipChannelPair refAmplitudeH(1, 15); // amplitude reference horizontal profile strip chip == 1, strip == 15
  double refHA = d->chipChannelCalibrationA[refH]; // reference horizontal value side-A
  double refHB = d->chipChannelCalibrationB[refH]; // reference horizontal value side-B
  double refVA = d->chipChannelCalibrationA[refV]; // reference vertical value side-A
  double refVB = d->chipChannelCalibrationB[refV]; // reference vertical value side-B
  double refAmplH = d->chipChannelCalibrationAmplitude[refAmplitudeH]; // reference horizontal amplitude value
  double refAmplV = d->chipChannelCalibrationAmplitude[refAmplitudeV]; // reference vertical amplitude value

  d->chipChannelInfoMap.clear();

  for (auto iter = d->chipsAddressesVector.begin(); iter != d->chipsAddressesVector.end(); ++iter)
  {
    int chipAddress = *iter;
    std::bitset< CHIPS_PER_CAMERA > tmp = chips;
    ReverseBits< CHIPS_PER_CAMERA >(tmp);
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
      double currA = d->chipChannelCalibrationA[curr]; // current value side-A
      double currB = d->chipChannelCalibrationB[curr]; // current value side-B
      double currAmpl = d->chipChannelCalibrationAmplitude[curr]; // current amplitude value
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

QT_END_NAMESPACE
