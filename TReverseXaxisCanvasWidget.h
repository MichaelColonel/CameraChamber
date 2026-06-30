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

#include "TCanvasWidget.h"

#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <TGaxis.h>
#include <TPad.h>

#include <typeinfo>
#include <memory>

#include <QDebug>

QT_BEGIN_NAMESPACE

class QWidget;

class TReverseXaxisCanvasWidget : public TCanvasWidget {
  Q_OBJECT

public:
  TReverseXaxisCanvasWidget(QWidget *parent = nullptr);
  ~TReverseXaxisCanvasWidget() override;
  void setGridPad(TPad*);
  TPad* getGridPad() const;
  /// Remove diagram T* from top grid pad
  template<class T> void removeDiagram(T*);
  /// Draw diagram T* in top grid pad
  template<class T> void drawDiagram(T*);
  /// Update reversed X-axis for diagram T
  template<class T> void updateDiagram(T*);

protected:
  std::unique_ptr< TPad > gridPad;
  std::unique_ptr< TGaxis > reversedXaxis;
};

template<class T> void TReverseXaxisCanvasWidget::drawDiagram(T* tnamedDiagram)
{
  if (this->gridPad)
  {
    this->gridPad->cd();
    if (typeid(T) == typeid(TGraph))
    {
      this->gridPad->Clear();
      tnamedDiagram->Draw();
    }
    else if (typeid(T) == typeid(TH1))
    {
      tnamedDiagram->Draw();
    }
    else if (typeid(T) == typeid(TH2))
    {
      tnamedDiagram->Draw("COLZ");
    }
    else
    {
      ;
    }
  }
}

template<class T> void TReverseXaxisCanvasWidget::updateDiagram(T* tnamedDiagram)
{
  if (!this->gridPad || !tnamedDiagram)
  {
    return;
  }
  this->gridPad->cd();
  // Remove the current axis
  tnamedDiagram->GetXaxis()->SetLabelOffset(999);
  tnamedDiagram->GetXaxis()->SetTickLength(0);

  // Redraw the new axis
  this->gridPad->Update();
  if (this->reversedXaxis)
  {
    this->gridPad->GetListOfPrimitives()->Remove(this->reversedXaxis.get());
  }
  TGaxis *newaxis = new TGaxis(this->gridPad->GetUxmax(),
    this->gridPad->GetUymin(),
    this->gridPad->GetUxmin(),
    this->gridPad->GetUymin(),
    tnamedDiagram->GetXaxis()->GetXmin(),
    tnamedDiagram->GetXaxis()->GetXmax(),
    510, "-");
  this->reversedXaxis.reset(newaxis);
  this->reversedXaxis->SetLabelOffset(-0.01);
  this->reversedXaxis->Draw();

  this->gridPad->Modified();
  this->gridPad->Update();
}


template<class T> void TReverseXaxisCanvasWidget::removeDiagram(T* tnamedDiagram)
{
  if (!this->gridPad)
  {
    return;
  }
  this->gridPad->cd();
  if (tnamedDiagram)
  {
    this->gridPad->GetListOfPrimitives()->Remove(tnamedDiagram);
  }
  if (this->reversedXaxis)
  {
    this->gridPad->GetListOfPrimitives()->Remove(this->reversedXaxis.get());
  }
}

QT_END_NAMESPACE
