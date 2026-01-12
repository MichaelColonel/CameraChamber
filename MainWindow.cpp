// Initial qtweb author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

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

#include "MainWindow.h"
#include "ui_MainWindow.h"

// RapidJSON includes
#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/filereadstream.h>

#include <QTextCharFormat>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QDebug>
#include <QTimer>
#include <QProgressDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>

#include <TROOT.h>
#include <TTree.h>

#include "CameraProfilesDialog.h"
#include "FullCamera.h"
#include "Camera2.h"
#include "RootFileCameraProfilesDialog.h"

QT_BEGIN_NAMESPACE

namespace {

MainWindow *mainWindowGlobal = nullptr;
QtMessageHandler oldMessageHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  if (!mainWindowGlobal)
  {
    return;
  }

  QString message = "%1: %2";
  QString contextStr = " (%1:%2, %3)";
  QString typeString;

  if (type == QtDebugMsg)
  {
    typeString = QObject::tr("Debug");
  }
  else if (type == QtInfoMsg)
  {
    typeString = QObject::tr("Info");
  }
  else if (type == QtWarningMsg)
  {
    typeString = QObject::tr("Warning");
  }
  else if (type == QtCriticalMsg)
  {
    typeString = QObject::tr("Critical");
  }
  else if (type == QtFatalMsg)
  {
    typeString = QObject::tr("Fatal");
  }

  message = message.arg(typeString).arg(msg);
  contextStr = contextStr.arg(context.file).arg(context.line).arg(context.function);

  QColor color = Qt::black;
  if (type == QtFatalMsg || type == QtCriticalMsg)
  {
    color = Qt::darkRed;
  }
  else if (type == QtWarningMsg)
  {
    color = Qt::darkYellow;
  }

  // Logging messages from backends are sent from different threads and need to be
  // synchronized with the GUI thread.
  QMetaObject::invokeMethod(mainWindowGlobal, "log",
    Qt::QueuedConnection, Q_ARG(QString, message),
    Q_ARG(QString, contextStr),
    Q_ARG(QColor, color));

  if (oldMessageHandler)
  {
    oldMessageHandler(type, context, msg);
  }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
  :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  initiationTimer(new QTimer(this))
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
   gROOT->SetWebDisplay("qt5");
#else
   gROOT->SetWebDisplay("qt6");
#endif

#if (!QT_NO_DEBUG)
  qDebug() << Q_FUNC_INFO << "Debug info";
#endif

  this->ui->setupUi(this);
  mainWindowGlobal = this;

  this->ui->PlainTextEdit_Log->setReadOnly(true);
  this->ui->PlainTextEdit_Log->setLineWrapMode(QPlainTextEdit::NoWrap);

  for (int i = 0; i < CHIPS_PER_PLANE * 2; ++i)
  {
    this->ui->CheckableComboBox_ChipsEnabled->addItem(tr("%1").arg(i + 1));
    QModelIndex index = this->ui->CheckableComboBox_ChipsEnabled->model()->index(i, 0);
    this->ui->CheckableComboBox_ChipsEnabled->setCheckState(index, Qt::Checked);
  }

  QObject::connect(this->ui->CheckableComboBox_ChipsEnabled, SIGNAL(checkedIndexesChanged()),
    this, SLOT(onChipsEnabledChanged()));
  QObject::connect(this->ui->ComboBox_Cameras, SIGNAL(currentIndexChanged(QString)),
    this, SLOT(onSelectedCameraChanged(QString)));
  QObject::connect(this->ui->PushButton_ConnectCamera, SIGNAL(clicked()),
    this, SLOT(onConnectCameraClicked()));
  QObject::connect(this->ui->PushButton_DisconnectCamera, SIGNAL(clicked()),
    this, SLOT(onDisconnectCameraClicked()));
  QObject::connect(this->ui->ButtonGroup_AdcResolution, SIGNAL(buttonClicked(QAbstractButton*)),
    this, SLOT(onAcquisitionAdcModeResolutionChanged(QAbstractButton*)));
  QObject::connect(this->ui->PushButton_InitiateCamera, SIGNAL(clicked()),
    this, SLOT(onInitiateCameraClicked()));
  QObject::connect(this->ui->PushButton_SetCapacity, SIGNAL(clicked()),
    this, SLOT(onSetCapacityClicked()));
  QObject::connect(this->ui->PushButton_SetIntegrationTime, SIGNAL(clicked()),
    this, SLOT(onSetIntegrationTimeClicked()));
  QObject::connect(this->ui->PushButton_SetChipsEnabled, SIGNAL(clicked()),
    this, SLOT(onSetChipsEnabledClicked()));
  QObject::connect(this->ui->PushButton_SetSamples, SIGNAL(clicked()),
    this, SLOT(onSetSamplesClicked()));
  QObject::connect(this->ui->PushButton_SetExternalStart, SIGNAL(clicked()),
    this, SLOT(onSetExternalStartClicked()));
  QObject::connect(this->ui->PushButton_SetAdcResolution, SIGNAL(clicked()),
    this, SLOT(onSetAdcResolutionClicked()));
  QObject::connect(this->ui->PushButton_WriteCapacities, SIGNAL(clicked()),
    this, SLOT(onWriteCapacitiesClicked()));
  QObject::connect(this->ui->PushButton_ChipReset, SIGNAL(clicked()),
    this, SLOT(onChipResetClicked()));
  QObject::connect(this->ui->PushButton_AlteraReset, SIGNAL(clicked()),
    this, SLOT(onAlteraResetClicked()));
  QObject::connect(this->ui->PushButton_Acquisition, SIGNAL(clicked()),
    this, SLOT(onAcquisitionClicked()));
  QObject::connect(this->ui->PushButton_OnceTimeExternalStart, SIGNAL(clicked()),
    this, SLOT(onOnceTimeExternalStartClicked()));

  QObject::connect(this->initiationTimer.data(), SIGNAL(timeout()),
    this, SLOT(onCameraFirstContactTimeout()));

  QObject::connect(this->ui->actionOpenRootFile, SIGNAL(triggered()),
    this, SLOT(onOpenRootFileActionTriggered()));
  QObject::connect(this->ui->actionExit, &QAction::triggered, [this](){ this->close(); });

  oldMessageHandler = qInstallMessageHandler(&messageHandler);

  this->getCamerasAvailable();
  this->initiationTimer->setInterval(400);
  this->setMinimumWidth(500);
}

MainWindow::~MainWindow()
{
/*
  TFile* file = new TFile("/tmp/test.root", "RECREATE");
  TDirectory* camera1 = file->mkdir("Camera1", "Camera1 data");
  file->cd("Camera1");
  TTree* data = new TTree("spillnum_date_time", "date and time of the spill");
  float var1;
  data->Branch("my_variable", &var1);
  for (int i = 0; i < 100; ++i)
  {
    var1 = float(i) * 1.1f;
    data->Fill();
  }
  camera1->WriteTObject(data, "Camera1 spill data");
  camera1->Close();
  file->Close();
  delete file;
*/

  for (auto& cameraDialogPair : this->cameraDeviceProfilesMap)
  {
    auto& camera = cameraDialogPair.first;
    auto& dialog = cameraDialogPair.second;
    if (camera && camera->isDeviceAlreadyConnected())
    {
      QSettings settings("ProfileCamera2D", "configure");
      camera->saveCalibration(&settings);
      camera->disconnect();
    }
    if (dialog)
    {
      dialog->close();
    }
  }

}

void MainWindow::onConnectCameraClicked()
{
  bool connected = false;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();
  AbstractCamera::CameraDeviceData cameraData;

  QPointer< AbstractCamera > cameraDevice;
  QPointer< CameraProfilesDialog > cameraProfiles;

  for (AbstractCamera::CameraDeviceData data : this->camerasAvalable)
  {
    if (cameraID == data.id)
    {
      cameraData = data;
      break;
    }
  }
  cameraDevice = this->getCamera(cameraID);
  cameraProfiles = this->getProfiles(cameraID);

  QString& commandDeviceName = cameraData.commandDeviceName;
  QString& dataDeviceName = cameraData.dataDeviceName;
  if (cameraDevice)
  {
    bool commandConnected = cameraDevice->isDeviceAlreadyConnected(commandDeviceName);
    bool dataConnected = cameraDevice->isDeviceAlreadyConnected(dataDeviceName);
    connected = (commandConnected || dataConnected);
  }

  if (!cameraDevice && !connected && !commandDeviceName.isEmpty() && !dataDeviceName.isEmpty())
  {
    if (cameraID == "Camera2")
    {
      cameraDevice = QPointer< AbstractCamera >(new Camera2(cameraData, this));
    }
    else
    {
      cameraDevice = QPointer< AbstractCamera >(new FullCamera(cameraData, this));
    }
    cameraProfiles = QPointer< CameraProfilesDialog >(new CameraProfilesDialog(cameraData, this));
    if (cameraDevice && cameraProfiles)
    {
      if (this->cameraDeviceProfilesMap.isEmpty())
      {
        QString name = tr("Run%1").arg(this->ui->SpinBox_RunNumber->value());
        QString prefix = this->ui->LineEdit_SpillPrefix->text();
        if (!prefix.isEmpty())
        {
          name += QString(QChar('_')) + prefix + QString(".root");
        }
        else
        {
          QDateTime dt = QDateTime::currentDateTime();
          name += QString(QChar('_')) + dt.toString("ddMMyyyy_hhmmss") + QString(".root");
        }
        this->rootFile.reset(new TFile(name.toLatin1().data(), "RECREATE"));
      }
      QSettings settings("ProfileCamera2D", "configure");
      this->cameraDeviceProfilesMap.insert(cameraID, qMakePair(cameraDevice, cameraProfiles));
      cameraDevice->loadCalibration(&settings);
      cameraProfiles->setCameraDevice(cameraDevice);

      QObject::connect(cameraDevice.data(), SIGNAL(logMessage(QString,QString,QColor)),
        this, SLOT(log(QString,QString,QColor)));
      QObject::connect(cameraProfiles.data(), SIGNAL(logMessage(QString,QString,QColor)),
        this, SLOT(log(QString,QString,QColor)));
      QObject::connect(this->ui->CheckBox_ShowCameraProfiles, SIGNAL(toggled(bool)),
        cameraProfiles.data(), SLOT(setVisible(bool)));
      QObject::connect(cameraDevice.data(), SIGNAL(initiationStarted()),
        this, SLOT(onCameraInitiationStarted()));
      QObject::connect(cameraDevice.data(), SIGNAL(initiationProgress(int)),
        this, SLOT(onCameraInitiationInProgress(int)));
      QObject::connect(cameraDevice.data(), SIGNAL(initiationFinished()),
        this, SLOT(onCameraFirstContactFinished()));
      QObject::connect(cameraDevice.data(), SIGNAL(commandWritten(QByteArray)),
        this, SLOT(onCameraCommandWritten(QByteArray)));
      QObject::connect(cameraDevice.data(), SIGNAL(firmwareCommandBufferIsReset()),
        this, SLOT(onCameraFirstContactFinished()));
    }
  }
  else if (cameraDevice && connected && !commandDeviceName.isEmpty() && !dataDeviceName.isEmpty())
  {
    return; // Error message, camera has been already connected
  }

  if (cameraDevice && cameraDevice->connect())
  {
    this->updateAcquisitionParameters(cameraDevice.data());
    // create TDirectory
    if (this->rootFile)
    {
      TDirectory* dir = this->rootFile->mkdir(cameraID.toLatin1().data());
      if (dir)
      {
        this->rootCameraDirectoryMap.insert(std::make_pair(cameraID.toStdString(), dir));
        cameraDevice->setRootDirectory(dir);
      }
    }
    // Camera connect
    if (cameraProfiles)
    {
      cameraProfiles->show();
      {
        QSignalBlocker block(this->ui->CheckBox_ShowCameraProfiles);
        this->ui->CheckBox_ShowCameraProfiles->setChecked(true);
      }

      this->initiationProgress.reset(new QProgressDialog("ATmega128 firmware command buffer reset...", "Close", 0, 0, this));
      this->initiationProgress->show();
      this->initiationTimer->start();
    }
  }
  else
  {
    QObject::disconnect(cameraDevice.data(), SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(cameraProfiles.data(), SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(this->ui->CheckBox_ShowCameraProfiles, SIGNAL(toggled(bool)),
      cameraProfiles.data(), SLOT(setVisible(bool)));
    QObject::disconnect(cameraDevice.data(), SIGNAL(initiationStarted()),
      this, SLOT(onCameraInitiationStarted()));
    QObject::disconnect(cameraDevice.data(), SIGNAL(initiationProgress(int)),
      this, SLOT(onCameraInitiationInProgress(int)));
    QObject::disconnect(cameraDevice.data(), SIGNAL(initiationFinished()),
      this, SLOT(onCameraFirstContactFinished()));
    QObject::disconnect(cameraDevice.data(), SIGNAL(commandWritten(QByteArray)),
      this, SLOT(onCameraCommandWritten(QByteArray)));
    QObject::disconnect(cameraDevice.data(), SIGNAL(firmwareCommandBufferIsReset()),
      this, SLOT(onCameraFirstContactFinished()));

    this->cameraConnectedFlag = false;
    this->ui->CheckBox_ShowCameraProfiles->setChecked(false);
    this->updateUiState();

    QMessageBox msgBox(this);
    msgBox.setModal(true);
    msgBox.setWindowTitle(tr("CameraProfile2D"));
    msgBox.setText(tr("Unable to connect port %1.\n%2.").arg(commandDeviceName).arg(cameraDevice.data()->getCommandPortError()));
    msgBox.exec();
    ui->statusbar->showMessage(QObject::tr("Unable to connect camera"), 1000);

    // delete camera and dialog
    this->cameraDeviceProfilesMap.remove(cameraID);
    return; // Can't connect camera
  }
}

void MainWindow::onDisconnectCameraClicked()
{
  QString cameraID = this->ui->ComboBox_Cameras->currentText();
  QPointer< CameraProfilesDialog > cameraProfiles = this->getProfiles(cameraID);
  QPointer< AbstractCamera > cameraDevice = this->getCamera(cameraID);
  if (cameraProfiles)
  {
    cameraProfiles->close();
    QObject::disconnect(cameraDevice.data(), SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(cameraProfiles.data(), SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(this->ui->CheckBox_ShowCameraProfiles, SIGNAL(toggled(bool)),
      cameraProfiles.data(), SLOT(setVisible(bool)));
    QObject::disconnect(cameraDevice.data(), SIGNAL(initiationStarted()),
      this, SLOT(onCameraInitiationStarted()));
    QObject::disconnect(cameraDevice.data(), SIGNAL(initiationProgress(int)),
      this, SLOT(onCameraInitiationInProgress(int)));
    QObject::disconnect(cameraDevice.data(), SIGNAL(initiationFinished()),
      this, SLOT(onCameraFirstContactFinished()));
    QObject::disconnect(cameraDevice.data(), SIGNAL(commandWritten(QByteArray)),
      this, SLOT(onCameraCommandWritten(QByteArray)));
    QObject::disconnect(cameraDevice.data(), SIGNAL(firmwareCommandBufferIsReset()),
      this, SLOT(onCameraFirstContactFinished()));
  }
  if (cameraDevice)
  {
    QSettings settings("ProfileCamera2D", "configure");
    cameraDevice->saveCalibration(&settings);
    cameraDevice->disconnect();
    this->cameraConnectedFlag = false;
    this->updateUiState();
//    this->cameraDeviceProfilesMap[cameraID] = qMakePair(QSharedPointer< AbstractCamera >(), QSharedPointer< CameraProfilesDialog >());
    this->cameraDeviceProfilesMap.remove(cameraID);
    this->log(tr("%1 has been disconnected!").arg(cameraID));
  }
  if (this->cameraDeviceProfilesMap.isEmpty())
  {
    this->rootFile->Close();
    this->rootFile.release();
  }
  this->ui->CheckBox_ShowCameraProfiles->setChecked(false);
}

void MainWindow::onSelectedCameraChanged(const QString &cameraID)
{
  bool present = false;
  bool connected = false;
  QStringList cameraIds = this->cameraDeviceProfilesMap.keys();
  for (const QString& cameraId : cameraIds)
  {
    if (cameraID == cameraId)
    {
      present = true;
    }
  }
  if (present)
  {
    auto cameraDevice = this->getCamera(cameraID);
    if (cameraDevice.data() && cameraDevice->isDeviceAlreadyConnected())
    {
      this->getProfiles(cameraID)->show();
      connected = true;
    }
    this->updateAcquisitionParameters(cameraDevice.data());
  }
  this->cameraConnectedFlag = connected;
  this->updateUiState();
}

void MainWindow::updateUiState()
{
  this->ui->PushButton_ConnectCamera->setEnabled(!this->cameraConnectedFlag);
  this->ui->PushButton_DisconnectCamera->setEnabled(this->cameraConnectedFlag);
  this->ui->CollapsibleButton_AcquisitionCommands->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_SetCapacity->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_SetIntegrationTime->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_SetSamples->setEnabled(this->cameraConnectedFlag);
  this->ui->CheckBox_ExternalStart->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_SetExternalStart->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_SetChipsEnabled->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_SetAdcResolution->setEnabled(this->cameraConnectedFlag);

  this->ui->PushButton_WriteCapacities->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_AlteraReset->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_ChipReset->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_Acquisition->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_InitiateCamera->setEnabled(this->cameraConnectedFlag);
  this->ui->PushButton_OnceTimeExternalStart->setEnabled(this->cameraConnectedFlag);

  this->ui->CollapsibleButton_RunParameters->setEnabled(this->cameraConnectedFlag);
}

void MainWindow::updateAcquisitionParameters(AbstractCamera* cameraDevice)
{
  if (!cameraDevice)
  {
    return;
  }

  AcquisitionParameters params = cameraDevice->getAcquisitionParameters();
  for (int i = 0; i < CHIPS_PER_CAMERA; ++i)
  {
    QModelIndex index = this->ui->CheckableComboBox_ChipsEnabled->model()->index(i, 0);
    Qt::CheckState state = params.ChipsEnabled.test(i) ? Qt::Checked : Qt::Unchecked;
    this->ui->CheckableComboBox_ChipsEnabled->setCheckState(index, state);
  }
}

void MainWindow::getCamerasAvailable()
{
  // Load JSON descriptor file
  FILE *fp = fopen("Cameras.json", "r");
  if (!fp)
  {
    this->log(tr("Can't open JSON file \"Cameras.json\""), QString(), Qt::red);
    return;
  }
  const size_t size = 100000;
  std::unique_ptr< char[] > buffer(new char[size]);
  rapidjson::FileReadStream fs(fp, buffer.get(), size);

  rapidjson::Document d;
  if (d.ParseStream(fs).HasParseError())
  {
    this->log(tr("Can't parse JSON file\"Cameras.json\""), QString(), Qt::red);
    fclose(fp);
    return;
  }
  fclose(fp);

  const rapidjson::Value& cameraValues = d["Cameras"];
  if (cameraValues.IsArray())
  {
    this->camerasAvalable.clear();
    for (rapidjson::SizeType i = 0; i < cameraValues.Size(); i++) // Uses SizeType instead of size_t
    {
      AbstractCamera::CameraDeviceData cameraData;
      const rapidjson::Value& camera = cameraValues[i];
      if (!camera.IsObject())
      {
        continue;
      }
      cameraData.id = QString(camera["ID"].GetString());
      cameraData.dataDirectory = QString(camera["directory"].GetString());
      cameraData.commandDeviceName = QString(camera["command-device"].GetString());
      cameraData.dataDeviceName = QString(camera["data-device"].GetString());
      this->camerasAvalable.push_back(cameraData);
    }
  }
  this->ui->ComboBox_Cameras->clear();
  for (const auto& cameraData : this->camerasAvalable)
  {
    this->ui->ComboBox_Cameras->addItem(cameraData.id);
  }
}

int MainWindow::chipsEnabledCode() const
{
  std::bitset< CHIPS_PER_CAMERA > enabled;
  for (size_t i = 0; i < CHIPS_PER_CAMERA; ++i)
  {
    QModelIndex index = this->ui->CheckableComboBox_ChipsEnabled->model()->index(i, 0);
    Qt::CheckState checked = this->ui->CheckableComboBox_ChipsEnabled->checkState(index);
    enabled.set(CHIPS_PER_CAMERA - i - 1, checked == Qt::Checked);
  }
  return static_cast< int >(enabled.to_ulong());
}

void MainWindow::onChipsEnabledChanged()
{
//  this->log(tr("Chips enabled code: %1").arg(this->chipsEnabledCode()));
}

QPointer< AbstractCamera > MainWindow::getCamera(const QString& cameraID) const
{
  const CameraDeviceProfilesPair& pair = this->cameraDeviceProfilesMap[cameraID];
  return pair.first;
}

QPointer< CameraProfilesDialog > MainWindow::getProfiles(const QString& cameraID) const
{
  const CameraDeviceProfilesPair& pair = this->cameraDeviceProfilesMap[cameraID];
  return pair.second;
}

QPointer< AbstractCamera > MainWindow::getCurrentCamera() const
{
  QString commandDeviceName, dataDeviceName;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();

  for (const auto& data : this->camerasAvalable)
  {
    if (cameraID == data.id)
    {
      commandDeviceName = data.commandDeviceName;
      dataDeviceName = data.dataDeviceName;
      break;
    }
  }
  return this->getCamera(cameraID);
}

QPointer< CameraProfilesDialog > MainWindow::getCurrentProfiles() const
{
  QString commandDeviceName, dataDeviceName;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();

  for (const auto& data : this->camerasAvalable)
  {
    if (cameraID == data.id)
    {
      commandDeviceName = data.commandDeviceName;
      dataDeviceName = data.dataDeviceName;
      break;
    }
  }
  return this->getProfiles(cameraID);
}

void MainWindow::onChipResetClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray com = camera->getResetChipCommand();
    camera->writeCommand(com);
  }
}

void MainWindow::onAlteraResetClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray com = camera->getResetAlteraCommand();
    camera->writeCommand(com);
  }
}

void MainWindow::onSetExternalStartClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    bool extStart = this->ui->CheckBox_ExternalStart->isChecked();
    QByteArray com = camera->getSetExternalStartCommand(extStart);
    camera->writeCommand(com);
  }
}

void MainWindow::onAcquisitionAdcModeResolutionChanged(QAbstractButton*)
{
}

void MainWindow::onAcquisitionClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray com = camera->getStartAcquisitionCommand();
    camera->writeCommand(com);
  }
}

void MainWindow::onOnceTimeExternalStartClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray com = camera->getOnceTimeExternalStartCommand();
    camera->writeCommand(com);
  }
}

void MainWindow::onWriteCapacitiesClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray com = camera->getWriteChipsCapacitiesCommand();
    camera->writeCommand(com);
  }
}

void MainWindow::onSetIntegrationTimeClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    int timeMs = static_cast< int >(this->ui->SliderWidget_IntegrationTime->value());
    QByteArray com = camera->getSetIntegrationTimeCommand(timeMs);
    camera->writeCommand(com);
  }
}

void MainWindow::onSetCapacityClicked()
{
  auto camera = this->getCurrentCamera();

  if (camera)
  {
    int capasityCode = this->ui->ComboBox_AcquisitionCapacity->currentIndex();
    QByteArray com = camera->getSetCapacityCommand(capasityCode);
    camera->writeCommand(com);
  }
}

void MainWindow::onSetChipsEnabledClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    int chipsCode = this->chipsEnabledCode();
    QByteArray com = camera->getSetChipsEnabledCommand(chipsCode);
    camera->writeCommand(com);
  }
}

void MainWindow::onSetSamplesClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    int samples = static_cast< int >(this->ui->SliderWidget_Samples->value());
    QByteArray com = camera->getSetSamplesCommand(samples);
    camera->writeCommand(com);
  }
}

void MainWindow::onSetNumberOfChipsClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray com = camera->getSetNumberOfChipsCommand();
    camera->writeCommand(com);
  }
}

void MainWindow::onSetAdcResolutionClicked()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    bool adc20BitMode = this->ui->RadioButton_Adc20Bit->isChecked();
    QByteArray com = camera->getSetAdcResolutionCommand(adc20BitMode);
    camera->writeCommand(com);
  }
}

void MainWindow::onInitiateCameraClicked()
{
  QByteArrayList initCommands;

  QPointer< AbstractCamera > camera = this->getCurrentCamera();
  if (!camera)
  {
    return;
  }

  QByteArray com = camera->getResetAlteraCommand();
  initCommands.append(com); // Reset Altera

  com = camera->getResetChipCommand();
  initCommands.append(com); // Reset chip

  com = camera->getSetNumberOfChipsCommand();
  initCommands.append(com); // write number of chips == 12

  int chipsEnabled = this->chipsEnabledCode();
  com = camera->getSetChipsEnabledCommand(chipsEnabled);
  initCommands.append(com); // write enabled chips

  int samples = static_cast< int >(this->ui->SliderWidget_Samples->value());
  com = camera->getSetSamplesCommand(samples);
  initCommands.append(com); // write number of samples

  int intTimeMs = this->ui->SliderWidget_IntegrationTime->value();
  com = camera->getSetIntegrationTimeCommand(intTimeMs); // intergration time in ms
  initCommands.append(com); // write integration time

  int capacityIndex = this->ui->ComboBox_AcquisitionCapacity->currentIndex();
  com = camera->getSetCapacityCommand(capacityIndex); // chip capacity
  initCommands.append(com);

  bool adc20Bit = this->ui->RadioButton_Adc20Bit->isChecked(); // ADC Resolution (16 or 20 bit)
  com = camera->getSetAdcResolutionCommand(adc20Bit); // chip capacity
  initCommands.append(com);

  com = camera->getWriteChipsCapacitiesCommand(); // write chips capacities (write DDC232 configureation register)
  initCommands.append(com);

  bool extStart = this->ui->CheckBox_ExternalStart->isChecked(); // external start flag
  com = camera->getSetExternalStartCommand(extStart); // write beam extraction interrupts
  initCommands.append(com);

  com = camera->getListChipsEnabledCommand(); // write list of chips enabled
  initCommands.append(com); // write list of chips enabled

  com = camera->getResetAlteraCommand();
  initCommands.append(com); // Reset Altera

  this->initiationProgress.reset(new QProgressDialog("Chips initiation...", "Close", 0, initCommands.size(), this));
//  this->initiationProgress->setAutoClose(false);
  this->initiationProgress->show();
  camera->setInitiationList(initCommands);
}

void MainWindow::onCameraCommandWritten(const QByteArray&)
{
  auto camera = this->getCurrentCamera();
  if (camera && !camera->isInitiationListEmpty())
  {
    QTimer::singleShot(100, camera.data(), SLOT(writeNextCommandFromInitiationList()));
  }
}

void MainWindow::onCameraInitiationStarted()
{
  auto camera = this->getCurrentCamera();
  if (camera && !camera->isInitiationListEmpty())
  {
    QTimer::singleShot(100, camera.data(), SLOT(writeNextCommandFromInitiationList()));
  }
}

void MainWindow::onCameraInitiationInProgress(int prog)
{
  if (this->initiationProgress)
  {
    int max = this->initiationProgress->maximum();
    this->initiationProgress->setValue(max - prog);
  }
}

void MainWindow::onCameraInitiationFinished()
{
  if (this->initiationProgress)
  {
    this->initiationProgress->close();
  }
}

void MainWindow::log(const QString &text, const QString &context, QColor color)
{
  auto cf = this->ui->PlainTextEdit_Log->currentCharFormat();
  cf.setForeground(color);
  this->ui->PlainTextEdit_Log->setCurrentCharFormat(cf);
  this->ui->PlainTextEdit_Log->appendPlainText(text);

  if (!context.isEmpty())
  {
    cf.setForeground(Qt::gray);
    this->ui->PlainTextEdit_Log->setCurrentCharFormat(cf);
    this->ui->PlainTextEdit_Log->insertPlainText(context);
  }
}

void MainWindow::log(const QString &text, QColor color)
{
  this->log(text, QString(), color);
}

void MainWindow::onCameraFirstContactTimeout()
{
  auto camera = this->getCurrentCamera();
  if (camera)
  {
    QByteArray initCommand = camera->getFirstContactCommand();
    camera->writeCommand(initCommand);
  }
}

void MainWindow::onCameraFirstContactFinished()
{
  this->initiationTimer->stop();
  if (this->initiationProgress)
  {
    this->initiationProgress->close();
  }
  this->cameraConnectedFlag = true;
  this->updateUiState();
}

void MainWindow::onOpenRootFileActionTriggered()
{
  QFileDialog* dialog = new QFileDialog(this, tr("Open ROOT File..."));
  dialog->setAcceptMode(QFileDialog::AcceptOpen);
  dialog->setFileMode(QFileDialog::ExistingFile);

  QStringList filters;
  filters << tr("ROOT Files *.root (*.root)");
  dialog->setNameFilters(filters);

  QStringList fileNames;
  if (dialog->exec())
  {
    fileNames = dialog->selectedFiles();

    if (!fileNames.isEmpty())
    {
      this->rootFileName = fileNames[0];
    }
  }
  delete dialog;

  if (this->rootFileName.isEmpty())
  {
    return;
  }

  QDialog* rootFileDialog = new RootFileCameraProfilesDialog(this->rootFileName, this);
  if (rootFileDialog->exec())
  {
    ;
  }
  delete rootFileDialog;
}

QT_END_NAMESPACE
