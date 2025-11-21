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

#include "AbstractCamera.h"

AbstractCamera::AbstractCamera(const QString& commandPortDeviceName,
  const QString& dataPortDeviceName, QObject *parent)
  :
  QObject(parent)
{
  this->commandPortName = commandPortDeviceName;
  this->dataPortName = dataPortDeviceName;
}

AbstractCamera::~AbstractCamera()
{
}

bool AbstractCamera::connect()
{

}

void AbstractCamera::disconnect()
{

}

bool AbstractCamera::isDeviceAlreadyConnected(const QString &otherPort)
{
  if (this->commandPort && this->commandPort->isOpen())
  {
    return this->commandPortName == otherPort;
  }
  if (this->dataPort && this->dataPort->isOpen())
  {
    return this->dataPortName == otherPort;
  }
  return false;
}

void AbstractCamera::onCommandPortDataReady()
{
}

void AbstractCamera::onDataPortDataReady()
{
}
