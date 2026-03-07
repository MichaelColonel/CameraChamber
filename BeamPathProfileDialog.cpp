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

#include "BeamPathProfileDialog.h"
#include "ui_BeamPathProfileDialog.h"

#include <QPointer>

#include <THttpServer.h>
#include <TCanvas.h>
#include <TLatex.h>
#include <TPad.h>

#include "AbstractCamera.h"
#include "BeamPathTableModel.h"

QT_BEGIN_NAMESPACE

class BeamPathProfileDialogPrivate
{
  Q_DECLARE_PUBLIC(BeamPathProfileDialog);
protected:
  BeamPathProfileDialog* const q_ptr;
  QScopedPointer< Ui::BeamPathProfileDialog > ui;
public:
  BeamPathProfileDialogPrivate(BeamPathProfileDialog &object);
  virtual ~BeamPathProfileDialogPrivate();

  QPointer< AbstractCamera > camera1; // first camera (begin), near collimator-1 or wall
  QPointer< AbstractCamera > camera2; // last camera (end), near isocenter

  std::unique_ptr< TLatex > latex;
  QScopedPointer< BeamPathTableModel > beamPathModel;
};

BeamPathProfileDialogPrivate::BeamPathProfileDialogPrivate(BeamPathProfileDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::BeamPathProfileDialog),
  beamPathModel(new BeamPathTableModel(&object))
{
}

BeamPathProfileDialogPrivate::~BeamPathProfileDialogPrivate()
{
}

BeamPathProfileDialog::BeamPathProfileDialog(QWidget *parent) :
  QDialog(parent),
  d_ptr(new BeamPathProfileDialogPrivate(*this))
{
  Q_D(BeamPathProfileDialog);
  d->ui->setupUi(this);
  TCanvas* canvas = d->ui->RootCanvas_Calibration2D->getCanvas();
  canvas->cd();

  this->setWindowTitle(tr("Beam Path and Profile Dialog"));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);
  d->ui->TableView_BeamPath->setModel(d->beamPathModel.data());
}

BeamPathProfileDialog::~BeamPathProfileDialog()
{
  Q_D(BeamPathProfileDialog);
}

void BeamPathProfileDialog::setCamerasDevices(QPointer< AbstractCamera > cam1, QPointer< AbstractCamera > cam2)
{
  Q_D(BeamPathProfileDialog);
  d->camera1 = cam1;
  d->camera2 = cam2;
}

void BeamPathProfileDialog::profileIsReady(const QString &cameraID)
{
  Q_D(BeamPathProfileDialog);
  Q_UNUSED(cameraID);
}

QT_END_NAMESPACE
