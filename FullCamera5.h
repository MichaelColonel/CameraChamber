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

#include "FullCamera4.h"

QT_BEGIN_NAMESPACE

class FullCamera5Private;

class FullCamera5 : public FullCamera4
{
  Q_OBJECT
public:
  explicit FullCamera5(const AbstractCamera::CameraDeviceData& data, QObject* parent = nullptr);
  ~FullCamera5() override;
public:
  TGraph* createProfile(CameraProfileType profileType, bool withErrors = false) override;
  void updateProfiles(TGraph* vertProfile = nullptr, TGraph* horizProfile = nullptr, bool withErrors = false) override;

protected:
  QScopedPointer< FullCamera5Private > d_ptr;

private:
  Q_DECLARE_PRIVATE(FullCamera5);
  Q_DISABLE_COPY(FullCamera5);
};

QT_END_NAMESPACE
