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

#include <QAbstractListModel>
#include <QVector>
#include <QPair>
#include <QMap>

class TDirectory;
class TTree;

QT_BEGIN_NAMESPACE

class CameraSpillsListModel : public QAbstractListModel
{
  Q_OBJECT
public:
  static constexpr int NOF_HEADERS = 1;
  explicit CameraSpillsListModel(QObject *parent = nullptr);
  void clearListModel();
  void setSpillsData(const std::vector< std::pair< TDirectory*, TTree* > >& dirSpillVector);
  std::pair< TDirectory*, TTree* > getSpillData(const QModelIndex& index) const;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
protected:
  QString headerAt(int offset) const;
  QVector< QPair< TDirectory*, TTree* > > cameraSpillsVector;
//  QMap< TDirectory*, QVector< TTree* > > cameraSpillsMap; // for future use
};

QT_END_NAMESPACE
