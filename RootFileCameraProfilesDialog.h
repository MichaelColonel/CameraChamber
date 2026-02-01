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

class QListWidgetItem;

QT_BEGIN_NAMESPACE

class RootFileCameraProfilesDialogPrivate;

class RootFileCameraProfilesDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RootFileCameraProfilesDialog(const QString& rootFile, QWidget *parent = nullptr);
  virtual ~RootFileCameraProfilesDialog();

public Q_SLOTS:
//  void onCurrentRootTreeItemChanged(QListWidgetItem* newItem,QListWidgetItem*); // not used
  void onUpdateGraphClicked();
  void onUpdateProfilesClicked();
  void onPedestalBeginChanged(double pos);
  void onPedestalEndChanged(double pos);
  void onSignalBeginChanged(double pos);
  void onSignalEndChanged(double pos);
  void onProfileFrameRangeValueChanged(int);
  void onClearProfilesClicked();
  void onPlayProfilesClicked();
  void updateProfileFrame();
  void onCameraSpillsModelIndexClicked(const QModelIndex& index);

Q_SIGNALS:
  void logMessage(const QString& msg, const QString& context, QColor color);

protected:
  QScopedPointer< RootFileCameraProfilesDialogPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(RootFileCameraProfilesDialog);
  Q_DISABLE_COPY(RootFileCameraProfilesDialog);
};

QT_END_NAMESPACE
