/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
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

#define CHANNELS_PER_PLANE (CHANNELS_PER_CHIP * CHIPS_PER_PLANE)
#define CHANNELS_VERTICAL_STRIPS_PLANE (CHIPS_VERTICAL_STRIPS_PLANE * CHIPS_PER_PLANE)
#define CHANNELS_HORIZONTAL_STRIPS_PLANE (CHIPS_HORIZONTAL_STRIPS_PLANE * CHIPS_PER_PLANE)


typedef boost::accumulators::accumulator_set< \
  double, \
  boost::accumulators::stats< \
    boost::accumulators::tag::sum, \
    boost::accumulators::tag::count, \
    boost::accumulators::tag::mean, \
    boost::accumulators::tag::moment<2> \
  > \
> MeanDispAccumSet;

typedef std::map< std::pair< int, int >, double > ChipChannelCalibrationMap;
typedef std::array< std::pair< int, int >, CHANNELS_PER_PLANE > ChipChannelArray;
typedef std::map< int, std::array< std::vector< int >, CHANNELS_PER_CHIP > > ChannelsCountsMap;
typedef std::pair< ChipChannelCalibrationMap, ChipChannelCalibrationMap > ChipChannelCalibrationMapPair;
typedef std::map< int, ChipChannelCalibrationMapPair > AdcOffsetCalibrationMap; // ADC-Offset calibration
