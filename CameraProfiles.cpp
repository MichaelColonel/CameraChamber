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

#include "CameraProfiles.h"

#include "AbstractCamera.h"

#include <QDebug>
#include <QDateTime>

#include <TGraph.h>
#include <TH2.h>
#include <TH1.h>
#include <TLatex.h>

#include <iomanip>

QT_BEGIN_NAMESPACE

class CameraProfilesPrivate
{
  Q_DECLARE_PUBLIC(CameraProfiles);
protected:
  CameraProfiles* const q_ptr;
public:
  CameraProfilesPrivate(CameraProfiles &object);
  virtual ~CameraProfilesPrivate();
  bool invalidPedestalSignalGate() const {
    return (this->pedestalSignalGate[0] == -1 || this->pedestalSignalGate[1] == -1 ||
      this->pedestalSignalGate[2] == -1 || this->pedestalSignalGate[3] == -1);
  }
  AbstractCamera* camera{ nullptr };
  std::array< int, 4 > pedestalSignalGate{ -1, -1, -1, -1 };
};

CameraProfilesPrivate::CameraProfilesPrivate(CameraProfiles &object)
  :
  q_ptr(&object)
{
}

CameraProfilesPrivate::~CameraProfilesPrivate()
{
}

CameraProfiles::CameraProfiles(AbstractCamera* cam, QObject *parent)
  :
  QObject(parent),
  d_ptr(new CameraProfilesPrivate(*this))
{
  Q_D(CameraProfiles);
  d->camera = cam;
  if (d->camera)
  {
    d->camera->getPedestalSignalGate(d->pedestalSignalGate);
  }
}

CameraProfiles::~CameraProfiles()
{
  Q_D(CameraProfiles);
  if (d->camera && !d->invalidPedestalSignalGate())
  {
    int& pedMin = d->pedestalSignalGate[0];
    int& pedMax = d->pedestalSignalGate[1];
    int& sigMin = d->pedestalSignalGate[2];
    int& sigMax = d->pedestalSignalGate[3];
    d->camera->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

TH2* CameraProfiles::createAverageTimeStepProfile2D(int timeStepMs,
  IntegratorType integrType)
{
  Q_D(CameraProfiles);
  if (!d->camera)
  {
    return nullptr;
  }

  std::array< int, 4 > pedestalSignalGate;
  d->camera->getPedestalSignalGate(pedestalSignalGate);

  int& pedMin = pedestalSignalGate[0];
  int& pedMax = pedestalSignalGate[1];
  int& sigMin = pedestalSignalGate[2];
  int& sigMax = pedestalSignalGate[3];

  int gate = sigMax - sigMin;
  if (gate <= 0)
  {
    return nullptr;
  }

  int steps = gate / timeStepMs;
  if (steps < 1)
  {
    steps = 1;
    timeStepMs = gate;
  }

  TH2* histSpill = d->camera->createProfile2D();
  histSpill->SetDirectory(nullptr);

  for (int j = 0; j < steps; ++j)
  {
    int sigBegin = sigMin + (j * timeStepMs);
    int sigEnd = sigBegin + timeStepMs;
    d->camera->setPedestalSignalGate(pedMin, pedMax, sigBegin, sigEnd);
    d->camera->processDataCounts(false, false, integrType);
    std::unique_ptr< TH2 > cam2Hist2(d->camera->createProfile2D());
    cam2Hist2->SetDirectory(nullptr);
    d->camera->updateProfiles2D(cam2Hist2.get(), nullptr);
    histSpill->Add(cam2Hist2.get());
  }
  histSpill->Scale(1. / Double_t(steps));
  return histSpill;
}

TGraph* CameraProfiles::createTimeIntensityProfile(int pedMin, int pedMax, int sigMin, int sigMax,
  CameraProfileType profileType, ProfileRepresentationType representationType,
  IntegratorType integrType)
{
  Q_D(CameraProfiles);
  if (!d->camera)
  {
    return nullptr;
  }

  int intTimeMs = d->camera->getIntegrationTimeMs();

  std::vector< int > vertProfChips;
  std::vector< int > horizProfChips;

  d->camera->getVerticalProfileChips(vertProfChips);
  d->camera->getHorizontalProfileChips(horizProfChips);

  std::vector< Double_t > timeProfileV, timeProfileH;
  std::vector< Double_t > time;
  for (int i = sigMin; i < sigMax - intTimeMs; i += intTimeMs)
  {
    d->camera->setPedestalSignalGate(pedMin, pedMax, i, i + intTimeMs);
    d->camera->processDataCounts(false, false, integrType, representationType);
    Double_t amplV = 0., amplH = 0.;
    for (int chip : vertProfChips)
    {
      for (int channel = 1; channel <= CHANNELS_PER_CHIP; ++channel)
      {
        ChannelInfoPair infoPair;
        if (d->camera->getChipChannelInfo(chip, channel, infoPair))
        {
          amplV += infoPair.second.Signal;
        }
      }
    }
    for (int chip : horizProfChips)
    {
      for (int channel = 1; channel <= CHANNELS_PER_CHIP; ++channel)
      {
        ChannelInfoPair infoPair;
        if (d->camera->getChipChannelInfo(chip, channel, infoPair))
        {
          amplH += infoPair.second.Signal;
        }
      }
    }
    timeProfileH.push_back(amplH);
    timeProfileV.push_back(amplV);
    time.push_back(Double_t(i - sigMin));
  }

  TGraph* gr = nullptr;
  switch (profileType)
  {
  case CameraProfileType::PROFILE_VERTICAL:
    gr = new TGraph(time.size(), time.data(), timeProfileV.data());
    break;
  case CameraProfileType::PROFILE_HORIZONTAL:
    gr = new TGraph(time.size(), time.data(), timeProfileH.data());
    break;
  default:
    break;
  }

  return gr;
}

int CameraProfiles::GetBinX(TH2* hist2, double posX)
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

int CameraProfiles::GetBinY(TH2* hist2, double posY)
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

int CameraProfiles::GetProfileBin(TH1* hist, double pos)
{
  int binPos = -1;
  if (!hist)
  {
    return binPos;
  }
  for (int i = 1; i <= hist->GetXaxis()->GetNbins(); ++i)
  {
    Double_t binLow = hist->GetXaxis()->GetBinLowEdge(i);
    Double_t binUp = hist->GetXaxis()->GetBinUpEdge(i);
    if ((pos >= binLow) && (pos < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

double CameraProfiles::GetNorm(TGraph* profile)
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

double CameraProfiles::GetMean(TGraph* profile, double norm)
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

double CameraProfiles::GetSigma(TGraph* profile, double mean, double norm)
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

void CameraProfiles::ReverseProfileValues(TH1* h)
{
  if (!h)
  {
    return;
  }
  Int_t n = h->GetNbinsX();
  if (n <= 1)
  {
    return;
  }
  for (Int_t i = 0; i < n / 2; ++i)
  {
    Int_t reverseBin = n - i - 1;
    Double_t tempContent = h->GetBinContent(i);
    Double_t tempError = h->GetBinError(i);

    Double_t content = h->GetBinContent(reverseBin);
    Double_t error = h->GetBinError(reverseBin);

    h->SetBinContent(i, content);
    h->SetBinError(i, error);

    h->SetBinContent(reverseBin, tempContent);
    h->SetBinError(reverseBin, tempError);
  }
}

void CameraProfiles::ReverseProfileValues(TGraph* gr)
{
  if (!gr)
  {
    return;
  }
  Int_t n = gr->GetN();
  if (n <= 1)
  {
    return;
  }
  for (Int_t i = 0; i < n / 2; ++i)
  {
    Int_t reverseBin = n - i - 1;
    Double_t tempX = gr->GetPointX(i);
    Double_t tempY = gr->GetPointY(i);

    Double_t x = gr->GetPointX(reverseBin);
    Double_t y = gr->GetPointY(reverseBin);

    gr->SetPointX(i, x);
    gr->SetPointY(i, y);

    gr->SetPointX(reverseBin, tempX);
    gr->SetPointY(reverseBin, tempY);
  }
}

bool CameraProfiles::CheckNoBeamProfile(TGraph* profile)
{

  if (!profile)
  {
    return true;
  }
  if (profile->GetN() <= 1)
  {
    return true;
  }

  Double_t sum = 0.;
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t y = profile->GetPointY(i);
    sum += std::abs(y);
  }
  return (sum < 0.0001);
}

void CameraProfiles::ProcessBeamPathProfile(TGraph* profile)
{

  if (!profile)
  {
    return;
  }
  if (profile->GetN() <= 0)
  {
    return;
  }

  if (CheckNoBeamProfile(profile))
  {
    return;
  }

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
}

BeamProfileArray CameraProfiles::GetProfilesParameters(TGraph* profileX, TGraph* profileY)
{
  BeamProfileArray bp{ 0.0, 0.0, -1., -1. };
  double normX = CameraProfiles::GetNorm(profileX);
  double sigmaX = -1.;
  if (std::abs(normX) > DBL_EPSILON)
  {
    bp[0] = CameraProfiles::GetMean(profileX, normX);
    sigmaX = CameraProfiles::GetSigma(profileX, bp[0], normX);
  }

  double normY = CameraProfiles::GetNorm(profileY);
  double sigmaY = -1.;
  if (std::abs(normY) > DBL_EPSILON)
  {
    bp[1] = CameraProfiles::GetMean(profileY, normY);
    sigmaY = CameraProfiles::GetSigma(profileY, bp[1], normY);
  }

  bp[2] = -1.;
  if (sigmaY >= 0. && sigmaX >= 0.)
  {
    bp[2] = sigmaX;
    bp[3] = sigmaY;
  }
  return bp;
}

TLatex* CameraProfiles::createBeamPathTable(int timeStepMs, int totalSteps, QDateTime& spillDateTime)
{
  Q_D(CameraProfiles);

  constexpr int steps = 9;
  constexpr int stepInterval = 100;
  constexpr std::array< double, steps > pos{.7, .65, .6, .55, .5, .45, .4, .35, .3};
  //  constexpr int steps = 7;
  //  constexpr std::array< double, steps > pos{.7, .65, .6, .55, .5, .45, .4};

  TLatex* latexTable = new TLatex();
  QString msg = QObject::tr("Время вывода: %1").arg(spillDateTime.toString("hh:mm:ss"));
  std::string msgBytes = msg.toStdString();
  const char* str = msgBytes.data();

  latexTable->DrawLatex(.2, 0.95, str);
  latexTable->SetTextSize(0.05);
  latexTable->SetTextAlign(13);  //align at top
  latexTable->DrawLatex(.0, .9, "Время, (мс)");
  latexTable->DrawLatex(.3, .9, "X");
  latexTable->DrawLatex(.5, .9, "Y");
  latexTable->DrawLatex(.7, .9, "Dx");
  latexTable->DrawLatex(.9, .9, "Dy");

  latexTable->DrawLatex(.0,.85, "           ");
  latexTable->DrawLatex(.3,.85, "     ");
  latexTable->DrawLatex(.5,.85, "     ");
  latexTable->DrawLatex(.7,.85, "     ");
  latexTable->DrawLatex(.9,.85, "     ");

  int offset = d->pedestalSignalGate[2];
  latexTable->SetTextSize(0.04);
  for (int step = 0; step < totalSteps; ++step)
  {
    int sigBegin = offset + (step * stepInterval);
    int sigEnd = sigBegin + timeStepMs;
    d->camera->setPedestalSignalGate(d->pedestalSignalGate[0], d->pedestalSignalGate[1], sigBegin, sigEnd);
    d->camera->processDataCounts();
    TGraph* profileV = d->camera->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
    TGraph* profileH = d->camera->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);

    CameraProfiles::ProcessBeamPathProfile(profileV);
    CameraProfiles::ProcessBeamPathProfile(profileH);
    CameraProfiles::ReverseProfileValues(profileH);

    BeamProfileArray pBegin = CameraProfiles::GetProfilesParameters(profileH, profileV);
    bool profilesAreWithBeam = !(CheckNoBeamProfile(profileH) || CheckNoBeamProfile(profileV));

    delete profileV;
    delete profileH;

    QString integrTime = QString::number((step + 1) * stepInterval);
    QString val1 = QObject::tr("%1").arg(-1. * pBegin[0], 5, 'f', 1, QChar(' '));
    QString val2 = QObject::tr("%1").arg(pBegin[1], 5, 'f', 1, QChar(' '));
    QString val3 = QObject::tr("%1").arg(6. * pBegin[2], 5, 'f', 1, QChar(' '));
    QString val4 = QObject::tr("%1").arg(6. * pBegin[3], 5, 'f', 1, QChar(' '));

    latexTable->DrawLatex(.0, pos[step], integrTime.toLatin1().data());
    if (pBegin[2] >= 0. && pBegin[3] >= 0. && profilesAreWithBeam)
    {
      latexTable->DrawLatex(.3, pos[step], val1.toLatin1().data());
      latexTable->DrawLatex(.5, pos[step], val2.toLatin1().data());
      latexTable->DrawLatex(.7, pos[step], val3.toLatin1().data());
      latexTable->DrawLatex(.9, pos[step], val4.toLatin1().data());
    }
    else
    {
      const QString NoBeamStr("NB");
      latexTable->DrawLatex(.3, pos[step], NoBeamStr.toLatin1().data());
      latexTable->DrawLatex(.5, pos[step], NoBeamStr.toLatin1().data());
      latexTable->DrawLatex(.7, pos[step], NoBeamStr.toLatin1().data());
      latexTable->DrawLatex(.9, pos[step], NoBeamStr.toLatin1().data());
    }
  }
  return latexTable;
}

TGraph* CameraProfiles::HistToGraph(TH1* hist)
{
  if (!hist)
  {
    return nullptr;
  }
  Int_t bins = hist->GetXaxis()->GetNbins();
  TGraph* gr = new TGraph(bins);
  for (Int_t i = 1; i <= bins; ++i)
  {
    Double_t x, y;
    x = hist->GetBinCenter(i);
    y = hist->GetBinContent(i);
    gr->SetPoint(i - 1, x, y);
  }
  return gr;
}

QT_END_NAMESPACE
