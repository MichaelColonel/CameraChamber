// Author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <TCanvas.h>
#include <TH1.h>
#include <TMath.h>
#include <TFormula.h>
#include <TF1.h>
#include <TPad.h>
#include <TROOT.h>

#include <QPushButton>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
  :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
   gROOT->SetWebDisplay("qt5");
#else
   gROOT->SetWebDisplay("qt6");
#endif

#if (!QT_NO_DEBUG)
  qDebug() << Q_FUNC_INFO << "Debug info";
#endif

  this->ui->setupUi(this);

  connect(this->ui->PushButton_Update, SIGNAL(clicked()), this, SLOT(onUpdateButtonClicked()));

  // create sqroot formula
  new TFormula("form1", "abs(sin(x)/x)");
  TF1* sqroot = new TF1("sqroot", "x*gaus(0) + [3]*form1", 0, 10);
  sqroot->SetParameters(10, 4, 1, 20);

  // create sample histogram
  hist = new TH1F("gaus1", "Example of TH1 drawing in TCanvas", 200, 0., 10.);
  hist->FillRandom("sqroot", 10000);
  hist->SetDirectory(nullptr);
  hist->SetFillColor(kViolet + 2);
  hist->SetFillStyle(3001);

//  gPad = this->ui->RootCanvasWidget->getCanvas();
  TPad* pad = this->ui->RootCanvasWidget->getPad();
  if (!pad)
  {
    return;
  }
  pad->cd();
  pad->SetBorderMode(0);
  pad->SetFillColor(0);
  pad->SetGrid();
  hist->Draw();
}

MainWindow::~MainWindow()
{
  delete hist;
  delete ui;
}

void MainWindow::onUpdateButtonClicked()
{
  // Handle the "Update" button clicked() event.
  // Update a one dimensional histogram (one float per bin)
  // and fill it following the distribution in function sqroot.
  hist->Reset();
  hist->FillRandom("sqroot", 10000);
  //  gPad = this->ui->RootCanvasWidget->getCanvas();
  TPad* pad = this->ui->RootCanvasWidget->getPad();
  if (!pad)
  {
    return;
  }
  pad->cd();
  pad->Modified();
  pad->Update();
}
