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
#include <QSize>

#include "ChannelInfoTableModel.h"

QT_BEGIN_NAMESPACE

ChannelInfoTableModel::ChannelInfoTableModel(QObject *parent)
  :
  QAbstractTableModel(parent)
{
}

int ChannelInfoTableModel::rowCount(const QModelIndex &) const
{
  return this->infoMap.size();
}

int ChannelInfoTableModel::columnCount(const QModelIndex &) const
{
  return NOF_HEADERS;
}

QVariant ChannelInfoTableModel::data(const QModelIndex& index, int role) const
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
    if (index.row() >= this->infoMap.size())
    {
      return QVariant();
    }
    if (index.column() >= NOF_HEADERS)
    {
      return QVariant();
    }
    int chipIndex = index.row() / CHANNELS_PER_CHIP;
    int channelIndex = index.row() - (chipIndex * CHANNELS_PER_CHIP);
    ChipChannelPair chipChannel(chipIndex + 1, channelIndex + 1);
    int i = 0;
    for (auto iter = this->infoMap.begin(); iter != this->infoMap.end(); ++iter, ++i)
    {
      if (i == index.row())
      {
        chipChannel = iter->first;
      }
    }
    const ChannelInfoPair& infoPair = this->infoMap.at(chipChannel);
    const ChannelInfo& rawInfo = infoPair.first;
    const ChannelInfo& calibInfo = infoPair.second;
    QVariant res;
    switch (index.column())
    {
    case 0:
      res = QString::number(chipChannel.first); // chipIndex + 1
      break;
    case 1:
      res = QString::number(chipChannel.second); // channelIndex + 1
      break;
    case 2:
      res = QString::number(rawInfo.PedMeanA);
      break;
    case 3:
      res = QString::number(std::sqrt(rawInfo.PedMom2A - rawInfo.PedMeanA * rawInfo.PedMeanA));
      break;
    case 4:
      res = QString::number(rawInfo.PedMeanB);
      break;
    case 5:
      res = QString::number(std::sqrt(rawInfo.PedMom2B - rawInfo.PedMeanB * rawInfo.PedMeanB));
      break;
    case 6:
      res = QString::number(rawInfo.SigMeanA);
      break;
    case 7:
      res = QString::number(std::sqrt(rawInfo.SigMom2A - rawInfo.SigMeanA * rawInfo.SigMeanA));
      break;
    case 8:
      res = QString::number(rawInfo.SigMeanB);
      break;
    case 9:
      res = QString::number(std::sqrt(rawInfo.SigMom2B - rawInfo.SigMeanB * rawInfo.SigMeanB));
      break;
    case 10: // Mean ADC raw signal-A
      res = QString::number(rawInfo.SigMeanA - rawInfo.PedMeanA);
      break;
    case 11: // Mean ADC raw signal-B
      res = QString::number(rawInfo.SigMeanB - rawInfo.PedMeanB);
      break;
    case 12: // Mean ADC raw SNR-A
      {
        double sigA = std::sqrt(rawInfo.SigMom2A - rawInfo.SigMeanA * rawInfo.SigMeanA);
        if (sigA == 0.0)
        {
          res = "-0.0";
        }
        else
        {
          res = QString::number((rawInfo.SigMeanA - rawInfo.PedMeanA) / sigA);
        }
      }
      break;
    case 13: // Mean ADC raw SNR-B
      {
        double sigB = std::sqrt(rawInfo.SigMom2B - rawInfo.SigMeanB * rawInfo.SigMeanB);
        if (sigB == 0.0)
        {
          res = "-0.0";
        }
        else
        {
          res = QString::number((rawInfo.SigMeanB - rawInfo.PedMeanB) / sigB);
        }
      }
      break;
    case 14: // raw integral signal
      res = QString::number(rawInfo.Signal);
      break;
    case 15: // raw integral signal no amplification
      res = QString::number(rawInfo.SignalNoAmp);
      break;
    case 16: // calib integral signal
      res = QString::number(calibInfo.Signal);
      break;
    case 17:  // calib integral signal no amplification
      res = QString::number(calibInfo.SignalNoAmp);
      break;
    default:
      res = QVariant();
      break;
    }
    return res;
  }
  return QVariant();
}

QVariant ChannelInfoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
  {
    return headerAt(section);
  }
  return QVariant();
}

QString ChannelInfoTableModel::headerAt(int offset) const
{
  if (offset >= 0 && offset < tableHeadersList.size())
  {
    return tableHeadersList.at(offset);
  }
  return QString();
}

void ChannelInfoTableModel::setChipChannelInfo(const std::map< ChipChannelPair, ChannelInfoPair >& newInfoMap)
{
  this->beginResetModel();
  this->infoMap.clear();
  for (const auto& chipChannelInfoPair : newInfoMap)
  {
    this->infoMap.insert(chipChannelInfoPair);
  }
  this->endResetModel();
}

QT_END_NAMESPACE
