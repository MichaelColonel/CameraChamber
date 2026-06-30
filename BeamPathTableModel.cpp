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

#include "BeamPathTableModel.h"

BeamPathTableModel::BeamPathTableModel(QObject *parent) : QAbstractTableModel(parent)
{

}

int BeamPathTableModel::rowCount(const QModelIndex &) const
{
  return this->beamProfileMap.size();
}

int BeamPathTableModel::columnCount(const QModelIndex &) const
{
  return NOF_HEADERS;
}

QVariant BeamPathTableModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
  {
    return QVariant();
  }
  if (role == Qt::TextAlignmentRole)
  {
    return int(Qt::AlignHCenter | Qt::AlignVCenter);
  }
  if (role == Qt::DisplayRole)
  {
    if (index.row() >= this->beamProfileMap.size())
    {
      return QVariant();
    }
    if (index.column() >= NOF_HEADERS)
    {
      return QVariant();
    }
    double time = -1.;
    BeamProfileArray prof{ 0., 0., -1., -1.};
    int i = 0;
    for (auto iter = this->beamProfileMap.begin(); iter != this->beamProfileMap.end(); ++iter, ++i)
    {
      if (i == index.row())
      {
        time = iter->first;
        prof = iter->second;
      }
    }
    QVariant res;
    switch (index.column())
    {
    case 0:
      res = QString::number(time);
      break;
    case 1:
      {
        if (prof[2] < 0)
        {
          res = tr("No beam");
        }
        else
        {
          res = QObject::tr("%1").arg(-1. * prof[0], 5, 'f', 1, QChar(' ')); // position X
        }
      }
      break;
    case 2:
      {
        if (prof[2] < 0)
        {
          res = tr("No beam");
        }
        else
        {
          res = QObject::tr("%1").arg(prof[1], 5, 'f', 1, QChar(' ')); // position Y
        }
      }
      break;
    case 3:
      {
        if (prof[2] < 0)
        {
          res = tr("No beam");
        }
        else
        {
          res = QObject::tr("%1").arg(6. * prof[2], 5, 'f', 1, QChar(' ')); // Diameter Dx (== 6sigma)
        }
      }
      break;
    case 4:
      {
        if (prof[2] < 0)
        {
          res = tr("No beam");
        }
        else
        {
          res = QObject::tr("%1").arg(6. * prof[3], 5, 'f', 1, QChar(' ')); // Diameter Dy (== 6sigma)
        }
      }
      break;
    default:
      res = QVariant();
      break;
    }
    return res;
  }
  return QVariant();
}

QVariant BeamPathTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
  {
    return headerAt(section);
  }
  return QVariant();
}

QString BeamPathTableModel::headerAt(int offset) const
{
  if (offset >= 0 && offset < tableHeadersList.size())
  {
    return tableHeadersList.at(offset);
  }
  return QString();
}

void BeamPathTableModel::setBeamProfile(const BeamProfileMap& newBeamProfileMap)
{
  this->beginResetModel();
  this->beamProfileMap = newBeamProfileMap;
  this->endResetModel();
}
