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

class TBeamPathCanvasWidgetPrivate;
class TGraph;
class TLatex;
class AbstractCamera;

class TBeamPathCanvasWidget : public TCanvasWidget {
  Q_OBJECT

public:
  TBeamPathCanvasWidget(QWidget *parent = nullptr);
  ~TBeamPathCanvasWidget() override;
  void updateProfiles(TGraph* cam1HorizProfile, TGraph* cam1VertProfile,
    TGraph* cam2HorizProfile, TGraph* cam2VertProfile);
  void updateBeamPathTables(AbstractCamera* cam1, AbstractCamera* cam2,
    QDateTime& cam1DateTime, QDateTime& cam2DateTime, int intergTime);

protected:
  QScopedPointer< TBeamPathCanvasWidgetPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(TBeamPathCanvasWidget);
  Q_DISABLE_COPY(TBeamPathCanvasWidget);
};


QT_END_NAMESPACE
