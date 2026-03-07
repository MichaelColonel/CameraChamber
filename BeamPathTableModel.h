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

#include <QAbstractTableModel>
#include <QMap>
#include <QPair>

#include "typedefs.h"

QT_BEGIN_NAMESPACE

class BeamPathTableModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  static constexpr int NOF_HEADERS = 2;
  BeamPathTableModel(QObject *parent = nullptr);
  void setBeamPath(const BeamPathMap& newBeamPathMap);
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
protected:
  QString headerAt(int offset) const;
  BeamPathMap beamMap;
  const QStringList tableHeadersList{
    "IC1 / Wall",
    "IC2 / Isocenter"
  };
};

QT_END_NAMESPACE
