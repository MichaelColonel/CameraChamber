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
#include <TCanvas.h>
#include <TLine.h>

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

#include "RootFileCameraProfilesDialog.h"
#include "ui_RootFileCameraProfilesDialog.h"

#include "FullCamera.h"
#include "Camera2.h"

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

  QMap< QString, QPointer< AbstractCamera > > camerasMap;
  std::unique_ptr< TFile > rootFile;

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
};

RootFileCameraProfilesDialogPrivate::RootFileCameraProfilesDialogPrivate(RootFileCameraProfilesDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::RootFileCameraProfilesDialog),
  timer(new QTimer(&object))
{

  AbstractCamera::CameraDeviceData data1, data2;
  data1.id = "Camera1";
  data1.dataDirectory = "C26AIC01";
  this->camerasMap.insert(data1.id, new FullCamera(data1, &object));
  data2.id = "Camera2";
  data2.dataDirectory = "C26AIC02";
  this->camerasMap.insert(data2.id, new Camera2(data2, &object));

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
  rootFile.reset(TFile::Open(fileName.data()));
  TIter keyList(rootFile->GetListOfKeys());
  TKey* key = nullptr;
  while ((key = dynamic_cast< TKey* >(keyList())))
  {
    TClass* cl = gROOT->GetClass(key->GetClassName());
    if (cl && !cl->InheritsFrom("TTree"))
    {
      continue;
    }

    TTree *spillTree = dynamic_cast< TTree* >(key->ReadObj());
    if (spillTree)
    {
      const char* name = spillTree->GetName();
      this->ui->listWidget->addItem(QString(name));
    }
  }
}

RootFileCameraProfilesDialog::RootFileCameraProfilesDialog(const QString& rootFile, QWidget *parent)
  :
  QDialog(parent),
  d_ptr(new RootFileCameraProfilesDialogPrivate(*this))
{
  Q_D(RootFileCameraProfilesDialog);
  d->ui->setupUi(this);

  d->timer->setInterval(100);

  this->setWindowTitle(tr("Process ROOT file data"));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);

  QObject::connect(d->ui->listWidget, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
    this, SLOT(onCurrentRootTreeItemChanged(QListWidgetItem*,QListWidgetItem*)));
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

  QObject::connect(d->timer.data(), SIGNAL(timeout()), this, SLOT(updateProfileFrame()));

  d->graphChannel.reset(new TGraph());
  TCanvas* canvas = d->ui->RootCanvas_ChannelGraph->getCanvas();
  canvas->cd();
  d->padChannel = std::unique_ptr< TPad >(new TPad("pCh", "Grid", 0., 0., 1., 1.));
  d->padChannel->Draw();
  d->padChannel->SetGrid();
  d->padChannel->cd();
  d->graphChannel->Draw("APL*");

  canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile = std::unique_ptr< TPad >(new TPad("pVerProf", "Grid", 0., 0., 1., 1.));
  d->padVerticalProfile->SetGrid();
  d->padVerticalProfile->Draw();
  d->padVerticalProfile->cd();

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile = std::unique_ptr< TPad >(new TPad("pHorizProf", "Grid", 0., 0., 1., 1.));
  d->padHorizontalProfile->SetGrid();
  d->padHorizontalProfile->Draw();
  d->padHorizontalProfile->cd();

  canvas = d->ui->RootCanvas_Profiles2D->getCanvas();
  canvas->cd();
  d->padPseudo2D = std::unique_ptr< TPad >(new TPad("pPs2D", "Grid", 0., 0., 1., 1.));
  d->padPseudo2D->SetGrid();
  d->padPseudo2D->Draw();

  d->padVerticalProfile->cd();
  if (d->padVerticalProfile && d->graphVerticalProfile)
  {
    d->padVerticalProfile->GetListOfPrimitives()->Remove(d->graphVerticalProfile.get());
  }

  d->padHorizontalProfile->cd();
  if (d->padHorizontalProfile && d->graphHorizontalProfile)
  {
    d->padHorizontalProfile->GetListOfPrimitives()->Remove(d->graphHorizontalProfile.get());
  }

  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];

  TGraph* prof = cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
  d->graphVerticalProfile.reset(prof);
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");

  prof = cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);
  d->graphHorizontalProfile.reset(prof);
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");

  d->padPseudo2D->cd();
  TH2 *h2D = cam->createProfile2D(false);
  d->histPseudo2D.reset(h2D);

  d->histPseudo2D->GetXaxis()->SetTitle("Horizontal profile, mm");
  d->histPseudo2D->GetYaxis()->SetTitle("Vertical profile, mm");
  d->histPseudo2D->Draw("COLZ");

  d->processRootFile(rootFile);
}

RootFileCameraProfilesDialog::~RootFileCameraProfilesDialog()
{
  Q_D(RootFileCameraProfilesDialog);

  d->padVerticalProfile->cd();
  if (d->padVerticalProfile && d->graphVerticalProfile)
  {
    d->padVerticalProfile->GetListOfPrimitives()->Remove(d->graphVerticalProfile.get());
  }

  d->padHorizontalProfile->cd();
  if (d->padHorizontalProfile && d->graphHorizontalProfile)
  {
    d->padHorizontalProfile->GetListOfPrimitives()->Remove(d->graphHorizontalProfile.get());
  }

  d->padPseudo2D->cd();
  if (d->padPseudo2D && d->histPseudo2D)
  {
    d->padPseudo2D->GetListOfPrimitives()->Remove(d->histPseudo2D.get());
  }
}

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
  std::string treeStdName = treeName.toStdString();
  TTree* spillTree = dynamic_cast< TTree* >(d->rootFile->Get(treeStdName.c_str()));
  if (spillTree)
  {
    QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];
    if (cam)
    {
      cam->processExternalData(spillTree);
      QString chipsStr = cam->getChipsAddresses();
      d->ui->LineEdit_EnabledChips->setText(chipsStr);
    }
  }
}

void RootFileCameraProfilesDialog::onUpdateGraphClicked()
{
  Q_D(RootFileCameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];

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
  int rangeSizeAB = adcData.size();

  TCanvas* canvas = d->ui->RootCanvas_ChannelGraph->getCanvas();
  canvas->cd();
  d->padChannel->cd();
  if (d->graphChannel)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->graphChannel.get());
  }

  Int_t i = 0;
  d->graphChannel.reset(new TGraph(adcData.size()));
  d->graphChannel->SetTitle("Channel data;Time (ms);Amplitude");
  d->graphChannel->SetLineColor(kBlack);
  d->graphChannel->SetLineWidth(2);
  d->graphChannel->SetMarkerColor(kBlack);
  d->graphChannel->SetMarkerSize(1.4);
  d->graphChannel->SetMarkerStyle(2);
  for (int adcValue : adcData)
  {
    d->graphChannel->SetPoint(i, i * intTimeMs, Double_t(adcValue));
    ++i;
  }
  d->graphChannel->Draw("APL*");
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

  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];

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
  cam->processDataCounts();

  cam->updateProfiles(d->graphVerticalProfile.get(), d->graphHorizontalProfile.get(), false);
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);

  TCanvas* canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile->cd();
  if (d->padVerticalProfile)
  {
    d->padVerticalProfile->GetListOfPrimitives()->Remove(d->graphVerticalProfile.get());
  }
  d->graphVerticalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");
  d->padVerticalProfile->Modified();
  d->padVerticalProfile->Update();

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile->cd();
  if (d->padHorizontalProfile)
  {
    d->padHorizontalProfile->GetListOfPrimitives()->Remove(d->graphHorizontalProfile.get());
  }
  d->graphHorizontalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");
  d->padHorizontalProfile->Modified();
  d->padHorizontalProfile->Update();

  canvas = d->ui->RootCanvas_Profiles2D->getCanvas();
  canvas->cd();
  d->padPseudo2D->cd();
  d->padPseudo2D->Modified();
  d->padPseudo2D->Update();
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
  d->linePedBegin.reset(new TLine(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum()));
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
  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];
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
  d->linePedEnd.reset(new TLine(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum()));
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
  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];
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
  d->lineSigBegin.reset(new TLine(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum()));
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
  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];
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
  d->lineSigEnd.reset(new TLine(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum()));
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
  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onProfileFrameRangeValueChanged(int frame)
{
  Q_D(RootFileCameraProfilesDialog);
  qDebug() << Q_FUNC_INFO << frame;
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
    return;
  }

  QPointer< AbstractCamera > cam = d->camerasMap[QString("Camera1")];

  if (!cam)
  {
    return;
  }
  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->HorizontalSlider_FramesRange->value() - 50);
  int sigMax = static_cast< int >(d->ui->HorizontalSlider_FramesRange->value() + 50);

  cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  cam->processDataCounts();

  cam->updateProfiles(d->graphVerticalProfile.get(), nullptr, false);
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);

  TCanvas* canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile->cd();
  if (d->padVerticalProfile)
  {
    d->padVerticalProfile->GetListOfPrimitives()->Remove(d->graphVerticalProfile.get());
  }
  d->graphVerticalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");
  d->padVerticalProfile->Modified();
  d->padVerticalProfile->Update();

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile->cd();
  if (d->padHorizontalProfile)
  {
    d->padHorizontalProfile->GetListOfPrimitives()->Remove(d->graphHorizontalProfile.get());
  }
  d->graphHorizontalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");
  d->padHorizontalProfile->Modified();
  d->padHorizontalProfile->Update();

  canvas = d->ui->RootCanvas_Profiles2D->getCanvas();
  canvas->cd();
  d->padPseudo2D->cd();
  d->padPseudo2D->Modified();
  d->padPseudo2D->Update();
  d->ui->HorizontalSlider_FramesRange->setValue(d->ui->HorizontalSlider_FramesRange->value() + 30);
  d->timer->start();
}

void RootFileCameraProfilesDialog::onPlayProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  d->timer->start();
}

QT_END_NAMESPACE
