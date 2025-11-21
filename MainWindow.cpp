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

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "CameraProfilesDialog.h"

#include <TCanvas.h>
#include <TH1.h>
#include <TMath.h>
#include <TFormula.h>
#include <TF1.h>
#include <TPad.h>
#include <TROOT.h>

// RapidJSON includes
#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/filereadstream.h>

#include <QPushButton>
#include <QDebug>

#include "FullCamera.h"

MainWindow::MainWindow(QWidget *parent)
  :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
   gROOT->SetWebDisplay("qt5");
#else
   gROOT->SetWebDisplay("qt6");
#endif

#if (!QT_NO_DEBUG)
  qDebug() << Q_FUNC_INFO << "Debug info";
#endif

  this->ui->setupUi(this);
  this->getCamerasAvailable();
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::onConnectCamera()
{
  bool connected = false;
  QString commandDevice, dataDevice;
  QString cameraID = this->ui->ComboBox_Cameras->currentText();
  // 0 - for camera[0], 1 - for camera[1]
  bool cameraIndex = this->ui->RadioButton_CameraEnd->isChecked();

  for (CameraDeviceData data : this->camerasAvalable)
  {
    if (cameraID == QString::fromStdString(data.id))
    {
      commandDevice = QString::fromStdString(data.commandDeviceName);
      dataDevice = QString::fromStdString(data.dataDeviceName);
      break;
    }
  }
  if (this->getCamera(cameraIndex))
  {
    connected = false;
  }
  else
  {
    bool commandConnected = this->getCamera(cameraIndex)->isDeviceAlreadyConnected(commandDevice);
    bool dataConnected = this->getCamera(cameraIndex)->isDeviceAlreadyConnected(dataDevice);
    connected = (commandConnected || dataConnected);
  }

  if (!connected && !commandDevice.isEmpty() && !dataDevice.isEmpty())
  {
    if (cameraID == "Camera1" || cameraID == "Camera3")
    {
      this->camera[cameraIndex] = new FullCamera(commandDevice, dataDevice, this);
    }
  }
  else if (connected && !commandDevice.isEmpty() && !dataDevice.isEmpty())
  {
    return; // Error message, camera has been already connected
  }
  if (this->getCamera(cameraIndex))
  {
    ; // Do connection, and other things (update UI, etc)
  }
}

void MainWindow::onDisconnectCamera()
{

}

void MainWindow::getCamerasAvailable()
{
  // Load JSON descriptor file
  FILE *fp = fopen("Cameras.json", "r");
  if (!fp)
  {
    qWarning() << Q_FUNC_INFO << ": Can't open JSON file \"Cameras.json\"";
    return;
  }
  const size_t size = 100000;
  std::unique_ptr< char[] > buffer(new char[size]);
  rapidjson::FileReadStream fs(fp, buffer.get(), size);

  rapidjson::Document d;
  if (d.ParseStream(fs).HasParseError())
  {
    qWarning() << Q_FUNC_INFO << "Can't parse JSON file\"Cameras.json\"";
    fclose(fp);
    return;
  }
  fclose(fp);

  const rapidjson::Value& cameraValues = d["Cameras"];
  if (cameraValues.IsArray())
  {
    this->camerasAvalable.clear();
    for (rapidjson::SizeType i = 0; i < cameraValues.Size(); i++) // Uses SizeType instead of size_t
    {
      CameraDeviceData cameraData;
      const rapidjson::Value& camera = cameraValues[i];
      if (!camera.IsObject())
      {
        continue;
      }
      cameraData.id = std::string(camera["ID"].GetString());
      cameraData.dataDirectory = std::string(camera["directory"].GetString());
      cameraData.commandDeviceName = std::string(camera["command-device"].GetString());
      cameraData.dataDeviceName = std::string(camera["data-device"].GetString());
      this->camerasAvalable.push_back(cameraData);
    }
  }
  this->ui->ComboBox_Cameras->clear();
  for (CameraDeviceData cameraData : this->camerasAvalable)
  {
    this->ui->ComboBox_Cameras->addItem(QString::fromStdString(cameraData.id));
  }
}
