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

#include "TBeamPathCanvasWidget.h"

#include <TCanvas.h>
#include <TGraph.h>
#include <TLatex.h>
#include <TGaxis.h>
#include <TAxis.h>

#include "CameraProfiles.h"
#include "AbstractCamera.h"

QT_BEGIN_NAMESPACE

class TBeamPathCanvasWidgetPrivate
{
  Q_DECLARE_PUBLIC(TBeamPathCanvasWidget);
protected:
  TBeamPathCanvasWidget* const q_ptr;
public:
  TBeamPathCanvasWidgetPrivate(TBeamPathCanvasWidget &object);
  virtual ~TBeamPathCanvasWidgetPrivate();
  template<class T> TGaxis* reverseHorizontalXaxis(TPad* pad, T* diagram, TGaxis* prevReverseXaxis);

  std::unique_ptr< TLatex > beginBeamPathTable;
  std::unique_ptr< TLatex > endBeamPathTable;

  std::unique_ptr< TGraph > beginHorizontalProfileGraph;
  std::unique_ptr< TGraph > beginVerticalProfileGraph;
  std::unique_ptr< TGraph > endHorizontalProfileGraph;
  std::unique_ptr< TGraph > endVerticalProfileGraph;

  std::unique_ptr< TGaxis > beginReverseHorizXaxis;
  std::unique_ptr< TGaxis > endReverseHorizXaxis;
};

TBeamPathCanvasWidgetPrivate::TBeamPathCanvasWidgetPrivate(TBeamPathCanvasWidget &object)
  :
  q_ptr(&object)
{
}

TBeamPathCanvasWidgetPrivate::~TBeamPathCanvasWidgetPrivate()
{
}

template<class T> TGaxis* TBeamPathCanvasWidgetPrivate::reverseHorizontalXaxis(TPad* pad,
  T* diagram, TGaxis* prevReverseXaxis)
{
  if (!pad || !diagram)
  {
    return nullptr;
  }
  pad->cd();
  // Remove the current axis
  diagram->GetXaxis()->SetLabelOffset(999);
  diagram->GetXaxis()->SetTickLength(0);

  // Redraw the new axis
  pad->Update();
  if (prevReverseXaxis)
  {
    pad->GetListOfPrimitives()->Remove(prevReverseXaxis);
  }
  TGaxis *newaxis = new TGaxis(pad->GetUxmax(),
    pad->GetUymin(),
    pad->GetUxmin(),
    pad->GetUymin(),
    diagram->GetXaxis()->GetXmin(),
    diagram->GetXaxis()->GetXmax(),
    510, "-");
  newaxis->SetLabelOffset(-0.01);
  newaxis->Draw();

  return newaxis;
}

TBeamPathCanvasWidget::TBeamPathCanvasWidget(QWidget *parent)
  :
  TCanvasWidget(parent),
  d_ptr(new TBeamPathCanvasWidgetPrivate(*this))
{
  Q_D(TBeamPathCanvasWidget);

  setObjectName("TBeamPathCanvasWidget");
  TCanvas* canvas = this->getCanvas();
  if (canvas)
  {
    canvas->Divide(3, 2);
  }
}

TBeamPathCanvasWidget::~TBeamPathCanvasWidget()
{
  Q_D(TBeamPathCanvasWidget);
  TPad* padHorizProfileBegin = this->getPad(1);
  if (d->beginHorizontalProfileGraph)
  {
    padHorizProfileBegin->GetListOfPrimitives()->Remove(d->beginHorizontalProfileGraph.get());
  }
  TPad* padVerticalProfileBegin = this->getPad(2);
  if (d->beginVerticalProfileGraph)
  {
    padVerticalProfileBegin->GetListOfPrimitives()->Remove(d->beginVerticalProfileGraph.get());
  }
  TPad* padBeamPathTableBegin = this->getPad(3);
  if (d->beginBeamPathTable)
  {
    padBeamPathTableBegin->GetListOfPrimitives()->Remove(d->beginBeamPathTable.get());
  }

  TPad* padHorizProfileEnd = this->getPad(4);
  if (d->endHorizontalProfileGraph)
  {
    padHorizProfileEnd->GetListOfPrimitives()->Remove(d->endHorizontalProfileGraph.get());
  }
  TPad* padVerticalProfileEnd = this->getPad(5);
  if (d->endVerticalProfileGraph)
  {
    padVerticalProfileEnd->GetListOfPrimitives()->Remove(d->endVerticalProfileGraph.get());
  }
  TPad* padBeamPathTableEnd = this->getPad(6);
  if (d->endBeamPathTable)
  {
    padBeamPathTableEnd->GetListOfPrimitives()->Remove(d->endBeamPathTable.get());
  }

  padHorizProfileBegin->Clear();
  padVerticalProfileBegin->Clear();
  padBeamPathTableBegin->Clear();

  padHorizProfileEnd->Clear();
  padVerticalProfileEnd->Clear();
  padBeamPathTableEnd->Clear();
}

void TBeamPathCanvasWidget::updateProfiles(TGraph* cam1HorizProfile, TGraph* cam1VertProfile,
  TGraph* cam2HorizProfile, TGraph* cam2VertProfile)
{
  Q_D(TBeamPathCanvasWidget);

  TPad* padHorizProfileBegin = this->getPad(1);
  padHorizProfileBegin->cd();
  if (d->beginHorizontalProfileGraph)
  {
    padHorizProfileBegin->GetListOfPrimitives()->Remove(d->beginHorizontalProfileGraph.get());
    d->beginHorizontalProfileGraph.reset(nullptr);
  }
  d->beginHorizontalProfileGraph.reset(cam1HorizProfile);
  d->beginHorizontalProfileGraph->SetLineColor(kBlue);
  d->beginHorizontalProfileGraph->SetLineWidth(2);
  d->beginHorizontalProfileGraph->SetTitle("Горизонтальный профиль;Положение стрипа (mm);Заряд (пКл)");
  d->beginHorizontalProfileGraph->Draw("AL");
  TGaxis* newaxis = d->reverseHorizontalXaxis(padHorizProfileBegin, cam1HorizProfile, d->beginReverseHorizXaxis.get());
  if (newaxis)
  {
    d->beginReverseHorizXaxis.reset(newaxis);
  }
  padHorizProfileBegin->Modified();
  padHorizProfileBegin->Update();

  TPad* padVerticalProfileBegin = this->getPad(2);
  padVerticalProfileBegin->cd();
  if (d->beginVerticalProfileGraph)
  {
    padVerticalProfileBegin->GetListOfPrimitives()->Remove(d->beginVerticalProfileGraph.get());
    d->beginVerticalProfileGraph.reset(nullptr);
  }
  d->beginVerticalProfileGraph.reset(cam1VertProfile);
  d->beginVerticalProfileGraph->SetLineColor(kRed);
  d->beginVerticalProfileGraph->SetLineWidth(2);
  d->beginVerticalProfileGraph->SetTitle("Вертикальный профиль;Положение стрипа (mm);Заряд (пКл)");
  d->beginVerticalProfileGraph->Draw("AL");
  padVerticalProfileBegin->Modified();
  padVerticalProfileBegin->Update();

  TPad* padHorizProfileEnd = this->getPad(4);
  padHorizProfileEnd->cd();
  if (d->endHorizontalProfileGraph)
  {
    padHorizProfileEnd->GetListOfPrimitives()->Remove(d->endHorizontalProfileGraph.get());
    d->endHorizontalProfileGraph.reset(nullptr);
  }
  d->endHorizontalProfileGraph.reset(cam2HorizProfile);
  d->endHorizontalProfileGraph->SetLineColor(kBlue);
  d->endHorizontalProfileGraph->SetLineWidth(2);
  d->endHorizontalProfileGraph->SetTitle("Горизонтальный профиль;Положение стрипа (mm);Заряд (пКл)");
  d->endHorizontalProfileGraph->Draw("AL");
  newaxis = d->reverseHorizontalXaxis(padHorizProfileEnd, cam2HorizProfile, d->endReverseHorizXaxis.get());
  if (newaxis)
  {
    d->endReverseHorizXaxis.reset(newaxis);
  }
  padHorizProfileEnd->Modified();
  padHorizProfileEnd->Update();

  TPad* padVerticalProfileEnd = this->getPad(5);
  padVerticalProfileEnd->cd();
  if (d->endVerticalProfileGraph)
  {
    padVerticalProfileEnd->GetListOfPrimitives()->Remove(d->endVerticalProfileGraph.get());
    d->endVerticalProfileGraph.reset(nullptr);
  }
  d->endVerticalProfileGraph.reset(cam2VertProfile);
  d->endVerticalProfileGraph->SetLineColor(kRed);
  d->endVerticalProfileGraph->SetLineWidth(2);
  d->endVerticalProfileGraph->SetTitle("Вертикальный профиль;Положение стрипа (mm);Заряд (пКл)");
  d->endVerticalProfileGraph->Draw("AL");
  padVerticalProfileEnd->Modified();
  padVerticalProfileEnd->Update();
}

void TBeamPathCanvasWidget::updateBeamPathTables(AbstractCamera* cam1, AbstractCamera* cam2,
  QDateTime& cam1DateTime, QDateTime& cam2DateTime, int intergTime)
{
  Q_D(TBeamPathCanvasWidget);

  TPad* padBeamPathTableBegin = this->getPad(3);
  if (d->beginBeamPathTable)
  {
    padBeamPathTableBegin->GetListOfPrimitives()->Remove(d->beginBeamPathTable.get());
    d->beginBeamPathTable.reset(nullptr);
  }
  padBeamPathTableBegin->cd();
  padBeamPathTableBegin->Clear();

  CameraProfiles beginProfs(cam1);
  PedestalSignalGateArray arr;
  cam1->getPedestalSignalGate(arr);
  int totalStepsMs = (arr[3] - arr[2]) / intergTime;
  TLatex* beginTableLatex = beginProfs.createBeamPathTable(intergTime, totalStepsMs, cam1DateTime);
  if (beginTableLatex)
  {
    d->beginBeamPathTable.reset(beginTableLatex);
  }
  padBeamPathTableBegin->Modified();
  padBeamPathTableBegin->Update();

  TPad* padBeamPathTableEnd = this->getPad(6);
  if (d->endBeamPathTable)
  {
    padBeamPathTableEnd->GetListOfPrimitives()->Remove(d->endBeamPathTable.get());
    d->endBeamPathTable.reset(nullptr);
  }
  padBeamPathTableEnd->cd();
  padBeamPathTableEnd->Clear();

  CameraProfiles endProfs(cam2);
  TLatex* endTableLatex = endProfs.createBeamPathTable(intergTime, totalStepsMs, cam2DateTime);
  if (endTableLatex)
  {
    d->endBeamPathTable.reset(endTableLatex);
  }
  padBeamPathTableEnd->Modified();
  padBeamPathTableEnd->Update();
}

QT_END_NAMESPACE
