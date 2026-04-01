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
#include <QDateTime>

#include <TROOT.h>
#include <TTree.h>
#include <TH1F.h>
#include <TCanvas.h>
#include <TF1.h>

#include "FullCamera.h"
#include "Camera2.h"
#include "CameraUtils.h"

#include "RootFileCameraProfilesDialog.h"
#include "CameraProfilesDialog.h"
#include "SettingsDialog.h"
#include "HttpServerDialog.h"
#include "BeamPathProfileDialog.h"

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

  for (int i = 0; i < CHIPS_PER_CAMERA; ++i)
  {
    this->ui->CheckableComboBox_ChipsEnabled->addItem(tr("%1").arg(i + 1));
    QModelIndex index = this->ui->CheckableComboBox_ChipsEnabled->model()->index(i, 0);
    this->ui->CheckableComboBox_ChipsEnabled->setCheckState(index, Qt::Checked);
  }

  QObject::connect(this->ui->CheckableComboBox_ChipsEnabled, SIGNAL(checkedIndexesChanged()),
    this, SLOT(onChipsEnabledChanged()));
  QObject::connect(this->ui->ComboBox_Cameras, SIGNAL(currentTextChanged(QString)),
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
  QObject::connect(this->ui->PushButton_ShowBeamPathProfilesDialog, SIGNAL(clicked()),
    this, SLOT(onBeamActionTriggered()));

  QObject::connect(this->initiationTimer.data(), SIGNAL(timeout()),
    this, SLOT(onCameraFirstContactTimeout()));

  QObject::connect(this->ui->actionOpenRootFile, SIGNAL(triggered()),
    this, SLOT(onOpenRootFileActionTriggered()));
  QObject::connect(this->ui->actionCameraSettings, SIGNAL(triggered()),
    this, SLOT(onCameraSettingsActionTriggered()));
  QObject::connect(this->ui->actionHttpServer, SIGNAL(triggered()),
    this, SLOT(onHttpServerActionTriggered()));
  QObject::connect(this->ui->actionBeam, SIGNAL(triggered()),
    this, SLOT(onBeamActionTriggered()));

  QObject::connect(this->ui->PushButton_Test, SIGNAL(clicked()),
    this, SLOT(onTestClicked()));

  QObject::connect(this->ui->actionExit, &QAction::triggered, [this](){ this->close(); });

  oldMessageHandler = qInstallMessageHandler(&messageHandler);

  this->getCamerasAvailable();
  this->initiationTimer->setInterval(400);
  this->setMinimumWidth(500);

  QSettings settings("ProfileCamera2D", "configure");
  this->loadSettings(&settings);
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
  camera1->WriteTObject(data);
  camera1->Close();
  file->Close();
  delete file;
*/
}

void
MainWindow::closeEvent(QCloseEvent* event)
{
  int res = QMessageBox::warning( this, tr("Close program"), \
    tr("Acquisition is still active.\nDo you want to quit program?"), \
      QMessageBox::Yes, QMessageBox::No | QMessageBox::Default);

  if (res == QMessageBox::Yes)
  {
    QSettings settings("ProfileCamera2D", "configure");
    this->saveSettings(&settings);

    if (this->beamPathProfile)
    {
      this->beamPathProfile->close();
    }
    this->beamPathProfile.reset();

    for (auto& cameraDialogPair : this->cameraDeviceProfilesMap)
    {
      AbstractCamera* camera = cameraDialogPair.first;
      CameraProfilesDialog* dialog = cameraDialogPair.second;
      if (camera && camera->isDeviceAlreadyConnected())
      {
        camera->saveSettings(&settings);
        camera->disconnect();
      }
      if (dialog)
      {
        dialog->close();
        delete dialog;
        dialog = nullptr;
      }
      if (camera)
      {
        delete camera;
        camera = nullptr;
      }
    }
    if (this->rootFile && this->rootFile->IsOpen())
    {
      this->rootFile->cd();
      this->rootFile->Write();
      this->rootFile->Close();
    }
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void MainWindow::onConnectCameraClicked()
{
  bool connected = false;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();
  AbstractCamera::CameraDeviceData cameraData;

  AbstractCamera* cameraDevice{ nullptr };
  CameraProfilesDialog* cameraProfiles{ nullptr };

  auto camerasAvailable = CameraUtils::getCamerasData();
  for (const AbstractCamera::CameraDeviceData& data : camerasAvailable)
  {
    if (cameraID == data.ID)
    {
      cameraData = data;
      break;
    }
  }
  cameraDevice = this->getCamera(cameraID);
  cameraProfiles = this->getProfiles(cameraID);

  QString& commandDeviceName = cameraData.CommandDeviceName;
  QString& dataDeviceName = cameraData.DataDeviceName;
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
      cameraDevice = new Camera2(cameraData, this);
    }
    else
    {
      cameraDevice = new FullCamera(cameraData, this);
    }
    cameraProfiles = new CameraProfilesDialog(cameraData, this);
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
        // Create ROOT file
        this->rootFile = std::make_unique< TFile >(name.toLatin1().data(), "RECREATE");
      }
      QSettings settings("ProfileCamera2D", "configure");
      this->cameraDeviceProfilesMap.insert(cameraID, qMakePair(cameraDevice, cameraProfiles));
      cameraDevice->loadSettings(&settings);
      cameraProfiles->setCameraDevice(cameraDevice);

      QObject::connect(cameraDevice, SIGNAL(logMessage(QString,QString,QColor)),
        this, SLOT(log(QString,QString,QColor)));
      QObject::connect(cameraProfiles, SIGNAL(logMessage(QString,QString,QColor)),
        this, SLOT(log(QString,QString,QColor)));
      QObject::connect(this->ui->CheckBox_ShowCameraProfiles, SIGNAL(toggled(bool)),
        cameraProfiles, SLOT(setVisible(bool)));
      QObject::connect(cameraDevice, SIGNAL(initiationStarted()),
        this, SLOT(onCameraInitiationStarted()));
      QObject::connect(cameraDevice, SIGNAL(initiationProgress(int)),
        this, SLOT(onCameraInitiationInProgress(int)));
      QObject::connect(cameraDevice, SIGNAL(initiationFinished()),
        this, SLOT(onCameraFirstContactFinished()));
      QObject::connect(cameraDevice, SIGNAL(commandWritten(QByteArray)),
        this, SLOT(onCameraCommandWritten(QByteArray)));
      QObject::connect(cameraDevice, SIGNAL(firmwareCommandBufferIsReset()),
        this, SLOT(onCameraFirstContactFinished()));
      QObject::connect(cameraDevice, SIGNAL(acquisitionStarted()),
        this, SLOT(onCameraAcquisitionStarted()));
      QObject::connect(cameraDevice, SIGNAL(acquisitionFinished()),
        this, SLOT(onCameraAcquisitionFinished()));
    }
  }
  else if (cameraDevice && connected && !commandDeviceName.isEmpty() && !dataDeviceName.isEmpty())
  {
    return; // Error message, camera has been already connected
  }

  if (cameraDevice && cameraDevice->connect())
  {
    this->updateAcquisitionParameters(cameraDevice);
    // create TDirectory for camera data
    if (this->rootFile)
    {
      TDirectory* dir = this->rootFile->mkdir(cameraID.toLatin1().data(), "camera data", kTRUE);
      cameraDevice->setRootDirectory(dir);
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
    QObject::disconnect(cameraDevice, SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(cameraProfiles, SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(this->ui->CheckBox_ShowCameraProfiles, SIGNAL(toggled(bool)),
      cameraProfiles, SLOT(setVisible(bool)));
    QObject::disconnect(cameraDevice, SIGNAL(initiationStarted()),
      this, SLOT(onCameraInitiationStarted()));
    QObject::disconnect(cameraDevice, SIGNAL(initiationProgress(int)),
      this, SLOT(onCameraInitiationInProgress(int)));
    QObject::disconnect(cameraDevice, SIGNAL(initiationFinished()),
      this, SLOT(onCameraFirstContactFinished()));
    QObject::disconnect(cameraDevice, SIGNAL(commandWritten(QByteArray)),
      this, SLOT(onCameraCommandWritten(QByteArray)));
    QObject::disconnect(cameraDevice, SIGNAL(firmwareCommandBufferIsReset()),
      this, SLOT(onCameraFirstContactFinished()));
    QObject::disconnect(cameraDevice, SIGNAL(acquisitionStarted()),
      this, SLOT(onCameraAcquisitionStarted()));
    QObject::disconnect(cameraDevice, SIGNAL(acquisitionFinished()),
      this, SLOT(onCameraAcquisitionFinished()));

    this->cameraConnectedFlag = false;
    this->ui->CheckBox_ShowCameraProfiles->setChecked(false);
    this->updateUiState();

    QMessageBox msgBox(this);
    msgBox.setModal(true);
    msgBox.setWindowTitle(tr("CameraProfile2D"));
    msgBox.setText(tr("Unable to connect port %1.\n%2.").arg(commandDeviceName).arg(cameraDevice->getCommandPortError()));
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
    QObject::disconnect(cameraProfiles.data(), SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
    QObject::disconnect(this->ui->CheckBox_ShowCameraProfiles, SIGNAL(toggled(bool)),
      cameraProfiles.data(), SLOT(setVisible(bool)));
  }
  if (cameraDevice)
  {
    QObject::disconnect(cameraDevice.data(), SIGNAL(logMessage(QString,QString,QColor)),
      this, SLOT(log(QString,QString,QColor)));
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
    QSettings settings("ProfileCamera2D", "configure");
    cameraDevice->saveSettings(&settings);
    cameraDevice->disconnect();
    this->cameraConnectedFlag = false;
    this->updateUiState();
//    this->cameraDeviceProfilesMap[cameraID] = qMakePair(QSharedPointer< AbstractCamera >(), QSharedPointer< CameraProfilesDialog >());
    this->cameraDeviceProfilesMap.remove(cameraID);
    this->log(tr("%1 has been disconnected!").arg(cameraID));
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
    if (cameraDevice && cameraDevice->isDeviceAlreadyConnected())
    {
      this->getProfiles(cameraID)->show();
      connected = true;
    }
    this->updateAcquisitionParameters(cameraDevice);
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

//  this->ui->CollapsibleButton_RunParameters->setEnabled(this->cameraConnectedFlag);
}

void MainWindow::updateAcquisitionParameters(AbstractCamera* cameraDevice)
{
  if (!cameraDevice)
  {
    return;
  }

  AcquisitionParameters params = cameraDevice->getAcquisitionParameters();
  std::bitset< CHIPS_PER_CAMERA > chipsEnabled = params.ChipsEnabled;
  AbstractCamera::ReverseBits< CHIPS_PER_CAMERA >(chipsEnabled); // important for GUI
  for (int i = 0; i < CHIPS_PER_CAMERA; ++i)
  {
    QModelIndex index = this->ui->CheckableComboBox_ChipsEnabled->model()->index(i, 0);
    Qt::CheckState state = chipsEnabled.test(i) ? Qt::Checked : Qt::Unchecked;
    this->ui->CheckableComboBox_ChipsEnabled->setCheckState(index, state);
  }
  this->ui->SliderWidget_IntegrationTime->setValue(double(params.IntegrationTimeMs));
  this->ui->SliderWidget_Samples->setValue(double(params.IntegrationSamples));
  this->ui->CheckBox_ExternalStart->setChecked(params.ExternalStartFlag);
  this->ui->ComboBox_AcquisitionCapacity->setCurrentIndex(params.CapacityCode);
  this->ui->RadioButton_Adc16Bit->setChecked(params.AdcMode == AdcResolutionType::ADC_16_BIT);
}

void MainWindow::getCamerasAvailable()
{
  QStringList cameraIDs = CameraUtils::getCameraIDs();
  this->ui->ComboBox_Cameras->clear();
  for (const QString& id : cameraIDs)
  {
    this->ui->ComboBox_Cameras->addItem(id);
    this->ui->ComboBox_BeamPathCamera1->addItem(id);
    this->ui->ComboBox_BeamPathCamera2->addItem(id);
  }
}

int MainWindow::getChipsEnabledCode() const
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
//  this->log(tr("Chips enabled code: %1").arg(this->getChipsEnabledCode()));
}

AbstractCamera* MainWindow::getCamera(const QString& cameraID) const
{
  if (cameraID.isEmpty())
  {
    return nullptr;
  }
  const CameraDeviceProfilesPair& pair = this->cameraDeviceProfilesMap[cameraID];
  return pair.first;
}

CameraProfilesDialog* MainWindow::getProfiles(const QString& cameraID) const
{
  if (cameraID.isEmpty())
  {
      return nullptr;
  }
  const CameraDeviceProfilesPair& pair = this->cameraDeviceProfilesMap[cameraID];
  return pair.second;
}

AbstractCamera* MainWindow::getCurrentCamera() const
{
//  QString commandDeviceName, dataDeviceName;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();

  auto camerasData = CameraUtils::getCamerasData();
  bool found = false;
  for (const AbstractCamera::CameraDeviceData& data : camerasData)
  {
    if (cameraID == data.ID)
    {
//      commandDeviceName = data.CommandDeviceName;
//      dataDeviceName = data.DataDeviceName;
      found = true;
      break;
    }
  }
  return (found ? this->getCamera(cameraID) : nullptr);
}

CameraProfilesDialog* MainWindow::getCurrentProfiles() const
{
//  QString commandDeviceName, dataDeviceName;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();

  auto camerasData = CameraUtils::getCamerasData();
  bool found = false;
  for (const AbstractCamera::CameraDeviceData& data : camerasData)
  {
    if (cameraID == data.ID)
    {
//      commandDeviceName = data.CommandDeviceName;
//      dataDeviceName = data.DataDeviceName;
      found = true;
      break;
    }
  }
  return (found ? this->getProfiles(cameraID) : nullptr);
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
    int chipsCode = this->getChipsEnabledCode();
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

  int chipsEnabled = this->getChipsEnabledCode();
  com = camera->getSetChipsEnabledCommand(chipsEnabled);
  initCommands.append(com); // write enabled chips

  int samples = static_cast< int >(this->ui->SliderWidget_Samples->value());
  com = camera->getSetSamplesCommand(samples);
  initCommands.append(com); // write number of samples

  int intTimeMs = this->ui->SliderWidget_IntegrationTime->value();
  com = camera->getSetIntegrationTimeCommand(intTimeMs); // intergration time in ms
  initCommands.append(com); // write integration time

  int capacityIndex = this->ui->ComboBox_AcquisitionCapacity->currentIndex();
  com = camera->getSetCapacityCommand(capacityIndex);
  initCommands.append(com); // write chip capacity

  bool adc20Bit = this->ui->RadioButton_Adc20Bit->isChecked(); // ADC Resolution (16 or 20 bit)
  com = camera->getSetAdcResolutionCommand(adc20Bit);
  initCommands.append(com); // write ADC resolution

  com = camera->getWriteChipsCapacitiesCommand();
  initCommands.append(com); // write chips capacities (write DDC232 configureation register)

  bool extStart = this->ui->CheckBox_ExternalStart->isChecked(); // external start flag
  com = camera->getSetExternalStartCommand(extStart);
  initCommands.append(com); // write beam extraction interrupts

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
    QTimer::singleShot(100, camera, SLOT(writeNextCommandFromInitiationList()));
  }
}

void MainWindow::onCameraInitiationStarted()
{
  auto camera = this->getCurrentCamera();
  if (camera && !camera->isInitiationListEmpty())
  {
    QTimer::singleShot(100, camera, SLOT(writeNextCommandFromInitiationList()));
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

void MainWindow::onCameraAcquisitionStarted()
{
  AbstractCamera* cam = qobject_cast< AbstractCamera* >(this->sender());
  if (!cam)
  {
    return;
  }
  AbstractCamera::CameraDeviceData data = cam->getCameraData();
  QString msg = tr("%1 acquisition is started").arg(data.ID);
  this->log(msg, Qt::blue);
}

void MainWindow::onCameraAcquisitionFinished()
{
  AbstractCamera* cam = qobject_cast< AbstractCamera* >(this->sender());
  if (!cam)
  {
    return;
  }
  AbstractCamera::CameraDeviceData data = cam->getCameraData();
  QString msg = tr("%1 acquisition is finished").arg(data.ID);
  this->log(msg, Qt::blue);
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
  QScopedPointer< QFileDialog > dialog(new QFileDialog(this, tr("Open ROOT File...")));
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

  if (this->rootFileName.isEmpty())
  {
    return;
  }

  QScopedPointer< RootFileCameraProfilesDialog > rootFileDialog(new RootFileCameraProfilesDialog(this->rootFileName, this));
  connect(rootFileDialog.get(), SIGNAL(logMessage(QString, QString, QColor)), this, SLOT(log(QString, QString, QColor)));
  if (rootFileDialog->exec())
  {
    ;
  }
}

void MainWindow::onCameraSettingsActionTriggered()
{
  QScopedPointer< SettingsDialog > settingsDialog(new SettingsDialog(this->getCurrentCamera(), this));
  if (settingsDialog->exec())
  {
    ;
  }
}

void MainWindow::onHttpServerActionTriggered()
{
  HttpServerDialog* serverDialog = new HttpServerDialog(httpServer, this);
  CameraDeviceProfilesMap::iterator iter = cameraDeviceProfilesMap.begin();

  while (iter != cameraDeviceProfilesMap.end())
  {
//    QString id = iter.key();
    CameraDeviceProfilesPair& deviceProfiles = iter.value();
    CameraProfilesDialog* profilesDialog = deviceProfiles.second;
//    if (profilesDialog)
//    {
//      profilesDialog->addBeamProfilesToServer(serverDialog->getUpdatedServer().get());
//    }
    ++iter;
//    if (serverDialog)
//    {
//      serverDialog->registerHistograms(id, profilesDialog);
//    }
  }
  if (serverDialog->exec())
  {
    ;
  }
  delete serverDialog;
}

void MainWindow::onBeamActionTriggered()
{
  QString id_camera1 = this->ui->ComboBox_BeamPathCamera1->currentText();
  QString id_camera2 = this->ui->ComboBox_BeamPathCamera2->currentText();

  AbstractCamera* camera1 = this->getCamera(id_camera1);
  AbstractCamera* camera2 = this->getCamera(id_camera2);

  CameraProfilesDialog* dialog1 = this->getProfiles(id_camera1);
  CameraProfilesDialog* dialog2 = this->getProfiles(id_camera2);

  if (!beamPathProfile)
  {
    beamPathProfile.reset(new BeamPathProfileDialog(this));
    if (dialog1 && dialog2)
    {
      connect(dialog1, SIGNAL(profilesUpdated(const QString&)),
        beamPathProfile.get(), SLOT(processCameraProfiles(const QString&)));
      connect(dialog2, SIGNAL(profilesUpdated(const QString&)),
        beamPathProfile.get(), SLOT(processCameraProfiles(const QString&)));
    }
    beamPathProfile->setCamerasDevices(camera1, camera2);
    beamPathProfile->addBeamProfilesToServer(httpServer);
    beamPathProfile->show();
  }
  if (beamPathProfile && beamPathProfile->isHidden())
  {
    beamPathProfile->show();
    beamPathProfile->raise();
  }
}

bool MainWindow::saveSettings(QSettings *settings)
{
  Q_UNUSED(settings);
  return false;
}

bool MainWindow::loadSettings(QSettings *settings)
{
  if (!settings)
  {
    return false;
  }
  settings->beginGroup("RootHttpServer");
  QString host = settings->value("host", "localhost").toString();
  int port = settings->value("port", 8080).toInt();
  bool connect = settings->value("connect-on-startup", true).toBool();
  settings->endGroup();
  if (connect)
  {
    QString hostPortString = host + QString(':') + QString::number(port);
    QByteArray bytesString = hostPortString.toLatin1();
//    this->httpServer = std::shared_ptr< THttpServer >(new THttpServer(bytesString.data()));
    this->httpServer = std::shared_ptr< THttpServer >(new THttpServer("http:8080"));
  }
  return true;
}

void MainWindow::onTestClicked()
{
  auto hist = new TH1F("gaus_standalone","Example of standalone TCanvas", 100, -5, 5);
  hist->FillRandom("gaus", 10000);
  hist->SetDirectory(nullptr);
  Double_t m = hist->GetMaximum();
  hist->Scale(100. / m);

  auto canvas = new TCanvas("standalone", "Standalone canvas");
  canvas->cd();
  auto form = new TF1("prof", "gaus", -3., 3.);
  form->SetParameter(0, 100.);
  form->SetParameter(1, 0.0);
  form->SetParameter(2, 1.0);
  hist->Fit("prof");

  hist->Draw("hist");
  form->Draw("SAME");

  canvas->Modified();
  canvas->Update();
}

QT_END_NAMESPACE
