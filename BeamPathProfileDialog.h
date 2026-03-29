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

QT_BEGIN_NAMESPACE

namespace Ui {
  class BeamPathProfileDialog;
}

class AbstractCamera;

class BeamPathProfileDialogPrivate;
class THttpServer;

class BeamPathProfileDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BeamPathProfileDialog(QWidget *parent = nullptr);
  ~BeamPathProfileDialog();
  void setCamerasDevices(AbstractCamera* camera1, AbstractCamera* camera2);
  void addBeamProfilesToServer(std::shared_ptr< THttpServer > server); // register TPad or TCanvas

Q_SIGNALS:
  void logMessage(const QString& msg, const QString& context, QColor color);

public Q_SLOTS:
  void processCameraProfiles(const QString& cameraID);
  void processCameraBeginProfiles();
  void processCameraEndProfiles();
//  void onBeamPathCalculationClicked();
//  void onCalibrationCalculationClicked();
//  void onScanningCalculationClicked();
  void onScanHomoHorizonalBeginChanged(double pos);
  void onScanHomoHorizonalEndChanged(double pos);
  void onScanHomoVerticalBeginChanged(double pos);
  void onScanHomoVerticalEndChanged(double pos);

protected:
  QScopedPointer< BeamPathProfileDialogPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(BeamPathProfileDialog);
  Q_DISABLE_COPY(BeamPathProfileDialog);
};

QT_END_NAMESPACE
