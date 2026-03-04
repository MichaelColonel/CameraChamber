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

#include <QDialog>
#include <QScopedPointer>

#include "AbstractCamera.h"

QT_BEGIN_NAMESPACE

class CameraProfilesDialogPrivate;

class CameraProfilesDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CameraProfilesDialog(const AbstractCamera::CameraDeviceData& cameraInfo, QWidget* parent = nullptr);
  virtual ~CameraProfilesDialog();
  void setCameraDevice(QPointer< AbstractCamera > cam);
  void getProfiles(TGraph* horiz, TGraph* vert, TH2* hist2d);

public Q_SLOTS:
  void onAcquisitionStarted();
  void onAcquisitionFinished();
  void onUpdateGraphClicked();
  void onUpdateProfilesClicked();
  void onPedestalBeginChanged(double pos);
  void onPedestalEndChanged(double pos);
  void onSignalBeginChanged(double pos);
  void onSignalEndChanged(double pos);
  void onResetIntegralPseudo2dClicked();

Q_SIGNALS:
  void logMessage(const QString& msg, const QString& context, QColor color);

protected:
  QScopedPointer< CameraProfilesDialogPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(CameraProfilesDialog);
  Q_DISABLE_COPY(CameraProfilesDialog);
};

QT_END_NAMESPACE
