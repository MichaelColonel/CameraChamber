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

#include "BeamPathProfileDialog.h"
#include "ui_BeamPathProfileDialog.h"

#include <QPair>

#include <THttpServer.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TLine.h>
#include <TH2.h>
#include <TLatex.h>
#include <TH1.h>
#include <TPad.h>
#include <TMultiGraph.h>
#include <TStyle.h>

#include <QElapsedTimer>

#include "AbstractCamera.h"
#include "BeamPathTableModel.h"

QT_BEGIN_NAMESPACE

class BeamPathProfileDialogPrivate
{
  Q_DECLARE_PUBLIC(BeamPathProfileDialog);
protected:
  BeamPathProfileDialog* const q_ptr;
  QScopedPointer< Ui::BeamPathProfileDialog > ui;
public:
  BeamPathProfileDialogPrivate(BeamPathProfileDialog &object);
  bool createProfiles();

  virtual ~BeamPathProfileDialogPrivate();
  void processBeamPathCalculation();

  BeamProfileArray processBeamProfile(AbstractCamera* cam, int pedMin, int pedMax, int signalMin, int signalMax);
  void processBeamPathProfile(TGraph* profile); // normalize (for 100% in maximum ) and cut (less than 3% signal => zero)
  void calculateBeamPath();
  void calculateCalibration();
  void calculateScanning();

  double getNorm(TGraph* profile);
  double getMean(TGraph* profile, double norm);
  double getSigma(TGraph* profile, double mean, double norm);
  double homogenousValue();
  int getBinScanningX(double posX);
  int getBinScanningY(double posY);
  double getCenterPositionScanningX();
  double getCenterPositionScanningY();

  BeamProfileArray getProfilesParameters(TGraph* profileX, TGraph* profileY);
  TGraph* getScanningProjection(TH1* histProjection);

  bool areCamerasProfilesReady() const { return (camerasProfilesReady.first && camerasProfilesReady.second); }
  AbstractCamera* camera1() const { return beamPathCameras.first; }
  AbstractCamera* camera2() const { return beamPathCameras.second; }

  void processIsocenterCalibration();
  void processIsocenterScanning();

  // first camera (begin), near collimator-1 or wall
  // second, last camera (end), near isocenter
  QPair< AbstractCamera*, AbstractCamera* > beamPathCameras{ nullptr, nullptr };
  QPair< bool, bool > camerasProfilesReady{ false, false };

  std::weak_ptr< THttpServer > httpServer;
  std::unique_ptr< TLatex > latexBeamPathTable;

  std::unique_ptr< TPad > padBeamPathTable;
  std::unique_ptr< TPad > padCalibration2D;
  std::unique_ptr< TPad > padScanning2D;

  std::unique_ptr< TPad > padScanningProfiles;
  std::unique_ptr< TGraph > graphScanningVerticalProfile;
  std::unique_ptr< TGraph > graphScanningHorizontalProfile;

  std::unique_ptr< TH2 > histCalibration2D;
  std::unique_ptr< TH2 > histScanning2D;

  std::unique_ptr< TLine > lineScanningVerticalBegin;
  std::unique_ptr< TLine > lineScanningVerticalEnd;
  std::unique_ptr< TLine > lineScanningHorizontalBegin;
  std::unique_ptr< TLine > lineScanningHorizontalEnd;

  QScopedPointer< BeamPathTableModel > beamPathModel;
  static constexpr std::array< Int_t, BEAM_PATH_CUSTOM_COLORS_PALETTE > calibrationScanningPalette{
    kBlack, // (< 0)
    kBlue + 4, // (5-0)
    kBlue + 3, // (10-5)
    kBlue, // (15-10)
    51 + 6, // (20-15)
    51 + 9, // (25-20)
    51 + 12, // (30-25)
    51 + 15, // (35-30)
    51 + 18, // (40-35)
    51 + 21, // (45-40)
    51 + 24, // (50-45)
    51 + 27, // (55-50)
    51 + 30, // (60-55)
    51 + 31, // (75-60)
    51 + 33, // (80-75)
    51 + 34, // (85-80)
    51 + 36, // (90-85)
    51 + 39, // (95-90)
    kYellow, // (100-95)
    kRed // (> 100)
  };
};

BeamPathProfileDialogPrivate::BeamPathProfileDialogPrivate(BeamPathProfileDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::BeamPathProfileDialog),
  beamPathModel(new BeamPathTableModel(&object))
{
}

BeamPathProfileDialogPrivate::~BeamPathProfileDialogPrivate()
{
}

double BeamPathProfileDialogPrivate::getNorm(TGraph* profile)
{
  int size = profile->GetN();
  double norm = 0.;
  for(int i = 0; i < size - 1; ++i)
  {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    profile->GetPoint(i, x1, y1);
    profile->GetPoint(i + 1, x2, y2);
    norm += .5 * (y2 + y1) / (x2 - x1);
  }
  return norm;
}

double BeamPathProfileDialogPrivate::getMean(TGraph* profile, double norm)
{
  int size = profile->GetN();
  double m = 0;
  for(int i = 0; i < size - 1; ++i)
  {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    profile->GetPoint(i, x1, y1);
    profile->GetPoint(i + 1, x2, y2);
    m += .5 * (x1 * y1 + x2 * y2) / (x2 - x1);
  }
  return m / norm;
}

double BeamPathProfileDialogPrivate::getSigma(TGraph* profile, double mean, double norm)
{
  int size = profile->GetN();
  double var = 0;
  for(int i = 0; i < size - 1; ++i)
  {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    profile->GetPoint(i, x1, y1);
    profile->GetPoint(i + 1, x2, y2);
    double p1 = (x1 - mean) * (x1 - mean);
    double p2 = (x2 - mean) * (x2 - mean);
    var += .5 * (p1 * y1 + p2 * y2) / (x2 - x1);
  }
  if (var < 0. || (var > 0. && norm < 0.))
  {
    return -1.;
  }
  return std::sqrt(var / norm);
}

double BeamPathProfileDialogPrivate::homogenousValue()
{
  double d = -1.;
  double hmin = this->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  double hmax = this->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
  double vmin = this->ui->RangeWidget_ScanHomoVertical->minimumValue();
  double vmax = this->ui->RangeWidget_ScanHomoVertical->maximumValue();

  std::vector< Double_t > values;
  for (Int_t h = 1; h <= this->histScanning2D->GetXaxis()->GetNbins(); ++h)
  {
    for (Int_t v = 1; v <= this->histScanning2D->GetYaxis()->GetNbins(); ++v)
    {
      Double_t posX = this->histScanning2D->GetXaxis()->GetBinCenter(h);
      Double_t posY = this->histScanning2D->GetYaxis()->GetBinCenter(v);
      Double_t content = this->histScanning2D->GetBinContent(h, v);
      bool withinX = false, withinY = false;
      if (posX >= hmin && posX <= hmax)
      {
        withinX = true;
      }
      if (posY >= vmin && posY <= vmax)
      {
        withinY = true;
      }
      if (withinX && withinY)
      {
        values.push_back(content);
      }
    }
  }
  if (values.size() > 1)
  {
    Double_t min = *std::min_element(values.begin(), values.end());
    Double_t max = *std::max_element(values.begin(), values.end());
    d = min / max;
  }
  if (values.size() == 1)
  {
    d = 1.;
  }
  if (!values.size())
  {
    d = -1.;
  }
  return d;
}

BeamProfileArray BeamPathProfileDialogPrivate::getProfilesParameters(TGraph* profileX, TGraph* profileY)
{
  BeamProfileArray bp{ 0.0, 0.0, -1. };
  double normX = this->getNorm(profileX);
  double sigmaX = -1.;
  if (std::abs(normX) > DBL_EPSILON)
  {
    bp[0] = this->getMean(profileX, normX);
    sigmaX = this->getSigma(profileX, bp[0], normX);
  }

  double normY = this->getNorm(profileY);
  double sigmaY = -1.;
  if (std::abs(normY) > DBL_EPSILON)
  {
    bp[1] = this->getMean(profileY, normY);
    sigmaY = this->getSigma(profileY, bp[1], normY);
  }
  bp[2] = -1.;
  if (sigmaY >= 0. && sigmaX >= 0.)
  {
    bp[2] = std::sqrt(sigmaX * sigmaX + sigmaY * sigmaY);
  }
  return bp;
}

BeamProfileArray BeamPathProfileDialogPrivate::processBeamProfile(AbstractCamera* cam,
  int pedMin, int pedMax, int signalMin, int signalMax)
{
  Q_Q(BeamPathProfileDialog);
  BeamProfileArray prof{0., 0., -1.};
  if (!cam)
  {
    return prof;
  }
  cam->setPedestalSignalGate(pedMin, pedMax, signalMin, signalMax);
  cam->processDataCounts();

  TGraph* profH = cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
  TGraph* profV = cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);
  if (profH)
  {

  }
  if (profV)
  {

  }
  return prof;
}

int BeamPathProfileDialogPrivate::getBinScanningX(double posX)
{
  int binPos = -1;
  if (this->histScanning2D)
  {
    return binPos;
  }
  for (int i = 1; i <= this->histScanning2D->GetXaxis()->GetNbins(); ++i)
  {
    Double_t binLow = this->histScanning2D->GetXaxis()->GetBinLowEdge(i);
    Double_t binUp = this->histScanning2D->GetXaxis()->GetBinUpEdge(i);
    if ((posX >= binLow) && (posX < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

int BeamPathProfileDialogPrivate::getBinScanningY(double posY)
{
  int binPos = -1;
  if (this->histScanning2D)
  {
    return binPos;
  }
  for (int i = 1; i <= this->histScanning2D->GetYaxis()->GetNbins(); ++i)
  {
    Double_t binLow = this->histScanning2D->GetYaxis()->GetBinLowEdge(i);
    Double_t binUp = this->histScanning2D->GetYaxis()->GetBinUpEdge(i);
    if ((posY >= binLow) && (posY < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

TGraph* BeamPathProfileDialogPrivate::getScanningProjection(TH1* histProj)
{
  if (histProj)
  {
    return nullptr;
  }
  Int_t n = histProj->GetXaxis()->GetNbins();
  TGraph* gr = new TGraph(n);
  for (Int_t i = 0; i < n; ++i)
  {
    Double_t v, pos;
    pos = histProj->GetXaxis()->GetBinCenter(i + 1);
    v = histProj->GetBinContent(i + 1);
    gr->SetPoint(i, pos, v);
  }
  return gr;
}

bool BeamPathProfileDialogPrivate::createProfiles()
{
  if (AbstractCamera* isocenterCamera = this->camera2())
  {
    gStyle->SetPalette();
    this->padCalibration2D->cd();
    if (this->histCalibration2D)
    {
      this->padCalibration2D->GetListOfPrimitives()->Remove(this->histCalibration2D.get());
    }
    TH2* histCalib = isocenterCamera->createProfile2D(false);
    if (histCalib)
    {
      histCalib->SetName("hCalib2D");
    }
//    histCalib->SetDirectory(nullptr);
    this->histCalibration2D.reset(histCalib);
    this->histCalibration2D->GetXaxis()->SetTitle("Horizontal profile, mm");
    this->histCalibration2D->GetYaxis()->SetTitle("Vertical profile, mm");
    this->histCalibration2D->Draw("COLZ");

    this->padCalibration2D->Modified();
    this->padCalibration2D->Update();

    this->padScanning2D->cd();
    if (this->histScanning2D)
    {
      this->padScanning2D->GetListOfPrimitives()->Remove(this->histScanning2D.get());
    }

    TH2* histScan = isocenterCamera->createProfile2D(false);
    if (histScan)
    {
      histScan->SetName("hScan2D");
    }
//    histScan->SetDirectory(nullptr);
    this->histScanning2D.reset(histScan);
    this->histScanning2D->GetXaxis()->SetTitle("Horizontal profile, mm");
    this->histScanning2D->GetYaxis()->SetTitle("Vertical profile, mm");
    this->histScanning2D->Draw("COLZ");

    this->padScanning2D->Modified();
    this->padScanning2D->Update();
  }
  return bool(this->histScanning2D && this->histCalibration2D);
}

void BeamPathProfileDialogPrivate::processBeamPathProfile(TGraph* profile)
{
  Q_Q(BeamPathProfileDialog);
//  Double_t max = profile->GetMaximum();

  profile->Scale(100. / profile->GetMaximum());
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t x = -1000., y = -1000.;
    profile->GetPoint(i, x, y);
//    Double_t v = 100. * y / max;
//    profile->SetPoint(i, x, v);
    constexpr Double_t thres = 3.;
//    Double_t v = 100. * y / max;
    profile->SetPoint(i, x, (y < thres) ? 0. : y);
  }
}

void BeamPathProfileDialogPrivate::calculateBeamPath()
{
  Q_Q(BeamPathProfileDialog);
  if (!this->ui->PushButton_BeamPathCalculation->isCheckable() ||
    !this->ui->PushButton_BeamPathCalculation->isChecked())
  {
    qDebug() << Q_FUNC_INFO << "Skip beam path calculation";
    return;
  }

  if (!this->camera1() || !this->camera2() || !this->areCamerasProfilesReady())
  {
    return;
  }

  std::array< int, 4 > gatecam1;
  this->camera1()->getPedestalSignalGate(gatecam1);
  std::array< int, 4 > gatecam2;
  this->camera2()->getPedestalSignalGate(gatecam2);

  const int offset = 200;
  const int integInterval = 100;
  constexpr int steps = 9;
  constexpr std::array< double, steps > pos{.8, .75, .7, .65, .6, .55, .5, .45, .4 };
  BeamPathMap beamPathMap;

  this->padBeamPathTable->cd();
  this->padBeamPathTable->Clear();
  this->latexBeamPathTable = std::unique_ptr< TLatex >(new TLatex);
  this->latexBeamPathTable->SetTextSize(0.05);
  this->latexBeamPathTable->SetTextAlign(13);  //align at top
  this->latexBeamPathTable->DrawLatex(.0, 1.,"Время, (мс)");
  this->latexBeamPathTable->DrawLatex(.3, 1.,"ПР1 / Стена");
  this->latexBeamPathTable->DrawLatex(.6, 1.,"ПР2 / Изоцентр");

  this->latexBeamPathTable->DrawLatex(.0,.95,"           ");
  this->latexBeamPathTable->DrawLatex(.3,.95,"X,Y,D (мм)");
  this->latexBeamPathTable->DrawLatex(.6,.95,"X,Y,D (мм)");

  this->latexBeamPathTable->SetTextSize(0.04);
  for (int i = 0; i < steps; ++i)
  {
    int sigBegin = offset + (i * integInterval);
    int sigEnd = sigBegin + integInterval;
    this->camera1()->setPedestalSignalGate(gatecam1[0], gatecam1[1], sigBegin, sigEnd);
    this->camera1()->processDataCounts();
    TGraph* cam1ProfileV = this->camera1()->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
    TGraph* cam1ProfileH = this->camera1()->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);

    this->camera2()->setPedestalSignalGate(gatecam2[0], gatecam2[1], sigBegin, sigEnd);
    this->camera2()->processDataCounts();
    TGraph* cam2ProfileV = this->camera2()->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
    TGraph* cam2ProfileH = this->camera2()->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);

    this->processBeamPathProfile(cam1ProfileV);
    this->processBeamPathProfile(cam1ProfileH);
    this->processBeamPathProfile(cam2ProfileV);
    this->processBeamPathProfile(cam2ProfileH);

    BeamProfileArray pBegin = this->getProfilesParameters(cam1ProfileH, cam1ProfileV);
    BeamProfileArray pEnd = this->getProfilesParameters(cam2ProfileH, cam2ProfileV);

    delete cam1ProfileV;
    delete cam1ProfileH;
    delete cam2ProfileV;
    delete cam2ProfileH;
    beamPathMap[double(i + 1) * double(integInterval)] = std::make_pair(pBegin, pEnd);
    QString integrTime = QString::number((i + 1) * integInterval);
    QString cam1Position("No Beam");
    if (pBegin[2] >= 0)
    {
      cam1Position = QObject::tr("%1, %2, %3").arg(pBegin[0], 0, 'g', 1).arg(pBegin[1], 0, 'g', 1).arg(pBegin[2], 0, 'g', 2);
    }
    QString cam2Position("No Beam");
    if (pEnd[2] >= 0)
    {
      cam2Position = QObject::tr("%1, %2, %3").arg(pEnd[0], 0, 'g', 1).arg(pEnd[1], 0, 'g', 1).arg(pEnd[2], 0, 'g', 2);
    }

    this->latexBeamPathTable->DrawLatex(.0, pos[i], integrTime.toLatin1().data());
    this->latexBeamPathTable->DrawLatex(.3, pos[i], cam1Position.toLatin1().data());
    this->latexBeamPathTable->DrawLatex(.6, pos[i], cam1Position.toLatin1().data());
  }
  this->beamPathModel->setBeamPath(beamPathMap);
  this->padBeamPathTable->Modified();
  this->padBeamPathTable->Update();
}

void BeamPathProfileDialogPrivate::calculateScanning()
{
  Q_Q(BeamPathProfileDialog);
  if (!this->ui->PushButton_ScanningCalculation->isCheckable() ||
    !this->ui->PushButton_ScanningCalculation->isChecked())
  {
    qDebug() << Q_FUNC_INFO << "Skip scanning calculation";
    return;
  }

  QElapsedTimer timer;
  timer.start();

  if (!this->camera2() || !this->camerasProfilesReady.second)
  {
    return;
  }

  int scanIter = this->ui->SliderWidget_ScanningSpills->value();
  if (scanIter == 0)
  {
    // no more iterations
    return;
  }

  // pedestals signal gate time is default for full beam spill
  this->camera2()->processDataCounts();
  TH2* cam2Hist2 = this->camera2()->createProfile2D();
  this->camera2()->updateProfiles2D(cam2Hist2, nullptr);
  this->histScanning2D->Add(cam2Hist2);
  delete cam2Hist2;

  this->histScanning2D->Scale(100. / this->histScanning2D->GetMaximum());
//  this->histScanning2D->GetZaxis()->SetRangeUser(0., 100.);
  // calculate and display homogenous value D
  double homoValue = this->homogenousValue();
  scanIter--;
  TString scanTitle;
  if (homoValue > 0.)
  {
    if (scanIter > 0)
    {
      scanTitle.Form("Scanning mode: D = %1.3f, Spills left: %d", homoValue, scanIter);
    }
    else if (scanIter == 0)
    {
      scanTitle.Form("Scanning mode: D = %1.3f, no more spills", homoValue);
    }
    else
    {
      scanTitle.Form("Scanning mode: D = %1.3f");
    }
  }
  if (homoValue > 0.)
  {
    this->ui->Label_Homogeneous->setText(QObject::tr("%1 %").arg(homoValue * 100., 0, 'g', 1));
  }
  else
  {
    this->ui->Label_Homogeneous->setText("");
  }
  this->histScanning2D->SetTitle(scanTitle.Data());
  this->padScanning2D->Modified();
  this->padScanning2D->Update();

  double hmin = this->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  double hmax = this->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
  double vmin = this->ui->RangeWidget_ScanHomoVertical->minimumValue();
  double vmax = this->ui->RangeWidget_ScanHomoVertical->maximumValue();

  double centerPosX = hmin + (hmax - hmin) / 2.;
  double centerPosY = vmin + (vmax - vmin) / 2.;
  int binCenterPosX = this->getBinScanningX(centerPosX);
  int binCenterPosY = this->getBinScanningY(centerPosY);

  if (this->graphScanningHorizontalProfile)
  {
    this->padScanningProfiles->GetListOfPrimitives()->Remove(this->graphScanningHorizontalProfile.get());
  }
  if (this->graphScanningVerticalProfile)
  {
    this->padScanningProfiles->GetListOfPrimitives()->Remove(this->graphScanningVerticalProfile.get());
  }

  TH1* profH = this->histScanning2D->ProjectionX("_px", binCenterPosX);
  TH1* profV = this->histScanning2D->ProjectionY("_py", binCenterPosY);
  this->padScanningProfiles->cd();
  TGraph* profX = this->getScanningProjection(profH);
  TGraph* profY = this->getScanningProjection(profV);
  delete profH;
  delete profV;
  profX->Draw();
  profY->Draw("SAME");
  this->graphScanningHorizontalProfile.reset(profX);
  this->graphScanningVerticalProfile.reset(profY);

  if (scanIter >= 0)
  {
    this->ui->SliderWidget_ScanningSpills->setValue(scanIter);
  }
  qDebug() << Q_FUNC_INFO << ": Scanning calculation, time elapsed " << timer.elapsed() << ' ' \
    << ", bin center X: " << binCenterPosX << ", bin center Y: " << binCenterPosY;
}

void BeamPathProfileDialogPrivate::calculateCalibration()
{
  Q_Q(BeamPathProfileDialog);
  if (!this->ui->PushButton_CalibrationCalculation->isCheckable() ||
    !this->ui->PushButton_CalibrationCalculation->isChecked())
  {
    qDebug() << Q_FUNC_INFO << "Skip calibration calculation";
    return;
  }

  QElapsedTimer timer;
  timer.start();

  if (!this->camera2() || !this->camerasProfilesReady.second)
  {
    return;
  }

  std::array< int, 4 > gatecam2;
  this->camera2()->getPedestalSignalGate(gatecam2);

  const int offset = 200;
  double maxv = this->ui->RangeWidget_CalibBatchAcqTime->maximumValue();
  double minv = this->ui->RangeWidget_CalibBatchAcqTime->minimumValue();

  double mx = this->ui->RangeWidget_CalibBatchAcqTime->maximum();
  double mn = this->ui->RangeWidget_CalibBatchAcqTime->minimum();

  int initOffset = static_cast< int >(minv - mn);
  int integRange = static_cast< int >(mx - mn);
  int integInterval = static_cast< int >(maxv - minv);
  const int steps = 9;

  this->histCalibration2D->Reset();

  Int_t* ptr = const_cast< Int_t* >(this->calibrationScanningPalette.data());
  Int_t SIZE = this->calibrationScanningPalette.size();
  gStyle->SetPalette(SIZE, ptr);

  for (int i = 0; i < steps; ++i)
  {
    int sigBegin = offset + (i * integRange) + initOffset;
    int sigEnd = sigBegin + integInterval;
    this->camera2()->setPedestalSignalGate(gatecam2[0], gatecam2[1], sigBegin, sigEnd);
    this->camera2()->processDataCounts();
    TH2* cam2Hist2 = this->camera2()->createProfile2D();
    this->camera2()->updateProfiles2D(cam2Hist2, nullptr);
    this->histCalibration2D->Add(cam2Hist2, 1. / Double_t(steps));
    delete cam2Hist2;
  }
  this->histCalibration2D->Scale(100. / this->histCalibration2D->GetMaximum());
//  this->histCalibration2D->GetZaxis()->SetRangeUser(0., 100.);
  this->padCalibration2D->Modified();
  this->padCalibration2D->Update();
}

BeamPathProfileDialog::BeamPathProfileDialog(QWidget *parent) :
  QDialog(parent),
  d_ptr(new BeamPathProfileDialogPrivate(*this))
{
  Q_D(BeamPathProfileDialog);
  d->ui->setupUi(this);

  TCanvas* canvas = d->ui->RootCanvas_BeamPathTable->getCanvas();
  canvas->cd();
  d->padBeamPathTable = std::unique_ptr< TPad >(new TPad("pBeamPathTable", "Grid", 0., 0., 1., 1.));
  d->padBeamPathTable->Draw();

  canvas = d->ui->RootCanvas_Calibration2D->getCanvas();
  canvas->cd();
  d->padCalibration2D = std::unique_ptr< TPad >(new TPad("pCalib2D", "Calibration Grid", 0., 0., 1., 1.));
  d->padCalibration2D->SetGrid();
  d->padCalibration2D->Draw();

  canvas = d->ui->RootCanvas_Scanning2D->getCanvas();
  canvas->cd();
  d->padScanning2D = std::unique_ptr< TPad >(new TPad("pScan2D", "Scanning Grid", 0., 0., 1., 1.));
  d->padScanning2D->SetGrid();
  d->padScanning2D->Draw();

  canvas = d->ui->RootCanvas_ScanningProfiles->getCanvas();
  canvas->cd();
  d->padScanningProfiles = std::unique_ptr< TPad >(new TPad("pScanProf", "Profiles Grid", 0., 0., 1., 1.));
  d->padScanningProfiles->SetGrid();
  d->padScanningProfiles->Draw();

  this->setWindowTitle(tr("Beam Path and Profile Dialog"));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);
  d->ui->TableView_BeamPath->setModel(d->beamPathModel.data());

//  QObject::connect(d->ui->PushButton_BeamPathCalculation,
//    SIGNAL(clicked()), this, SLOT(onBeamPathCalculationClicked()));
//  QObject::connect(d->ui->PushButton_CalibrationCalculation,
//    SIGNAL(clicked()), this, SLOT(onCalibrationCalculationClicked()));
//  QObject::connect(d->ui->PushButton_ScanningCalculation,
//    SIGNAL(clicked()), this, SLOT(onScanningCalculationClicked()));

  QObject::connect(d->ui->RangeWidget_ScanHomoHorizontal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onScanHomoHorizonalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_ScanHomoHorizontal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onScanHomoHorizonalEndChanged(double)));
  QObject::connect(d->ui->RangeWidget_ScanHomoVertical, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onScanHomoVerticalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_ScanHomoVertical, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onScanHomoVerticalEndChanged(double)));

  gStyle->SetPalette(d->calibrationScanningPalette.size(),
    const_cast< Int_t* >(d->calibrationScanningPalette.data()));
}

BeamPathProfileDialog::~BeamPathProfileDialog()
{
  Q_D(BeamPathProfileDialog);
  std::shared_ptr< THttpServer > server = d->httpServer.lock();
  if (server)
  {
    if (d->padBeamPathTable)
    {
      server->Unregister(d->padBeamPathTable.get());
    }
    if (d->padCalibration2D)
    {
      server->Unregister(d->padCalibration2D.get());
    }
    if (d->padScanning2D)
    {
      server->Unregister(d->padScanning2D.get());
    }
    if (d->padScanningProfiles)
    {
      server->Unregister(d->padScanningProfiles.get());
    }
  }

  if (d->padScanning2D)
  {
    d->padScanning2D->cd();
    if (d->lineScanningHorizontalBegin)
    {
      d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningHorizontalBegin.get());
    }
    if (d->lineScanningHorizontalEnd)
    {
      d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningHorizontalEnd.get());
    }
    if (d->lineScanningVerticalBegin)
    {
      d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningVerticalBegin.get());
    }
    if (d->lineScanningVerticalEnd)
    {
      d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningVerticalEnd.get());
    }
    if (d->padScanning2D)
    {
      d->padScanning2D->GetListOfPrimitives()->Remove(d->histScanning2D.get());
    }
  }
}

void BeamPathProfileDialog::setCamerasDevices(AbstractCamera* camera1, AbstractCamera* camera2)
{
  Q_D(BeamPathProfileDialog);
  d->beamPathCameras.first = camera1;
  d->beamPathCameras.second = camera2;
  if (d->createProfiles())
  {
    Double_t hmin = d->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
    this->onScanHomoHorizonalBeginChanged(hmin);
    Double_t hmax = d->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
    this->onScanHomoHorizonalEndChanged(hmax);

    Double_t vmin = d->ui->RangeWidget_ScanHomoVertical->minimumValue();
    this->onScanHomoVerticalBeginChanged(vmin);
    Double_t vmax = d->ui->RangeWidget_ScanHomoVertical->maximumValue();
    this->onScanHomoVerticalEndChanged(vmax);
  }
}

void BeamPathProfileDialog::addBeamProfilesToServer(std::shared_ptr< THttpServer > server)
{
  Q_D(BeamPathProfileDialog);
  if (!server)
  {
    return;
  }
  d->httpServer = server;
  server->Register("/", d->padBeamPathTable.get());
  server->Register("/", d->padCalibration2D.get());
  server->Register("/", d->padScanning2D.get());
  server->Register("/", d->padScanningProfiles.get());
  server->SetItemField("/","_monitoring","1000");
}

void BeamPathProfileDialog::processCameraProfiles(const QString &cameraID)
{
  Q_D(BeamPathProfileDialog);
  if (d->camera1() && (cameraID == d->camera1()->getCameraData().ID))
  {
    d->camerasProfilesReady.first = true;
  }
  if (d->camera2() && (cameraID == d->camera2()->getCameraData().ID))
  {
    d->camerasProfilesReady.second = true;
  }
  if (d->camerasProfilesReady.first && d->camerasProfilesReady.second)
  {
    // get profiles data, start processing of beam path, calibration, scanning
    d->calculateBeamPath();
    d->calculateCalibration();
    d->calculateScanning();
    d->camerasProfilesReady.first = false;
    d->camerasProfilesReady.second = false;
  }
}

void BeamPathProfileDialog::processCameraBeginProfiles()
{
  Q_D(BeamPathProfileDialog);
  if (d->camera1())
  {
    d->camerasProfilesReady.first = true;
  }
}

void BeamPathProfileDialog::processCameraEndProfiles()
{
  Q_D(BeamPathProfileDialog);
  if (d->camera2())
  {
    d->camerasProfilesReady.second = true;
  }
}

void BeamPathProfileDialog::onScanHomoHorizonalBeginChanged(double pos)
{
  Q_D(BeamPathProfileDialog);
  if (!d->camera2() || !d->camerasProfilesReady.second)
  {
    return;
  }
  if (d->lineScanningHorizontalBegin)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningHorizontalBegin.get());
  }

  Double_t rangeMin = d->histScanning2D->GetYaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetYaxis()->GetXmax();
  d->lineScanningHorizontalBegin = std::make_unique< TLine >(pos, rangeMin, pos, rangeMax);

  d->padScanning2D->cd();

  d->lineScanningHorizontalBegin->SetLineColor(kBlue);
  d->lineScanningHorizontalBegin->SetLineStyle(7);
  d->lineScanningHorizontalBegin->SetLineWidth(3.);
  d->lineScanningHorizontalBegin->SetVertical(kTRUE);
  d->lineScanningHorizontalBegin->Draw();

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

void BeamPathProfileDialog::onScanHomoHorizonalEndChanged(double pos)
{
  Q_D(BeamPathProfileDialog);
  if (!d->camera2() || !d->camerasProfilesReady.second)
  {
    return;
  }
  if (d->lineScanningHorizontalEnd)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningHorizontalEnd.get());
  }

  Double_t rangeMin = d->histScanning2D->GetYaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetYaxis()->GetXmax();
  d->lineScanningHorizontalEnd = std::make_unique< TLine >(pos, rangeMin, pos, rangeMax);

  d->padScanning2D->cd();

  d->lineScanningHorizontalEnd->SetLineColor(kBlue);
  d->lineScanningHorizontalEnd->SetLineStyle(7);
  d->lineScanningHorizontalEnd->SetLineWidth(3.);
  d->lineScanningHorizontalEnd->SetVertical(kTRUE);
  d->lineScanningHorizontalEnd->Draw();

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

void BeamPathProfileDialog::onScanHomoVerticalBeginChanged(double pos)
{
  Q_D(BeamPathProfileDialog);
  if (!d->camera2() || !d->camerasProfilesReady.second)
  {
    return;
  }
  if (d->lineScanningVerticalBegin)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningVerticalBegin.get());
  }

  Double_t rangeMin = d->histScanning2D->GetXaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetXaxis()->GetXmax();

  d->lineScanningVerticalBegin = std::make_unique< TLine >(rangeMin, pos, rangeMax, pos);

  d->padScanning2D->cd();

  d->lineScanningVerticalBegin->SetLineColor(kBlue);
  d->lineScanningVerticalBegin->SetLineStyle(7);
  d->lineScanningVerticalBegin->SetLineWidth(3.);
  d->lineScanningVerticalBegin->SetHorizontal(kTRUE);
  d->lineScanningVerticalBegin->Draw();

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

void BeamPathProfileDialog::onScanHomoVerticalEndChanged(double pos)
{
  Q_D(BeamPathProfileDialog);
  if (!d->camera2() || !d->camerasProfilesReady.second)
  {
    return;
  }
  if (d->lineScanningVerticalEnd)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningVerticalEnd.get());
  }

  Double_t rangeMin = d->histScanning2D->GetXaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetXaxis()->GetXmax();

  d->lineScanningVerticalEnd = std::make_unique< TLine >(rangeMin, pos, rangeMax, pos);


  d->padScanning2D->cd();

  d->lineScanningVerticalEnd->SetLineColor(kBlue);
  d->lineScanningVerticalEnd->SetLineStyle(7);
  d->lineScanningVerticalEnd->SetLineWidth(3.);
  d->lineScanningVerticalEnd->SetHorizontal(kTRUE);
  d->lineScanningVerticalEnd->Draw();

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

QT_END_NAMESPACE
