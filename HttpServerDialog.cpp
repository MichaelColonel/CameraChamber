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

#include "HttpServerDialog.h"
#include "ui_HttpServerDialog.h"

#include "CameraProfilesDialog.h"

#include <THttpServer.h>

QT_BEGIN_NAMESPACE

class HttpServerDialogPrivate
{
  Q_DECLARE_PUBLIC(HttpServerDialog);
protected:
  HttpServerDialog* const q_ptr;
  QScopedPointer< Ui::HttpServerDialog > ui;
public:
  HttpServerDialogPrivate(HttpServerDialog &object);
  virtual ~HttpServerDialogPrivate();

  std::shared_ptr< THttpServer > httpServer;
};

HttpServerDialogPrivate::HttpServerDialogPrivate(HttpServerDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::HttpServerDialog)
{
}

HttpServerDialogPrivate::~HttpServerDialogPrivate()
{
}

HttpServerDialog::HttpServerDialog(std::shared_ptr< THttpServer >& server, QWidget *parent) :
  QDialog(parent),
  d_ptr(new HttpServerDialogPrivate(*this))
{
  Q_D(HttpServerDialog);
  d->ui->setupUi(this);
  if (server)
  {
    d->httpServer = server;
  }
  else
  {
    d->httpServer = std::shared_ptr< THttpServer >(new THttpServer("http:10.163.1.39:8080"));
  }
}

HttpServerDialog::~HttpServerDialog()
{
  Q_D(HttpServerDialog);
}

std::shared_ptr< THttpServer > HttpServerDialog::getUpdatedServer()
{
  Q_D(HttpServerDialog);
  return d->httpServer;
}

bool HttpServerDialog::registerHistograms(const QString& cameraID, CameraProfilesDialog* profilesDialog)
{
  Q_D(HttpServerDialog);
  TGraph* verticalProfile;
  TGraph* horizontalProfile;
  TH2* pseudo2D;
  profilesDialog->getProfiles(horizontalProfile, verticalProfile, pseudo2D);

/*
  return d->httpServer;

  // register histograms
  serv->Register("/", hpx);
  serv->Register("/", hpxpy);
//   serv->AddLocation("mydir/", "/home/tmp");

  // enable monitoring and
  // specify items to draw when page is opened
  serv->SetItemField("/","_monitoring","5000");
  serv->SetItemField("/","_layout","grid2x2");
  serv->SetItemField("/","_drawitem","[hpxpy,hpx,Debug]");
  serv->SetItemField("/","_drawopt","col");
*/
  return true;
}

QT_END_NAMESPACE
