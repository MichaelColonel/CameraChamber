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

QT_BEGIN_NAMESPACE

class SettingsDialogPrivate;

class SettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SettingsDialog(const QString& cameraID, QWidget *parent = nullptr);
  virtual ~SettingsDialog();

Q_SIGNALS:
  void logMessage(const QString& msg, const QString& context, QColor color);

protected:
  QScopedPointer< SettingsDialogPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(SettingsDialog);
  Q_DISABLE_COPY(SettingsDialog);
};

QT_END_NAMESPACE
