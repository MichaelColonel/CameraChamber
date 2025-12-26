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

#include <array>
#include <map>
#include <utility>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>

#define CHANNELS_PER_CHIP 32

#define CHIPS_PER_PLANE 6
#define CHIPS_VERTICAL_STRIPS_PLANE 6
#define CHIPS_HORIZONTAL_STRIPS_PLANE 6
#define CHIPS_PER_CAMERA (CHIPS_VERTICAL_STRIPS_PLANE + CHIPS_HORIZONTAL_STRIPS_PLANE)

#define CHANNELS_PER_PLANE (CHANNELS_PER_CHIP * CHIPS_PER_PLANE)
#define CHANNELS_VERTICAL_STRIPS_PLANE (CHIPS_VERTICAL_STRIPS_PLANE * CHIPS_PER_PLANE)
#define CHANNELS_HORIZONTAL_STRIPS_PLANE (CHIPS_HORIZONTAL_STRIPS_PLANE * CHIPS_PER_PLANE)
#define CHANNELS_PER_CAMERA (CHANNELS_VERTICAL_STRIPS_PLANE + CHANNELS_HORIZONTAL_STRIPS_PLANE)

#define STRIP_STEP_PER_SIDE_MM 2.0

typedef boost::accumulators::accumulator_set< \
  double, \
  boost::accumulators::stats< \
    boost::accumulators::tag::sum, \
    boost::accumulators::tag::count, \
    boost::accumulators::tag::mean, \
    boost::accumulators::tag::moment< 2 > \
  > \
> MeanDispAccumSet;

typedef std::pair< int, int > ChipChannelPair;
typedef std::map< ChipChannelPair, double > ChipChannelCalibrationMap;
typedef std::array< ChipChannelPair, CHANNELS_PER_PLANE > ChipChannelArray;
typedef std::map< int, std::array< std::vector< int >, CHANNELS_PER_CHIP > > ChipChannelsCountsMap;
typedef std::pair< ChipChannelCalibrationMap, ChipChannelCalibrationMap > ChipChannelCalibrationMapPair;
typedef std::map< int, ChipChannelCalibrationMapPair > AdcOffsetCalibrationMap; // ADC-Offset calibration

enum StripsOrientationType : int {
  ORIENTATION_HORIZONTAL = 0,
  ORIENTATION_VERTICAL,
  StripsOrientationType_Last
};

enum CameraProfileType : int {
  PROFILE_VERTICAL = StripsOrientationType::ORIENTATION_HORIZONTAL,
  PROFILE_HORIZONTAL = StripsOrientationType::ORIENTATION_VERTICAL,
  CameraProfileType_Last
};

enum IntegratorType : int {
  A = 0,
  B,
  IntegratorType_Last
};

enum AdcTimeType : int {
  INTEGRATOR_A,
  INTEGRATOR_B,
  INTEGRATOR_AB
};

enum AdcResolutionType : int {
  ADC_16_BIT = 16,
  ADC_18_BIT = 18,
  ADC_20_BIT = 20,
  AdcResolutionType_Last
};

enum ProfileRepresentationType : int {
  MEAN, // mean ADC count
  INTEGRAL, // integral ADC count
  CHARGE // charge in pC
};

struct CameraResponse {
  int ChipsEnabled{ -1 };
  unsigned short ChipsEnabledCode{ 0 };
  int IntegrationTimeCode{ -1 }; // Integration time code(0...15)
  int AdcMode{ AdcResolutionType_Last }; // 16-bit or 20-bit (16 or 20)
  int CapacityCode{ -1 }; // Capacity code (0...7)
  int AdcSamples{ -1 };
  bool ExternalStartState{ false };
};

struct ChannelInfo {
  double PedMeanA{ -1. };
  double PedMeanB{ -1. };
  double PedMom2A{ -1. };
  double PedMom2B{ -1. };
  double SigMeanA{ -1. };
  double SigMeanB{ -1. };
  double SigMom2A{ -1. };
  double SigMom2B{ -1. };
  double SigCountA{ -1. };
  double SigCountB{ -1. };
  double SigSumA{ -1. };
  double SigSumB{ -1. };
  double Signal{ -1. };
  double SignalNoAmp{ -1. };
};

typedef std::pair< struct ChannelInfo, struct ChannelInfo > ChannelInfoPair;
