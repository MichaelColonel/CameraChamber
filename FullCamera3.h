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

#include "FullCamera.h"

QT_BEGIN_NAMESPACE

class FullCamera3Private;

class FullCamera3 : public FullCamera
{
  Q_OBJECT
public:
  explicit FullCamera3(const AbstractCamera::CameraDeviceData& data, QObject* parent = nullptr);
  ~FullCamera3() override;
public:
  TH2* createProfile2D(bool integral = false) override;
  void updateProfiles2D(TH2* pseudo2D = nullptr, TH2* integPseudo2D = nullptr) override;
  void updateProfiles(TGraph* vertProfile = nullptr, TGraph* horizProfile = nullptr, bool withErrors = false) override;

protected:
  QScopedPointer< FullCamera3Private > d_ptr;

private:
  Q_DECLARE_PRIVATE(FullCamera3);
  Q_DISABLE_COPY(FullCamera3);
};

QT_END_NAMESPACE
