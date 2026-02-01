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

#include "Camera2.h"

#include <QDebug>

#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>

#include <array>

QT_BEGIN_NAMESPACE

class Camera2Private
{
  Q_DECLARE_PUBLIC(Camera2);
protected:
  Camera2* const q_ptr;
public:
  Camera2Private(Camera2 &object);
  virtual ~Camera2Private();
  std::vector< int > getProfileBrokenChipChannelsStrips(CameraProfileType type, bool splitData = false) const;
};

Camera2Private::Camera2Private(Camera2 &object)
  :
  q_ptr(&object)
{
}

Camera2Private::~Camera2Private()
{
}


std::vector< int > Camera2Private::getProfileBrokenChipChannelsStrips(CameraProfileType type, bool splitData) const
{
  Q_Q(const Camera2);

  std::vector< int > profPos;

  constexpr int VSIZE = Camera2::VERTICAL_PROFILE_STRIPS;
  constexpr int HSIZE = Camera2::HORIZONTAL_PROFILE_STRIPS;

  std::array< ChipChannelPair, VSIZE > VerticalProfileChipChannels; // for mixed channels between opposite chips
  std::array< ChipChannelPair, HSIZE > HorizontalProfileChipChannels; // for mixed channels between opposite chips

  auto formChipStripV = [](const std::vector< ChipChannelPair >& chipStripMask,
    std::array< ChipChannelPair, VSIZE >& formStripMask) {
    const size_t half = chipStripMask.size() / 2;
    for (size_t i = 0, j = 0; j < chipStripMask.size(); )
    {
      formStripMask[j++] = chipStripMask[half + i];
      formStripMask[j++] = chipStripMask[i];
      i++;
    }
  };
  auto formChipStripH = [](const std::vector< ChipChannelPair >& chipStripMask,
    std::array< ChipChannelPair, HSIZE >& formStripMask) {
    std::array< ChipChannelPair, CHANNELS_PER_CHIP > chip12;
    std::copy(chipStripMask.begin(), chipStripMask.end(), formStripMask.begin());

    // save chip12
    for (size_t i = CHANNELS_PER_CHIP * 2, j = 0; i < CHANNELS_PER_CHIP * 3; ++i)
    {
      chip12[j++] = chipStripMask[i];
    }
    // mix chip1, chip2
    for (size_t i = 0, j = 0; i < CHANNELS_PER_CHIP; i++)
    {
      ChipChannelPair chip2 = chipStripMask[CHANNELS_PER_CHIP + i];
      ChipChannelPair chip1 = chipStripMask[CHANNELS_PER_CHIP * 3 + i];
      formStripMask[CHANNELS_PER_CHIP + j++] = chip1;
      formStripMask[CHANNELS_PER_CHIP + j++] = chip2;
    }
    // copy chip12 to the end
    for (size_t i = CHANNELS_PER_CHIP * 3, j = 0; i < chipStripMask.size(); ++i, ++j)
    {
      formStripMask[i] = chip12[j];
    }
  };

  const std::vector< ChipChannelPair >& vertChipChannels = q->getVerticalProfileChipChannelStrips();
  const std::vector< ChipChannelPair >& horizChipChannels = q->getHorizontalProfileChipChannelStrips();
  if (vertChipChannels.size() != VSIZE)
  {
    qWarning() << Q_FUNC_INFO << QObject::tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(VSIZE);
  }

  if (horizChipChannels.size() != HSIZE)
  {
    qWarning() << Q_FUNC_INFO << QObject::tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(HSIZE);
  }
  // Fill mixed channels array of strip positions between opposite chips

  formChipStripV(vertChipChannels, VerticalProfileChipChannels); // vertical profile
  formChipStripH(horizChipChannels, HorizontalProfileChipChannels); // horizontal profile

  std::map< ChipChannelPair, ChannelInfoPair > infoMap = q->getChipChannelInfoMap();
  const std::vector< int >& vertProfChips = q->getVerticalProfileChips();
  const std::vector< int >& horizProfChips = q->getHorizontalProfileChips();

  // sort, split info map data for corresponding profile chip and channel
  // to reconstruct vertical and horizontal profile

  for (const auto& chipChannelInfoPair : infoMap)
  {
    const ChipChannelPair& chipChannelPair = chipChannelInfoPair.first;

    auto findChipStripArrayV = [chipChannelPair](const std::array< ChipChannelPair, VSIZE >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };
    auto findChipStripVectorV = [chipChannelPair](const std::vector< ChipChannelPair >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    auto findChipStripArrayH = [chipChannelPair](const std::array< ChipChannelPair, HSIZE >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    auto findChipStripVectorH = [chipChannelPair](const std::vector< ChipChannelPair >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    // valid value from 0...VSIZE-1 for vert. profile
    // valid value from 0...HSIZE-1 for horiz. profile
    int chipStrip = -1;
    if (std::find(std::begin(vertProfChips), std::end(vertProfChips), chipChannelPair.first) != vertProfChips.end())
    { // Horizontal strips (Vertical profile)
      if (!splitData)
      {
        chipStrip = findChipStripArrayV(VerticalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipStripVectorV(vertChipChannels);
      }

      if (chipStrip >= 0 && chipStrip < VSIZE)
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
        chipStrip = findChipStripArrayH(HorizontalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipStripVectorH(horizChipChannels);
      }

      if (chipStrip >= 0 && chipStrip < HSIZE)
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

Camera2::Camera2(const AbstractCamera::CameraDeviceData& data, QObject *parent)
  :
  AbstractCamera(data, parent),
  d_ptr(new Camera2Private(*this))
{
  Q_D(Camera2);
//  ChipChannelPair& refAdcV = this->getReferenceAdcVerticalChipChannel();
//  ChipChannelPair& refAdcH = this->getReferenceAdcHorizontalChipChannel();
//  ChipChannelPair& refAmpV = this->getReferenceAmplitudeVerticalChipChannel();
//  ChipChannelPair& refAmpH = this->getReferenceAmplitudeHorizontalChipChannel();

//  refAdcV = std::make_pair(3, 15); // vert prof
//  refAdcH = std::make_pair(1, 15); // horiz prof
//  refAmpV = std::make_pair(3, 15); // vert prof
//  refAmpH = std::make_pair(1, 15); // horiz prof

  std::vector< double > vertProf = this->getVerticalProfile();
  std::vector< double > horizProf = this->getHorizontalProfile();
  if (vertProf.size() != VERTICAL_PROFILE_STRIPS && horizProf.size() != HORIZONTAL_PROFILE_STRIPS)
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of strips in profiles");
    vertProf.resize(VERTICAL_PROFILE_STRIPS);
    horizProf.resize(HORIZONTAL_PROFILE_STRIPS);
  }

  std::fill(std::begin(vertProf), std::end(vertProf), 0.);
  std::fill(std::begin(horizProf), std::end(horizProf), 0.);
}

Camera2::~Camera2()
{

}

void Camera2::processDataCounts(bool splitData,
  IntegratorType integType, ProfileRepresentationType profileType)
{
  Q_D(Camera2);
  AbstractCamera::processDataCounts(splitData, integType, profileType);

  std::array< ChipChannelPair, VERTICAL_PROFILE_STRIPS > VerticalProfileChipChannels; // for mixed channels between opposite chips
  std::array< ChipChannelPair, HORIZONTAL_PROFILE_STRIPS > HorizontalProfileChipChannels; // for mixed channels between opposite chips

  auto formChipStripV = [](const std::vector< ChipChannelPair >& chipStripMask,
    std::array< ChipChannelPair, VERTICAL_PROFILE_STRIPS >& formStripMask) {
    const size_t half = chipStripMask.size() / 2;
    for (size_t i = 0, j = 0; j < chipStripMask.size(); )
    {
      formStripMask[j++] = chipStripMask[half + i];
      formStripMask[j++] = chipStripMask[i];
      i++;
    }
  };
  auto formChipStripH = [](const std::vector< ChipChannelPair >& chipStripMask,
    std::array< ChipChannelPair, HORIZONTAL_PROFILE_STRIPS >& formStripMask) {
    std::array< ChipChannelPair, CHANNELS_PER_CHIP > chip12;
    std::copy(chipStripMask.begin(), chipStripMask.end(), formStripMask.begin());

    // save chip12
    for (size_t i = CHANNELS_PER_CHIP * 2, j = 0; i < CHANNELS_PER_CHIP * 3; ++i)
    {
      chip12[j++] = chipStripMask[i];
    }
    // mix chip1, chip2
    for (size_t i = 0, j = 0; i < CHANNELS_PER_CHIP; i++)
    {
      ChipChannelPair chip2 = chipStripMask[CHANNELS_PER_CHIP + i];
      ChipChannelPair chip1 = chipStripMask[CHANNELS_PER_CHIP * 3 + i];
      formStripMask[CHANNELS_PER_CHIP + j++] = chip1;
      formStripMask[CHANNELS_PER_CHIP + j++] = chip2;
    }
    // copy chip12 to the end
    for (size_t i = CHANNELS_PER_CHIP * 3, j = 0; i < chipStripMask.size(); ++i, ++j)
    {
      formStripMask[i] = chip12[j];
    }
  };

  const std::vector< ChipChannelPair >& vertChipChannels = this->getVerticalProfileChipChannelStrips();
  const std::vector< ChipChannelPair >& horizChipChannels = this->getHorizontalProfileChipChannelStrips();
  if (vertChipChannels.size() != VERTICAL_PROFILE_STRIPS)
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(VERTICAL_PROFILE_STRIPS);
  }

  if (horizChipChannels.size() != HORIZONTAL_PROFILE_STRIPS)
  {
    qWarning() << Q_FUNC_INFO << tr(": Wrong number of strips for vertical profile.\n Enabled: %1, acquired: %2").arg(vertChipChannels.size()).arg(HORIZONTAL_PROFILE_STRIPS);
  }
  // Fill mixed channels array of strip positions between opposite chips

  formChipStripV(vertChipChannels, VerticalProfileChipChannels); // vertical profile
  formChipStripH(horizChipChannels, HorizontalProfileChipChannels); // horizontal profile

  double IntegralAdcSignalToCharge = AbstractCamera::CAPACITY_RANGE[this->getCapasityCode()];
  if (this->getCapasityCode() == ADC_16_BIT)
  {
    IntegralAdcSignalToCharge /= double(AbstractCamera::RESOLUTION_16BIT);
  }
  else if (this->getCapasityCode() == ADC_20_BIT)
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
//    const ChannelInfo& info = channelInfoPair.first;

    auto findChipStripArrayV = [chipChannelPair](const std::array< ChipChannelPair, VERTICAL_PROFILE_STRIPS >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };
    auto findChipStripVectorV = [chipChannelPair](const std::vector< ChipChannelPair >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    auto findChipStripArrayH = [chipChannelPair](const std::array< ChipChannelPair, HORIZONTAL_PROFILE_STRIPS >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    auto findChipStripVectorH = [chipChannelPair](const std::vector< ChipChannelPair >& chipStripMask) -> int {
      auto iterPos = std::find(chipStripMask.begin(), chipStripMask.end(), chipChannelPair);
      if (iterPos != chipStripMask.end())
      {
        return static_cast< int >(iterPos - chipStripMask.begin());
      }
      return -1;
    };

    // valid value from 0...VERTICAL_PROFILE_STRIPS-1 for vert. profile
    // valid value from 0...HORIZONTAL_PROFILE_STRIPS-1 for horiz. profile
    int chipStrip = -1;
    if (std::find(std::begin(vertProfChips), std::end(vertProfChips), chipChannelPair.first) != vertProfChips.end())
    { // Horizontal strips (Vertical profile)
      if (!splitData)
      {
        chipStrip = findChipStripArrayV(VerticalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipStripVectorV(vertChipChannels);
      }
      // fill vertical profile
      double v = calibInfo.Signal;
      if (chipStrip >= 0 && chipStrip < VERTICAL_PROFILE_STRIPS)
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
        chipStrip = findChipStripArrayH(HorizontalProfileChipChannels);
      }
      else
      {
        chipStrip = findChipStripVectorH(horizChipChannels);
      }
      // fill horizontal profile
      double v = calibInfo.Signal;
      if (chipStrip >= 0 && chipStrip < HORIZONTAL_PROFILE_STRIPS)
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
    if (pos >= 0 && pos < VERTICAL_PROFILE_STRIPS)
    {
      vertProf[pos] = (vertProf[pos - 1] + vertProf[pos + 1]) / 2.;
    }
  }
  broken = d->getProfileBrokenChipChannelsStrips(CameraProfileType::PROFILE_HORIZONTAL, splitData);
  for (int pos : broken)
  {
    if (pos >= 0 && pos < HORIZONTAL_PROFILE_STRIPS)
    {
      horizProf[pos] = (horizProf[pos - 1] + horizProf[pos + 1]) / 2.;
    }
  }
}

TGraph* Camera2::createProfile(CameraProfileType profileType, bool withErrors)
{
  Q_D(Camera2);

  size_t vertProfSize = this->getVerticalProfile().size();
  size_t horizProfSize = this->getHorizontalProfile().size();

  TGraph* profile = nullptr;
  switch (profileType)
  {
  case CameraProfileType::PROFILE_VERTICAL:
    profile = (withErrors) ? new TGraphErrors(vertProfSize) : new TGraph(vertProfSize);
    this->updateProfiles(profile, nullptr, withErrors);
    break;
  case CameraProfileType::PROFILE_HORIZONTAL:
    profile = (withErrors) ? new TGraphErrors(horizProfSize) : new TGraph(horizProfSize);
    this->updateProfiles(nullptr, profile, withErrors);
    break;
  default:
    break;
  }
  return profile;
}

void Camera2::updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors)
{
  Q_D(Camera2);
  TGraphErrors* vertProf = nullptr;
  TGraphErrors* horizProf = nullptr;
  if (withErrors)
  {
    vertProf = dynamic_cast< TGraphErrors* >(vertProfile);
    horizProf = dynamic_cast< TGraphErrors* >(horizProfile);
  }
  withErrors = (vertProf && horizProf);

  const std::vector< double >& vertProfStrips = this->getVerticalProfileStripsNumbers();
//  const std::vector< double >& horizProfStrips = this->getHorizontalProfileStripsNumbers();
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  if (vertProfile)
  {
    for (Int_t i = 0; i < vertProfData.size(); ++i)
    {
      vertProfile->SetPoint(i, Double_t(vertProfStrips[i]), Double_t(vertProfData[i]));
    }
  }
  if (horizProfile)
  {
    size_t xBins = horizProfData.size();
    std::vector< double > horiz = Camera2::GenerateHorizontalProfileStripsBinsBorders(xBins + 1);
    const double* xBinsBorders = horiz.data();
    constexpr double step2 = STRIP_STEP_PER_SIDE_MM;
    constexpr double step1 = step2 / 2.;
    for (Int_t i = 0; i < xBins; ++i)
    {
      horizProfile->SetPoint(i, xBinsBorders[i] + step1, Double_t(horizProfData[i]));
    }
  }
}

void Camera2::updateProfiles2D(TH2* pseudo2D, TH2* integPseudo2D)
{
  Q_D(Camera2);
  if (!pseudo2D && !integPseudo2D)
  {
    return;
  }
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  size_t xBins = horizProfData.size();
  std::vector< double > horiz = Camera2::GenerateHorizontalProfileStripsBinsBorders(xBins + 1);
  const double* xBinsBorders = horiz.data();

  if (pseudo2D)
  {
    pseudo2D->Reset();

    for (Int_t row = 0; row < vertProfData.size(); ++row)
    {
      for (Int_t column = 0; column < xBins; ++column)
      {
        Double_t pixel = horizProfData[column] * vertProfData[row];
        pseudo2D->Fill(xBinsBorders[column], (vertProfData.size() - 1) - row, pixel);
      }
    }
  }
  if (integPseudo2D)
  {
    for (Int_t row = 0; row < vertProfData.size(); ++row)
    {
      for (Int_t column = 0; column < xBins; ++column)
      {
        Double_t pixel = horizProfData[column] * vertProfData[row];
        integPseudo2D->Fill(xBinsBorders[column], (vertProfData.size() - 1) - row, pixel);
      }
    }
  }
}

TH2* Camera2::createProfile2D(bool integral)
{
  Q_D(Camera2);
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  size_t xBins = horizProfData.size();
  std::vector< double > horiz = Camera2::GenerateHorizontalProfileStripsBinsBorders(xBins + 1);
  const double* xBinsBorders = horiz.data();

  size_t yBins = vertProfData.size();
  std::vector< double > vert = AbstractCamera::GenerateFullProfileStripsBinsBorders(yBins + 1);
  const double* yBinsBorders = vert.data();

  AbstractCamera::CameraDeviceData cameraData = this->getCameraData();
  QString hist2Name = QString("h2_") + cameraData.ID;
  QString hist2IntegName = QString("hi2_") + cameraData.ID;

  QString histName = (integral) ? hist2IntegName : hist2Name;
  QByteArray name = histName.toLatin1();
  TH2* hist = new TH2D(name.data(), "Pseudo 2D Distribution", xBins, xBinsBorders, yBins, yBinsBorders);
  return hist;
}

std::vector< double > Camera2::GenerateHorizontalProfileStripsBinsBorders(size_t n)
{
  std::vector< double > v(n, 0.);
  v[0] = 0.0;
  constexpr double step2 = STRIP_STEP_PER_SIDE_MM;
  constexpr double step1 = step2 / 2.;
  for (size_t i = 1; i < CHANNELS_PER_CHIP; ++i)
  {
    v[i] = v[i - 1] + step2; // 2 mm step
  }
  for (size_t i = CHANNELS_PER_CHIP; i < 3 * CHANNELS_PER_CHIP; ++i)
  {
    v[i] = v[i - 1] + step1; // 1 mm step
  }
  for (size_t i = 3 * CHANNELS_PER_CHIP; i < v.size(); ++i)
  {
    v[i] = v[i - 1] + step2; // 2 mm step
  }
  return v;
}

QT_END_NAMESPACE
