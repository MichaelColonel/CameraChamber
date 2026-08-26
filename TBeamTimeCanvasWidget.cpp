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

#include "TBeamTimeCanvasWidget.h"

#include <TCanvas.h>
#include <TGraph.h>
#include <TMultiGraph.h>

#include "CameraProfiles.h"
#include "AbstractCamera.h"

QT_BEGIN_NAMESPACE

class TBeamTimeCanvasWidgetPrivate
{
  Q_DECLARE_PUBLIC(TBeamTimeCanvasWidget);
protected:
  TBeamTimeCanvasWidget* const q_ptr;
public:
  TBeamTimeCanvasWidgetPrivate(TBeamTimeCanvasWidget &object);
  virtual ~TBeamTimeCanvasWidgetPrivate();

  std::unique_ptr< TMultiGraph > beginBeamCenterMultiGraph;
  std::unique_ptr< TMultiGraph > beginBeamWidthMultiGraph;

  std::unique_ptr< TMultiGraph > endBeamCenterMultiGraph;
  std::unique_ptr< TMultiGraph > endBeamWidthMultiGraph;

  std::unique_ptr< TGraph > beginBeamCenterVerticalGraph;
  std::unique_ptr< TGraph > beginBeamCenterHorizontalGraph;

  std::unique_ptr< TGraph > beginBeamWidthVerticalGraph;
  std::unique_ptr< TGraph > beginBeamWidthHorizontalGraph;

  std::unique_ptr< TGraph > endBeamCenterVerticalGraph;
  std::unique_ptr< TGraph > endBeamCenterHorizontalGraph;

  std::unique_ptr< TGraph > endBeamWidthVerticalGraph;
  std::unique_ptr< TGraph > endBeamWidthHorizontalGraph;
};

TBeamTimeCanvasWidgetPrivate::TBeamTimeCanvasWidgetPrivate(TBeamTimeCanvasWidget &object)
  :
  q_ptr(&object)
{
}

TBeamTimeCanvasWidgetPrivate::~TBeamTimeCanvasWidgetPrivate()
{
}

TBeamTimeCanvasWidget::TBeamTimeCanvasWidget(QWidget *parent)
  :
  TCanvasWidget(parent),
  d_ptr(new TBeamTimeCanvasWidgetPrivate(*this))
{
  Q_D(TBeamTimeCanvasWidget);

  setObjectName("TBeamTimeCanvasWidget");
  TCanvas* canvas = this->getCanvas();
  if (canvas)
  {
    canvas->Divide(2, 1);
  }
}

TBeamTimeCanvasWidget::~TBeamTimeCanvasWidget()
{
  Q_D(TBeamTimeCanvasWidget);
  TPad* padCenter = this->getPad(1);
  TPad* padWidth = this->getPad(2);
  if (padCenter)
  {
    padCenter->Clear();
  }
  if (padWidth)
  {
    padWidth->Clear();
  }
}

void TBeamTimeCanvasWidget::updateBeamTimeGraphs(AbstractCamera* beginCam, AbstractCamera* endCam,
  QDateTime& cam1DateTime, QDateTime& cam2DateTime, int intergTime)
{
  Q_D(TBeamTimeCanvasWidget);
}

QT_END_NAMESPACE
