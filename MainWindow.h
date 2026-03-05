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

#pragma once

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QScopedArrayPointer>
#include <QPointer>

// #include <QOpcUaClient> // OPC UA client

#include <TFile.h>
#include <TDirectory.h>
#include <THttpServer.h>

#include <memory>

#include "AbstractCamera.h"

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}

class QOpcUaClient;
class QOpcUaProvider;
class OpcUaModel;
class QCheckBox;
class QAbstractButton;
class QTimer;
class QProgressDialog;

class CameraProfilesDialog;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  Q_INVOKABLE void log(const QString &text, const QString &context, QColor color);
  void log(const QString &text, QColor color = Qt::black);

private slots:
  void onSelectedCameraChanged(const QString& cameraID);
  void onConnectCameraClicked();
  void onDisconnectCameraClicked();
  void onChipsEnabledChanged();
  void onChipResetClicked();
  void onAlteraResetClicked();
  void onAcquisitionAdcModeResolutionChanged(QAbstractButton*);
  void onAcquisitionClicked();
  void onOnceTimeExternalStartClicked();
  void onWriteCapacitiesClicked();
  void onSetIntegrationTimeClicked();
  void onSetCapacityClicked();
  void onSetChipsEnabledClicked();
  void onSetSamplesClicked();
  void onSetNumberOfChipsClicked();
  void onSetAdcResolutionClicked();
  void onSetExternalStartClicked();
  void onInitiateCameraClicked();
  void onCameraCommandWritten(const QByteArray& command);
  void onCameraInitiationStarted();
  void onCameraInitiationInProgress(int prog);
  void onCameraInitiationFinished();
  void onCameraFirstContactTimeout();
  void onCameraFirstContactFinished();
  void onCameraAcquisitionStarted();
  void onCameraAcquisitionFinished();
  void onOpenRootFileActionTriggered();
  void onCameraSettingsActionTriggered();
  void onHttpServerActionTriggered();
  void onBeamActionTriggered();

private:
  void updateUiState();

protected:
  int getChipsEnabledCode() const;
  void getCamerasAvailable();
  void updateAcquisitionParameters(AbstractCamera* cameraDevice);

  AbstractCamera* getCamera(const QString& cameraID) const;
  CameraProfilesDialog* getProfiles(const QString& cameraID) const;
  AbstractCamera* getCurrentCamera() const;
  CameraProfilesDialog* getCurrentProfiles() const;

  QScopedPointer< Ui::MainWindow > ui;
  bool cameraConnectedFlag{ false };

  QScopedPointer< QTimer > initiationTimer;
  QScopedPointer< QProgressDialog > initiationProgress;

  // ROOT file name for cameras data
  QString rootFileName;
  std::unique_ptr< TFile > rootFile;
  std::shared_ptr< THttpServer > httpServer;

//  OpcUaModel* opcUaModelData{ nullptr };
//  QOpcUaProvider* opcUaProvider{ nullptr };
//  QOpcUaClient* opcUaClient{ nullptr };

  typedef QPair< QPointer< AbstractCamera >, QPointer< CameraProfilesDialog > > CameraDeviceProfilesPair;
  typedef QMap< QString, CameraDeviceProfilesPair > CameraDeviceProfilesMap;
  CameraDeviceProfilesMap cameraDeviceProfilesMap;
};

QT_END_NAMESPACE
