// Initial qtweb author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#pragma once

#include <QWidget>

#include <QDateTime>
#include <QByteArray>
#include <QMainWindow>
#include <QSerialPort> // Serial port
#include <QOpcUaClient> // OPC UA client

#include <list>
#include <array>
#include <map>
#include <memory>
#include <chrono>

#include "typedefs.h"

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

class QTimer;
class QOpcUaClient;
class QOpcUaProvider;
class OpcUaModel;

class TFile;
class TTree;
class TGraph;
class TPad;
class TCanvas;
class TH2;
class TH1;
class TH2F;
class TH1F;
class TLine;
class TGraphErrors;
class TMultiGraph;
class TF1;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

public slots:
  void onUpdateButtonClicked();

protected:
  Ui::MainWindow* ui{ nullptr };
 
  TH1F* hist{nullptr};  ///< histogram for display in TCanvas
  QSerialPort* commandPort{ nullptr };
  QSerialPort* dataPort{ nullptr };

  std::chrono::steady_clock::time_point timeWaitingBegin, timeWaitingEnd;
  QTimer* timeoutTimer{ nullptr }; /// extraction (spill) or initiation timeout

  QByteArray commandResponseBuffer;
  QByteArray dataResponseBuffer;

  TFile* rootFile{ nullptr };
  QString rootFileName;
  ChannelsCountsMap channelsCountsA; // Side-A
  ChannelsCountsMap channelsCountsB; // Side-B

  OpcUaModel* opcUaModelData{ nullptr };
  QOpcUaProvider* opcUaProvider{ nullptr };
  QOpcUaClient* opcUaClient{ nullptr };
};
