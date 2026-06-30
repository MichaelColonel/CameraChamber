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

#include "TReverseXaxisCanvasWidget.h"

#include <TCanvas.h>

QT_BEGIN_NAMESPACE

TReverseXaxisCanvasWidget::TReverseXaxisCanvasWidget(QWidget *parent)
  :
  TCanvasWidget(parent)
{
  setObjectName("TReverseXaxisCanvasWidget");
}

TReverseXaxisCanvasWidget::~TReverseXaxisCanvasWidget()
{
}

void TReverseXaxisCanvasWidget::setGridPad(TPad* newGridPad)
{
  TCanvas* canvas = this->getCanvas();
  canvas->cd();
  this->gridPad.reset(newGridPad);
//  this->gridPad->SetGrid();
  this->gridPad->Draw();
}

TPad* TReverseXaxisCanvasWidget::getGridPad() const
{
  return this->gridPad.get();
}


QT_END_NAMESPACE

