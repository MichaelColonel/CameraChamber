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

#include "AbstractCamera.h"
#include <QObject>

QT_BEGIN_NAMESPACE

class Camera2Private;

class Camera2 : public AbstractCamera
{
  Q_OBJECT
public:
  // Vertically oriented strips => horizontal profile
  static constexpr int HORIZONTAL_PROFILE_STRIPS = CHANNELS_PER_CHIP * 4;
  // Horizontally oriented strips => vertical profile
  static constexpr int VERTICAL_PROFILE_STRIPS = CHANNELS_PER_PLANE;
  Camera2(const AbstractCamera::CameraDeviceData& data, QObject *parent = nullptr);
  ~Camera2() override;

  void processDataCounts(bool splitData = false, IntegratorType integType = IntegratorType::A,
    ProfileRepresentationType profileType = ProfileRepresentationType::CHARGE) override;
  TH2* createProfile2D(bool integral = false) override;
  void updateProfiles2D(TH2* pseudo2D, TH2* integPseudo2D) override;
  void updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors) override;

  static std::vector< double > GenerateHorizontalProfileStripsBinsBorders(size_t n);

protected:
  QScopedPointer< Camera2Private > d_ptr;

private:
  Q_DECLARE_PRIVATE(Camera2);
  Q_DISABLE_COPY(Camera2);
};

QT_END_NAMESPACE
