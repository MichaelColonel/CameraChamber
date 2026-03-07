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

#include <QTimer>
#include <QDateTime>
#include <QDebug>

#include "HttpServerDialog.h"
#include "ui_HttpServerDialog.h"

#include "CameraProfilesDialog.h"

#include <THttpServer.h>
#include <TLatex.h>
#include <TGraph.h>
#include <TCanvas.h>
#include <TPad.h>

#include <string>

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
  std::unique_ptr< TPad > pad;
  std::unique_ptr< TLatex > latex;
  QScopedPointer< QTimer > timer;
};

HttpServerDialogPrivate::HttpServerDialogPrivate(HttpServerDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::HttpServerDialog),
  timer(new QTimer(&object))
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

  TCanvas* canvas = d->ui->RootCanvas_Latex->getCanvas();
  canvas->cd();
  d->pad = std::unique_ptr< TPad >(new TPad("pLatex", "Grid", 0., 0., 1., 1.));
  d->pad->Draw();

  if (server)
  {
    d->httpServer = server;
  }
  else
  {
    d->httpServer = std::shared_ptr< THttpServer >(new THttpServer("http:8080?loopback"));
//    d->httpServer = std::shared_ptr< THttpServer >(new THttpServer("http:10.163.1.39:8080?loopback"));
//    d->httpServer->SetJSROOT("https://jsroot.gsi.de/latest/");

    // register histograms
    d->pad->cd();
    d->latex = std::unique_ptr< TLatex >(new TLatex);
    d->latex->SetTextSize(0.05);
    d->latex->SetTextAlign(13);  //align at top
    d->latex->DrawLatex(.0, 1.,"Время, (мс)");
    d->latex->DrawLatex(.3, 1.,"ПР1 / Стена");
    d->latex->DrawLatex(.6, 1.,"ПР2 / Изоцентр");

    d->latex->DrawLatex(.0,.95,"           ");
    d->latex->DrawLatex(.3,.95,"X,Y,D (мм)");
    d->latex->DrawLatex(.6,.95,"X,Y,D (мм)");

    d->latex->SetTextSize(0.04);
    d->latex->DrawLatex(.0,.8,"100");
    d->latex->DrawLatex(.3,.8,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.8,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.75,"200");
    d->latex->DrawLatex(.3,.75,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.75,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.7,"300");
    d->latex->DrawLatex(.3,.7,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.7,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.65,"400");
    d->latex->DrawLatex(.3,.65,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.65,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.6,"500");
    d->latex->DrawLatex(.3,.6,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.6,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.55,"600");
    d->latex->DrawLatex(.3,.55,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.55,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.5,"700");
    d->latex->DrawLatex(.3,.5,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.5,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.45,"800");
    d->latex->DrawLatex(.3,.45,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.45,"20.3, 144.5, 50.4");

    d->latex->DrawLatex(.0,.4,"900");
    d->latex->DrawLatex(.3,.4,"30.3, 124.5, 47.2");
    d->latex->DrawLatex(.6,.4,"20.3, 144.5, 50.4");
    d->pad->Modified();
    d->pad->Update();

    d->httpServer->Register("/p1", d->pad.get());
    TGraph* gr = new TGraph(10);
    gr->SetName("gr1");
    d->httpServer->Register("graphs/subfolder", gr);
    d->httpServer->SetItemField("/","_monitoring","1000");
  }
  d->timer->setInterval(5000);
  QObject::connect(d->timer.get(), SIGNAL(timeout()), this, SLOT(updateLatex()));
  d->timer->start();
}

HttpServerDialog::~HttpServerDialog()
{
  Q_D(HttpServerDialog);
}

void HttpServerDialog::updateLatex()
{
  Q_D(HttpServerDialog);
  d->pad->cd();
//  d->pad->GetListOfPrimitives()->Remove(d->latex.get());
  d->pad->Clear();
  d->latex = std::unique_ptr< TLatex >(new TLatex);
  d->latex->SetTextSize(0.05);
  d->latex->SetTextAlign(13);  //align at top
  d->latex->DrawLatex(.0, 1.,"Время, (мс)");
  d->latex->DrawLatex(.3, 1.,"ПР1 / Стена");
  d->latex->DrawLatex(.6, 1.,"ПР2 / Изоцентр");

  d->latex->DrawLatex(.0,.95,"           ");
  d->latex->DrawLatex(.3,.95,"X,Y,D (мм)");
  d->latex->DrawLatex(.6,.95,"X,Y,D (мм)");

  d->latex->SetTextSize(0.04);
  QDateTime dt = QDateTime::currentDateTime();
  QString str = dt.toString("ddMMyyyy_hhmmss");
//  static char c_str[100];
  std::string ss = str.toStdString();
//  std::copy(ss.begin(), ss.end(), c_str);
  d->latex->DrawLatex(.0,.8,ss.c_str());
  d->pad->Modified();
  d->pad->Update();
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
