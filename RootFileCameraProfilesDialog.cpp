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

#include <TGraph.h>
#include <TH2.h>
#include <TPad.h>
#include <TWebPadPainter.h>
#include <TWebCanvas.h>
#include <TCanvas.h>
#include <TFrame.h>
#include <TLine.h>
#include <TImage.h>
#include <TImageDump.h>
#include <TAttImage.h>
#include <TSystem.h>

#include <TFile.h>
#include <TTree.h>
#include <TBranch.h>
#include <TKey.h>
#include <TROOT.h>

#include <QScopedPointer>
#include <QPointer>
#include <QMap>
#include <QString>
#include <QTimer>
#include <QSettings>
#include <QProgressDialog>
#include <QListWidgetItem>
#include <QFile>
#include <QDir>

#include "RootFileCameraProfilesDialog.h"
#include "ui_RootFileCameraProfilesDialog.h"

#include "FullCamera.h"
#include "Camera2.h"
#include "CameraUtils.h"

#include "CameraSpillsListModel.h"

#include <sstream>

QT_BEGIN_NAMESPACE

namespace Ui {
class RootFileCameraProfilesDialog;
}

class RootFileCameraProfilesDialogPrivate
{
  Q_DECLARE_PUBLIC(RootFileCameraProfilesDialog);
protected:
  RootFileCameraProfilesDialog* const q_ptr;
  QScopedPointer< Ui::RootFileCameraProfilesDialog > ui;
public:
  RootFileCameraProfilesDialogPrivate(RootFileCameraProfilesDialog &object);
  virtual ~RootFileCameraProfilesDialogPrivate();
  void processRootFile(const QString& filename);
  AbstractCamera* getCamera(const QString& cameraId) const;
  AbstractCamera* getCamera() const { return this->getCamera(this->cameraID); }

  QMap< QString, QPointer< AbstractCamera > > camerasMap;
  std::unique_ptr< TFile > rootFile;
  QScopedPointer< CameraSpillsListModel > cameraDirectorySpillsModel;

  std::unique_ptr< TGraph > graphChannel; // TGraph adc time data
  std::unique_ptr< TPad > padChannel;

  std::unique_ptr< TLine > linePedBegin;
  std::unique_ptr< TLine > linePedEnd;
  std::unique_ptr< TLine > lineSigBegin;
  std::unique_ptr< TLine > lineSigEnd;

  std::unique_ptr< TGraph > graphVerticalProfile;
  std::unique_ptr< TPad > padVerticalProfile;

  std::unique_ptr< TGraph > graphHorizontalProfile;
  std::unique_ptr< TPad > padHorizontalProfile;

  std::unique_ptr< TH2 > histPseudo2D;
  std::unique_ptr< TPad > padPseudo2D;
  QScopedPointer< QTimer > timer;
  std::unique_ptr< TFile > rootFramesFile;
  QString cameraID;
};

RootFileCameraProfilesDialogPrivate::RootFileCameraProfilesDialogPrivate(RootFileCameraProfilesDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::RootFileCameraProfilesDialog),
  cameraDirectorySpillsModel(new CameraSpillsListModel(&object)),
  timer(new QTimer(&object))
{
  QSettings settings("ProfileCamera2D", "configure");
  QStringList camerasID = CameraUtils::getCameraIDs();
  int i = 1;
  for (const QString& cameraID : camerasID)
  {
    AbstractCamera::CameraDeviceData data;
    if (cameraID == "Camera2")
    {
      data = CameraUtils::getPartialCamera2Data();
      AbstractCamera* camera = new Camera2(data, &object);
      this->camerasMap.insert(data.ID, camera);
      camera->loadSettings(&settings);
    }
    else
    {
      data = CameraUtils::getFullCameraData(i);
      AbstractCamera* fullCamera = new FullCamera(data, &object);
      this->camerasMap.insert(data.ID, fullCamera);
      fullCamera->loadSettings(&settings);
    }
    ++i;
  }
/*
  AbstractCamera::CameraDeviceData data1, data2, data3, data4;

  data1 = CameraUtils::getFullCameraData(1);
  AbstractCamera* fullCamera = new FullCamera(data1, &object);
  this->camerasMap.insert(data1.ID, fullCamera);
  fullCamera->loadSettings(&settings);

  data2 = CameraUtils::getPartialCamera2Data();
  AbstractCamera* camera2 = new Camera2(data2, &object);
  this->camerasMap.insert(data2.ID, camera2);
  camera2->loadSettings(&settings);

  data3 = CameraUtils::getFullCameraData(3);
  fullCamera = new FullCamera(data3, &object);
  this->camerasMap.insert(data3.ID, fullCamera);
  fullCamera->loadSettings(&settings);

  data4 = CameraUtils::getFullCameraData(4);
  fullCamera = new FullCamera(data4, &object);
  this->camerasMap.insert(data4.ID, fullCamera);
  fullCamera->loadSettings(&settings);
*/
}

RootFileCameraProfilesDialogPrivate::~RootFileCameraProfilesDialogPrivate()
{
  if (rootFile)
  {
    rootFile->Close();
  }
}

void RootFileCameraProfilesDialogPrivate::processRootFile(const QString& rootFileName)
{
  QByteArray fileName = rootFileName.toLatin1();
  rootFile.reset(TFile::Open(fileName.data(), "READ"));
  TIter keyList(rootFile->GetListOfKeys());
  TKey* key = nullptr;
  std::vector< std::pair< TDirectory*, TTree* > > spillData;
  while (key = dynamic_cast< TKey* >(keyList()))
  {
    TClass* cl = gROOT->GetClass(key->GetClassName());
    if (cl && !cl->InheritsFrom("TDirectory"))
    {
      continue;
    }
    if (TDirectory* dir = dynamic_cast< TDirectory* >(key->ReadObj()))
    {
      TIter keyDirList(dir->GetListOfKeys());
      while (TKey* treeKey = dynamic_cast< TKey* >(keyDirList()))
      {
        TClass* treeClass = gROOT->GetClass(treeKey->GetClassName());
        if (treeClass && !treeClass->InheritsFrom("TTree"))
        {
          continue;
        }
        if (TTree *spillTree = dynamic_cast< TTree* >(treeKey->ReadObj()))
        {
          // add TDir and TTree to the CameraSpillsListModel
          spillData.push_back(std::make_pair(dir, spillTree));
        }
      }
    }
  }
  this->cameraDirectorySpillsModel->setSpillsData(spillData);
}

AbstractCamera* RootFileCameraProfilesDialogPrivate::getCamera(const QString& cameraId) const
{
  return !cameraId.isEmpty() ? this->camerasMap[cameraId].data() : nullptr;
/*
  if (!cameraId.isEmpty())
  {
    bool res = false;
    QStringList keys = this->camerasMap.keys();
    for (const auto& cameraIdKey : keys)
    {
      if (cameraIdKey == cameraId)
      {
        res = true;
        break;
      }
    }
    return res ? this->camerasMap[cameraId].data() : nullptr;
  }
  return nullptr;
*/
}

RootFileCameraProfilesDialog::RootFileCameraProfilesDialog(const QString& rootFileName, QWidget *parent)
  :
  QDialog(parent),
  d_ptr(new RootFileCameraProfilesDialogPrivate(*this))
{
  Q_D(RootFileCameraProfilesDialog);
  d->ui->setupUi(this);

  d->ui->ListView_CameraSpills->setModel(d->cameraDirectorySpillsModel.data());

  {
    QSignalBlocker blockerPed(d->ui->RangeWidget_Pedestal);
    d->ui->RangeWidget_Pedestal->setValues(30., 190.);
    QSignalBlocker blockerSig(d->ui->RangeWidget_Signal);
    d->ui->RangeWidget_Signal->setValues(199., 1070.);
  }

  d->timer->setInterval(100);

  this->setWindowTitle(tr("Process ROOT file data"));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);

  QObject::connect(d->ui->PushButton_UpdateProfiles, SIGNAL(clicked()),
    this, SLOT(onUpdateProfilesClicked()));
  QObject::connect(d->ui->RangeWidget_Pedestal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onPedestalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_Pedestal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onPedestalEndChanged(double)));
  QObject::connect(d->ui->RangeWidget_Signal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onSignalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_Signal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onSignalEndChanged(double)));
  QObject::connect(d->ui->PushButton_UpdateChannelGraph, SIGNAL(clicked()),
    this, SLOT(onUpdateGraphClicked()));
  QObject::connect(d->ui->HorizontalSlider_FramesRange, SIGNAL(valueChanged(int)),
    this, SLOT(onProfileFrameRangeValueChanged(int)));
  QObject::connect(d->ui->PushButton_ClearProfiles2D, SIGNAL(clicked()),
    this, SLOT(onClearProfilesClicked()));
  QObject::connect(d->ui->PushButton_PlayProfiles2D, SIGNAL(clicked()),
    this, SLOT(onPlayProfilesClicked()));
  QObject::connect(d->ui->ListView_CameraSpills, SIGNAL(clicked(QModelIndex)),
    this, SLOT(onCameraSpillsModelIndexClicked(QModelIndex)));

  QObject::connect(d->timer.data(), SIGNAL(timeout()), this, SLOT(updateProfileFrame()));

  d->graphChannel.reset(new TGraph());
  TCanvas* canvas = d->ui->RootCanvas_ChannelGraph->getCanvas();
  canvas->cd();
  d->padChannel = std::unique_ptr< TPad >(new TPad("pCh", "Grid", 0., 0., 1., 1.));
  d->padChannel->Draw();
  d->padChannel->SetGrid();
  d->padChannel->cd();
  d->graphChannel->Draw("AL*");

  canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile = std::unique_ptr< TPad >(new TPad("pVerProf", "Grid", 0., 0., 1., 1.));
  d->padVerticalProfile->SetGrid();
  d->padVerticalProfile->Draw();

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile = std::unique_ptr< TPad >(new TPad("pHorizProf", "Grid", 0., 0., 1., 1.));
  d->padHorizontalProfile->SetGrid();
  d->padHorizontalProfile->Draw();

  canvas = d->ui->RootCanvas_Profiles2D->getCanvas();
  canvas->cd();
  d->padPseudo2D = std::unique_ptr< TPad >(new TPad("pPs2D", "Grid", 0., 0., 1., 1.));
  d->padPseudo2D->SetGrid();
  d->padPseudo2D->Draw();

  d->processRootFile(rootFileName);
}

RootFileCameraProfilesDialog::~RootFileCameraProfilesDialog()
{
  Q_D(RootFileCameraProfilesDialog);
}

/*
void RootFileCameraProfilesDialog::onCurrentRootTreeItemChanged(QListWidgetItem* newItem,QListWidgetItem*)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->rootFile)
  {
    return;
  }
  if (!newItem)
  {
    return;
  }

  QString treeName = newItem->text();
  QByteArray treeNameByteArray = treeName.toLatin1();
  TIter keyList(d->rootFile->GetListOfKeys());
  TKey* key = nullptr;
  while (key = dynamic_cast< TKey* >(keyList()))
  {
    TClass* cl = gROOT->GetClass(key->GetClassName());
    if (cl && !cl->InheritsFrom("TDirectory"))
    {
      continue;
    }
    if (TDirectory* dir = dynamic_cast< TDirectory* >(key->ReadObj()))
    {
      TTree* spillTree = dynamic_cast< TTree* >(dir->Get(treeNameByteArray.data()));
      if (spillTree)
      {
        AbstractCamera* cam = d->getCamera();
        if (cam)
        {
          cam->processExternalData(spillTree);
          QString chipsStr = cam->getChipsAddresses();
          d->ui->LineEdit_EnabledChips->setText(chipsStr);
        }
      }
    }
  }
}
*/

void RootFileCameraProfilesDialog::onUpdateGraphClicked()
{
  Q_D(RootFileCameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  AbstractCamera* cam = d->getCamera();

  if (cam)
  {
    intTimeMs = cam->getIntegrationTimeMs() / 2;
    nofSamples = cam->getAdcSamples();
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Camera is invalid";
//    emit logMessage("Camera is invalid", "", Qt::red);
    return;
  }
  int chipIndex = static_cast< int >(d->ui->SliderWidget_Chip->value()) - 1;
  int channelIndex = static_cast< int >(d->ui->SliderWidget_Channel->value()) - 1;

  std::vector< int > adcData;
  size_t adcDataSize = cam->getAdcData(chipIndex, channelIndex, adcData, AdcTimeType::INTEGRATOR_AB);
  if (!adcDataSize)
  {
    qWarning() << Q_FUNC_INFO << "ADC Side A and B are empty";
//    emit logMessage("ADC Side A and B are empty", d->camera->getCameraData().id, Qt::red);
    return;
  }

  // Clear pad, reset channel graph
  d->padChannel->cd();
  d->padChannel->Clear();

  int rangeSizeAB = adcData.size();
  d->graphChannel = std::make_unique< TGraph >(rangeSizeAB);
  d->graphChannel->SetTitle("Channel data;Time (ms);Amplitude");
  d->graphChannel->SetLineColor(kBlack);
  d->graphChannel->SetLineWidth(2);
  d->graphChannel->SetMarkerColor(kBlack);
  d->graphChannel->SetMarkerSize(1.4);
  d->graphChannel->SetMarkerStyle(2);
  Int_t i = 0;
  for (int adcValue : adcData)
  {
    d->graphChannel->SetPoint(i, i * intTimeMs, Double_t(adcValue));
    ++i;
  }
  d->graphChannel->Draw("AL*");
  d->padChannel->Modified();
  d->padChannel->Update();

  d->ui->RangeWidget_Pedestal->setRange(0., 2. * intTimeMs * rangeSizeAB);
  d->ui->RangeWidget_Pedestal->setSingleStep(intTimeMs);
  d->ui->RangeWidget_Signal->setRange(0., 2. * intTimeMs * rangeSizeAB);
  d->ui->RangeWidget_Signal->setSingleStep(intTimeMs);

  this->onPedestalBeginChanged(d->ui->RangeWidget_Pedestal->minimumValue());
  this->onPedestalEndChanged(d->ui->RangeWidget_Pedestal->maximumValue());
  this->onSignalBeginChanged(d->ui->RangeWidget_Signal->minimumValue());
  this->onSignalEndChanged(d->ui->RangeWidget_Signal->maximumValue());
}

void RootFileCameraProfilesDialog::onUpdateProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);

  AbstractCamera* cam = d->getCamera();

  if (!cam)
  {
    qWarning() << Q_FUNC_INFO << "Camera is invalid";
//    emit logMessage("Camera is invalid", "", Qt::red);
    return;
  }
  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());

  cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  // process data and upgrade profiles graphs and 2d histos
  cam->processDataCounts();

  std::map< ChipChannelPair, ChannelInfoPair > infoMap;
  cam->getChipChannelInfo(infoMap);
  if (!infoMap.size())
  {
//    qWarning() << Q_FUNC_INFO << "Camera data is empty";
    emit logMessage("Camera data is empty", "", Qt::red);
    return;
  }

  cam->updateProfiles(d->graphVerticalProfile.get(), d->graphHorizontalProfile.get(), false);
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);

  // clear vertical profile pad, reset vertical profile graph
  d->padVerticalProfile->cd();
  d->padVerticalProfile->Clear();
  d->graphVerticalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");
  d->padVerticalProfile->Modified();
  d->padVerticalProfile->Update();

  // clear horizontal profile pad, reset horizontal profile graph
  d->padHorizontalProfile->cd();
  d->padHorizontalProfile->Clear();
  d->graphHorizontalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");
  d->padHorizontalProfile->Modified();
  d->padHorizontalProfile->Update();

  // update pseudo 2D histogram
  d->padPseudo2D->cd();
  d->padPseudo2D->Modified();
  d->padPseudo2D->Update();

  // dump info into temporary text file
  auto cameraData = cam->getCameraData();
  QString dataFileName = QDir::tempPath() + QString(QDir::separator()) + cameraData.ID + "_statdump.txt";
  QScopedPointer< QFile > dataFile(new QFile(dataFileName));
  bool res = dataFile->open(QIODevice::WriteOnly | QIODevice::Text);
  if (!res)
  {
    return;
  }
  QTextStream dataFileStream(dataFile.get()); // dump info to temp file for amplitude calibration
  using MapConstIter = std::map< ChipChannelPair, ChannelInfoPair >::const_iterator;
  for (MapConstIter iter = infoMap.begin(); iter != infoMap.end(); ++iter)
  {
    const ChipChannelPair& chipChannelPair = (*iter).first;
    const ChannelInfoPair& infoPair = (*iter).second;
    const ChannelInfo& rawInfo = infoPair.first;
    const ChannelInfo& adcCalibInfo = infoPair.second;
    double pedDispA = rawInfo.PedMom2A - rawInfo.PedMeanA * rawInfo.PedMeanA;
    double sigDispA = rawInfo.SigMom2A - rawInfo.SigMeanA * rawInfo.SigMeanA;
    double signalA = rawInfo.SigMeanA - rawInfo.PedMeanA;
    double skoA = std::sqrt(pedDispA + sigDispA);
    if (res)
    {
      dataFileStream << qSetFieldWidth(10) \
        << chipChannelPair.first << ' ' << chipChannelPair.second << ' ' \
        << rawInfo.SignalNoAmp << ' ' << adcCalibInfo.SignalNoAmp << ' ' \
        << signalA << ' ' << skoA << '\n';
    }
  }
  dataFileStream.flush();
  res = dataFile->flush();
  if (res)
  {
    dataFile->close();
  }
}

void RootFileCameraProfilesDialog::onPedestalBeginChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->linePedBegin)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->linePedBegin.get());
  }
  d->linePedBegin = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->linePedBegin->SetLineColor(kBlue);
  d->linePedBegin->SetLineStyle(7);
  d->linePedBegin->SetLineWidth(3.);
  d->linePedBegin->SetVertical(kTRUE);
  d->linePedBegin->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(pos);
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onPedestalEndChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->linePedEnd)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->linePedEnd.get());
  }
  d->linePedEnd = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->linePedEnd->SetLineColor(kBlue);
  d->linePedEnd->SetLineStyle(7);
  d->linePedEnd->SetLineWidth(3.);
  d->linePedEnd->SetVertical(kTRUE);
  d->linePedEnd->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(pos);
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onSignalBeginChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->lineSigBegin)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->lineSigBegin.get());
  }
  d->lineSigBegin = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->lineSigBegin->SetLineColor(kRed);
  d->lineSigBegin->SetLineStyle(7);
  d->lineSigBegin->SetLineWidth(3.);
  d->lineSigBegin->SetVertical(kTRUE);
  d->lineSigBegin->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(pos);
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onSignalEndChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->lineSigEnd)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->lineSigEnd.get());
  }
  d->lineSigEnd = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->lineSigEnd->SetLineColor(kRed);
  d->lineSigEnd->SetLineStyle(7);
  d->lineSigEnd->SetLineWidth(3.);
  d->lineSigEnd->SetVertical(kTRUE);
  d->lineSigEnd->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(pos);
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onProfileFrameRangeValueChanged(int)
{
  Q_D(RootFileCameraProfilesDialog);
}

void RootFileCameraProfilesDialog::onClearProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  d->timer->stop();
  d->ui->HorizontalSlider_FramesRange->setValue(d->ui->HorizontalSlider_FramesRange->minimum());
}

void RootFileCameraProfilesDialog::updateProfileFrame()
{
  Q_D(RootFileCameraProfilesDialog);
  d->timer->stop();
  if (d->ui->HorizontalSlider_FramesRange->value() == d->ui->HorizontalSlider_FramesRange->maximum())
  {
    this->onUpdateProfilesClicked();
    d->rootFramesFile->Close();
    d->rootFramesFile.release();
    return;
  }

  AbstractCamera* cam = d->getCamera();
  if (!cam)
  {
    return;
  }
  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->HorizontalSlider_FramesRange->value() - d->ui->SliderWidget_TimeFrameMargin->value());
  int sigMax = static_cast< int >(d->ui->HorizontalSlider_FramesRange->value() + d->ui->SliderWidget_TimeFrameMargin->value());

  cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  cam->processDataCounts();

//  cam->updateProfiles(d->graphVerticalProfile.get(), d->graphHorizontalProfile.get(), false);
//  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);

  if (!d->ui->CheckBox_OffscreenProcess->isChecked())
  {
    // update vertical profile
    d->padVerticalProfile->cd();
    d->padVerticalProfile->Clear();
    d->graphVerticalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
    d->graphVerticalProfile->SetLineColor(kRed);
    d->graphVerticalProfile->SetLineWidth(2);
    d->graphVerticalProfile->SetMarkerColor(kBlue);
    d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
    d->graphVerticalProfile->Draw("AL*");
    d->padVerticalProfile->Modified();
    d->padVerticalProfile->Update();

    // update horizontal profile
    d->padHorizontalProfile->cd();
    d->padHorizontalProfile->Clear();
    d->graphHorizontalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
    d->graphHorizontalProfile->SetLineColor(kRed);
    d->graphHorizontalProfile->SetLineWidth(2);
    d->graphHorizontalProfile->SetMarkerColor(kBlue);
    d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
    d->graphHorizontalProfile->Draw("AL*");
    d->padHorizontalProfile->Modified();
    d->padHorizontalProfile->Update();
  }

  // update pseudo 2D histogram
  d->padPseudo2D->cd();
  if (d->ui->CheckBox_ChargeRange->isChecked())
  {
    d->histPseudo2D->GetZaxis()->SetRangeUser(d->ui->RangeWidget_ChargeRange->minimumValue(),
      d->ui->RangeWidget_ChargeRange->maximumValue());
  }
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);
  if (!d->ui->CheckBox_OffscreenProcess->isChecked())
  {
    d->padPseudo2D->Modified();
    d->padPseudo2D->Update();
  }

  QString titleString = QString::number(d->ui->HorizontalSlider_FramesRange->value());
  std::string titleStdString = "frame_" + titleString.toStdString();
//  d->rootFramesFile->cd();
//  d->histPseudo2D->Write(titleStdString.c_str());
  d->rootFramesFile->WriteTObject(d->histPseudo2D.get(), titleStdString.c_str());

  d->ui->HorizontalSlider_FramesRange->setValue(d->ui->HorizontalSlider_FramesRange->value() + d->ui->SliderWidget_TimeFrameStep->value());
  d->timer->start();
}

void RootFileCameraProfilesDialog::onPlayProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  if (d->ui->CheckBox_OffscreenProcess->isChecked())
  {
    d->timer->setInterval(10);
  }
  else
  {
    d->timer->setInterval(100);
  }
  d->timer->start();

  QDateTime dateTime = QDateTime::currentDateTime();
  std::stringstream sstream;
  sstream << "/tmp/Frames_" << d->cameraID.toStdString() << '_' << dateTime.toString("ddMMyyyy_hhmmss").toStdString() << ".root";
  std::string name = sstream.str();
  d->rootFramesFile = std::unique_ptr< TFile >(new TFile(name.c_str(), "RECREATE"));
}

void RootFileCameraProfilesDialog::onCameraSpillsModelIndexClicked(const QModelIndex& index)
{
  Q_D(RootFileCameraProfilesDialog);
  std::pair< TDirectory*, TTree* > dirSpillPair = d->cameraDirectorySpillsModel->getSpillData(index);
  TDirectory* dir = dirSpillPair.first;
  TTree* tree = dirSpillPair.second;

  if (!dir || !tree) // both pointers are valid
  {
    return;
  }

  QString camID(dir->GetName());
  AbstractCamera* cam = d->getCamera(camID);
  if (!cam) // camera ID is valid
  {
    return;
  }

  d->cameraID = camID;

  d->padVerticalProfile->cd();
  d->padVerticalProfile->Clear();
  TGraph* profv = cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
  d->graphVerticalProfile.reset(profv);
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");

  d->padHorizontalProfile->cd();
  d->padHorizontalProfile->Clear();
  TGraph* profh = cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);
  d->graphHorizontalProfile.reset(profh);
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");

  d->padPseudo2D->cd();
  if (d->histPseudo2D)
  {
    d->padPseudo2D->Remove(d->histPseudo2D.get());
  }
  TH2* h2D = cam->createProfile2D(false);
  h2D->SetDirectory(nullptr);
  d->histPseudo2D.reset(h2D);
  d->histPseudo2D->GetXaxis()->SetTitle("Horizontal profile, mm");
  d->histPseudo2D->GetYaxis()->SetTitle("Vertical profile, mm");
  d->histPseudo2D->Draw("COLZ");

  if (cam->processExternalData(tree))
  {
    QString chipsStr = cam->getChipsAddresses();
    d->ui->LineEdit_EnabledChips->setText(chipsStr);
  }
  else
  {
    emit logMessage("No chips available", d->cameraID, Qt::red);
    d->ui->LineEdit_EnabledChips->setText("");
  }
}

QT_END_NAMESPACE
