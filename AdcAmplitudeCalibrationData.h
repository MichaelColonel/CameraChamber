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

#pragma once

#include "typedefs.h"

class AdcAmplitudeCalibrationData {
public:
  AdcAmplitudeCalibrationData(AdcAmplitudeCalibrationMap& data);
  virtual ~AdcAmplitudeCalibrationData();
  bool checkCapacityCalibrationIsPresent(int chip, int capacityCode) const;
  bool checkCapacityTimeCalibrationIsPresent(int chip, int capacityCode,
    int integrationTimeCode, bool& integrationTimeIsPresent) const;
private:
  AdcAmplitudeCalibrationMap& adcAmpCalibrationMap;
};

AdcAmplitudeCalibrationData::AdcAmplitudeCalibrationData(AdcAmplitudeCalibrationMap &data)
  :
  adcAmpCalibrationMap(data)
{
}

AdcAmplitudeCalibrationData::~AdcAmplitudeCalibrationData()
{
}

bool AdcAmplitudeCalibrationData::checkCapacityCalibrationIsPresent(int chip, int capacityCode) const
{
  bool result = false;
  bool capacityIsPresent = false;
  bool chipIsPresent = false;
  for (auto iter = adcAmpCalibrationMap.begin(); iter != adcAmpCalibrationMap.end(); ++iter)
  {
    CapacityIntegrationTimeCodesPair capTimeCodes = (*iter).first;
    struct LinearAmplitudeCalibration& adcAmpData = (*iter).second;
    ChipChannelCalibrationMap& adcLinA = adcAmpData.linearAdcCalibration.first;
//    ChipChannelCalibrationMap& adcLinB = adcAmpData.linearAdcCalibration.second;
//    ChipChannelCalibrationMap& amp = adcAmpData.uniformAmplitudeCalibration;
    capacityIsPresent = capTimeCodes.first == capacityCode;
    for (auto iterAdcLin = adcLinA.begin(); iterAdcLin != adcLinA.end(); ++iterAdcLin)
    {
      ChipChannelPair chipChannel = (*iterAdcLin).first;
      chipIsPresent = chipChannel.first == chip;
      if (chipIsPresent)
      {
        break;
      }
    }
    result == capacityIsPresent && chipIsPresent;
    if (result)
    {
      break;
    }
  }
  return result;
}

bool AdcAmplitudeCalibrationData::checkCapacityTimeCalibrationIsPresent(int chip, int capacityCode,
  int integrationTimeCode, bool& integrationTimeIsPresent) const
{
  bool result = false;
  bool capacityIsPresent = false;
  bool chipIsPresent = false;
  CapacityIntegrationTimeCodesPair capTimeCodesRequired(capacityCode, integrationTimeCode);

  for (auto iter = adcAmpCalibrationMap.begin(); iter != adcAmpCalibrationMap.end(); ++iter)
  {
    CapacityIntegrationTimeCodesPair capTimeCodes = (*iter).first;
    struct LinearAmplitudeCalibration& adcAmpData = (*iter).second;
    ChipChannelCalibrationMap& adcLinA = adcAmpData.linearAdcCalibration.first;
//    ChipChannelCalibrationMap& adcLinB = adcAmpData.linearAdcCalibration.second;
//    ChipChannelCalibrationMap& amp = adcAmpData.uniformAmplitudeCalibration;
    capacityIsPresent = capTimeCodes.first == capacityCode;
    integrationTimeIsPresent = capTimeCodesRequired == capTimeCodes;
    for (auto iterAdcLin = adcLinA.begin(); iterAdcLin != adcLinA.end(); ++iterAdcLin)
    {
      ChipChannelPair chipChannel = (*iterAdcLin).first;
      chipIsPresent = chipChannel.first == chip;
      if (chipIsPresent)
      {
        break;
      }
    }
    result == capacityIsPresent && chipIsPresent && integrationTimeIsPresent;
    if (result)
    {
      break;
    }
  }
  return result;
}
