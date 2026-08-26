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

#include <QDateTime>

QT_BEGIN_NAMESPACE

class TBeamTimeCanvasWidgetPrivate;
class TGraph;
class TLatex;
class AbstractCamera;

class TBeamTimeCanvasWidget : public TCanvasWidget {
  Q_OBJECT

public:
  TBeamTimeCanvasWidget(QWidget *parent = nullptr);
  ~TBeamTimeCanvasWidget() override;
  void updateBeamTimeGraphs(AbstractCamera* beginCam, AbstractCamera* endCam,
    QDateTime& cam1DateTime, QDateTime& cam2DateTime, int intergTime);

protected:
  QScopedPointer< TBeamTimeCanvasWidgetPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(TBeamTimeCanvasWidget);
  Q_DISABLE_COPY(TBeamTimeCanvasWidget);
};


QT_END_NAMESPACE
