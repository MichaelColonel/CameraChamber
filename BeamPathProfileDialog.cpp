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
#include <TF1.h>
#include <TPad.h>
#include <TMultiGraph.h>
#include <TStyle.h>
#include <TImage.h>
#include <TFile.h>

#include <QElapsedTimer>
#include <QTimer>
#include <QProcess>

#include "AbstractCamera.h"
#include "BeamPathTableModel.h"

#include <iomanip>
#include <sstream>
#include <string>

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
  std::pair< double, double > homogenousValue();
  int getBinScanningX(double posX);
  int getBinScanningY(double posY);
  int getBinX(TH2* h2, double posX);
  int getBinY(TH2* h2, double posY);

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
  std::unique_ptr< TLatex > latexBeamPathTable1;
  std::unique_ptr< TLatex > latexBeamPathTable2;

  std::unique_ptr< TPad > padBeamPathTable1;
  std::unique_ptr< TPad > padBeamPathTable2;
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

  std::unique_ptr< TImage > imageCalibration2D;
  std::unique_ptr< TImage > imageScanning2D;

  QScopedPointer< BeamPathTableModel > beamPathModel;
  QScopedPointer< QTimer > timer;
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
  int ScanningCount{ 1 };
};

BeamPathProfileDialogPrivate::BeamPathProfileDialogPrivate(BeamPathProfileDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::BeamPathProfileDialog),
  beamPathModel(new BeamPathTableModel(&object)),
  timer(new QTimer(&object))
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

std::pair< double, double > BeamPathProfileDialogPrivate::homogenousValue()
{
  std::pair< double, double > res{ -1, -1 };
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
    res.first = min / max;
    res.second = (max - min) / (max + min);
  }
  if (values.size() == 1)
  {
    res.first = 1.;
    res.second = 0.;
  }
  if (!values.size())
  {
    res.first = -1.;
    res.second = -1.;
  }
  return res;
}

BeamProfileArray BeamPathProfileDialogPrivate::getProfilesParameters(TGraph* profileX, TGraph* profileY)
{
  BeamProfileArray bp{ 0.0, 0.0, -1., -1. };
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
    bp[2] = sigmaX;
    bp[3] = sigmaY;
  }
  return bp;
}

BeamProfileArray BeamPathProfileDialogPrivate::processBeamProfile(AbstractCamera* cam,
  int pedMin, int pedMax, int signalMin, int signalMax)
{
  Q_Q(BeamPathProfileDialog);
  BeamProfileArray prof{0., 0., -1., -1.};
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

int BeamPathProfileDialogPrivate::getBinX(TH2* hist2, double posX)
{
  int binPos = -1;
  if (!hist2)
  {
    return binPos;
  }
  for (int i = 1; i <= hist2->GetXaxis()->GetNbins(); ++i)
  {
    Double_t binLow = hist2->GetXaxis()->GetBinLowEdge(i);
    Double_t binUp = hist2->GetXaxis()->GetBinUpEdge(i);
    if ((posX >= binLow) && (posX < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

int BeamPathProfileDialogPrivate::getBinY(TH2* hist2, double posY)
{
  int binPos = -1;
  if (!hist2)
  {
    return binPos;
  }
  for (int i = 1; i <= hist2->GetYaxis()->GetNbins(); ++i)
  {
    Double_t binLow = hist2->GetYaxis()->GetBinLowEdge(i);
    Double_t binUp = hist2->GetYaxis()->GetBinUpEdge(i);
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
      histCalib->SetTitle("Calibration Mode");
    }
//    histCalib->SetDirectory(nullptr);
    this->histCalibration2D.reset(histCalib);
    this->histCalibration2D->GetXaxis()->SetTitle("Horizontal profile, mm");
    this->histCalibration2D->GetYaxis()->SetTitle("Vertical profile, mm");
//    this->histCalibration2D->Draw("COLZ");

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
      histScan->SetTitle("Scanning Mode");
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

  std::vector< Double_t > v;
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t y = profile->GetPointY(i);
    v.push_back(y);
  }
  Double_t max = *std::max_element(std::begin(v), std::end(v));

  profile->Scale(100. / max);
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t x, y;
    profile->GetPoint(i, x, y);
    constexpr Double_t thres = 3.;
    Double_t new_y = (y < thres) ? 0. : y;
    profile->SetPoint(i, x, new_y);
  }
/*
  Double_t max = profile->GetMaximum();

//  profile->Scale(100. / max);
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t x = -1000., y = -1000.;
    profile->GetPoint(i, x, y);
//    Double_t v = 100. * y / max;
//    profile->SetPoint(i, x, v);
    constexpr Double_t thres = 3.;
    Double_t v = profile->GetMaximum();
    Double_t new_y = (y < thres) ? 0. : y;
    profile->SetPoint(i, x, new_y);
    qDebug() << Q_FUNC_INFO << x << ' ' << y << ' ' << v << ' ' << new_y << ' ' << max;
  }
*/
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

//  const int offset = 20;
  const int integInterval = 100;
  constexpr int steps = 9;
  constexpr std::array< double, steps > pos{.7, .65, .6, .55, .5, .45, .4, .35, .3};
//  constexpr int steps = 7;
//  constexpr std::array< double, steps > pos{.7, .65, .6, .55, .5, .45, .4};

  this->padBeamPathTable1->cd();
  this->padBeamPathTable1->Clear();
  this->latexBeamPathTable1 = std::unique_ptr< TLatex >(new TLatex);
  this->latexBeamPathTable2 = std::unique_ptr< TLatex >(new TLatex);
  QDateTime dt = QDateTime::currentDateTime();
  QString msg = QObject::tr("Местное время -- %1, Count %2").arg(dt.toString("hh:mm:ss")).arg(this->ScanningCount);
  std::string msgBytes = msg.toStdString();
  const char* str = msgBytes.data();

  this->latexBeamPathTable1->DrawLatex(.2, 0.95, str);
  this->latexBeamPathTable1->SetTextSize(0.05);
  this->latexBeamPathTable1->SetTextAlign(13);  //align at top
  this->latexBeamPathTable1->DrawLatex(.0, .9,"Время, (мс)");
  this->latexBeamPathTable1->DrawLatex(.3, .9,"X");
  this->latexBeamPathTable1->DrawLatex(.5, .9,"Y");
  this->latexBeamPathTable1->DrawLatex(.7, .9,"Dx");
  this->latexBeamPathTable1->DrawLatex(.9, .9,"Dy");

  this->latexBeamPathTable1->DrawLatex(.0,.85,"           ");
  this->latexBeamPathTable1->DrawLatex(.3,.85,"     ");
  this->latexBeamPathTable1->DrawLatex(.5,.85,"     ");
  this->latexBeamPathTable1->DrawLatex(.7,.85,"     ");
  this->latexBeamPathTable1->DrawLatex(.9,.85,"     ");

  if (!this->camera1() || !this->camera2() || !this->areCamerasProfilesReady())
  {
    this->padBeamPathTable1->Modified();
    this->padBeamPathTable1->Update();
    this->padBeamPathTable2->Modified();
    this->padBeamPathTable2->Update();
    return;
  }

  std::array< int, 4 > gatecam1;
  this->camera1()->getPedestalSignalGate(gatecam1);
  std::array< int, 4 > gatecam2;
  this->camera2()->getPedestalSignalGate(gatecam2);
/*
  this->latexBeamPathTable1->SetTextSize(0.04);
  this->latexBeamPathTable2->SetTextSize(0.04);
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
    QString cam1PositionPos("No Beam");
    QString cam1PositionDia("No Beam");
    if (pBegin[2] >= 0)
    {
      std::stringstream ssPos;
      ssPos << std::right << std::fixed << std::setw(6) << std::setprecision(2) << pBegin[0] << ", " << pBegin[1];
      std::string strPos = ssPos.str();
      cam1PositionPos = QString::fromStdString(strPos);

      std::stringstream ssDia;
      ssDia << std::right << std::fixed << std::setw(6) << std::setprecision(2) << 6. * pBegin[2] << ", " << 6. * pBegin[3];
      std::string strDia = ssDia.str();
      cam1PositionDia = QString::fromStdString(strDia);
//      cam1Position = QObject::tr("%1, %2, %3").arg(pBegin[0], 5, 'f', 1).arg(pBegin[1], 5, 'f', 1).arg(6. * pBegin[2], 5, 'f', 1);
    }
    QString cam2PositionPos("No Beam");
    QString cam2PositionDia("No Beam");
    if (pEnd[2] >= 0)
    {
        std::stringstream ssPos;
        ssPos << std::right << std::fixed << std::setw(6) << std::setprecision(2) << pEnd[0] << ", " << pEnd[1];
        std::string strPos = ssPos.str();
        cam2PositionPos = QString::fromStdString(strPos);

        std::stringstream ssDia;
        ssDia << std::right << std::fixed << std::setw(6) << std::setprecision(2) << 6. * pEnd[2] << ", " << 6. * pEnd[3];
        std::string strDia = ssDia.str();
        cam2PositionDia = QString::fromStdString(strDia);
  //      cam1Position = QObject::tr("%1, %2, %3").arg(pBegin[0], 5, 'f', 1).arg(pBegin[1], 5, 'f', 1).arg(6. * pBegin[2], 5, 'f', 1);
    }

    this->latexBeamPathTable1->DrawLatex(.0, pos[i], integrTime.toLatin1().data());
    this->latexBeamPathTable1->DrawLatex(.3, pos[i], cam1PositionPos.toLatin1().data());
    this->latexBeamPathTable1->DrawLatex(.6, pos[i], cam1PositionDia.toLatin1().data());

    this->latexBeamPathTable2->DrawLatex(.0, pos[i], integrTime.toLatin1().data());
    this->latexBeamPathTable2->DrawLatex(.3, pos[i], cam2PositionPos.toLatin1().data());
    this->latexBeamPathTable2->DrawLatex(.6, pos[i], cam2PositionDia.toLatin1().data());
  }
  this->beamPathModel->setBeamPath(beamPathMap);
  this->padBeamPathTable1->Modified();
  this->padBeamPathTable1->Update();


  this->padBeamPathTable2->Modified();
  this->padBeamPathTable2->Update();
*/

  int offset = gatecam1[2];
  this->latexBeamPathTable1->SetTextSize(0.04);
  for (int i = 0; i < steps; ++i)
  {
    int sigBegin = offset + (i * integInterval);
    int sigEnd = sigBegin + integInterval;
    this->camera1()->setPedestalSignalGate(gatecam1[0], gatecam1[1], sigBegin, sigEnd);
    this->camera1()->processDataCounts();
    TGraph* cam1ProfileV = this->camera1()->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
    TGraph* cam1ProfileH = this->camera1()->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);

    this->processBeamPathProfile(cam1ProfileV);
    this->processBeamPathProfile(cam1ProfileH);

    BeamProfileArray pBegin = this->getProfilesParameters(cam1ProfileH, cam1ProfileV);

    delete cam1ProfileV;
    delete cam1ProfileH;
    QString integrTime = QString::number((i + 1) * integInterval);
    QString cam1PositionPos("No Beam");
    QString cam1PositionDia("No Beam");
    if (pBegin[2] >= 0)
    {
      std::stringstream ssPos;
      ssPos << std::right << std::fixed << std::setw(8) << std::setprecision(1) << pBegin[0] << ' ' << std::setw(6) << pBegin[1];
      std::string strPos = ssPos.str();
      cam1PositionPos = QObject::tr("%1  %2").arg(pBegin[0], 5, 'f', 1, QChar(' ')).arg(pBegin[1], 5, 'f', 1, QChar(' '));

      std::stringstream ssDia;
      ssDia << std::right << std::fixed << std::setw(8) << std::setprecision(1) << 6. * pBegin[2] << ' ' << std::setw(5) << 6. * pBegin[3];
      std::string strDia = ssDia.str();
      cam1PositionDia = QObject::tr("%1  %2").arg(6. * pBegin[2], 5, 'f', 1, QChar(' ')).arg(6. * pBegin[3], 5, 'f', 1, QChar(' '));
    }

    this->latexBeamPathTable1->DrawLatex(.0, pos[i], integrTime.toLatin1().data());
    QString val1 = QObject::tr("%1").arg(pBegin[0], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable1->DrawLatex(.3, pos[i], val1.toLatin1().data());
    QString val2 = QObject::tr("%1").arg(pBegin[1], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable1->DrawLatex(.5, pos[i], val2.toLatin1().data());
    QString val3 = QObject::tr("%1").arg(6. * pBegin[2], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable1->DrawLatex(.7, pos[i], val3.toLatin1().data());
    QString val4 = QObject::tr("%1").arg(6. * pBegin[3], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable1->DrawLatex(.9, pos[i], val4.toLatin1().data());
  }
  this->padBeamPathTable1->Modified();
  this->padBeamPathTable1->Update();

  offset = gatecam2[2];
  this->padBeamPathTable2->cd();
  this->padBeamPathTable2->Clear();
  this->latexBeamPathTable2->DrawLatex(.2, 0.95, str);
  this->latexBeamPathTable2->SetTextSize(0.05);
  this->latexBeamPathTable2->SetTextAlign(13);  //align at top
  this->latexBeamPathTable2->DrawLatex(.0, .9,"Время, (мс)");
  this->latexBeamPathTable2->DrawLatex(.3, .9,"X");
  this->latexBeamPathTable2->DrawLatex(.5, .9,"Y");
  this->latexBeamPathTable2->DrawLatex(.7, .9,"Dx");
  this->latexBeamPathTable2->DrawLatex(.9, .9,"Dy");
  this->latexBeamPathTable2->DrawLatex(.0,.85,"           ");
  this->latexBeamPathTable2->DrawLatex(.3,.85,"     ");
  this->latexBeamPathTable2->DrawLatex(.5,.85,"     ");
  this->latexBeamPathTable2->DrawLatex(.7,.85,"     ");
  this->latexBeamPathTable2->DrawLatex(.9,.85,"     ");
  this->latexBeamPathTable2->SetTextSize(0.04);
  for (int i = 0; i < steps; ++i)
  {
    int sigBegin = offset + (i * integInterval);
    int sigEnd = sigBegin + integInterval;

//    qDebug() << Q_FUNC_INFO << sigBegin << ' ' << sigEnd;
    this->camera2()->setPedestalSignalGate(gatecam2[0], gatecam2[1], sigBegin, sigEnd);
    this->camera2()->processDataCounts();
    TGraph* cam2ProfileV = this->camera2()->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
    TGraph* cam2ProfileH = this->camera2()->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);
    this->processBeamPathProfile(cam2ProfileV);
    this->processBeamPathProfile(cam2ProfileH);

    BeamProfileArray pEnd = this->getProfilesParameters(cam2ProfileH, cam2ProfileV);

    delete cam2ProfileV;
    delete cam2ProfileH;
    QString integrTime = QString::number((i + 1) * integInterval);

    QString cam2PositionPos("No Beam");
    QString cam2PositionDia("No Beam");
    if (pEnd[2] >= 0)
    {
        std::stringstream ssPos;
        ssPos << std::right << std::fixed << std::setw(6) << std::setprecision(1) << pEnd[0] << ' ' << std::setw(6) << pEnd[1];
        std::string strPos = ssPos.str();
        cam2PositionPos = QObject::tr("%1  %2").arg(pEnd[0], 5, 'f', 1, QChar(' ')).arg(pEnd[1], 5, 'f', 1, QChar(' '));

        std::stringstream ssDia;
        ssDia << std::right << std::fixed << std::setw(6) << std::setprecision(1) << 6. * pEnd[2] << ' ' << std::setw(6) << 6. * pEnd[3];
        std::string strDia = ssDia.str();
        cam2PositionDia = QObject::tr("%1  %2").arg(6. * pEnd[1], 5, 'f', 1, QChar(' ')).arg(6. * pEnd[2], 5, 'f', 1, QChar(' '));
    }
    this->latexBeamPathTable2->DrawLatex(.0, pos[i], integrTime.toLatin1().data());
    QString val1 = QObject::tr("%1").arg(pEnd[0], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable2->DrawLatex(.3, pos[i], val1.toLatin1().data());
    QString val2 = QObject::tr("%1").arg(pEnd[1], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable2->DrawLatex(.5, pos[i], val2.toLatin1().data());
    QString val3 = QObject::tr("%1").arg(6. * pEnd[2], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable2->DrawLatex(.7, pos[i], val3.toLatin1().data());
    QString val4 = QObject::tr("%1").arg(6. * pEnd[3], 5, 'f', 1, QChar(' '));
    this->latexBeamPathTable2->DrawLatex(.9, pos[i], val4.toLatin1().data());
  }
  this->padBeamPathTable2->Modified();
  this->padBeamPathTable2->Update();
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
  std::array< int, 4 > gatecam2;
  this->camera2()->getPedestalSignalGate(gatecam2);

  const double time = 900.;
  const double step = 30.;
  const int steps = time / step;

  if (this->ScanningCount >= 10)
  {
    this->histScanning2D->Reset();
//    this->ScanningCount = 1;
  }
  else
  {
//    this->ScanningCount++;
  }
  for (int j = 0; j < steps; ++j)
  {
    int sigBegin = gatecam2[2] + (j * step);
    int sigEnd = sigBegin + step;
    this->camera2()->setPedestalSignalGate(gatecam2[0], gatecam2[1], sigBegin, sigEnd);
    this->camera2()->processDataCounts();
    TH2* cam2Hist2 = this->camera2()->createProfile2D();
    this->camera2()->updateProfiles2D(cam2Hist2, nullptr);
    this->histScanning2D->Add(cam2Hist2);
    delete cam2Hist2;
  }
/*
  TH2* normHist2 = dynamic_cast< TH2* >(this->histScanning2D->Clone("_cloned"));

  if (normHist2)
  {
    normHist2->SetDirectory(nullptr);
    normHist2->Scale(100. / this->histScanning2D->GetMaximum());
    normHist2->GetZaxis()->SetRangeUser(0., 100.);
  }
*/

  // calculate and display homogenous value D
  std::pair< double, double > homoValue = this->homogenousValue();
  scanIter--;
  TString scanTitle;
  if (homoValue.first > 0.)
  {
    if (scanIter > 0)
    {
      scanTitle.Form("Scanning: D = %1.3f, Spills left: %d", homoValue.first, scanIter);
    }
    else if (scanIter == 0)
    {
      scanTitle.Form("Scanning: D = %1.3f, no more spills", homoValue.first);
    }
    else
    {
      scanTitle.Form("Scanning: D = %1.3f");
    }
  }
  if (homoValue.first > 0.)
  {
    this->ui->Label_Homogeneous->setText(QObject::tr("%1 %, %2 %").arg(homoValue.first * 100., 0, 'f', 1).arg(homoValue.second * 100., 0, 'f', 1));
  }
  else
  {
    this->ui->Label_Homogeneous->setText("");
  }
  this->histScanning2D->SetTitle(scanTitle.Data());
//  this->histScanning2D->SetStats(kFALSE);
  this->padScanning2D->Modified();
  this->padScanning2D->Update();
/*
  auto file = std::unique_ptr< TFile >(new TFile("/tmp/profile.root", "RECREATE"));
  if (file)
  {
    file->WriteTObject(normHist2, "hist2_cloned");
    file->Close();
  }
  delete normHist2;

  QString program = "./profile_offline";
  QStringList arguments;
  arguments << "/tmp/profile.root";

  QProcess *myProcess = new QProcess(q);
  myProcess->start(program, arguments);
  if (myProcess->waitForFinished())
  {
    qDebug() << Q_FUNC_INFO << "2D is ready";
    TImage* img = TImage::Open("/tmp/profile2.png");
    if (img)
    {
      this->padScanning2D->cd();
      img->Draw("SAME");
      qDebug() << Q_FUNC_INFO << "Image is ready";
      this->padScanning2D->Modified();
      this->padScanning2D->Update();
    }
  }
  delete myProcess;
*/
  double hmin = this->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  double hmax = this->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
  double vmin = this->ui->RangeWidget_ScanHomoVertical->minimumValue();
  double vmax = this->ui->RangeWidget_ScanHomoVertical->maximumValue();

  double centerPosX = hmin + (hmax - hmin) / 2.;
  double centerPosY = vmin + (vmax - vmin) / 2.;
  int binCenterPosX = this->getBinScanningX(centerPosX);
  int binCenterPosY = this->getBinScanningY(centerPosY);
/*
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
*/
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

  const int offset = gatecam2[2];
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
//  this->histCalibration2D->Scale(100. / this->histCalibration2D->GetMaximum());
//  this->histCalibration2D->GetZaxis()->SetRangeUser(0., 100.);
  this->histCalibration2D->SetStats(kFALSE);

  auto file = std::unique_ptr< TFile >(new TFile("/tmp/profile.root", "RECREATE"));
  if (file)
  {
    file->WriteTObject(this->histCalibration2D.get(), "hist2");
    file->Close();
  }
  QString program = "./profile_offline";
  QStringList arguments;
  arguments << "/tmp/profile.root";

  QProcess *myProcess = new QProcess(q);
  myProcess->start(program, arguments);
  if (myProcess->waitForFinished())
  {
    qDebug() << Q_FUNC_INFO << "2D is ready";
    TImage* img = TImage::Open("/tmp/profile.png");
    if (img)
    {
      this->padCalibration2D->cd();
      img->Draw();
      qDebug() << Q_FUNC_INFO << "Image is ready";
      this->padCalibration2D->Modified();
      this->padCalibration2D->Update();
    }
  }
  delete myProcess;

//  this->padCalibration2D->Modified();
//  this->padCalibration2D->Update();

//  std::unique_ptr< TImage > img(TImage::Create());
//  img->FromPad(this->padCalibration2D.get());
//  img->WriteImage("/tmp/test.png");
}

BeamPathProfileDialog::BeamPathProfileDialog(QWidget *parent) :
  QDialog(parent),
  d_ptr(new BeamPathProfileDialogPrivate(*this))
{
  Q_D(BeamPathProfileDialog);
  d->ui->setupUi(this);

  TCanvas* canvas = d->ui->RootCanvas_BeamPathTable1->getCanvas();
  canvas->cd();
  d->padBeamPathTable1 = std::unique_ptr< TPad >(new TPad("BeamPathTable1", "Grid", 0., 0., 1., 1.));
  d->padBeamPathTable1->Draw();

  canvas = d->ui->RootCanvas_BeamPathTable2->getCanvas();
  canvas->cd();
  d->padBeamPathTable2 = std::unique_ptr< TPad >(new TPad("BeamPathTable2", "Grid", 0., 0., 1., 1.));
  d->padBeamPathTable2->Draw();

  canvas = d->ui->RootCanvas_Calibration2D->getCanvas();
  canvas->cd();
  d->padCalibration2D = std::unique_ptr< TPad >(new TPad("Calibration2D", "Calibration Grid", 0., 0., 1., 1.));
//  d->padCalibration2D->SetGrid();
  d->padCalibration2D->Draw();

  canvas = d->ui->RootCanvas_Scanning2D->getCanvas();
  canvas->cd();
  d->padScanning2D = std::unique_ptr< TPad >(new TPad("Scanning2D", "Scanning Grid", 0., 0., 1., 1.));
//  d->padScanning2D->SetGrid();
  d->padScanning2D->Draw();

  canvas = d->ui->RootCanvas_ScanningProfiles->getCanvas();
  canvas->cd();
  d->padScanningProfiles = std::unique_ptr< TPad >(new TPad("ScanningProfiles", "Profiles Grid", 0., 0., 1., 1.));
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

  QObject::connect(d->ui->PushButton_ResetCalibration, SIGNAL(clicked()),
    this, SLOT(onResetCalibrationClicked()));
  QObject::connect(d->ui->PushButton_ResetScanning, SIGNAL(clicked()),
    this, SLOT(onResetScanningClicked()));

  gStyle->SetPalette(d->calibrationScanningPalette.size(),
    const_cast< Int_t* >(d->calibrationScanningPalette.data()));

//  QObject::connect(d->timer.get(), SIGNAL(timeout()), this, SLOT(onTimeoutUpdate()));
//  d->timer->setInterval(1000);
//  d->timer->start();
}

BeamPathProfileDialog::~BeamPathProfileDialog()
{
  Q_D(BeamPathProfileDialog);
  std::shared_ptr< THttpServer > server = d->httpServer.lock();
  if (server)
  {
    if (d->padBeamPathTable1)
    {
      server->Unregister(d->padBeamPathTable1.get());
    }
    if (d->padBeamPathTable2)
    {
      server->Unregister(d->padBeamPathTable2.get());
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
  server->Register("/", d->padBeamPathTable1.get());
  server->Register("/", d->padBeamPathTable2.get());
  server->Register("/", d->padCalibration2D.get());
  server->Register("/", d->padScanning2D.get());
  server->Register("/", d->padScanningProfiles.get());
  server->SetItemField("/","_monitoring","1000");
  server->SetItemField("/","_layout","grid3x2");
  server->SetItemField("/","_drawitem","[Camera2/pHorProf,Camera2/pVerProf,BeamPathTable1,Camera4/pHorProf,Camera4/pVerProf,BeamPathTable2]");
//  server->SetItemField("/","_layout","simple");
//  server->SetItemField("/","_drawitem","[Scanning2D]");
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

  if (d->camerasProfilesReady.second)
  {
//    qDebug() << Q_FUNC_INFO << "Process scanning";
    // get profiles data, start processing of beam path, calibration, scanning
  }

  if (d->camerasProfilesReady.first && d->camerasProfilesReady.second)
  {
    // get profiles data, start processing of beam path, calibration, scanning
    d->calculateBeamPath();
    d->calculateScanning();
    d->camerasProfilesReady.first = false;
    d->camerasProfilesReady.second = false;
    if (d->ScanningCount >= 10)
    {
      d->ScanningCount = 1;
    }
    else
    {
      d->ScanningCount++;
    }
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
    qDebug() << Q_FUNC_INFO << "Isocenter is ready";
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

void BeamPathProfileDialog::onTimeoutUpdate()
{
  Q_D(BeamPathProfileDialog);
  d->calculateBeamPath();
}


void BeamPathProfileDialog::onResetCalibrationClicked()
{
  Q_D(BeamPathProfileDialog);
  d->histCalibration2D->Reset();
  d->padCalibration2D->Modified();
  d->padCalibration2D->Update();
}

void BeamPathProfileDialog::onResetScanningClicked()
{
  Q_D(BeamPathProfileDialog);
  d->histScanning2D->Reset();
  d->padScanning2D->Modified();
  d->padScanning2D->Update();
}

void BeamPathProfileDialog::onResetIntegrationClicked()
{
  Q_D(BeamPathProfileDialog);
}

QT_END_NAMESPACE
