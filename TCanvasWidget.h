// Author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#pragma once

#include <QWidget>
#include <QWebEngineView>

class TCanvas;
class TPad;
class TObject;

class TCanvasWidget : public QWidget {
  Q_OBJECT

public:
  TCanvasWidget(QWidget *parent = nullptr);
  ~TCanvasWidget() override;

  /// returns canvas shown in the widget
  TCanvas* getCanvas() { return rootCanvas; }
  TPad* getPad() const;

signals:
  void CanvasUpdated();
  void SelectedPadChanged(TPad*);
  void PadClicked(TPad*,int,int);
  void PadDblClicked(TPad*,int,int);

public slots:
  void activateEditor(TPad *pad = nullptr, TObject *obj = nullptr);
  void activateStatusLine();
  void setEditorVisible(bool flag = true);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void SetPrivateCanvasFields(bool on_init);
  QWebEngineView* view{nullptr};  ///< qt webwidget to show
  TCanvas* rootCanvas{nullptr};
};
