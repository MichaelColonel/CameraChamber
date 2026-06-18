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

#include "FullCamera6.h"

#include <QDebug>

#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>
#include <TPad.h>

#include <array>

QT_BEGIN_NAMESPACE

class FullCamera6Private
{
  Q_DECLARE_PUBLIC(FullCamera6);
protected:
  FullCamera6* const q_ptr;
public:
  FullCamera6Private(FullCamera6 &object);
  virtual ~FullCamera6Private();
};

FullCamera6Private::FullCamera6Private(FullCamera6 &object)
  :
  q_ptr(&object)
{
}

FullCamera6Private::~FullCamera6Private()
{
}

FullCamera6::FullCamera6(const AbstractCamera::CameraDeviceData& data, QObject *parent)
  :
  FullCamera4(data, parent),
  d_ptr(new FullCamera6Private(*this))
{
  Q_D(FullCamera6);
}

FullCamera6::~FullCamera6()
{

}

TGraph* FullCamera6::createProfile(CameraProfileType profileType, bool withErrors)
{
  Q_D(FullCamera6);

  const std::vector< double >& vertProfStrips = this->getVerticalProfileStripsNumbers();
  const std::vector< double >& horizProfStrips = this->getHorizontalProfileStripsNumbers();
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

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
      double halfSizeVert = *std::max_element(vertProfStrips.begin(), vertProfStrips.end()) / 2.;
      for (Int_t i = 0; i < vertProfData.size(); ++i)
      {
        profile->SetPoint(i, Double_t(vertProfStrips[i] - halfSizeVert), Double_t(vertProfData[i]));
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
      double halfSizeHoriz = *std::max_element(horizProfStrips.begin(), horizProfStrips.end()) / 2.;
      for (Int_t i = 0; i < horizProfData.size(); ++i)
      {
        profile->SetPoint(i, Double_t(horizProfStrips[i] - halfSizeHoriz), Double_t(horizProfData[i]));
      }
    }
    break;
  default:
    break;
  }
  return profile;
}

void FullCamera6::updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors)
{
  Q_D(FullCamera6);
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
      vertProfile->SetPoint(i, Double_t(vertProfDataCopy[i]), Double_t(vertProfStrips[i] - halfSizeVert));
///      vertProfile->SetPoint(i, Double_t(vertProfStrips[i] - halfSizeVert), Double_t(vertProfDataCopy[i]));
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

QT_END_NAMESPACE
