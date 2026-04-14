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

#include <QDebug>

// RapidJSON includes
#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/filereadstream.h>

#include "CameraUtils.h"

QT_BEGIN_NAMESPACE

namespace {

AbstractCamera::CameraDataList CameraList;

} // namespace

int CameraUtils::getNumberOfCamerasAvailable()
{
 const AbstractCamera::CameraDataList& cameras = getCamerasData();
 return cameras.size();
}

QStringList CameraUtils::getCameraIDs()
{
  const AbstractCamera::CameraDataList& cameras = getCamerasData();
  QStringList cameraIDs;
  for (const AbstractCamera::CameraDeviceData& cameraData : cameras)
  {
    cameraIDs.append(cameraData.ID);
  }
  return cameraIDs;
}

const QList< AbstractCamera::CameraDeviceData >& CameraUtils::getCamerasData()
{
  if (!CameraList.isEmpty())
  {
    return CameraList;
  }

  // Load JSON descriptor file
  FILE *fp = fopen("Cameras.json", "r");
  if (!fp)
  {
    qCritical() << Q_FUNC_INFO << QObject::tr("Can't open JSON file \"Cameras.json\"");
    return CameraList;
  }
  const size_t size = 100000;
  std::unique_ptr< char[] > buffer(new char[size]);
  rapidjson::FileReadStream fs(fp, buffer.get(), size);

  rapidjson::Document d;
  if (d.ParseStream(fs).HasParseError())
  {
    qCritical() << Q_FUNC_INFO << QObject::tr("Can't parse JSON file\"Cameras.json\"");
    fclose(fp);
    return CameraList;
  }
  fclose(fp);

  const rapidjson::Value& cameraValues = d["Cameras"];
  if (cameraValues.IsArray())
  {
    for (rapidjson::SizeType i = 0; i < cameraValues.Size(); i++) // Uses SizeType instead of size_t
    {
      AbstractCamera::CameraDeviceData cameraData;
      const rapidjson::Value& camera = cameraValues[i];
      if (!camera.IsObject())
      {
        continue;
      }
      cameraData.ID = QString(camera["ID"].GetString());
      cameraData.DataDirectory = QString(camera["directory"].GetString());
      cameraData.CommandDeviceName = QString(camera["command-device"].GetString());
      cameraData.DataDeviceName = QString(camera["data-device"].GetString());
      CameraList.append(cameraData);
    }
  }
  return CameraList;
}

AbstractCamera::CameraDeviceData CameraUtils::getFullCameraData(int cameraNumber)
{
  AbstractCamera::CameraDeviceData data;
  auto camerasAvailable = CameraUtils::getCamerasData();
  if (cameraNumber <= 0 || cameraNumber > camerasAvailable.size())
  {
    return data;
  }
  QString cameraID = QString("Camera") + QString::number(cameraNumber);
  for (const AbstractCamera::CameraDeviceData& cameraData : camerasAvailable)
  {
    if (cameraID == cameraData.ID)
    {
      data = cameraData;
    }
  }
  return data;
}

AbstractCamera::CameraDeviceData CameraUtils::getPartialCamera2Data()
{
  AbstractCamera::CameraDeviceData data;
  auto camerasAvailable = CameraUtils::getCamerasData();

  QString cameraID = QString("Camera2");
  for (const AbstractCamera::CameraDeviceData& cameraData : camerasAvailable)
  {
    if (cameraID == cameraData.ID)
    {
      data = cameraData;
    }
  }
  return data;
}

QT_END_NAMESPACE
