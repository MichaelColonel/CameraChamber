// Initial qtweb author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

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

#include "TCanvasWidget.h"

#include <TCanvas.h>
#include <TROOT.h>
#include <TClass.h>
#include <TEnv.h>

#include <TWebCanvas.h>

#include <iostream>

QT_BEGIN_NAMESPACE

TCanvasWidget::TCanvasWidget(QWidget *parent)
  :
  QWidget(parent)
{
  setObjectName("TCanvasWidget");

  setSizeIncrement(QSize(100, 100));

  setUpdatesEnabled(true);
  setMouseTracking(true);

  setFocusPolicy(Qt::TabFocus);
  setCursor(Qt::CrossCursor);

  setAcceptDrops(true);

  static int wincnt = 1;

  this->rootCanvas = new TCanvas(kFALSE);
  this->rootCanvas->SetName(Form("Canvas%d", wincnt++));
  this->rootCanvas->SetTitle("ROOT Canvas");
  this->rootCanvas->ResetBit(TCanvas::kShowEditor);
  this->rootCanvas->SetCanvas(this->rootCanvas);
  this->rootCanvas->SetBatch(kTRUE); // mark canvas as batch

  TPad* pad = dynamic_cast< TPad* >(this->rootCanvas->GetPad(0));
  if (pad)
  {
    pad->Clear();
    pad->cd();
  }
  bool read_only = gEnv->GetValue("WebGui.FullCanvas", (Int_t) 1) == 0;
  TWebCanvas* web = new TWebCanvas(this->rootCanvas, "title", 0, 0, 800, 600, read_only);

  this->rootCanvas->SetCanvasImp(web);

  SetPrivateCanvasFields(true);

  web->SetCanCreateObjects(kFALSE); // not yet create objects on server side
  web->SetUpdatedHandler([this](){ emit CanvasUpdated(); });
  web->SetActivePadChangedHandler([this](TPad *pad){ emit SelectedPadChanged(pad); });
  web->SetPadClickedHandler([this](TPad *pad, int x, int y) { emit PadClicked(pad, x, y); });
  web->SetPadDblClickedHandler([this](TPad *pad, int x, int y) { emit PadDblClicked(pad, x, y); });
  auto where = ROOT::RWebDisplayArgs::GetQt5EmbedQualifier(this, "noopenui", QT_VERSION);

  web->ShowWebWindow(where);

  this->view = findChild< QWebEngineView* >("RootWebView");
  if (!this->view)
  {
    std::cerr << "FAIL TO FIND QWebEngineView - ROOT Qt5Web plugin does not work properly !!!!!" << std::endl;
    exit(11);
  }
  this->view->resize(width(), height());
  this->rootCanvas->SetCanvasSize(width(), height());
}

TCanvasWidget::~TCanvasWidget()
{
  if (this->rootCanvas)
  {
    SetPrivateCanvasFields(false);
    gROOT->GetListOfCanvases()->Remove(this->rootCanvas);
    this->rootCanvas->Close();
    delete this->rootCanvas;
    this->rootCanvas = nullptr;
  }
}

void TCanvasWidget::SetPrivateCanvasFields(bool on_init)
{
  if (!this->rootCanvas)
  {
    return;
  }
  Long_t offset = TCanvas::Class()->GetDataMemberOffset("fCanvasID");
  if (offset > 0)
  {
    Int_t *id = (Int_t *)((char *)this->rootCanvas + offset);
    if (*id == this->rootCanvas->GetCanvasID())
    {
      *id = on_init ? 111222333 : -1;
    }
  }
  else
  {
    std::cerr << "ERROR: Cannot modify fCanvasID data member" << std::endl;
  }

  offset = TCanvas::Class()->GetDataMemberOffset("fMother");
  if (offset > 0)
  {
    TPad **moth = (TPad **)((char *)this->rootCanvas + offset);
    if (*moth == this->rootCanvas->GetMother())
    {
      *moth = on_init ? this->rootCanvas : nullptr;
    }
   }
  else
  {
    std::cerr << "ERROR: Cannot set fMother data member in canvas" << std::endl;
  }
}

void TCanvasWidget::resizeEvent(QResizeEvent *event)
{
  if (!this->rootCanvas || !this->view)
  {
    return;
  }
  Q_UNUSED(event);
  this->view->resize(width(), height());
  this->rootCanvas->SetCanvasSize(width(), height());
}

void TCanvasWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
}

void TCanvasWidget::activateEditor(TPad *pad, TObject *obj)
{
  if (!this->rootCanvas)
  {
    return;
  }
  TWebCanvas *cimp = dynamic_cast< TWebCanvas* >(this->rootCanvas->GetCanvasImp());
  if (cimp)
  {
    cimp->ShowEditor(kTRUE);
    cimp->ActivateInEditor(pad, obj);
  }
}

void TCanvasWidget::setEditorVisible(bool flag)
{
  if (!this->rootCanvas)
  {
    return;
  }
  TCanvasImp *cimp = this->rootCanvas->GetCanvasImp();
  if (cimp)
  {
    cimp->ShowEditor(flag);
  }
}

void TCanvasWidget::activateStatusLine()
{
  if (!this->rootCanvas)
  {
    return;
  }
  TCanvasImp *cimp = this->rootCanvas->GetCanvasImp();
  if (cimp)
  {
    cimp->ShowStatusBar(kFALSE);
  }
}

TPad* TCanvasWidget::getPad(int subPadNumber) const
{
  return (this->rootCanvas) ? dynamic_cast< TPad* >(this->rootCanvas->GetPad(subPadNumber)) : nullptr;
}

QT_END_NAMESPACE
