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

#include "FullCamera4.h"

#include <QDebug>

#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>
#include <TPad.h>

#include <array>

QT_BEGIN_NAMESPACE

class FullCamera4Private
{
  Q_DECLARE_PUBLIC(FullCamera4);
protected:
  FullCamera4* const q_ptr;
public:
  FullCamera4Private(FullCamera4 &object);
  virtual ~FullCamera4Private();
};

FullCamera4Private::FullCamera4Private(FullCamera4 &object)
  :
  q_ptr(&object)
{
}

FullCamera4Private::~FullCamera4Private()
{
}

FullCamera4::FullCamera4(const AbstractCamera::CameraDeviceData& data, QObject *parent)
  :
  FullCamera(data, parent),
  d_ptr(new FullCamera4Private(*this))
{
  Q_D(FullCamera4);
}

FullCamera4::~FullCamera4()
{

}

void FullCamera4::processDataCounts(bool fullChipCalibration, bool splitData,
  IntegratorType integType, ProfileRepresentationType profileType)
{
  Q_UNUSED(fullChipCalibration);
  FullCamera::processDataCounts(true, splitData, integType, profileType);
}

void FullCamera4::updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors)
{
  Q_D(FullCamera4);
  TGraphErrors* vertProf = nullptr;
  TGraphErrors* horizProf = nullptr;
  if (withErrors)
  {
    vertProf = dynamic_cast< TGraphErrors* >(vertProfile);
    horizProf = dynamic_cast< TGraphErrors* >(horizProfile);
  }
  withErrors = (vertProf && horizProf);

  const std::vector< double >& vertProfStrips = this->getVerticalProfileStripsNumbers();
  const std::vector< double >& horizProfStrips = this->getHorizontalProfileStripsNumbers();
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  if (vertProfile)
  {
    double halfSizeVert = *std::max_element(vertProfStrips.begin(), vertProfStrips.end()) / 2.;
    std::vector< double > vertProfDataCopy(vertProfData);
    std::reverse(vertProfDataCopy.begin(), vertProfDataCopy.end());

    for (Int_t i = 0; i < vertProfData.size(); ++i)
    {
      vertProfile->SetPoint(i, Double_t(vertProfStrips[i] - halfSizeVert), Double_t(vertProfDataCopy[i]));
    }
  }

  if (horizProf)
  {
    double halfSizeHoriz = *std::max_element(horizProfStrips.begin(), horizProfStrips.end()) / 2.;
    std::vector< double > horizProfDataCopy(horizProfData);
    std::reverse(horizProfDataCopy.begin(), horizProfDataCopy.end());
    for (Int_t i = 0; i < horizProfData.size(); ++i)
    {
      horizProfile->SetPoint(i, Double_t(horizProfStrips[i] - halfSizeHoriz), Double_t(horizProfDataCopy[i]));
    }
  }
}

TH2* FullCamera4::createProfile2D(bool integral)
{
  Q_D(FullCamera4);

  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  int xBins = static_cast< int >(horizProfData.size());
  std::vector< double > horiz = AbstractCamera::GenerateFullProfileStripsBinsBorders(xBins + 1);

  double halfSizeHoriz = *std::max_element(horiz.begin(), horiz.end()) / 2.;
  auto shiftHoriz = [halfSizeHoriz](double& v) { v -= halfSizeHoriz; };
  std::for_each(horiz.begin(), horiz.end(), shiftHoriz);

  const double* xBinsBorders = horiz.data();

  int yBins = static_cast< int >(vertProfData.size());
  std::vector< double > vert = AbstractCamera::GenerateFullProfileStripsBinsBorders(yBins + 1);

  double halfSizeVert = *std::max_element(vert.begin(), vert.end()) / 2.;
  auto shiftVert = [halfSizeVert](double& v) { v -= halfSizeVert; };
  std::for_each(vert.begin(), vert.end(), shiftVert);

  const double* yBinsBorders = vert.data();

  AbstractCamera::CameraDeviceData cameraData = this->getCameraData();
  QString hist2Name = QString("h2_") + cameraData.ID;
  QString hist2IntegName = QString("hi2_") + cameraData.ID;

  QString histName = (integral) ? hist2IntegName : hist2Name;
  QByteArray name = histName.toLatin1();
  TH2* hist = new TH2D(name.data(), "Pseudo 2D Distribution", xBins, xBinsBorders, yBins, yBinsBorders);
  return hist;
}

void FullCamera4::updateProfiles2D(TH2* pseudo2D, TH2* integPseudo2D)
{
  Q_D(FullCamera4);

  if (!pseudo2D && !integPseudo2D)
  {
    return;
  }
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();
  if (pseudo2D)
  {
    pseudo2D->Reset();
    for (Int_t row = 0; row < vertProfData.size(); ++row)
    {
      Double_t centerY = pseudo2D->GetYaxis()->GetBinCenter(row + 1);
      for (Int_t column = 0; column < horizProfData.size(); ++column)
      {
        Double_t centerX = pseudo2D->GetXaxis()->GetBinCenter(column + 1);
        Double_t pixel = horizProfData[column] * vertProfData[row];
        pseudo2D->Fill(centerX, centerY, pixel);
      }
    }
  }
  if (integPseudo2D && pseudo2D)
  {
    integPseudo2D->Add(pseudo2D, 1.);
//    for (Int_t row = 0; row < vertProfData.size(); ++row)
//    {
//      for (Int_t column = 0; column < horizProfData.size(); ++column)
//      {
//        Double_t pixel = horizProfData[column] * vertProfData[row];
//        integPseudo2D->Fill(column, (horizProfData.size() - 1) - row, pixel);
//      }
//    }
  }
}

QT_END_NAMESPACE
