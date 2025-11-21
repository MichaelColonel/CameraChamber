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

#include <QOpcUaClient> // OPC UA client
#include <QScopedArrayPointer>

#include <list>
#include <array>
#include <map>
#include <memory>
#include <chrono>

#include "AbstractCamera.h"
#include "typedefs.h"

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

class QOpcUaClient;
class QOpcUaProvider;
class OpcUaModel;

class TFile;
class CameraProfilesDialog;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;
  AbstractCamera* cameraBegin() const { return this->camera[0]; }
  AbstractCamera* cameraEnd() const { return this->camera[1]; }
  AbstractCamera* getCamera(bool index = false) const { return this->camera[index]; }

private slots:
  void onConnectCamera();
  void onDisconnectCamera();

protected:
  void getCamerasAvailable();

  Ui::MainWindow* ui{ nullptr };

  TFile* rootFile{ nullptr };
  QString rootFileName;

  OpcUaModel* opcUaModelData{ nullptr };
  QOpcUaProvider* opcUaProvider{ nullptr };
  QOpcUaClient* opcUaClient{ nullptr };
  AbstractCamera* camera[2]{ nullptr, nullptr };
  CameraProfilesDialog* cameraDialog[2]{ nullptr, nullptr };

  struct CameraDeviceData {
    std::string id;
    std::string dataDirectory;
    std::string commandDeviceName;
    std::string dataDeviceName;
  };
  std::list< CameraDeviceData > camerasAvalable;
};
