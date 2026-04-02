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

#include <QPointer>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "CameraProfilesDialog.h"
#include "ui_CameraProfilesDialog.h"

#include <TCanvas.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TH1.h>
#include <TH2.h>
#include <TPad.h>
#include <TLine.h>
#include <TFile.h>
#include <THttpServer.h>

#include <TROOT.h>

#include "ChannelInfoTableModel.h"

#include "typedefs.h"

#include <sstream>

QT_BEGIN_NAMESPACE

namespace Ui {
class CameraProfilesDialog;
}

class CameraProfilesDialogPrivate
{
  Q_DECLARE_PUBLIC(CameraProfilesDialog);
protected:
  CameraProfilesDialog* const q_ptr;
  QScopedPointer< Ui::CameraProfilesDialog > ui;
public:
  CameraProfilesDialogPrivate(CameraProfilesDialog &object);
  virtual ~CameraProfilesDialogPrivate();
  AdcTimeType getAdcTimeType() const;
  size_t updateChannelGraph(int chipIndex, int channelIndex, AdcTimeType dataType);
  void updateHistSideA(int chipIndex, int channelIndex, AdcTimeType dataType);
  void updateHistSideB(int chipIndex, int channelIndex, AdcTimeType dataType);
  bool saveSpillProfiles();

  AbstractCamera* camera{ nullptr };
  QScopedPointer< ChannelInfoTableModel > chipChannelInfoModel;

  std::unique_ptr< TGraph > graphChannel;
  std::unique_ptr< TPad > padChannel;

  std::unique_ptr< TGraph > graphHorizontalProfile;
  std::unique_ptr< TPad > padHorizontalProfile;

  std::unique_ptr< TGraph > graphVerticalProfile;
  std::unique_ptr< TPad > padVerticalProfile;

  std::unique_ptr< TPad > padPseudo2D;
  std::unique_ptr< TPad > padPseudoIntegral2D;

  std::unique_ptr< TLine > linePedBegin;
  std::unique_ptr< TLine > linePedEnd;
  std::unique_ptr< TLine > lineSigBegin;
  std::unique_ptr< TLine > lineSigEnd;

  std::unique_ptr< TPad > padHist;
  std::unique_ptr< TH1 > histA;
  std::unique_ptr< TH1 > histB;

  std::unique_ptr< TH2 > histPseudo2D;
  std::unique_ptr< TH2 > histPseudoIntegral2D;
};

CameraProfilesDialogPrivate::CameraProfilesDialogPrivate(CameraProfilesDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::CameraProfilesDialog),
  chipChannelInfoModel(new ChannelInfoTableModel(&object))
{
}

CameraProfilesDialogPrivate::~CameraProfilesDialogPrivate()
{
}

AdcTimeType CameraProfilesDialogPrivate::getAdcTimeType() const
{
  AdcTimeType adcDataType = AdcTimeType::INTEGRATOR_AB;
  if (this->ui->CheckBox_SplitData->isChecked() && this->ui->RadioButton_IntegA->isChecked())
  {
    adcDataType = AdcTimeType::INTEGRATOR_A;
  }
  else if (this->ui->CheckBox_SplitData->isChecked() && this->ui->RadioButton_IntegB->isChecked())
  {
    adcDataType = AdcTimeType::INTEGRATOR_B;
  }
  return adcDataType;
}

size_t CameraProfilesDialogPrivate::updateChannelGraph(int chipIndex, int channelIndex, AdcTimeType dataType)
{
  Q_Q(CameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  if (this->camera)
  {
    intTimeMs = this->camera->getIntegrationTimeMs() / 2;
    nofSamples = this->camera->getAdcSamples();
  }
  else
  {
//    qWarning() << Q_FUNC_INFO << "Camera is invalid";
    emit q->logMessage("Camera is invalid", "", Qt::red);
    return 0;
  }
  bool doubleTime = true;
  if (dataType == AdcTimeType::INTEGRATOR_AB)
  {
    doubleTime = false;
  }

  std::vector< int > chipChannelAdcData;
  bool res = this->camera->getAdcData(chipIndex, channelIndex, chipChannelAdcData, dataType);
  if (!res)
  {
//    qWarning() << Q_FUNC_INFO << "ADC data is invalid";
    emit q->logMessage("ADC data is invalid", this->camera->getCameraData().ID, Qt::red);
    return 0;
  }

  TCanvas* canvas = this->ui->RootCanvas_ChannelGraph->getCanvas();
  canvas->cd();
  this->padChannel->cd();
  if (this->graphChannel)
  {
    this->padChannel->GetListOfPrimitives()->Remove(this->graphChannel.get());
  }

  this->graphChannel = std::make_unique< TGraph >(chipChannelAdcData.size());
  this->graphChannel->SetTitle("Channel data;Time (ms);Amplitude");
  Int_t i = 0;
  for (int adcValue : chipChannelAdcData)
  {
    Double_t x = i;
    if (doubleTime)
    {
      x *= 2.;
    }
    this->graphChannel->SetPoint(i, x * intTimeMs, Double_t(adcValue));
    ++i;
  }
  this->graphChannel->Draw("AL*");
  this->padChannel->Modified();
  this->padChannel->Update();
  return chipChannelAdcData.size();
}

void CameraProfilesDialogPrivate::updateHistSideA(int chipIndex, int channelIndex, AdcTimeType dataType)
{
  Q_Q(CameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  if (this->camera)
  {
    intTimeMs = this->camera->getIntegrationTimeMs() / 2;
    nofSamples = this->camera->getAdcSamples();
  }
  else
  {
//    qWarning() << Q_FUNC_INFO << "Camera is invalid";
    emit q->logMessage("Camera is invalid", "", Qt::red);
    return;
  }

  TCanvas* canvas = this->ui->RootCanvas_ChannelHist->getCanvas();
  canvas->cd();
  this->padHist->cd();
  std::vector< int > chipChannelAdcData;
  bool res = this->camera->getAdcData(chipIndex, channelIndex, chipChannelAdcData, AdcTimeType::INTEGRATOR_A);
  this->padHist->GetListOfPrimitives()->Remove(this->histA.get());
  if (res && this->histA)
  {
    this->histA->Reset();
    if (dataType != AdcTimeType::INTEGRATOR_B)
    {
      for (int adcCount : chipChannelAdcData)
      {
        this->histA->Fill(Double_t(adcCount));
      }
    }
    if (dataType == AdcTimeType::INTEGRATOR_A || dataType == AdcTimeType::INTEGRATOR_AB)
    {
      this->histA->Draw();
    }
  }
}

void CameraProfilesDialogPrivate::updateHistSideB(int chipIndex, int channelIndex, AdcTimeType dataType)
{
  Q_Q(CameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  if (this->camera)
  {
    intTimeMs = this->camera->getIntegrationTimeMs() / 2;
    nofSamples = this->camera->getAdcSamples();
  }
  else
  {
//    qWarning() << Q_FUNC_INFO << "Camera is invalid";
    emit q->logMessage("Camera is invalid", "", Qt::red);
    return;
  }

  TCanvas* canvas = this->ui->RootCanvas_ChannelHist->getCanvas();
  canvas->cd();
  this->padHist->cd();
  std::vector< int > chipChannelAdcData;
  bool res = this->camera->getAdcData(chipIndex, channelIndex, chipChannelAdcData, AdcTimeType::INTEGRATOR_B);
  this->padHist->GetListOfPrimitives()->Remove(this->histB.get());
  if (res && this->histB)
  {
    this->histB->Reset();
    if (dataType != AdcTimeType::INTEGRATOR_A)
    {
      for (int adcCount : chipChannelAdcData)
      {
        this->histB->Fill(Double_t(adcCount));
      }
    }
    if (dataType == AdcTimeType::INTEGRATOR_B)
    {
      this->histB->Draw();
    }
    else if (dataType == AdcTimeType::INTEGRATOR_AB)
    {
      this->histB->Draw("SAME");
    }
  }
}

bool CameraProfilesDialogPrivate::saveSpillProfiles()
{
  Q_Q(CameraProfilesDialog);
  if (!this->camera)
  {
    return false;
  }
  AbstractCamera::CameraDeviceData camData = this->camera->getCameraData();

  std::stringstream sstream;
  sstream << "/tmp/spillProfile_" << camData.ID.toStdString() << ".root";
  std::string name = sstream.str();
  std::unique_ptr< TFile > spillFile = std::unique_ptr< TFile >(new TFile(name.c_str(), "RECREATE"));
  if (!spillFile)
  {
    return false;
  }
  spillFile->WriteTObject(this->graphVerticalProfile.get(), "vp");
  spillFile->WriteTObject(this->graphHorizontalProfile.get(), "hp");
  spillFile->WriteTObject(this->histPseudo2D.get(), "h2p");
  spillFile->Close();
  return true;
}

CameraProfilesDialog::CameraProfilesDialog(const AbstractCamera::CameraDeviceData& info,
  QWidget* parent)
  :
  QDialog(parent),
  d_ptr(new CameraProfilesDialogPrivate(*this))
{
  Q_D(CameraProfilesDialog);
  d->ui->setupUi(this);
  this->setWindowTitle(tr("Camera Profiles & Data Dialog : [%1]").arg(info.ID));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);

  d->ui->TableView_ChannelsInfo->setModel(d->chipChannelInfoModel.data());

  TCanvas* canvas = d->ui->RootCanvas_ChannelGraph->getCanvas();
  canvas->cd();
  d->padChannel = std::unique_ptr< TPad >(new TPad("pCh", "Grid", 0., 0., 1., 1.));
  d->padChannel->SetGrid();
  d->padChannel->Draw();
  d->padChannel->cd();
  d->graphChannel = std::unique_ptr< TGraph >(new TGraph());
  d->graphChannel->Draw("AL*");

  constexpr Int_t histResolution = AbstractCamera::RESOLUTION_16BIT / 4;
  TCanvas* canvasHist = d->ui->RootCanvas_ChannelHist->getCanvas();
  canvasHist->cd();
  d->padHist = std::unique_ptr< TPad >(new TPad("pSide", "Grid", 0., 0., 1., 1.));
  d->padHist->SetGrid();
  d->padHist->Draw();
  d->padHist->cd();

  QString nameHistA = QString("hA_") + info.ID;
  QString nameHistB = QString("hB_") + info.ID;
  const char* nameA = nameHistA.toLatin1().data();
  const char* nameB = nameHistB.toLatin1().data();

  TH1* hist = new TH1I(nameA, "ADC counts",
    histResolution, -1000., Double_t(AbstractCamera::RESOLUTION_16BIT) - 1001.);
  hist->SetDirectory(nullptr);
  d->histA.reset(hist);
  hist->SetFillColor(kRed);
  hist->SetFillStyle(3001);
  hist->Draw();
  hist->GetXaxis()->SetTitle("ADC");
  hist->GetYaxis()->SetTitle("Events");

  hist = new TH1I(nameB, "ADC counts",
    histResolution, -1000., Double_t(AbstractCamera::RESOLUTION_16BIT) - 1001.);
  hist->SetDirectory(nullptr);
  d->histB.reset(hist);
  hist->SetFillColor(kBlue);
  hist->SetFillStyle(3001);
  hist->Draw("SAME");

  QObject::connect(d->ui->PushButton_UpdateChannelGraph, SIGNAL(clicked()),
    this, SLOT(onUpdateGraphClicked()));
  QObject::connect(d->ui->PushButton_UpdateProfiles, SIGNAL(clicked()),
    this, SLOT(onUpdateProfilesClicked()));
  QObject::connect(d->ui->PushButton_ResetIntegral2D, SIGNAL(clicked()),
    this, SLOT(onResetIntegralPseudo2dClicked()));
  QObject::connect(d->ui->RangeWidget_Pedestal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onPedestalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_Pedestal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onPedestalEndChanged(double)));
  QObject::connect(d->ui->RangeWidget_Signal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onSignalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_Signal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onSignalEndChanged(double)));

  {
    QSignalBlocker block(d->ui->RangeWidget_Signal);
    d->ui->RangeWidget_Signal->setMinimumValue(200.);
    d->ui->RangeWidget_Signal->setMaximumValue(1100.);
  }
}

CameraProfilesDialog::~CameraProfilesDialog()
{
  Q_D(CameraProfilesDialog);

  if (d->padChannel)
  {
    d->padChannel->cd();
    if (d->linePedBegin)
    {
      d->padChannel->GetListOfPrimitives()->Remove(d->linePedBegin.get());
    }
    if (d->linePedEnd)
    {
      d->padChannel->GetListOfPrimitives()->Remove(d->linePedEnd.get());
    }
    if (d->lineSigBegin)
    {
      d->padChannel->GetListOfPrimitives()->Remove(d->lineSigBegin.get());
    }
    if (d->lineSigEnd)
    {
      d->padChannel->GetListOfPrimitives()->Remove(d->lineSigEnd.get());
    }
    if (d->graphChannel)
    {
      d->padChannel->GetListOfPrimitives()->Remove(d->graphChannel.get());
    }
  }
}

void CameraProfilesDialog::setCameraDevice(AbstractCamera* cam)
{
  Q_D(CameraProfilesDialog);
  d->camera = cam;
  if (!d->camera)
  {
    return;
  }
  QObject::connect(d->camera, SIGNAL(acquisitionStarted()), this,
    SLOT(onAcquisitionStarted()));
  QObject::connect(d->camera, SIGNAL(acquisitionFinished()), this,
    SLOT(onAcquisitionFinished()));

  TCanvas* canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile = std::unique_ptr< TPad >(new TPad("pVerProf", "Grid", 0., 0., 1., 1.));
  d->padVerticalProfile->SetGrid();
  d->padVerticalProfile->Draw();
  d->padVerticalProfile->cd();

  TGraph* prof = d->camera->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
  d->graphVerticalProfile.reset(prof);
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile (horizontal strips);Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile = std::unique_ptr< TPad >(new TPad("pHorProf", "Grid", 0., 0., 1., 1.));
  d->padHorizontalProfile->SetGrid();
  d->padHorizontalProfile->Draw();
  d->padHorizontalProfile->cd();

  prof = d->camera->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);
  d->graphHorizontalProfile.reset(prof);
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile (vertical strips);Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");

  canvas = d->ui->RootCanvas_Current2D->getCanvas();
  canvas->cd();
  d->padPseudo2D = std::unique_ptr< TPad >(new TPad("pPs2D", "Grid", 0., 0., 1., 1.));
  d->padPseudo2D->SetGrid();
  d->padPseudo2D->Draw();
  d->padPseudo2D->cd();
  TH2* hist = d->camera->createProfile2D(false);
  hist->SetDirectory(nullptr);
  d->histPseudo2D.reset(hist);
  d->histPseudo2D->GetXaxis()->SetTitle("Horizontal profile, mm");
  d->histPseudo2D->GetYaxis()->SetTitle("Vertical profile, mm");
  d->histPseudo2D->Draw("COLZ");

  canvas = d->ui->RootCanvas_Integrated2D->getCanvas();
  canvas->cd();
  d->padPseudoIntegral2D = std::unique_ptr< TPad >(new TPad("pPsInt2D", "Grid", 0., 0., 1., 1.));
  d->padPseudoIntegral2D->SetGrid();
  d->padPseudoIntegral2D->Draw();
  d->padPseudoIntegral2D->cd();
  hist = d->camera->createProfile2D(true);
  hist->SetDirectory(nullptr);
  hist->SetTitle("Pseudo integral 2D Distribution");
  d->histPseudoIntegral2D.reset(hist);
  d->histPseudoIntegral2D->GetXaxis()->SetTitle("Horizontal profile, mm");
  d->histPseudoIntegral2D->GetYaxis()->SetTitle("Vertical profile, mm");
  d->histPseudoIntegral2D->Draw("COLZ");
}

void CameraProfilesDialog::onPedestalBeginChanged(double pos)
{
  Q_D(CameraProfilesDialog);

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
  d->camera->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
}

void CameraProfilesDialog::onPedestalEndChanged(double pos)
{
  Q_D(CameraProfilesDialog);

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
  d->camera->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
}

void CameraProfilesDialog::onSignalBeginChanged(double pos)
{
  Q_D(CameraProfilesDialog);
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
  d->camera->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
}

void CameraProfilesDialog::onSignalEndChanged(double pos)
{
  Q_D(CameraProfilesDialog);
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
  d->camera->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
}

void CameraProfilesDialog::onAcquisitionStarted()
{
  Q_D(CameraProfilesDialog);
  QApplication::setOverrideCursor(Qt::WaitCursor);
}

void CameraProfilesDialog::onAcquisitionFinished()
{
  Q_D(CameraProfilesDialog);
  QString chipsStr;
  if (d->camera)
  {
    chipsStr = d->camera->getChipsAddresses();
  }
  d->ui->LineEdit_EnabledChips->setText(chipsStr);
  if (!d->camera->getOnceTimeExternalStartFlag())
  {
    this->onUpdateProfilesClicked();
    if (d->saveSpillProfiles())
    {
      QString cameraID = d->camera->getCameraData().ID;
//      qWarning() << Q_FUNC_INFO << "Spill profiles are saved for " << cameraID;
      emit logMessage("Spill profiles are saved", cameraID, Qt::green);
    }
  }
  QApplication::restoreOverrideCursor();
}

void CameraProfilesDialog::onUpdateGraphClicked()
{
  Q_D(CameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  if (d->camera)
  {
    intTimeMs = d->camera->getIntegrationTimeMs() / 2;
    nofSamples = d->camera->getAdcSamples();
  }
  else
  {
//    qWarning() << Q_FUNC_INFO << "Camera is invalid";
    emit logMessage("Camera is invalid", "", Qt::red);
    return;
  }
  int chipIndex = static_cast< int >(d->ui->SliderWidget_Chip->value()) - 1;
  int channelIndex = static_cast< int >(d->ui->SliderWidget_Channel->value()) - 1;

  AdcTimeType adcDataType = d->getAdcTimeType();
  bool doubleTime = true;
  if (adcDataType == AdcTimeType::INTEGRATOR_AB)
  {
    doubleTime = false;
  }
  size_t adcDataSize = d->updateChannelGraph(chipIndex, channelIndex, adcDataType);
  if (!adcDataSize)
  {
//    qWarning() << Q_FUNC_INFO << "ADC data is empty";
    emit logMessage("ADC data is empty", d->camera->getCameraData().ID, Qt::red);
    return;
  }

  d->updateHistSideA(chipIndex, channelIndex, adcDataType);
  d->updateHistSideB(chipIndex, channelIndex, adcDataType);
  d->padHist->Update();
  d->padHist->Modified();

  double stepMul = 1.;
  if (adcDataType != AdcTimeType::INTEGRATOR_AB)
  {
    stepMul = 2.;
  }
  d->ui->RangeWidget_Pedestal->setRange(0., 2. * intTimeMs * adcDataSize);
  d->ui->RangeWidget_Pedestal->setSingleStep(stepMul * intTimeMs);
  d->ui->RangeWidget_Signal->setRange(0., 2. * intTimeMs * adcDataSize);
  d->ui->RangeWidget_Signal->setSingleStep(stepMul * intTimeMs);
  this->onPedestalBeginChanged(d->ui->RangeWidget_Pedestal->minimumValue());
  this->onPedestalEndChanged(d->ui->RangeWidget_Pedestal->maximumValue());
  this->onSignalBeginChanged(d->ui->RangeWidget_Signal->minimumValue());
  this->onSignalEndChanged(d->ui->RangeWidget_Signal->maximumValue());
}

void CameraProfilesDialog::onUpdateProfilesClicked()
{
  Q_D(CameraProfilesDialog);
  if (!d->camera)
  {
//    qWarning() << Q_FUNC_INFO << "Camera is invalid";
    emit logMessage("Camera is invalid", "", Qt::red);
    return;
  }
  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());

  std::map< ChipChannelPair, ChannelInfoPair > infoMap;
  d->camera->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  d->camera->processDataCounts();
  d->camera->getChipChannelInfo(infoMap);
  if (!infoMap.size())
  {
//    qWarning() << Q_FUNC_INFO << "Camera data is empty";
    emit logMessage("Camera data is empty", "", Qt::red);
    return;
  }
  d->camera->updateProfiles(d->graphVerticalProfile.get(), d->graphHorizontalProfile.get(), false);
  d->camera->updateProfiles2D(d->histPseudo2D.get(), d->histPseudoIntegral2D.get());

  TCanvas* canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile->cd();
  if (d->padVerticalProfile)
  {
    d->padVerticalProfile->GetListOfPrimitives()->Remove(d->graphVerticalProfile.get());
  }
  d->graphVerticalProfile.reset(d->camera->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Horizontal profile (vertical strips);Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");
  d->padVerticalProfile->Modified();
  d->padVerticalProfile->Update();

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile->cd();
  if (d->graphHorizontalProfile)
  {
    d->padHorizontalProfile->GetListOfPrimitives()->Remove(d->graphHorizontalProfile.get());
  }
  d->graphHorizontalProfile.reset(d->camera->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile (vertical strips);Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");
  d->padHorizontalProfile->Modified();
  d->padHorizontalProfile->Update();

  canvas = d->ui->RootCanvas_Current2D->getCanvas();
  canvas->cd();
  d->padPseudo2D->cd();
  if (d->ui->CheckBox_ManualChargeRange->isChecked())
  {
    d->histPseudo2D->GetZaxis()->SetRangeUser(d->ui->RangeWidget_ChargeRange->minimumValue(),
      d->ui->RangeWidget_ChargeRange->maximumValue());
  }
  d->padPseudo2D->Modified();
  d->padPseudo2D->Update();

  canvas = d->ui->RootCanvas_Integrated2D->getCanvas();
  canvas->cd();
  d->padPseudoIntegral2D->cd();
  d->padPseudoIntegral2D->Modified();
  d->padPseudoIntegral2D->Update();

  emit profilesUpdated();
  emit profilesUpdated(d->camera->getCameraData().ID);
  d->chipChannelInfoModel->setChipChannelInfo(infoMap);

  // dump info into temporary text file
  auto cameraData = d->camera->getCameraData();
  QString dataFileName = QDir::tempPath() + QString(QDir::separator()) + cameraData.ID + "_infodump.txt";
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
    const ChipChannelPair& chipChannelPair = iter->first;
    const ChannelInfoPair& infoPair = iter->second;
    const ChannelInfo& rawInfo = infoPair.first;
    const ChannelInfo& adcCalibInfo = infoPair.second;
    if (res)
    {
      double meanRawSignal = (rawInfo.SigMeanA - rawInfo.PedMeanA + rawInfo.SigMeanB - rawInfo.PedMeanB) / 2.;
      double meanCalibSignal = (adcCalibInfo.SigMeanA - adcCalibInfo.PedMeanA + adcCalibInfo.SigMeanB - adcCalibInfo.PedMeanB) / 2.;
      dataFileStream << qSetFieldWidth(10) \
        << chipChannelPair.first << ' ' << chipChannelPair.second << ' ' \
        << meanRawSignal << ' ' << meanCalibSignal << '\n';
    }
  }
  dataFileStream.flush();
  res = dataFile->flush();
  if (res)
  {
    dataFile->close();
  }
}

void CameraProfilesDialog::onResetIntegralPseudo2dClicked()
{
  Q_D(CameraProfilesDialog);
  d->histPseudoIntegral2D->Reset();
  d->padPseudoIntegral2D->Modified();
  d->padPseudoIntegral2D->Update();
}

void CameraProfilesDialog::addBeamProfilesToServer(THttpServer* server)
{
  Q_D(CameraProfilesDialog);
  if (!server)
  {
    return;
  }
  QString cameraID;
  if (d->camera)
  {
    cameraID = d->camera->getCameraData().ID;
  }
  if (cameraID.isEmpty())
  {
    return;
  }
  QByteArray bytesStr = QByteArray(1, '/') + cameraID.toLatin1();
  const char* id_str = bytesStr.data();
  server->Register(id_str, d->padHorizontalProfile.get());
  server->Register(id_str, d->padVerticalProfile.get());
  server->Register(id_str, d->padPseudo2D.get());
  server->Register(id_str, d->padPseudoIntegral2D.get());

  server->SetItemField(id_str, "_layout", "grid2x2");
/*  QByteArray Str = cameraID.toLatin1();
  std::stringstream ss;
  ss << '[' << d->padHorizontalProfile.get()->GetName() \
     << ',' << d->padVerticalProfile.get()->GetName() \
     << ',' << d->padPseudo2D.get()->GetName() \
     << ',' << d->padPseudoIntegral2D.get()->GetName() << ']';
  std::string drawItems = ss.str();

  qDebug() << Q_FUNC_INFO << ": Camera items string: " << QString::fromStdString(drawItems);
  server->SetItemField(id_str, "_drawitem", drawItems.c_str());  // items
  server->SetItemField(id_str, "_monitoring", "1000");
*/
}

/*
void CameraProfilesDialog::getProfiles(TGraph *horiz, TGraph *vert, TH2 *hist2d)
{
  Q_D(CameraProfilesDialog);
  horiz = d->graphHorizontalProfile.get();
  vert = d->graphVerticalProfile.get();
  hist2d = d->histPseudo2D.get();
}
*/
QT_END_NAMESPACE
