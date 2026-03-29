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
  class HttpServerDialog;
}

class HttpServerDialogPrivate;
class CameraProfilesDialog;
class THttpServer;

class HttpServerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit HttpServerDialog(std::shared_ptr< THttpServer >& server, QWidget *parent = nullptr);
  ~HttpServerDialog();
//  bool registerHistograms(const QString& cameraID, CameraProfilesDialog* profilesDialog);
  std::shared_ptr< THttpServer > getUpdatedServer();

Q_SIGNALS:
  void logMessage(const QString& msg, const QString& context, QColor color);

public Q_SLOTS:
  void updateLatex();

protected:
  QScopedPointer< HttpServerDialogPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(HttpServerDialog);
  Q_DISABLE_COPY(HttpServerDialog);
};

QT_END_NAMESPACE
