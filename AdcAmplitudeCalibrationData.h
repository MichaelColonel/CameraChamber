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

class ChipCapacityCalibrationData {
public:
  ChipCapacityCalibrationData(const ChipCapacityIntegrationTimeCalibrationMap& data)
    : chipCapacityCalibrationMap(data) {}
  virtual ~ChipCapacityCalibrationData() {}
  bool checkCapacityCalibrationIsPresent(int chip, int capacityCode) const;
  bool checkCapacityTimeCalibrationIsPresent(int chip, int capacityCode,
    int integrationTimeCode, bool& integrationTimeIsPresent) const;
  ChipChannelCalibrationMap getFirstAdcLinearCalibrationA() const;
  ChipChannelCalibrationMap getFirstAdcLinearCalibrationB() const;
  ChipChannelCalibrationMap getFirstAmpUniformCalibration() const;
  ChipChannelCalibrationMap getAdcLinearCalibrationA(int capacityCode, bool& capCodeIsFound) const;
  ChipChannelCalibrationMap getAdcLinearCalibrationB(int capacityCode, bool& capCodeIsFound) const;
  ChipChannelCalibrationMap getAmpUniformCalibration(int capacityCode, bool& capCodeIsFound) const;
  ChipChannelCalibrationMap getAdcLinearCalibrationA(int capacityCode, int timeCode,
    bool& capCodeIsFound, bool& timeCodeIsFound) const;
  ChipChannelCalibrationMap getAdcLinearCalibrationB(int capacityCode, int timeCode,
    bool& capCodeIsFound, bool& timeCodeIsFound) const;
  ChipChannelCalibrationMap getAmpUniformCalibration(int capacityCode, int timeCode,
    bool& capCodeIsFound, bool& timeCodeIsFound) const;
private:
  enum CalibrationDataType : int {
    INTEGRATOR_A,
    INTEGRATOR_B,
    SIGNAL_AMPLITUDE
  };
  ChipChannelCalibrationMap getCalibration(CalibrationDataType type) const;
  ChipChannelCalibrationMap getCalibration(int capacityCode, bool& capCodeIsFound, CalibrationDataType type) const;
  ChipChannelCalibrationMap getCalibration(int capacityCode, int timeCode,
    bool& capCodeIsFound, bool& timeCodeIsFound, CalibrationDataType type) const;

  const ChipCapacityIntegrationTimeCalibrationMap& chipCapacityCalibrationMap;
};

bool ChipCapacityCalibrationData::checkCapacityCalibrationIsPresent(int chip, int capacityCode) const
{
  bool result = false;
  bool capacityIsPresent = false;
  bool chipIsPresent = false;
  for (auto iter = chipCapacityCalibrationMap.begin(); iter != chipCapacityCalibrationMap.end(); ++iter)
  {
    int chipNumber = (*iter).first;
    const CapacityIntegrationTimeCalibrationMap& capTimeCalibrationMap = (*iter).second;
    for (auto it = capTimeCalibrationMap.begin(); it != capTimeCalibrationMap.end(); ++it)
    {
      CapacityIntegrationTimeCodesPair capTimePair = (*it).first;
      capacityIsPresent = capTimePair.first == capacityCode;
      if (capacityIsPresent)
      {
        break;
      }
    }
    chipIsPresent = chipNumber == chip;
    result == capacityIsPresent && chipIsPresent;
    if (result)
    {
      break;
    }
  }
  return result;
}

bool ChipCapacityCalibrationData::checkCapacityTimeCalibrationIsPresent(int chip, int capacityCode,
  int integrationTimeCode, bool& integrationTimeIsPresent) const
{
  bool result = false;
  bool capacityIsPresent = false;
  bool timeIsPresent = false;
  bool chipIsPresent = false;
  for (auto iter = chipCapacityCalibrationMap.begin(); iter != chipCapacityCalibrationMap.end(); ++iter)
  {
    int chipNumber = (*iter).first;
    const CapacityIntegrationTimeCalibrationMap& capTimeCalibrationMap = (*iter).second;
    for (auto it = capTimeCalibrationMap.begin(); it != capTimeCalibrationMap.end(); ++it)
    {
      CapacityIntegrationTimeCodesPair capTimePair = (*it).first;
      capacityIsPresent = capTimePair.first == capacityCode;
      timeIsPresent = capTimePair.second == integrationTimeCode;
      if (capacityIsPresent && timeIsPresent)
      {
        break;
      }
    }
    chipIsPresent = chipNumber == chip;
    result == capacityIsPresent && chipIsPresent && timeIsPresent;
    if (result)
    {
      break;
    }
  }
  integrationTimeIsPresent = timeIsPresent;
  return result;
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getCalibration(
  ChipCapacityCalibrationData::CalibrationDataType type) const
{
  ChipChannelCalibrationMap map;
  for (auto iter = chipCapacityCalibrationMap.begin(); iter != chipCapacityCalibrationMap.end(); ++iter)
  {
    int chipNumber = (*iter).first;
    const CapacityIntegrationTimeCalibrationMap& capTimeCalibrationMap = (*iter).second;
    auto it = capTimeCalibrationMap.begin();
    const struct ChipCalibrationData& calibData = (*it).second;
    auto getCalibrationVector = [calibData](ChipCapacityCalibrationData::CalibrationDataType type) -> const CalibrationVector&
    {
      switch (type)
      {
      case CalibrationDataType::INTEGRATOR_A:
        return calibData.linearAdcCalibrationA;
        break;
      case CalibrationDataType::INTEGRATOR_B:
        return calibData.linearAdcCalibrationB;
        break;
      case CalibrationDataType::SIGNAL_AMPLITUDE:
        return calibData.uniformAmplitudeCalibration;
        break;
      default:
        break;
      }
      return calibData.linearAdcCalibrationA;
    };
    const CalibrationVector& vec = getCalibrationVector(type);

    for (size_t i = 0; i < vec.size(); ++i)
    {
      ChipChannelPair chipChannel(chipNumber, i + 1);
      map.insert(std::make_pair(chipChannel, vec[i]));
    }
  }
  return map;
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getCalibration(int capacityCode, bool& capCodeIsFound,
  ChipCapacityCalibrationData::CalibrationDataType type) const
{
  ChipChannelCalibrationMap map;
  bool capacityIsPresent = false;
  for (auto iter = chipCapacityCalibrationMap.begin(); iter != chipCapacityCalibrationMap.end(); ++iter)
  {
    int chipNumber = (*iter).first;
    const CapacityIntegrationTimeCalibrationMap& capTimeCalibrationMap = (*iter).second;
    for (auto it = capTimeCalibrationMap.begin(); it != capTimeCalibrationMap.end(); ++it)
    {
      CapacityIntegrationTimeCodesPair capTimePair = (*it).first;
      const struct ChipCalibrationData& calibData = (*it).second;
      capacityIsPresent = capTimePair.first == capacityCode;

      auto getCalibrationVector = [calibData](ChipCapacityCalibrationData::CalibrationDataType type) -> const CalibrationVector&
      {
        switch (type)
        {
        case CalibrationDataType::INTEGRATOR_A:
          return calibData.linearAdcCalibrationA;
          break;
        case CalibrationDataType::INTEGRATOR_B:
          return calibData.linearAdcCalibrationB;
          break;
        case CalibrationDataType::SIGNAL_AMPLITUDE:
          return calibData.uniformAmplitudeCalibration;
          break;
        default:
          break;
        }
        return calibData.linearAdcCalibrationA;
      };
      const CalibrationVector& vec = getCalibrationVector(type);

      if (capacityIsPresent)
      {
        for (size_t i = 0; i < vec.size(); ++i)
        {
          ChipChannelPair chipChannel(chipNumber, i + 1);
          map.insert(std::make_pair(chipChannel, vec[i]));
        }
      }
    }
  }
  capCodeIsFound = capacityIsPresent;
  return map;
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getCalibration(int capacityCode, int timeCode,
  bool& capCodeIsFound, bool& timeCodeIsFound, ChipCapacityCalibrationData::CalibrationDataType type) const
{
  ChipChannelCalibrationMap map;
  bool capacityIsPresent = false;
  bool timeIsPresent = false;
  for (auto iter = chipCapacityCalibrationMap.begin(); iter != chipCapacityCalibrationMap.end(); ++iter)
  {
    int chipNumber = (*iter).first;
    const CapacityIntegrationTimeCalibrationMap& capTimeCalibrationMap = (*iter).second;
    for (auto it = capTimeCalibrationMap.begin(); it != capTimeCalibrationMap.end(); ++it)
    {
      CapacityIntegrationTimeCodesPair capTimePair = (*it).first;
      const struct ChipCalibrationData& calibData = (*it).second;

      capacityIsPresent = capTimePair.first == capacityCode;
      timeIsPresent = capTimePair.second == timeCode;
      auto getCalibrationVector = [calibData](ChipCapacityCalibrationData::CalibrationDataType type) -> const CalibrationVector&
      {
        switch (type)
        {
        case CalibrationDataType::INTEGRATOR_A:
          return calibData.linearAdcCalibrationA;
          break;
        case CalibrationDataType::INTEGRATOR_B:
          return calibData.linearAdcCalibrationB;
          break;
        case CalibrationDataType::SIGNAL_AMPLITUDE:
          return calibData.uniformAmplitudeCalibration;
          break;
        default:
          break;
        }
        return calibData.linearAdcCalibrationA;
      };
      const CalibrationVector& vec = getCalibrationVector(type);

      if (capacityIsPresent && timeIsPresent)
      {
        for (size_t i = 0; i < vec.size(); ++i)
        {
          ChipChannelPair chipChannel(chipNumber, i + 1);
          map.insert(std::make_pair(chipChannel, vec[i]));
        }
      }
    }
  }
  capCodeIsFound = capacityIsPresent;
  timeCodeIsFound = timeIsPresent;
  return map;
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getFirstAdcLinearCalibrationA() const
{
  return this->getCalibration(CalibrationDataType::INTEGRATOR_A);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getAdcLinearCalibrationA(int capacityCode, bool& capCodeIsFound) const
{
  return this->getCalibration(capacityCode, capCodeIsFound, CalibrationDataType::INTEGRATOR_A);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getAdcLinearCalibrationA(int capacityCode, int timeCode,
  bool& capCodeIsFound, bool& timeCodeIsFound) const
{
  return this->getCalibration(capacityCode, timeCode, capCodeIsFound, timeCodeIsFound, CalibrationDataType::INTEGRATOR_A);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getFirstAdcLinearCalibrationB() const
{
  return this->getCalibration(CalibrationDataType::INTEGRATOR_B);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getAdcLinearCalibrationB(int capacityCode, bool& capCodeIsFound) const
{
  return this->getCalibration(capacityCode, capCodeIsFound, CalibrationDataType::INTEGRATOR_B);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getAdcLinearCalibrationB(int capacityCode, int timeCode,
  bool& capCodeIsFound, bool& timeCodeIsFound) const
{
  return this->getCalibration(capacityCode, timeCode, capCodeIsFound, timeCodeIsFound, CalibrationDataType::INTEGRATOR_B);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getFirstAmpUniformCalibration() const
{
  return this->getCalibration(CalibrationDataType::SIGNAL_AMPLITUDE);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getAmpUniformCalibration(int capacityCode, bool& capCodeIsFound) const
{
  return this->getCalibration(capacityCode, capCodeIsFound, CalibrationDataType::SIGNAL_AMPLITUDE);
}

ChipChannelCalibrationMap ChipCapacityCalibrationData::getAmpUniformCalibration(int capacityCode, int timeCode,
  bool& capCodeIsFound, bool& timeCodeIsFound) const
{
  return this->getCalibration(capacityCode, timeCode, capCodeIsFound, timeCodeIsFound, CalibrationDataType::SIGNAL_AMPLITUDE);
}
