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
  return this->beamMap.size();
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
    if (index.row() >= this->beamMap.size())
    {
      return QVariant();
    }
    if (index.column() >= NOF_HEADERS)
    {
      return QVariant();
    }
    double time = -1.;
    BeamPathPair pair{ { 0., 0., -1.}, { 0., 0., -1.} };
    int i = 0;
    for (auto iter = this->beamMap.begin(); iter != this->beamMap.end(); ++iter, ++i)
    {
      if (i == index.row())
      {
        time = iter->first;
        pair = iter->second;
      }
    }
    QVariant res;
    switch (index.column())
    {
    case 0:
      res = QObject::tr("%1, %2, %3").arg(pair.first[0]).arg(pair.first[1]).arg(pair.first[2]); // pos begin
      break;
    case 1:
      res = QObject::tr("%1, %2, %3").arg(pair.second[0]).arg(pair.second[1]).arg(pair.second[2]); // pos end
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

void BeamPathTableModel::setBeamPath(const BeamPathMap& newBeamPathMap)
{
  this->beginResetModel();
  this->beamMap = newBeamPathMap;
  this->endResetModel();
}
