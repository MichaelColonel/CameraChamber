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

#include "FullCamera.h"

#include <QDebug>

#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>
#include <TPad.h>

#include <array>

QT_BEGIN_NAMESPACE

class FullCameraPrivate
{
  Q_DECLARE_PUBLIC(FullCamera);
protected:
  FullCamera* const q_ptr;
public:
  FullCameraPrivate(FullCamera &object);
  virtual ~FullCameraPrivate();
  std::vector< int > getProfileBrokenChipChannelsStrips(CameraProfileType type, bool splitData = false) const;
};

FullCameraPrivate::FullCameraPrivate(FullCamera &object)
  :
  q_ptr(&object)
{
}

FullCameraPrivate::~FullCameraPrivate()
{
}

std::vector< int > FullCameraPrivate::getProfileBrokenChipChannelsStrips(CameraProfileType type, bool splitData) const
{
  Q_Q(const FullCamera);

  std::vector< int > profPos;

  constexpr int SIZE = FullCamera::PROFILE_STRIPS;
  std::array< ChipChannelPair, SIZE > VerticalProfileChipChannels; // for mixed channels between opposite chips
  std::array< ChipChannelPair, SIZE > HorizontalProfileChipChannels; // for mixed channels between opposite chips

  auto formChipStrip = [](const std::vector< ChipChannelPair >& chipStripMask,
    std::array< ChipChannelPair, SIZE >& formStripMask)
  {
    if (chipStripMask.size() != formStripMask.size())
    {
      return;
    }
    const size_t half = chipStripMask.size() / 2;
    for (size_t i = 0, j = 0; j < chipStripMask.size(); )
    {
      formStripMask[j++] = chipStripMask[half + i];
      formStripMask[j++] = chipStripMask[i];
      i++;
    }
  };

  const std::vector< ChipChannelPair >& vertChipChannels = q->getVerticalProfileChipChannelStrips();
  const std::vector< ChipChannelPair >& horizChipChannels = q->getHorizontalProfileChipChannelStrips();
  if (vertChipChannels.size() != SIZE)
  {
    qWarning() << Q_FUNC_INFO << QObject::tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(SIZE);
  }

  if (horizChipChannels.size() != SIZE)
  {
    qWarning() << Q_FUNC_INFO << QObject::tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(SIZE);
  }
  // Fill mixed channels array of strip positions between opposite chips
  formChipStrip(vertChipChannels, VerticalProfileChipChannels); // vertical profile
  formChipStrip(horizChipChannels, HorizontalProfileChipChannels); // horizontal profile

  std::map< ChipChannelPair, ChannelInfoPair > infoMap = q->getChipChannelInfoMap();
  const std::vector< int >& vertProfChips = q->getVerticalProfileChips();
  const std::vector< int >& horizProfChips = q->getHorizontalProfileChips();

  // sort, split info map data for corresponding profile chip and channel
  // to reconstruct vertical and horizontal profile
  for (const auto& chipChannelInfoPair : infoMap)
  {
    const ChipChannelPair& chipChannelPair = chipChannelInfoPair.first;
    int chipStrip = -1; // valid value from 0...PROFILE_STRIPS-1
    auto findChipChannelArray = [chipChannelPair](const std::array< ChipChannelPair, SIZE >& chipStripMask) -> int
    {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    auto findChipChannelVector = [chipChannelPair](const std::vector< ChipChannelPair >& chipStripMask) -> int
    {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    if (std::find(std::begin(vertProfChips), std::end(vertProfChips), chipChannelPair.first) != vertProfChips.end())
    { // Horizontal strips (Vertical profile)
      if (!splitData)
      {
        chipStrip = findChipChannelArray(VerticalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipChannelVector(vertChipChannels);
      }
      // fill vertical profile
      if (chipStrip >= 0 && chipStrip < SIZE)
      {
        std::vector< int > brokenChannelsStrips;
        CameraProfileType prof = q->getProfileBrokenChipChannelsStrips(chipChannelPair.first, brokenChannelsStrips);
        if (prof == type && prof == CameraProfileType::PROFILE_VERTICAL)
        {
          for (int broken : brokenChannelsStrips)
          {
            if (chipChannelPair.second == broken)
            {
              profPos.push_back(chipStrip);
            }
          }
        }
      }
    }
    else if (std::find(std::begin(horizProfChips), std::end(horizProfChips), chipChannelPair.first) != horizProfChips.end())
    { // Vertical strips (Horizontal profile)
      if (!splitData)
      {
        chipStrip = findChipChannelArray(HorizontalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipChannelVector(horizChipChannels);
      }
      if (chipStrip >= 0 && chipStrip < SIZE)
      {
        std::vector< int > brokenChannelsStrips;
        CameraProfileType prof = q->getProfileBrokenChipChannelsStrips(chipChannelPair.first, brokenChannelsStrips);
        if (prof == type && prof == CameraProfileType::PROFILE_HORIZONTAL)
        {
          for (int broken : brokenChannelsStrips)
          {
            if (chipChannelPair.second == broken)
            {
              profPos.push_back(chipStrip);
            }
          }
        }
      }
    }
  }
  return profPos;
}

FullCamera::FullCamera(const AbstractCamera::CameraDeviceData& data, QObject *parent)
  :
  AbstractCamera(data, parent),
  d_ptr(new FullCameraPrivate(*this))
{
  Q_D(FullCamera);
//  ChipChannelPair& refAdcV = this->getReferenceAdcVerticalChipChannel();
//  ChipChannelPair& refAdcH = this->getReferenceAdcHorizontalChipChannel();
//  ChipChannelPair& refAmpV = this->getReferenceAmplitudeVerticalChipChannel();
//  ChipChannelPair& refAmpH = this->getReferenceAmplitudeHorizontalChipChannel();

//  refAdcV = std::make_pair(2, 15); // vert prof
//  refAdcH = std::make_pair(1, 15); // horiz prof
//  refAmpV = std::make_pair(2, 15); // vert prof
//  refAmpH = std::make_pair(1, 15); // horiz prof

  std::vector< double > vertProf = this->getVerticalProfile();
  std::vector< double > horizProf = this->getHorizontalProfile();
  if (vertProf.size() != PROFILE_STRIPS && horizProf.size() != PROFILE_STRIPS)
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of strips in profiles");
    vertProf.resize(PROFILE_STRIPS);
    horizProf.resize(PROFILE_STRIPS);
  }
  std::fill(std::begin(vertProf), std::end(vertProf), 0.);
  std::fill(std::begin(horizProf), std::end(horizProf), 0.);
}

FullCamera::~FullCamera()
{

}

void FullCamera::processDataCounts(bool fullChipCalibration, bool splitData,
  IntegratorType integType, ProfileRepresentationType profileType)
{
  Q_D(FullCamera);
  AbstractCamera::processDataCounts(fullChipCalibration, splitData, integType, profileType);

  std::array< ChipChannelPair, PROFILE_STRIPS > VerticalProfileChipChannels; // for mixed channels between opposite chips
  std::array< ChipChannelPair, PROFILE_STRIPS > HorizontalProfileChipChannels; // for mixed channels between opposite chips

  auto formChipStrip = [](const std::vector< ChipChannelPair >& chipStripMask,
    std::array< ChipChannelPair, PROFILE_STRIPS >& formStripMask)
  {
    if (chipStripMask.size() != formStripMask.size())
    {
      return;
    }
    const size_t half = chipStripMask.size() / 2;
    for (size_t i = 0, j = 0; j < chipStripMask.size(); )
    {
      formStripMask[j++] = chipStripMask[half + i];
      formStripMask[j++] = chipStripMask[i];
      i++;
    }
  };

  const std::vector< ChipChannelPair >& vertChipChannels = this->getVerticalProfileChipChannelStrips();
  const std::vector< ChipChannelPair >& horizChipChannels = this->getHorizontalProfileChipChannelStrips();
  if (vertChipChannels.size() != PROFILE_STRIPS)
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(PROFILE_STRIPS);
  }

  if (horizChipChannels.size() != PROFILE_STRIPS)
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(PROFILE_STRIPS);
  }
  // Fill mixed channels array of strip positions between opposite chips
  formChipStrip(vertChipChannels, VerticalProfileChipChannels); // vertical profile
  formChipStrip(horizChipChannels, HorizontalProfileChipChannels); // horizontal profile

  double IntegralAdcSignalToCharge = AbstractCamera::CAPACITY_RANGE[this->getCapacityCode()];
  if (this->getAdcResolutionMode() == ADC_16_BIT)
  {
    IntegralAdcSignalToCharge /= double(AbstractCamera::RESOLUTION_16BIT);
  }
  else if (this->getAdcResolutionMode() == ADC_20_BIT)
  {
    IntegralAdcSignalToCharge /= double(AbstractCamera::RESOLUTION_20BIT);
  }
  else
  {
    IntegralAdcSignalToCharge /= double(AbstractCamera::RESOLUTION_16BIT);
  }
  std::map< ChipChannelPair, ChannelInfoPair > infoMap = this->getChipChannelInfoMap();
  const std::vector< int >& vertProfChips = this->getVerticalProfileChips();
  const std::vector< int >& horizProfChips = this->getHorizontalProfileChips();

  // sort, split info map data for corresponding profile chip and channel
  // to reconstruct vertical and horizontal profile
  std::vector< double >& vertProf = this->getVerticalProfile();
  std::vector< double >& horizProf = this->getHorizontalProfile();
  for (const auto& chipChannelInfoPair : infoMap)
  {
    const ChipChannelPair& chipChannelPair = chipChannelInfoPair.first;
    const ChannelInfoPair& channelInfoPair = chipChannelInfoPair.second;
    const ChannelInfo& calibInfo = channelInfoPair.second;
///    const ChannelInfo& info = channelInfoPair.first;
    int chipStrip = -1; // valid value from 0...PROFILE_STRIPS-1
    auto findChipChannelArray = [chipChannelPair](const std::array< ChipChannelPair, PROFILE_STRIPS >& chipStripMask) -> int
    {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    auto findChipChannelVector = [chipChannelPair](const std::vector< ChipChannelPair >& chipStripMask) -> int
    {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    if (std::find(std::begin(vertProfChips), std::end(vertProfChips), chipChannelPair.first) != vertProfChips.end())
    { // Horizontal strips (Vertical profile)
      if (!splitData)
      {
        chipStrip = findChipChannelArray(VerticalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipChannelVector(vertChipChannels);
      }
      // fill vertical profile
      double v = calibInfo.Signal;
      if (chipStrip >= 0 && chipStrip < PROFILE_STRIPS)
      {
        switch (profileType)
        {
        case ProfileRepresentationType::MEAN:
          v = 0.5 * calibInfo.Signal / (calibInfo.SigCountA + calibInfo.SigCountB);
//          v = 0.5 * (info.PedMeanA + info.PedMeanB);
          break;
        case ProfileRepresentationType::CHARGE:
          v = IntegralAdcSignalToCharge * calibInfo.Signal;
//          v = 0.5 * (info.PedMeanA + info.PedMeanB);
          break;
        case ProfileRepresentationType::INTEGRAL:
        default:
          v = calibInfo.Signal;
//          v = 0.5 * (info.PedMeanA + info.PedMeanB);
          break;
        }
        vertProf[chipStrip] = v;
      }
    }
    else if (std::find(std::begin(horizProfChips), std::end(horizProfChips), chipChannelPair.first) != horizProfChips.end())
    { // Vertical strips (Horizontal profile)
      if (!splitData)
      {
        chipStrip = findChipChannelArray(HorizontalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipChannelVector(horizChipChannels);
      }
      // fill horizontal profile
      double v = calibInfo.Signal;
      if (chipStrip >= 0 && chipStrip < PROFILE_STRIPS)
      {
        switch (profileType)
        {
        case ProfileRepresentationType::MEAN:
          v = 0.5 * calibInfo.Signal / (calibInfo.SigCountA + calibInfo.SigCountB);
//          v = 0.5 * (info.PedMeanA + info.PedMeanB);
          break;
        case ProfileRepresentationType::CHARGE:
          v = IntegralAdcSignalToCharge * calibInfo.Signal;
//          v = 0.5 * (info.PedMeanA + info.PedMeanB);
          break;
        case ProfileRepresentationType::INTEGRAL:
        default:
          v = calibInfo.Signal;
//          v = 0.5 * (info.PedMeanA + info.PedMeanB);
          break;
        }
        horizProf[chipStrip] = v;
      }
    }
  }

  // fix broken chip channels or strips
  std::vector< int > broken = d->getProfileBrokenChipChannelsStrips(CameraProfileType::PROFILE_VERTICAL, splitData);
  for (int pos : broken)
  {
    if (pos >= 0 && pos < PROFILE_STRIPS)
    {
      vertProf[pos] = (vertProf[pos - 1] + vertProf[pos + 1]) / 2.;
    }
  }
  broken = d->getProfileBrokenChipChannelsStrips(CameraProfileType::PROFILE_HORIZONTAL, splitData);
  for (int pos : broken)
  {
    if (pos >= 0 && pos < PROFILE_STRIPS)
    {
      horizProf[pos] = (horizProf[pos - 1] + horizProf[pos + 1]) / 2.;
    }
  }
}

void FullCamera::updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors)
{
  Q_D(FullCamera);
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
    if (this->getCameraData().ID == "Camera4")
    {
      std::reverse(vertProfDataCopy.begin(), vertProfDataCopy.end());
    }
    for (Int_t i = 0; i < vertProfData.size(); ++i)
    {
      vertProfile->SetPoint(i, Double_t(vertProfStrips[i] - halfSizeVert), Double_t(vertProfDataCopy[i]));
    }
  }

  if (horizProf)
  {
    double halfSizeHoriz = *std::max_element(horizProfStrips.begin(), horizProfStrips.end()) / 2.;
    std::vector< double > horizProfDataCopy(horizProfData);
    if (this->getCameraData().ID == "Camera4")
    {
      std::reverse(horizProfDataCopy.begin(), horizProfDataCopy.end());
    }
    for (Int_t i = 0; i < horizProfData.size(); ++i)
    {
      horizProfile->SetPoint(i, Double_t(horizProfStrips[i] - halfSizeHoriz), Double_t(horizProfDataCopy[i]));
    }
  }
}

TH2* FullCamera::createProfile2D(bool integral)
{
  Q_D(FullCamera);

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

void FullCamera::updateProfiles2D(TH2* pseudo2D, TH2* integPseudo2D)
{
  Q_D(FullCamera);

  if (!pseudo2D && !integPseudo2D)
  {
    return;
  }
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  std::vector< double > vertProfDataCopy(vertProfData);
  std::reverse(vertProfDataCopy.begin(), vertProfDataCopy.end());
  if (this->getCameraData().ID == "Camera4")
  {
    std::reverse(vertProfDataCopy.begin(), vertProfDataCopy.end());
  }
  if (pseudo2D)
  {
    pseudo2D->Reset();
    for (Int_t row = 0; row < vertProfData.size(); ++row)
    {
      Double_t centerY = pseudo2D->GetYaxis()->GetBinCenter(row + 1);
      for (Int_t column = 0; column < horizProfData.size(); ++column)
      {
        Double_t centerX = pseudo2D->GetXaxis()->GetBinCenter(column + 1);
        Double_t pixel = horizProfData[column] * vertProfDataCopy[row];
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
