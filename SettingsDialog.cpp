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

#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <QScopedPointer>
#include <QPointer>
#include <QMap>
#include <QString>
#include <QTimer>
#include <QDebug>
#include <QSettings>

#include "FullCamera.h"
#include "Camera2.h"
#include "CameraUtils.h"

QT_BEGIN_NAMESPACE

namespace Ui {
class SettingsDialog;
}

class SettingsDialogPrivate
{
  Q_DECLARE_PUBLIC(SettingsDialog);
protected:
  SettingsDialog* const q_ptr;
  QScopedPointer< Ui::SettingsDialog > ui;
public:
  SettingsDialogPrivate(SettingsDialog &object);
  virtual ~SettingsDialogPrivate();
  void updateChipChannel();
  void setChipChannel();
  bool isVerticalProfile() const;
  bool isHorizontalProfile() const;
  bool isAdcLinearCalibration() const;
  bool isAmpUniformCalibration() const;
  ChipChannelPair getChipChannel() const;

  AbstractCamera* camera{ nullptr };
  QSettings* settings{ nullptr };
};

SettingsDialogPrivate::SettingsDialogPrivate(SettingsDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::SettingsDialog)
{
  settings = new QSettings("ProfileCamera2D", "configure");
}

SettingsDialogPrivate::~SettingsDialogPrivate()
{
  delete settings;
  settings = nullptr;
}

bool SettingsDialogPrivate::isVerticalProfile() const
{
  QRadioButton* rButton = qobject_cast< QRadioButton* >(this->ui->ButtonGroup_ProfileType->checkedButton());
  if (rButton == this->ui->RadioButton_RefVerticalProfile)
  {
    return rButton->isChecked();
  }
  return false;
}

bool SettingsDialogPrivate::isHorizontalProfile() const
{
  QRadioButton* rButton = qobject_cast< QRadioButton* >(this->ui->ButtonGroup_ProfileType->checkedButton());
  if (rButton == this->ui->RadioButton_RefHorizontalProfile)
  {
    return rButton->isChecked();
  }
  return false;
}

bool SettingsDialogPrivate::isAmpUniformCalibration() const
{
  QRadioButton* rButton = qobject_cast< QRadioButton* >(this->ui->ButtonGroup_SignalCalibration->checkedButton());
  if (rButton == this->ui->RadioButton_RefAmpUniform)
  {
    return rButton->isChecked();
  }
  return false;
}

bool SettingsDialogPrivate::isAdcLinearCalibration() const
{
  QRadioButton* rButton = qobject_cast< QRadioButton* >(this->ui->ButtonGroup_SignalCalibration->checkedButton());
  if (rButton == this->ui->RadioButton_RefAdcLinear)
  {
    return rButton->isChecked();
  }
  return false;
}

void SettingsDialogPrivate::updateChipChannel()
{
  Q_Q(SettingsDialog);
  QString cameraID;
  if (this->camera)
  {
    cameraID = this->camera->getCameraData().ID;
  }
  else
  {
    cameraID = this->ui->ComboBox_Camera->currentText();
  }
  if (!cameraID.isEmpty())
  {
    this->settings->beginGroup(cameraID);
    if (this->isVerticalProfile() && this->isAdcLinearCalibration())
    {
      settings->setValue("ref-adc-chip-vertical", static_cast< int >(this->ui->SliderWidget_Chip->value()));
      settings->setValue("ref-adc-channel-vertical", static_cast< int >(this->ui->SliderWidget_Channel->value()));
    }
    else if (this->isHorizontalProfile() && this->isAdcLinearCalibration())
    {
      settings->setValue("ref-adc-chip-horizontal", static_cast< int >(this->ui->SliderWidget_Chip->value()));
      settings->setValue("ref-adc-channel-horizontal", static_cast< int >(this->ui->SliderWidget_Channel->value()));
    }
    else if (this->isVerticalProfile() && this->isAmpUniformCalibration())
    {
      settings->setValue("ref-amp-chip-vertical", static_cast< int >(this->ui->SliderWidget_Chip->value()));
      settings->setValue("ref-amp-channel-vertical", static_cast< int >(this->ui->SliderWidget_Channel->value()));
    }
    else if (this->isHorizontalProfile() && this->isAmpUniformCalibration())
    {
      settings->setValue("ref-amp-chip-horizontal", static_cast< int >(this->ui->SliderWidget_Chip->value()));
      settings->setValue("ref-amp-channel-horizontal", static_cast< int >(this->ui->SliderWidget_Channel->value()));
    }
    this->settings->endGroup();
  }
}

ChipChannelPair SettingsDialogPrivate::getChipChannel() const
{
  Q_Q(const SettingsDialog);
  QString cameraID;
  ChipChannelPair pair( -1, -1);
  if (this->camera)
  {
    cameraID = this->camera->getCameraData().ID;
  }
  else
  {
    cameraID = this->ui->ComboBox_Camera->currentText();
  }
  if (!cameraID.isEmpty())
  {
    this->settings->beginGroup(cameraID);
    if (this->isVerticalProfile() && this->isAdcLinearCalibration())
    {
      pair.first = settings->value("ref-adc-chip-vertical", 1).toInt();
      pair.second = settings->value("ref-adc-channel-vertical", 1).toInt();
    }
    else if (this->isHorizontalProfile() && this->isAdcLinearCalibration())
    {
      pair.first = settings->value("ref-adc-chip-horizontal", 1).toInt();
      pair.second = settings->value("ref-adc-channel-horizontal", 1).toInt();
    }
    else if (this->isVerticalProfile() && this->isAmpUniformCalibration())
    {
      pair.first = settings->value("ref-amp-chip-vertical", 1).toInt();
      pair.second = settings->value("ref-amp-channel-vertical", 1).toInt();
    }
    else if (this->isHorizontalProfile() && this->isAmpUniformCalibration())
    {
      pair.first = settings->value("ref-amp-chip-horizontal", 1).toInt();
      pair.second = settings->value("ref-amp-channel-horizontal", 1).toInt();
    }
    this->settings->endGroup();
  }
  return pair;
}

SettingsDialog::SettingsDialog(AbstractCamera* cameraDevice, QWidget *parent)
  :
  QDialog(parent),
  d_ptr(new SettingsDialogPrivate(*this))
{
  Q_D(SettingsDialog);
  d->ui->setupUi(this);

  auto cameraIDs = CameraUtils::getCameraIDs();
  for (const QString& id : cameraIDs)
  {
    d->ui->ComboBox_Camera->addItem(id);
  }
  if (cameraDevice)
  {
    d->camera = cameraDevice;
    ChipChannelPair refAdcVer = cameraDevice->getReferenceChipChannel(false, CameraProfileType::PROFILE_VERTICAL);
    d->ui->RadioButton_RefAdcLinear->setChecked(true);
    d->ui->RadioButton_RefVerticalProfile->setChecked(true);
    d->ui->SliderWidget_Chip->setValue(refAdcVer.first);
    d->ui->SliderWidget_Channel->setValue(refAdcVer.second);
  }
  connect(d->ui->ComboBox_Camera, SIGNAL(currentIndexChanged(QString)),
    this, SLOT(onCameraIdChanged(QString)));
  connect(d->ui->ButtonGroup_ProfileType, SIGNAL(buttonClicked(QAbstractButton*)),
    this, SLOT(onProfileTypeButtonChanged(QAbstractButton*)));
  connect(d->ui->ButtonGroup_SignalCalibration, SIGNAL(buttonClicked(QAbstractButton*)),
    this, SLOT(onSignalCalibrationTypeButtonChanged(QAbstractButton*)));
  connect(d->ui->SliderWidget_Channel, SIGNAL(valueChanged(double)),
    this, SLOT(onChannelNumberChanged(double)));
  connect(d->ui->SliderWidget_Chip, SIGNAL(valueChanged(double)),
    this, SLOT(onChipNumberChanged(double)));

  if (d->camera)
  {
    auto pos = std::find(cameraIDs.begin(), cameraIDs.end(), d->camera->getCameraData().ID);
    if (pos != cameraIDs.end())
    {
      d->ui->ComboBox_Camera->setCurrentIndex(pos - cameraIDs.begin());
      d->ui->ComboBox_Camera->setEnabled(false);
    }
  }

  {
    QSignalBlocker block1(d->ui->SliderWidget_Channel);
    QSignalBlocker block2(d->ui->SliderWidget_Chip);
    ChipChannelPair pair = d->getChipChannel();
    if (pair.first == -1 || pair.second == -1)
    {
      return;
    }
    d->ui->SliderWidget_Channel->setValue(pair.second);
    d->ui->SliderWidget_Chip->setValue(pair.first);
  }
}

SettingsDialog::~SettingsDialog()
{
  Q_D(SettingsDialog);
}

void SettingsDialog::onSignalCalibrationTypeButtonChanged(QAbstractButton *but)
{
  Q_D(SettingsDialog);
  QRadioButton* rButton = qobject_cast< QRadioButton* >(but);
  ChipChannelPair pair = d->getChipChannel();
  if (rButton && pair.first != -1 && pair.second != -1)
  {
    d->ui->SliderWidget_Channel->setValue(pair.second);
    d->ui->SliderWidget_Chip->setValue(pair.first);
  }
}

void SettingsDialog::onProfileTypeButtonChanged(QAbstractButton *but)
{
  Q_D(SettingsDialog);
  QRadioButton* rButton = qobject_cast< QRadioButton* >(but);
  ChipChannelPair pair = d->getChipChannel();
  if (rButton && pair.first != -1 && pair.second != -1)
  {
    d->ui->SliderWidget_Channel->setValue(pair.second);
    d->ui->SliderWidget_Chip->setValue(pair.first);
  }
}

void SettingsDialog::onChipNumberChanged(double)
{
  Q_D(SettingsDialog);
  d->updateChipChannel();
}

void SettingsDialog::onChannelNumberChanged(double)
{
  Q_D(SettingsDialog);
  d->updateChipChannel();
}

void SettingsDialog::onCameraIdChanged(const QString&)
{
  Q_D(SettingsDialog);
  d->updateChipChannel();
}

QT_END_NAMESPACE
