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

#include <TDirectory.h>
#include <TTree.h>

#include <QDebug>

#include "CameraSpillsListModel.h"

QT_BEGIN_NAMESPACE

CameraSpillsListModel::CameraSpillsListModel(QObject *parent)
  :
  QAbstractListModel(parent)
{

}

int CameraSpillsListModel::rowCount(const QModelIndex &) const
{
  return this->cameraSpillsVector.size();
}

int CameraSpillsListModel::columnCount(const QModelIndex &) const
{
  return CameraSpillsListModel::NOF_HEADERS;
}

QVariant CameraSpillsListModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
  {
    return QVariant();
  }
  if (role == Qt::TextAlignmentRole)
  {
    return int(Qt::AlignLeft | Qt::AlignVCenter);
  }
  if (role == Qt::DisplayRole)
  {
    if (index.row() >= this->cameraSpillsVector.size())
    {
      return QVariant();
    }
    if (index.column())
    {
      return QVariant();
    }
    const QPair< TDirectory*, TTree* >& dirTreePair = this->cameraSpillsVector.at(index.row());
    TDirectory* cameraDir = dirTreePair.first;
    TTree* spillTree = dirTreePair.second;
    QVariant res;
    switch (index.column())
    {
    case 0:
      {
        if (cameraDir && spillTree)
        {
          res = tr("[%1] : %2").arg(QString(cameraDir->GetName())).arg(QString(spillTree->GetName()));
        }
        else
        {
          res = QString(tr("Invalid spill data"));
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

QVariant CameraSpillsListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
  {
    return headerAt(section);
  }
  return QVariant();
}

QString CameraSpillsListModel::headerAt(int offset) const
{
  if (offset >= 0 && offset < CameraSpillsListModel::NOF_HEADERS)
  {
    return QString("Camera spill");
  }
  return QString();
}

void CameraSpillsListModel::setSpillsData(const std::vector< std::pair< TDirectory*, TTree* > >& dirSpillVector)
{
  this->beginResetModel();
  this->cameraSpillsVector.clear();
  for (const auto& dirSpillPair : dirSpillVector)
  {
    this->cameraSpillsVector.push_back(qMakePair(dirSpillPair.first, dirSpillPair.second));
  }
  this->endResetModel();
}

std::pair< TDirectory*, TTree* > CameraSpillsListModel::getSpillData(const QModelIndex& index) const
{
  if (index.row() < this->cameraSpillsVector.size())
  {
    const auto& pair = this->cameraSpillsVector.at(index.row());
    return std::make_pair(pair.first, pair.second);
  }
  return std::make_pair(nullptr, nullptr);
}

QT_END_NAMESPACE
