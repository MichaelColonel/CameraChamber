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

#include <TGraph.h>
#include <TH2.h>
#include <TPad.h>
#include <TWebPadPainter.h>
#include <TWebCanvas.h>
#include <TCanvas.h>
#include <TFrame.h>
#include <TLine.h>
#include <TImage.h>
#include <TImageDump.h>
#include <TAttImage.h>
#include <TSystem.h>
#include <TStyle.h>

#include <TFile.h>
#include <TTree.h>
#include <TBranch.h>
#include <TLegend.h>

#include <TKey.h>
#include <TROOT.h>

#include <QScopedPointer>
#include <QPointer>
#include <QMap>
#include <QString>
#include <QTimer>
#include <QSettings>
#include <QProgressDialog>
#include <QListWidgetItem>
#include <QFile>
#include <QDir>
#include <QProcess>

#include "RootFileCameraProfilesDialog.h"
#include "ui_RootFileCameraProfilesDialog.h"

#include "FullCamera.h" /// Camera-1
#include "Camera2.h" /// Camera-2
#include "FullCamera3.h" /// Camera-3
#include "FullCamera4.h" /// Camera-4
#include "FullCamera6.h" /// Camera-6
#include "CameraUtils.h"

#include "CameraSpillsListModel.h"
#include "ChannelInfoTableModel.h"

#include <sstream>

QT_BEGIN_NAMESPACE

namespace Ui {
class RootFileCameraProfilesDialog;
}

class RootFileCameraProfilesDialogPrivate
{
  Q_DECLARE_PUBLIC(RootFileCameraProfilesDialog);
protected:
  RootFileCameraProfilesDialog* const q_ptr;
  QScopedPointer< Ui::RootFileCameraProfilesDialog > ui;
public:
  RootFileCameraProfilesDialogPrivate(RootFileCameraProfilesDialog &object);
  virtual ~RootFileCameraProfilesDialogPrivate();
  void processRootFile(const QString& filename);
  AbstractCamera* getCamera(const QString& cameraId) const { return !cameraId.isEmpty() ? this->camerasMap[cameraId].data() : nullptr; }
  AbstractCamera* getCamera() const { return this->getCamera(this->cameraID); }
  double getNorm(TGraph* profile);
  double getMean(TGraph* profile, double norm);
  double getSigma(TGraph* profile, double mean, double norm);
  void processBeamPathProfile(TGraph* profile);
  BeamProfileArray getProfilesParameters(TGraph* profileX, TGraph* profileY);
  void updateScanningBorders();

  int getBinScanningX(double posX);
  int getBinScanningY(double posY);
  int getBinX(TH2* h2, double posX);
  int getBinY(TH2* h2, double posY);
  int getProfileBin(TH1* hist, double pos);

  std::pair< double, double > homogenousValue();
  std::pair< double, double > homogenousValueOnHorizontalProfile();
  std::pair< double, double > homogenousValueOnVerticalProfile();

  QMap< QString, QPointer< AbstractCamera > > camerasMap;
  std::unique_ptr< TFile > rootFile;
  QScopedPointer< CameraSpillsListModel > cameraDirectorySpillsModel;
  QScopedPointer< ChannelInfoTableModel > chipChannelInfoModel;

  std::unique_ptr< TGraph > graphChannel; // TGraph adc time data
  std::unique_ptr< TPad > padChannel;

  std::unique_ptr< TLine > linePedBegin;
  std::unique_ptr< TLine > linePedEnd;
  std::unique_ptr< TLine > lineSigBegin;
  std::unique_ptr< TLine > lineSigEnd;

  std::unique_ptr< TGraph > graphVerticalProfile;
  std::unique_ptr< TPad > padVerticalProfile;

  std::unique_ptr< TGraph > graphHorizontalProfile;
  std::unique_ptr< TPad > padHorizontalProfile;

  std::unique_ptr< TH2 > histPseudo2D;
  std::unique_ptr< TPad > padPseudo2D;
  QScopedPointer< QTimer > timer;
  std::unique_ptr< TFile > rootFramesFile;
  QString cameraID;

  std::unique_ptr< TPad > padScanning2D;

  std::unique_ptr< TPad > padScanningProfiles;
  std::unique_ptr< TH1 > histScanningVerticalProfile;
  std::unique_ptr< TH1 > histScanningHorizontalProfile;
  std::unique_ptr< TLegend > legendScanningProfiles;

  std::unique_ptr< TH2 > histScanning2D;

  std::unique_ptr< TLine > lineScanningVerticalBegin;
  std::unique_ptr< TLine > lineScanningVerticalEnd;
  std::unique_ptr< TLine > lineScanningHorizontalBegin;
  std::unique_ptr< TLine > lineScanningHorizontalEnd;

  static constexpr std::array< Int_t, BEAM_PATH_CUSTOM_COLORS_PALETTE > calibrationScanningPalette{
    kBlack, // (< 0)
    kBlue + 4, // (5-0)
    kBlue + 3, // (10-5)
    kBlue, // (15-10)
    51 + 6, // (20-15)
    51 + 9, // (25-20)
    51 + 12, // (30-25)
    51 + 15, // (35-30)
    51 + 18, // (40-35)
    51 + 21, // (45-40)
    51 + 24, // (50-45)
    51 + 27, // (55-50)
    51 + 30, // (60-55)
    51 + 31, // (75-60)
    51 + 33, // (80-75)
    51 + 34, // (85-80)
    51 + 36, // (90-85)
    51 + 39, // (95-90)
    kYellow, // (100-95)
    kRed // (> 100)
  };
};

RootFileCameraProfilesDialogPrivate::RootFileCameraProfilesDialogPrivate(RootFileCameraProfilesDialog &object)
  :
  q_ptr(&object),
  ui(new Ui::RootFileCameraProfilesDialog),
  cameraDirectorySpillsModel(new CameraSpillsListModel(&object)),
  chipChannelInfoModel(new ChannelInfoTableModel(&object)),
  timer(new QTimer(&object))
{
  QSettings settings("ProfileCamera2D", "configure");
  QStringList camerasID = CameraUtils::getCameraIDs();
  int i = 1;
  for (const QString& cameraID : camerasID)
  {
    AbstractCamera::CameraDeviceData data;
    if (cameraID == "Camera2")
    {
      data = CameraUtils::getPartialCamera2Data();
      AbstractCamera* camera = new Camera2(data, &object);
      this->camerasMap.insert(data.ID, camera);
      camera->loadSettings(&settings);
    }
    else if (cameraID == "Camera3")
    {
      data = CameraUtils::getFullCameraData(i);
      AbstractCamera* camera = new FullCamera3(data, &object);
      this->camerasMap.insert(data.ID, camera);
      camera->loadSettings(&settings);
    }
    else if (cameraID == "Camera4" || cameraID == "Camera5")
    {
      data = CameraUtils::getFullCameraData(i);
      AbstractCamera* camera = new FullCamera4(data, &object);
      this->camerasMap.insert(data.ID, camera);
      camera->loadSettings(&settings);
    }
    else if (cameraID == "Camera6")
    {
      data = CameraUtils::getFullCameraData(i);
      AbstractCamera* camera = new FullCamera6(data, &object);
      this->camerasMap.insert(data.ID, camera);
      camera->loadSettings(&settings);
    }
    else
    {
      data = CameraUtils::getFullCameraData(i);
      AbstractCamera* fullCamera = new FullCamera(data, &object);
      this->camerasMap.insert(data.ID, fullCamera);
      fullCamera->loadSettings(&settings);
    }
    ++i;
  }
/*
  AbstractCamera::CameraDeviceData data1, data2, data3, data4;

  data1 = CameraUtils::getFullCameraData(1);
  AbstractCamera* fullCamera = new FullCamera(data1, &object);
  this->camerasMap.insert(data1.ID, fullCamera);
  fullCamera->loadSettings(&settings);

  data2 = CameraUtils::getPartialCamera2Data();
  AbstractCamera* camera2 = new Camera2(data2, &object);
  this->camerasMap.insert(data2.ID, camera2);
  camera2->loadSettings(&settings);

  data3 = CameraUtils::getFullCameraData(3);
  fullCamera = new FullCamera(data3, &object);
  this->camerasMap.insert(data3.ID, fullCamera);
  fullCamera->loadSettings(&settings);

  data4 = CameraUtils::getFullCameraData(4);
  fullCamera = new FullCamera(data4, &object);
  this->camerasMap.insert(data4.ID, fullCamera);
  fullCamera->loadSettings(&settings);
*/
}

RootFileCameraProfilesDialogPrivate::~RootFileCameraProfilesDialogPrivate()
{
  if (rootFile)
  {
//    rootFile->Close();
  }
}

void RootFileCameraProfilesDialogPrivate::processRootFile(const QString& rootFileName)
{
  QByteArray fileName = rootFileName.toLatin1();
  rootFile.reset(TFile::Open(fileName.data(), "READ"));
  TIter keyList(rootFile->GetListOfKeys());
  TKey* key = nullptr;
  std::vector< std::pair< TDirectory*, TTree* > > spillData;
  while ((key = dynamic_cast< TKey* >(keyList())))
  {
    TClass* cl = gROOT->GetClass(key->GetClassName());
    if (cl && !cl->InheritsFrom("TDirectory"))
    {
      continue;
    }
    if (TDirectory* dir = dynamic_cast< TDirectory* >(key->ReadObj()))
    {
      TIter keyDirList(dir->GetListOfKeys());
      while (TKey* treeKey = dynamic_cast< TKey* >(keyDirList()))
      {
        TClass* treeClass = gROOT->GetClass(treeKey->GetClassName());
        if (treeClass && !treeClass->InheritsFrom("TTree"))
        {
          continue;
        }
        if (TTree *spillTree = dynamic_cast< TTree* >(treeKey->ReadObj()))
        {
          // add TDir and TTree to the CameraSpillsListModel
          spillData.push_back(std::make_pair(dir, spillTree));
        }
      }
    }
  }
  this->cameraDirectorySpillsModel->setSpillsData(spillData);
}

double RootFileCameraProfilesDialogPrivate::getNorm(TGraph* profile)
{
  int size = profile->GetN();
  double norm = 0.;
  for(int i = 0; i < size - 1; ++i)
  {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    profile->GetPoint(i, x1, y1);
    profile->GetPoint(i + 1, x2, y2);
    norm += .5 * (y2 + y1) / (x2 - x1);
  }
  return norm;
}

double RootFileCameraProfilesDialogPrivate::getMean(TGraph* profile, double norm)
{
  int size = profile->GetN();
  double m = 0;
  for(int i = 0; i < size - 1; ++i)
  {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    profile->GetPoint(i, x1, y1);
    profile->GetPoint(i + 1, x2, y2);
    m += .5 * (x1 * y1 + x2 * y2) / (x2 - x1);
  }
  return m / norm;
}

double RootFileCameraProfilesDialogPrivate::getSigma(TGraph* profile, double mean, double norm)
{
  int size = profile->GetN();
  double var = 0;
  for(int i = 0; i < size - 1; ++i)
  {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    profile->GetPoint(i, x1, y1);
    profile->GetPoint(i + 1, x2, y2);
    double p1 = (x1 - mean) * (x1 - mean);
    double p2 = (x2 - mean) * (x2 - mean);
    var += .5 * (p1 * y1 + p2 * y2) / (x2 - x1);
  }
  if (var < 0. || (var > 0. && norm < 0.))
  {
    return -1.;
  }
  return std::sqrt(var / norm);
}


int RootFileCameraProfilesDialogPrivate::getBinScanningX(double posX)
{
  int binPos = -1;
  if (!this->histScanning2D)
  {
    return binPos;
  }
  for (int i = 1; i <= this->histScanning2D->GetXaxis()->GetNbins(); ++i)
  {
    Double_t binLow = this->histScanning2D->GetXaxis()->GetBinLowEdge(i);
    Double_t binUp = this->histScanning2D->GetXaxis()->GetBinUpEdge(i);
    if ((posX >= binLow) && (posX < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

int RootFileCameraProfilesDialogPrivate::getBinScanningY(double posY)
{
  int binPos = -1;
  if (!this->histScanning2D)
  {
    return binPos;
  }
  for (int i = 1; i <= this->histScanning2D->GetYaxis()->GetNbins(); ++i)
  {
    Double_t binLow = this->histScanning2D->GetYaxis()->GetBinLowEdge(i);
    Double_t binUp = this->histScanning2D->GetYaxis()->GetBinUpEdge(i);
    if ((posY >= binLow) && (posY < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

int RootFileCameraProfilesDialogPrivate::getBinX(TH2* hist2, double posX)
{
  int binPos = -1;
  if (!hist2)
  {
    return binPos;
  }
  for (int i = 1; i <= hist2->GetXaxis()->GetNbins(); ++i)
  {
    Double_t binLow = hist2->GetXaxis()->GetBinLowEdge(i);
    Double_t binUp = hist2->GetXaxis()->GetBinUpEdge(i);
    if ((posX >= binLow) && (posX < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

int RootFileCameraProfilesDialogPrivate::getBinY(TH2* hist2, double posY)
{
  int binPos = -1;
  if (!hist2)
  {
    return binPos;
  }
  for (int i = 1; i <= hist2->GetYaxis()->GetNbins(); ++i)
  {
    Double_t binLow = hist2->GetYaxis()->GetBinLowEdge(i);
    Double_t binUp = hist2->GetYaxis()->GetBinUpEdge(i);
    if ((posY >= binLow) && (posY < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

int RootFileCameraProfilesDialogPrivate::getProfileBin(TH1* hist, double pos)
{
  int binPos = -1;
  if (!hist)
  {
    return binPos;
  }
  for (int i = 1; i <= hist->GetXaxis()->GetNbins(); ++i)
  {
    Double_t binLow = hist->GetXaxis()->GetBinLowEdge(i);
    Double_t binUp = hist->GetXaxis()->GetBinUpEdge(i);
    if ((pos >= binLow) && (pos < binUp))
    {
      binPos = i;
      break;
    }
  }
  return binPos;
}

std::pair< double, double > RootFileCameraProfilesDialogPrivate::homogenousValue()
{
  std::pair< double, double > res{ -1, -1 };
  double hmin = this->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  double hmax = this->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
  double vmin = this->ui->RangeWidget_ScanHomoVertical->minimumValue();
  double vmax = this->ui->RangeWidget_ScanHomoVertical->maximumValue();

  int binMinX = this->getBinScanningX(hmin);
  int binMaxX = this->getBinScanningX(hmax);
  int binMinY = this->getBinScanningY(vmin);
  int binMaxY = this->getBinScanningY(vmax);

  qDebug() << Q_FUNC_INFO << "X min" << binMinX << "X max" << binMaxX << "Y min" << binMinY <<"Y max" << binMaxY;

  std::vector< Double_t > values;
  for (Int_t h = binMinX; h <= binMaxX; ++h)
  {
    for (Int_t v = binMinY; v <= binMaxY; ++v)
    {
      Double_t posX = this->histScanning2D->GetXaxis()->GetBinCenter(h);
      Double_t posY = this->histScanning2D->GetYaxis()->GetBinCenter(v);
      Double_t content = this->histScanning2D->GetBinContent(h, v);
      qDebug() << Q_FUNC_INFO << posX << ' ' << posY << ' ' << content;
      values.push_back(content);
    }
  }
  if (values.size() > 1)
  {
    Double_t min = *std::min_element(values.begin(), values.end());
    Double_t max = *std::max_element(values.begin(), values.end());
    res.first = min / max;
    res.second = 1. - (max - min) / (max + min);
  }
  if (values.size() == 1)
  {
    res.first = 1.;
    res.second = 0.;
  }
  if (!values.size())
  {
    res.first = -1.;
    res.second = -1.;
  }
  return res;
}

std::pair< double, double > RootFileCameraProfilesDialogPrivate::homogenousValueOnHorizontalProfile()
{
  std::pair< double, double > res{ -1, -1 };
  double hmin = this->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  double hmax = this->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
  int binMin = this->getProfileBin(this->histScanningHorizontalProfile.get(), hmin);
  int binMax = this->getProfileBin(this->histScanningHorizontalProfile.get(), hmax);

  std::vector< Double_t > values;
  for (Int_t i = 1; i < this->histScanningHorizontalProfile->GetXaxis()->GetNbins(); ++i)
  {
    Double_t y = this->histScanningHorizontalProfile->GetBinContent(i);
    if (i >= binMin && i <= binMax)
    {
      values.push_back(y);
    }
  }
  if (values.size() > 1)
  {
    Double_t min = *std::min_element(values.begin(), values.end());
    Double_t max = *std::max_element(values.begin(), values.end());
    res.first = min / max;
    res.second = 1. - (max - min) / (max + min);
  }
  if (values.size() == 1)
  {
    res.first = 1.;
    res.second = 0.;
  }
  if (!values.size())
  {
    res.first = -1.;
    res.second = -1.;
  }
  return res;
}

std::pair< double, double > RootFileCameraProfilesDialogPrivate::homogenousValueOnVerticalProfile()
{
  std::pair< double, double > res{ -1, -1 };
  double vmin = this->ui->RangeWidget_ScanHomoVertical->minimumValue();
  double vmax = this->ui->RangeWidget_ScanHomoVertical->maximumValue();
  int binMin = this->getProfileBin(this->histScanningVerticalProfile.get(), vmin);
  int binMax = this->getProfileBin(this->histScanningVerticalProfile.get(), vmax);

  std::vector< Double_t > values;
  for (Int_t i = 1; i < this->histScanningVerticalProfile->GetXaxis()->GetNbins(); ++i)
  {
    Double_t y = this->histScanningVerticalProfile->GetBinContent(i);
    if (i >= binMin && i <= binMax)
    {
      values.push_back(y);
    }
  }
  if (values.size() > 1)
  {
    Double_t min = *std::min_element(values.begin(), values.end());
    Double_t max = *std::max_element(values.begin(), values.end());
    res.first = min / max;
    res.second = 1. - (max - min) / (max + min);
  }
  if (values.size() == 1)
  {
    res.first = 1.;
    res.second = 0.;
  }
  if (!values.size())
  {
    res.first = -1.;
    res.second = -1.;
  }
  return res;
}

void RootFileCameraProfilesDialogPrivate::processBeamPathProfile(TGraph* profile)
{
  std::vector< Double_t > v;
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t y = profile->GetPointY(i);
    v.push_back(y);
  }
  Double_t max = *std::max_element(std::begin(v), std::end(v));

  profile->Scale(100. / max);
  for (Int_t i = 0; i < profile->GetN(); ++i)
  {
    Double_t x, y;
    profile->GetPoint(i, x, y);
    constexpr Double_t thres = 3.;
    Double_t new_y = (y < thres) ? 0. : y;
    profile->SetPoint(i, x, new_y);
  }
}

BeamProfileArray RootFileCameraProfilesDialogPrivate::getProfilesParameters(TGraph* profileX, TGraph* profileY)
{
  BeamProfileArray bp{ 0.0, 0.0, -1. };
  double normX = this->getNorm(profileX);
  double sigmaX = -1.;
  if (std::abs(normX) > DBL_EPSILON)
  {
    bp[0] = this->getMean(profileX, normX);
    sigmaX = this->getSigma(profileX, bp[0], normX);
  }

  double normY = this->getNorm(profileY);
  double sigmaY = -1.;
  if (std::abs(normY) > DBL_EPSILON)
  {
    bp[1] = this->getMean(profileY, normY);
    sigmaY = this->getSigma(profileY, bp[1], normY);
  }
  bp[2] = -1.;
  if (sigmaY >= 0. && sigmaX >= 0.)
  {
    bp[2] = std::sqrt(sigmaX * sigmaX + sigmaY * sigmaY);
  }
  return bp;
}

/*
AbstractCamera* RootFileCameraProfilesDialogPrivate::getCamera(const QString& cameraId) const
{
  if (!cameraId.isEmpty())
  {
    bool res = false;
    QStringList keys = this->camerasMap.keys();
    for (const auto& cameraIdKey : keys)
    {
      if (cameraIdKey == cameraId)
      {
        res = true;
        break;
      }
    }
    return res ? this->camerasMap[cameraId].data() : nullptr;
  }
  return nullptr;
}
*/

void RootFileCameraProfilesDialogPrivate::updateScanningBorders()
{
  if (this->lineScanningHorizontalBegin)
  {
    this->padScanning2D->GetListOfPrimitives()->Remove(this->lineScanningHorizontalBegin.get());
  }

  // minX
  Double_t pos = this->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  Double_t rangeMinH = this->histScanning2D->GetYaxis()->GetXmin();
  Double_t rangeMaxH = this->histScanning2D->GetYaxis()->GetXmax();

  this->lineScanningHorizontalBegin.reset(new TLine(pos, rangeMinH, pos, rangeMaxH));
  this->lineScanningHorizontalBegin->SetLineColor(kWhite);
  this->lineScanningHorizontalBegin->SetLineStyle(7);
  this->lineScanningHorizontalBegin->SetLineWidth(3.);
  this->lineScanningHorizontalBegin->SetVertical(kTRUE);

  // maxX
  if (this->lineScanningHorizontalEnd)
  {
    this->padScanning2D->GetListOfPrimitives()->Remove(this->lineScanningHorizontalEnd.get());
  }
  pos = this->ui->RangeWidget_ScanHomoHorizontal->maximumValue();
  this->lineScanningHorizontalEnd.reset(new TLine(pos, rangeMinH, pos, rangeMaxH));
  this->lineScanningHorizontalEnd->SetLineColor(kWhite);
  this->lineScanningHorizontalEnd->SetLineStyle(7);
  this->lineScanningHorizontalEnd->SetLineWidth(3.);
  this->lineScanningHorizontalEnd->SetVertical(kTRUE);

  // minY
  pos = this->ui->RangeWidget_ScanHomoVertical->minimumValue();
  Double_t rangeMinV = this->histScanning2D->GetXaxis()->GetXmin();
  Double_t rangeMaxV = this->histScanning2D->GetXaxis()->GetXmax();

  this->lineScanningVerticalBegin.reset(new TLine(rangeMinV, pos, rangeMaxV, pos));
  this->lineScanningVerticalBegin->SetLineColor(kWhite);
  this->lineScanningVerticalBegin->SetLineStyle(7);
  this->lineScanningVerticalBegin->SetLineWidth(3.);
  this->lineScanningVerticalBegin->SetHorizontal(kTRUE);

  // maxY
  pos = this->ui->RangeWidget_ScanHomoVertical->maximumValue();
  this->lineScanningVerticalEnd.reset(new TLine(rangeMinV, pos, rangeMaxV, pos));
  this->lineScanningVerticalEnd->SetLineColor(kWhite);
  this->lineScanningVerticalEnd->SetLineStyle(7);
  this->lineScanningVerticalEnd->SetLineWidth(3.);
  this->lineScanningVerticalEnd->SetHorizontal(kTRUE);

  this->padScanning2D->cd();
  this->lineScanningHorizontalBegin->Draw();
  this->lineScanningHorizontalEnd->Draw();
  this->lineScanningVerticalBegin->Draw();
  this->lineScanningVerticalEnd->Draw();
  this->padScanning2D->Modified();
  this->padScanning2D->Update();
}

RootFileCameraProfilesDialog::RootFileCameraProfilesDialog(const QString& rootFileName, QWidget *parent)
  :
  QDialog(parent),
  d_ptr(new RootFileCameraProfilesDialogPrivate(*this))
{
  Q_D(RootFileCameraProfilesDialog);
  d->ui->setupUi(this);
  this->setWindowTitle(tr("ROOT File Camera Profiles & Data Dialog"));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);

  d->ui->ListView_CameraSpills->setModel(d->cameraDirectorySpillsModel.data());
  d->ui->TableView_ChannelsInfo->setModel(d->chipChannelInfoModel.data());

  {
    QSignalBlocker blockerPed(d->ui->RangeWidget_Pedestal);
    d->ui->RangeWidget_Pedestal->setValues(1010., 1190.);
    QSignalBlocker blockerSig(d->ui->RangeWidget_Signal);
    d->ui->RangeWidget_Signal->setValues(335., 345.);
  }

  d->timer->setInterval(100);

  this->setWindowTitle(tr("Process ROOT file data"));
  this->setWindowFlag(Qt::WindowMaximizeButtonHint, true);

  QObject::connect(d->ui->PushButton_UpdateProfiles, SIGNAL(clicked()),
    this, SLOT(onUpdateProfilesClicked()));
  QObject::connect(d->ui->RangeWidget_Pedestal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onPedestalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_Pedestal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onPedestalEndChanged(double)));
  QObject::connect(d->ui->RangeWidget_Signal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onSignalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_Signal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onSignalEndChanged(double)));
  QObject::connect(d->ui->PushButton_UpdateChannelGraph, SIGNAL(clicked()),
    this, SLOT(onUpdateGraphClicked()));
  QObject::connect(d->ui->HorizontalSlider_FramesRange, SIGNAL(valueChanged(int)),
    this, SLOT(onProfileFrameRangeValueChanged(int)));
  QObject::connect(d->ui->PushButton_ClearProfiles2D, SIGNAL(clicked()),
    this, SLOT(onClearProfilesClicked()));
  QObject::connect(d->ui->PushButton_PlayProfiles2D, SIGNAL(clicked()),
    this, SLOT(onPlayProfilesClicked()));
  QObject::connect(d->ui->ListView_CameraSpills, SIGNAL(clicked(QModelIndex)),
    this, SLOT(onCameraSpillsModelIndexClicked(QModelIndex)));
  QObject::connect(d->ui->PushButton_ProcessSpills, SIGNAL(clicked()),
    this, SLOT(onProcessSpillsClicked()));
  QObject::connect(d->ui->PushButton_ResetScanning, SIGNAL(clicked()),
    this, SLOT(onCalculateHomogenousClicked()));

  QObject::connect(d->timer.data(), SIGNAL(timeout()), this, SLOT(updateProfileFrame()));

  d->graphChannel.reset(new TGraph());
  TCanvas* canvas = d->ui->RootCanvas_ChannelGraph->getCanvas();
  canvas->cd();
  d->padChannel = std::unique_ptr< TPad >(new TPad("pCh", "Grid", 0., 0., 1., 1.));
  d->padChannel->Draw();
  d->padChannel->SetGrid();
  d->padChannel->cd();
  d->graphChannel->Draw("AL*");

  canvas = d->ui->RootCanvas_VerticalProfile->getCanvas();
  canvas->cd();
  d->padVerticalProfile = std::unique_ptr< TPad >(new TPad("pVerProf", "Grid", 0., 0., 1., 1.));
  d->padVerticalProfile->SetGrid();
  d->padVerticalProfile->Draw();

  canvas = d->ui->RootCanvas_HorizontalProfile->getCanvas();
  canvas->cd();
  d->padHorizontalProfile = std::unique_ptr< TPad >(new TPad("pHorizProf", "Grid", 0., 0., 1., 1.));
  d->padHorizontalProfile->SetGrid();
  d->padHorizontalProfile->Draw();

  canvas = d->ui->RootCanvas_Profiles2D->getCanvas();
  canvas->cd();
  d->padPseudo2D = std::unique_ptr< TPad >(new TPad("pPs2D", "Grid", 0., 0., 1., 1.));
//  d->padPseudo2D->SetGrid();
  d->padPseudo2D->Draw();

  d->processRootFile(rootFileName);

  canvas = d->ui->RootCanvas_Scanning2D->getCanvas();
  canvas->cd();
  d->padScanning2D = std::unique_ptr< TPad >(new TPad("Scanning2D", "Scanning Grid", 0., 0., 1., 1.));
  d->padScanning2D->SetGrid();
  d->padScanning2D->Draw();

  canvas = d->ui->RootCanvas_ScanningProfiles->getCanvas();
  canvas->cd();
  d->padScanningProfiles = std::unique_ptr< TPad >(new TPad("ScanningProfiles", "Profiles Grid", 0., 0., 1., 1.));
  d->padScanningProfiles->SetGrid();
  d->padScanningProfiles->Draw();

  QObject::connect(d->ui->RangeWidget_ScanHomoHorizontal, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onScanHomoHorizonalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_ScanHomoHorizontal, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onScanHomoHorizonalEndChanged(double)));
  QObject::connect(d->ui->RangeWidget_ScanHomoVertical, SIGNAL(minimumValueChanged(double)),
    this, SLOT(onScanHomoVerticalBeginChanged(double)));
  QObject::connect(d->ui->RangeWidget_ScanHomoVertical, SIGNAL(maximumValueChanged(double)),
    this, SLOT(onScanHomoVerticalEndChanged(double)));

//  gStyle->SetPalette(d->calibrationScanningPalette.size(),
//    const_cast< Int_t* >(d->calibrationScanningPalette.data()));
}

RootFileCameraProfilesDialog::~RootFileCameraProfilesDialog()
{
  Q_D(RootFileCameraProfilesDialog);
  QObject::disconnect(d->ui->ListView_CameraSpills, SIGNAL(clicked(QModelIndex)),
    this, SLOT(onCameraSpillsModelIndexClicked(QModelIndex)));
  d->rootFile->Close();
}

/*
void RootFileCameraProfilesDialog::onCurrentRootTreeItemChanged(QListWidgetItem* newItem,QListWidgetItem*)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->rootFile)
  {
    return;
  }
  if (!newItem)
  {
    return;
  }

  QString treeName = newItem->text();
  QByteArray treeNameByteArray = treeName.toLatin1();
  TIter keyList(d->rootFile->GetListOfKeys());
  TKey* key = nullptr;
  while ((key = dynamic_cast< TKey* >(keyList())))
  {
    TClass* cl = gROOT->GetClass(key->GetClassName());
    if (cl && !cl->InheritsFrom("TDirectory"))
    {
      continue;
    }
    if (TDirectory* dir = dynamic_cast< TDirectory* >(key->ReadObj()))
    {
      TTree* spillTree = dynamic_cast< TTree* >(dir->Get(treeNameByteArray.data()));
      if (spillTree)
      {
        AbstractCamera* cam = d->getCamera();
        if (cam)
        {
          cam->processExternalData(spillTree);
          QString chipsStr = cam->getChipsAddresses();
          d->ui->LineEdit_EnabledChips->setText(chipsStr);
        }
      }
    }
  }
}
*/

void RootFileCameraProfilesDialog::onUpdateGraphClicked()
{
  Q_D(RootFileCameraProfilesDialog);

  int intTimeMs = -1;
  int nofSamples = -1;
  AbstractCamera* cam = d->getCamera();

  if (cam)
  {
    intTimeMs = cam->getIntegrationTimeMs() / 2;
    nofSamples = cam->getAdcSamples();
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Camera is invalid";
//    emit logMessage("Camera is invalid", "", Qt::red);
    return;
  }
  int chipIndex = static_cast< int >(d->ui->SliderWidget_Chip->value()) - 1;
  int channelIndex = static_cast< int >(d->ui->SliderWidget_Channel->value()) - 1;

  std::vector< int > adcData;
  size_t adcDataSize = cam->getAdcData(chipIndex, channelIndex, adcData, AdcTimeType::INTEGRATOR_AB);
  if (!adcDataSize)
  {
    qWarning() << Q_FUNC_INFO << "ADC Side A and B are empty";
//    emit logMessage("ADC Side A and B are empty", d->camera->getCameraData().id, Qt::red);
    return;
  }

  // Clear pad, reset channel graph
  d->padChannel->cd();
  d->padChannel->Clear();

  int rangeSizeAB = adcData.size();
  d->graphChannel = std::make_unique< TGraph >(rangeSizeAB);
  d->graphChannel->SetTitle("Channel data;Time (ms);Amplitude");
  d->graphChannel->SetLineColor(kBlack);
  d->graphChannel->SetLineWidth(2);
  d->graphChannel->SetMarkerColor(kBlack);
  d->graphChannel->SetMarkerSize(1.4);
  d->graphChannel->SetMarkerStyle(2);
  Int_t i = 0;
  for (int adcValue : adcData)
  {
    d->graphChannel->SetPoint(i, i * intTimeMs, Double_t(adcValue));
    ++i;
  }
  d->graphChannel->Draw("AL*");
  d->padChannel->Modified();
  d->padChannel->Update();

  d->ui->RangeWidget_Pedestal->setRange(0., 2. * intTimeMs * rangeSizeAB);
  d->ui->RangeWidget_Pedestal->setSingleStep(intTimeMs);
  d->ui->RangeWidget_Signal->setRange(0., 2. * intTimeMs * rangeSizeAB);
  d->ui->RangeWidget_Signal->setSingleStep(intTimeMs);

  this->onPedestalBeginChanged(d->ui->RangeWidget_Pedestal->minimumValue());
  this->onPedestalEndChanged(d->ui->RangeWidget_Pedestal->maximumValue());
  this->onSignalBeginChanged(d->ui->RangeWidget_Signal->minimumValue());
  this->onSignalEndChanged(d->ui->RangeWidget_Signal->maximumValue());
}

void RootFileCameraProfilesDialog::onUpdateProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);

  AbstractCamera* cam = d->getCamera();

  if (!cam)
  {
//    qWarning() << Q_FUNC_INFO << "Camera is invalid";
    emit logMessage("Camera is invalid", "", Qt::red);
    return;
  }
  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());

  cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  // process data and upgrade profiles graphs and 2d histos
  cam->processDataCounts(false, false, IntegratorType::AB, ProfileRepresentationType::CHARGE);

  std::map< ChipChannelPair, ChannelInfoPair > infoMap;
  cam->getChipChannelInfo(infoMap);
  if (!infoMap.size())
  {
//    qWarning() << Q_FUNC_INFO << "Camera data is empty";
    emit logMessage("Camera data is empty", "", Qt::red);
    return;
  }

  TGraph* gV = cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
  TGraph* gH = cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);

//  cam->updateProfiles(gV, gH, false);
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);
/*
  for (Int_t i = 0; i < d->graphVerticalProfile->GetN(); i++)
  {
    Double_t y2 = d->graphVerticalProfile->GetPointY(i);
    Double_t yNew2 = gV->GetPointY(i);
    gV->SetPointY(i, y2 + yNew2);
  }

  for (Int_t i = 0; i < d->graphHorizontalProfile->GetN(); i++)
  {
    Double_t y2 = d->graphHorizontalProfile->GetPointY(i);
    Double_t yNew2 = gV->GetPointY(i);
    gH->SetPointY(i, y2 + yNew2);
  }
*/
  // clear vertical profile pad, reset vertical profile graph
  d->padVerticalProfile->cd();
  d->padVerticalProfile->Clear();

//  d->graphVerticalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
  d->graphVerticalProfile.reset(gV);
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");
  d->padVerticalProfile->Modified();
  d->padVerticalProfile->Update();

  // clear horizontal profile pad, reset horizontal profile graph
  d->padHorizontalProfile->cd();
  d->padHorizontalProfile->Clear();
//  d->graphHorizontalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
  d->graphHorizontalProfile.reset(gH);
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");
  d->padHorizontalProfile->Modified();
  d->padHorizontalProfile->Update();

  // update pseudo 2D histogram
  d->padPseudo2D->cd();
  d->padPseudo2D->Modified();
  d->padPseudo2D->Update();

  d->chipChannelInfoModel->setChipChannelInfo(infoMap);

  // dump info into temporary text file
  auto cameraData = cam->getCameraData();
  QString dataFileName = QDir::tempPath() + QString(QDir::separator()) + cameraData.ID + "_statdump.txt";
  QScopedPointer< QFile > dataFile(new QFile(dataFileName));
  bool res = dataFile->open(QIODevice::WriteOnly | QIODevice::Text);
  if (!res)
  {
    return;
  }
  QTextStream dataFileStream(dataFile.get()); // dump info to temp file for amplitude calibration
  using MapConstIter = std::map< ChipChannelPair, ChannelInfoPair >::const_iterator;
  for (MapConstIter iter = infoMap.begin(); iter != infoMap.end(); ++iter)
  {
    const ChipChannelPair& chipChannelPair = iter->first;
    const ChannelInfoPair& infoPair = iter->second;
    const ChannelInfo& rawInfo = infoPair.first;
    const ChannelInfo& adcCalibInfo = infoPair.second;
    double pedDispA = rawInfo.PedMom2A - rawInfo.PedMeanA * rawInfo.PedMeanA;
    double sigDispA = rawInfo.SigMom2A - rawInfo.SigMeanA * rawInfo.SigMeanA;
    double signalA = rawInfo.SigMeanA - rawInfo.PedMeanA;
    double skoA = std::sqrt(pedDispA + sigDispA);
    double meanRawSignalNoAmp = (2. * rawInfo.SignalNoAmp) / (rawInfo.SigCountA + rawInfo.SigCountB);
    double meanCalibSignalNoAmp = (2. * adcCalibInfo.SignalNoAmp) / (adcCalibInfo.SigCountA + adcCalibInfo.SigCountB);
    if (res)
    {
      dataFileStream << qSetFieldWidth(10) \
        << chipChannelPair.first << ' ' << chipChannelPair.second << ' ' \
        << meanRawSignalNoAmp << ' ' << meanCalibSignalNoAmp << ' ' \
        << signalA << ' ' << skoA << '\n';
    }
  }
  dataFileStream.flush();
  res = dataFile->flush();
  if (res)
  {
    dataFile->close();
  }
/*
  auto file = std::unique_ptr< TFile >(new TFile("/tmp/profile.root", "RECREATE"));
  if (file)
  {
    file->WriteTObject(d->histPseudo2D.get(), "hist2_cloned");
    file->Close();
  }
  QString program = "./profile_offline";
  QStringList arguments;
  arguments << "/tmp/profile.root";

  QProcess *myProcess = new QProcess(this);
  myProcess->start(program, arguments);
  if (myProcess->waitForFinished())
  {
    qDebug() << Q_FUNC_INFO << "2D is ready";
    TImage* img = TImage::Open("/tmp/profile2.png");
    if (img)
    {
      d->padPseudo2D->cd();
      img->Draw("SAME");
      qDebug() << Q_FUNC_INFO << "Image is ready";
      d->padPseudo2D->Modified();
      d->padPseudo2D->Update();
    }
  }
  delete myProcess;
*/
/*
  const int offset = 400;
  const int integInterval = 100;
  constexpr int steps = 7;
  std::array< int, 4 > gatecam1;
  cam->getPedestalSignalGate(gatecam1);

  for (int i = 0; i < steps; ++i)
  {
    int sigBegin = offset + (i * integInterval);
    int sigEnd = sigBegin + integInterval;
    cam->setPedestalSignalGate(gatecam1[0], gatecam1[1], sigBegin, sigEnd);
    cam->processDataCounts();
    TGraph* cam1ProfileV = cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
    TGraph* cam1ProfileH = cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);

    d->processBeamPathProfile(cam1ProfileV);
    d->processBeamPathProfile(cam1ProfileH);

    BeamProfileArray pBegin = d->getProfilesParameters(cam1ProfileH, cam1ProfileV);
//    qDebug() << Q_FUNC_INFO << pBegin[0] << ' ' << pBegin[1] << ' ' << pBegin[2];
    delete cam1ProfileV;
    delete cam1ProfileH;
  }
*/
}

void RootFileCameraProfilesDialog::onPedestalBeginChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->linePedBegin)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->linePedBegin.get());
  }
  d->linePedBegin = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->linePedBegin->SetLineColor(kBlue);
  d->linePedBegin->SetLineStyle(7);
  d->linePedBegin->SetLineWidth(3.);
  d->linePedBegin->SetVertical(kTRUE);
  d->linePedBegin->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(pos);
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onPedestalEndChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->linePedEnd)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->linePedEnd.get());
  }
  d->linePedEnd = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->linePedEnd->SetLineColor(kBlue);
  d->linePedEnd->SetLineStyle(7);
  d->linePedEnd->SetLineWidth(3.);
  d->linePedEnd->SetVertical(kTRUE);
  d->linePedEnd->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(pos);
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onSignalBeginChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->lineSigBegin)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->lineSigBegin.get());
  }
  d->lineSigBegin = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->lineSigBegin->SetLineColor(kRed);
  d->lineSigBegin->SetLineStyle(7);
  d->lineSigBegin->SetLineWidth(3.);
  d->lineSigBegin->SetVertical(kTRUE);
  d->lineSigBegin->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(pos);
  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onSignalEndChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (!d->padChannel)
  {
    return;
  }

  if (d->lineSigEnd)
  {
    d->padChannel->GetListOfPrimitives()->Remove(d->lineSigEnd.get());
  }
  d->lineSigEnd = std::make_unique< TLine >(pos, d->graphChannel->GetMinimum(), pos, d->graphChannel->GetMaximum());
  d->padChannel->cd();
  d->lineSigEnd->SetLineColor(kRed);
  d->lineSigEnd->SetLineStyle(7);
  d->lineSigEnd->SetLineWidth(3.);
  d->lineSigEnd->SetVertical(kTRUE);
  d->lineSigEnd->Draw();
  d->padChannel->Update();
  d->padChannel->Modified();

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
  int sigMax = static_cast< int >(pos);
  d->ui->HorizontalSlider_FramesRange->setRange(sigMin, sigMax);

  AbstractCamera* cam = d->getCamera();
  if (cam)
  {
    cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  }
}

void RootFileCameraProfilesDialog::onProfileFrameRangeValueChanged(int)
{
  Q_D(RootFileCameraProfilesDialog);
}

void RootFileCameraProfilesDialog::onClearProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  d->timer->stop();
  d->ui->HorizontalSlider_FramesRange->setValue(d->ui->HorizontalSlider_FramesRange->minimum());
  qDebug() << Q_FUNC_INFO << d->ui->HorizontalSlider_FramesRange->minimum() << ' ' << d->ui->HorizontalSlider_FramesRange->singleStep() << ' ' \
    << d->ui->HorizontalSlider_FramesRange->maximum();
}


void RootFileCameraProfilesDialog::onScanHomoHorizonalBeginChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);

  if (d->lineScanningHorizontalBegin)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningHorizontalBegin.get());
  }

  Double_t rangeMin = d->histScanning2D->GetYaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetYaxis()->GetXmax();
  d->lineScanningHorizontalBegin = std::make_unique< TLine >(pos, rangeMin, pos, rangeMax);

  d->padScanning2D->cd();

  d->lineScanningHorizontalBegin->SetLineColor(kWhite);
  d->lineScanningHorizontalBegin->SetLineStyle(7);
  d->lineScanningHorizontalBegin->SetLineWidth(3.);
  d->lineScanningHorizontalBegin->SetVertical(kTRUE);
  d->lineScanningHorizontalBegin->Draw();

//  qDebug() << Q_FUNC_INFO << "Beg X: " << d->getBinX(d->histScanning2D.get(), pos);

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}


void RootFileCameraProfilesDialog::onScanHomoHorizonalEndChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);
  if (d->lineScanningHorizontalEnd)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningHorizontalEnd.get());
  }

  Double_t rangeMin = d->histScanning2D->GetYaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetYaxis()->GetXmax();
  d->lineScanningHorizontalEnd = std::make_unique< TLine >(pos, rangeMin, pos, rangeMax);

  d->padScanning2D->cd();

  d->lineScanningHorizontalEnd->SetLineColor(kWhite);
  d->lineScanningHorizontalEnd->SetLineStyle(7);
  d->lineScanningHorizontalEnd->SetLineWidth(3.);
  d->lineScanningHorizontalEnd->SetVertical(kTRUE);
  d->lineScanningHorizontalEnd->Draw();

//  qDebug() << Q_FUNC_INFO << "End X: " << d->getBinX(d->histScanning2D.get(), pos);

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

void RootFileCameraProfilesDialog::onScanHomoVerticalBeginChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);
  if (d->lineScanningVerticalBegin)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningVerticalBegin.get());
  }

  Double_t rangeMin = d->histScanning2D->GetXaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetXaxis()->GetXmax();

  d->lineScanningVerticalBegin = std::make_unique< TLine >(rangeMin, pos, rangeMax, pos);

  d->padScanning2D->cd();

  d->lineScanningVerticalBegin->SetLineColor(kWhite);
  d->lineScanningVerticalBegin->SetLineStyle(7);
  d->lineScanningVerticalBegin->SetLineWidth(3.);
  d->lineScanningVerticalBegin->SetHorizontal(kTRUE);
  d->lineScanningVerticalBegin->Draw();

//  qDebug() << Q_FUNC_INFO << "Beg Y: " << d->getBinY(d->histScanning2D.get(), pos);

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

void RootFileCameraProfilesDialog::onScanHomoVerticalEndChanged(double pos)
{
  Q_D(RootFileCameraProfilesDialog);
  if (d->lineScanningVerticalEnd)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->lineScanningVerticalEnd.get());
  }

  Double_t rangeMin = d->histScanning2D->GetXaxis()->GetXmin();
  Double_t rangeMax = d->histScanning2D->GetXaxis()->GetXmax();

  d->lineScanningVerticalEnd = std::make_unique< TLine >(rangeMin, pos, rangeMax, pos);


  d->padScanning2D->cd();

  d->lineScanningVerticalEnd->SetLineColor(kWhite);
  d->lineScanningVerticalEnd->SetLineStyle(7);
  d->lineScanningVerticalEnd->SetLineWidth(3.);
  d->lineScanningVerticalEnd->SetHorizontal(kTRUE);
  d->lineScanningVerticalEnd->Draw();

//  qDebug() << Q_FUNC_INFO << "End Y: " << d->getBinY(d->histScanning2D.get(), pos);

  d->padScanning2D->Update();
  d->padScanning2D->Modified();
}

void RootFileCameraProfilesDialog::updateProfileFrame()
{
  Q_D(RootFileCameraProfilesDialog);
  d->timer->stop();
  if (d->ui->HorizontalSlider_FramesRange->value() == d->ui->HorizontalSlider_FramesRange->maximum())
  {
    this->onUpdateProfilesClicked();
    d->rootFramesFile->Close();
    d->rootFramesFile.release();
    return;
  }

  AbstractCamera* cam = d->getCamera();
  if (!cam)
  {
    return;
  }
  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->HorizontalSlider_FramesRange->value());
  int sigMax = static_cast< int >(d->ui->HorizontalSlider_FramesRange->value() + d->ui->SliderWidget_TimeFrameStep->value());

  cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
  cam->processDataCounts();

  TString scanTitle;
  scanTitle.Form("Wobbler time from %d ms to %d ms", sigMin - d->ui->HorizontalSlider_FramesRange->minimum(),
    sigMax - d->ui->HorizontalSlider_FramesRange->minimum());

  cam->updateProfiles(d->graphVerticalProfile.get(), d->graphHorizontalProfile.get(), false);
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);
  d->histPseudo2D->SetTitle(scanTitle.Data());

  if (!d->ui->CheckBox_OffscreenProcess->isChecked())
  {
    // update vertical profile
    d->padVerticalProfile->cd();
    d->padVerticalProfile->Clear();
    d->graphVerticalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false));
    d->graphVerticalProfile->SetLineColor(kRed);
    d->graphVerticalProfile->SetLineWidth(2);
    d->graphVerticalProfile->SetMarkerColor(kBlue);
    d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
    d->graphVerticalProfile->Draw("AL*");
    d->padVerticalProfile->Modified();
    d->padVerticalProfile->Update();

    // update horizontal profile
    d->padHorizontalProfile->cd();
    d->padHorizontalProfile->Clear();
    d->graphHorizontalProfile.reset(cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false));
    d->graphHorizontalProfile->SetLineColor(kRed);
    d->graphHorizontalProfile->SetLineWidth(2);
    d->graphHorizontalProfile->SetMarkerColor(kBlue);
    d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
    d->graphHorizontalProfile->Draw("AL*");
    d->padHorizontalProfile->Modified();
    d->padHorizontalProfile->Update();
  }

  // update pseudo 2D histogram
  gStyle->SetPalette(kBird);
  d->padPseudo2D->cd();
  if (d->ui->CheckBox_ChargeRange->isChecked())
  {
    d->histPseudo2D->GetZaxis()->SetRangeUser(d->ui->RangeWidget_ChargeRange->minimumValue(),
      d->ui->RangeWidget_ChargeRange->maximumValue());
  }
  cam->updateProfiles2D(d->histPseudo2D.get(), nullptr);
  if (!d->ui->CheckBox_OffscreenProcess->isChecked())
  {
    d->padPseudo2D->Modified();
    d->padPseudo2D->Update();
  }

  QString titleString = QString::number(d->ui->HorizontalSlider_FramesRange->value());
  std::string titleStdString = "frame_" + titleString.toStdString();
//  d->rootFramesFile->cd();
//  d->histPseudo2D->Write(titleStdString.c_str());
  d->rootFramesFile->WriteTObject(d->histPseudo2D.get(), titleStdString.c_str());

  d->ui->HorizontalSlider_FramesRange->setValue(d->ui->HorizontalSlider_FramesRange->value() + d->ui->SliderWidget_TimeFrameStep->value());
  d->timer->start();
}

void RootFileCameraProfilesDialog::onPlayProfilesClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  if (d->ui->CheckBox_OffscreenProcess->isChecked())
  {
    d->timer->setInterval(10);
  }
  else
  {
    d->timer->setInterval(100);
  }
  d->timer->start();

  QDateTime dateTime = QDateTime::currentDateTime();
  std::stringstream sstream;
  sstream << "/tmp/Frames_" << d->cameraID.toStdString() << '_' << dateTime.toString("ddMMyyyy_hhmmss").toStdString() << ".root";
  std::string name = sstream.str();
  d->rootFramesFile = std::unique_ptr< TFile >(new TFile(name.c_str(), "RECREATE"));
}

void RootFileCameraProfilesDialog::onCameraSpillsModelIndexClicked(const QModelIndex& index)
{
  Q_D(RootFileCameraProfilesDialog);
  std::pair< TDirectory*, TTree* > dirSpillPair = d->cameraDirectorySpillsModel->getSpillData(index);
  TDirectory* dir = dirSpillPair.first;
  TTree* tree = dirSpillPair.second;

  if (!dir || !tree) // both pointers are valid
  {
    return;
  }

  QString camID(dir->GetName());
  AbstractCamera* cam = d->getCamera(camID);
  if (!cam) // camera ID is valid
  {
    return;
  }

  d->cameraID = camID;

  d->padVerticalProfile->cd();
  d->padVerticalProfile->Clear();
  TGraph* profv = cam->createProfile(CameraProfileType::PROFILE_VERTICAL, false);
  d->graphVerticalProfile.reset(profv);
  d->graphVerticalProfile->SetLineColor(kRed);
  d->graphVerticalProfile->SetLineWidth(2);
  d->graphVerticalProfile->SetMarkerColor(kBlue);
  d->graphVerticalProfile->SetTitle("Vertical profile;Strip position (mm);Charge (pC)");
  d->graphVerticalProfile->Draw("AL*");

  d->padHorizontalProfile->cd();
  d->padHorizontalProfile->Clear();
  TGraph* profh = cam->createProfile(CameraProfileType::PROFILE_HORIZONTAL, false);
  d->graphHorizontalProfile.reset(profh);
  d->graphHorizontalProfile->SetLineColor(kRed);
  d->graphHorizontalProfile->SetLineWidth(2);
  d->graphHorizontalProfile->SetMarkerColor(kBlue);
  d->graphHorizontalProfile->SetTitle("Horizontal profile;Strip position (mm);Charge (pC)");
  d->graphHorizontalProfile->Draw("AL*");

  d->padPseudo2D->cd();
  if (d->histPseudo2D)
  {
    d->padPseudo2D->Remove(d->histPseudo2D.get());
  }
  TH2* h2D = cam->createProfile2D(false);
  h2D->SetDirectory(nullptr);
  d->histPseudo2D.reset(h2D);
  d->histPseudo2D->GetXaxis()->SetTitle("Horizontal profile, mm");
  d->histPseudo2D->GetYaxis()->SetTitle("Vertical profile, mm");
  d->histPseudo2D->Draw("COLZ");

  d->padScanning2D->cd();
  h2D = cam->createProfile2D(false);
  h2D->SetDirectory(nullptr);
  if (d->histScanning2D)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->histScanning2D.get());
  }
  d->histScanning2D.reset(h2D);
  d->histScanning2D->GetXaxis()->SetTitle("Horizontal profile, mm");
  d->histScanning2D->GetYaxis()->SetTitle("Vertical profile, mm");
  d->histScanning2D->Draw("COLZ");
  d->updateScanningBorders();

  if (cam->processExternalData(tree))
  {
    QString chipsStr = cam->getChipsAddresses();
    d->ui->LineEdit_EnabledChips->setText(chipsStr);
  }
  else
  {
    emit logMessage("No chips available", d->cameraID, Qt::red);
    d->ui->LineEdit_EnabledChips->setText("");
  }
}

void RootFileCameraProfilesDialog::onCalculateHomogenousClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  std::pair< double, double > v = d->homogenousValue();
  qDebug() << Q_FUNC_INFO << "Value: " << v.first << ' ' << v.second;

  v = d->homogenousValueOnVerticalProfile();
  qDebug() << Q_FUNC_INFO << "V Value: " << v.first << ' ' << v.second;

  v = d->homogenousValueOnHorizontalProfile();
  qDebug() << Q_FUNC_INFO << "H Value: " << v.first << ' ' << v.second;
}

void RootFileCameraProfilesDialog::onProcessSpillsClicked()
{
  Q_D(RootFileCameraProfilesDialog);
  QModelIndexList indexList = d->ui->ListView_CameraSpills->selectionModel()->selectedIndexes();
  QModelIndex firstIndex = indexList.first();

  std::pair< TDirectory*, TTree* > dirSpillPair = d->cameraDirectorySpillsModel->getSpillData(firstIndex);
  TDirectory* dir = dirSpillPair.first;
  TTree* tree = dirSpillPair.second;

  if (!dir || !tree) // both pointers are valid
  {
    return;
  }

  QString camID(dir->GetName());
  AbstractCamera* cam = d->getCamera(camID);
  if (!cam) // camera ID is valid
  {
    return;
  }

//  TH2* h2DInt = cam->createProfile2D(false);
//  h2DInt->SetDirectory(nullptr);
//  h2DInt->SetTitle("Radiographic film isocenter int");
//  h2DInt->GetXaxis()->SetTitle("Horizontal profile, mm");
//  h2DInt->GetYaxis()->SetTitle("Vertical profile, mm");

  d->cameraID = camID;

  int pedMin = static_cast< int >(d->ui->RangeWidget_Pedestal->minimumValue());
  int pedMax = static_cast< int >(d->ui->RangeWidget_Pedestal->maximumValue());
  int sigMin = static_cast< int >(d->ui->RangeWidget_Signal->minimumValue());
//  int sigMax = static_cast< int >(d->ui->RangeWidget_Signal->maximumValue());

  if (d->histScanning2D)
  {
    d->padScanning2D->GetListOfPrimitives()->Remove(d->histScanning2D.get());
  }

  TH2* histSpills = cam->createProfile2D();
  histSpills->SetDirectory(nullptr);

//  d->histScanning2D.reset(hist);

  for (const QModelIndex& index : indexList)
  {
//    qDebug() << Q_FUNC_INFO << index;
    dirSpillPair = d->cameraDirectorySpillsModel->getSpillData(index);
    dir = dirSpillPair.first;
    tree = dirSpillPair.second;

    if (!dir || !tree) // both pointers are valid
    {
      continue;
    }
/*
    if (cam->processExternalData(tree))
    {
      cam->setPedestalSignalGate(pedMin, pedMax, sigMin, sigMax);
      cam->processDataCounts();
      TH2* h2D = cam->createProfile2D(false);
      h2D->SetDirectory(nullptr);
      cam->updateProfiles2D(h2D, nullptr);
      d->histScanning2D->Add(h2D);
      delete h2D;
    }
*/
/*
    const double time = 900.;
    const double step = 30.;
    const int steps = time / step;
    for (int j = 0; j < steps; ++j)
    {
      int sigBegin = sigMin + (j * step);
      int sigEnd = sigBegin + step;
      cam->setPedestalSignalGate(pedMin, pedMax, sigBegin, sigEnd);
      cam->processDataCounts();
      TH2* cam2Hist2 = cam->createProfile2D();
      cam->updateProfiles2D(cam2Hist2, nullptr);
      d->histScanning2D->Add(cam2Hist2);
      delete cam2Hist2;
    }
*/

    if (cam->processExternalData(tree))
    {
      std::unique_ptr< TH2 > histSpill(cam->createProfile2D());
      histSpill->SetDirectory(nullptr);

      const double time = 900.;
      const double step = d->ui->SliderWidget_TimeFrameStep->value();
      const int steps = time / step;
      for (int j = 0; j < steps; ++j)
      {
        int sigBegin = sigMin + (j * step);
        int sigEnd = sigBegin + step;
        cam->setPedestalSignalGate(pedMin, pedMax, sigBegin, sigEnd);
        cam->processDataCounts();
        std::unique_ptr< TH2 > cam2Hist2(cam->createProfile2D());
        cam2Hist2->SetDirectory(nullptr);
        cam->updateProfiles2D(cam2Hist2.get(), nullptr);
        histSpill->Add(cam2Hist2.get());
      }
      histSpill->Scale(1. / Double_t(steps));
      histSpills->Add(histSpill.get());
    }
  }
  Double_t max = histSpills->GetMaximum();
  histSpills->Scale(100. / max);
  histSpills->SetStats(kFALSE);
  gStyle->SetPalette(d->calibrationScanningPalette.size(),
    const_cast< Int_t* >(d->calibrationScanningPalette.data()));
  d->padScanning2D->cd();
  d->histScanning2D.reset(histSpills);
  d->histScanning2D->Draw("COLZ");
  d->padScanning2D->Modified();

  d->padScanningProfiles->cd();
  if (d->histScanningHorizontalProfile)
  {
    d->padScanningProfiles->GetListOfPrimitives()->Remove(d->histScanningHorizontalProfile.get());
  }
  if (d->histScanningVerticalProfile)
  {
    d->padScanningProfiles->GetListOfPrimitives()->Remove(d->histScanningVerticalProfile.get());
  }
  double posMinX = d->ui->RangeWidget_ScanHomoHorizontal->minimumValue();
  double posMaxX = d->ui->RangeWidget_ScanHomoHorizontal->maximumValue();

  double posMinY = d->ui->RangeWidget_ScanHomoVertical->minimumValue();
  double posMaxY = d->ui->RangeWidget_ScanHomoVertical->maximumValue();
  int binMinX = d->getBinX(d->histScanning2D.get(), posMinX);
  int binMaxX = d->getBinX(d->histScanning2D.get(), posMaxX);
  int binMinY = d->getBinX(d->histScanning2D.get(), posMinY);
  int binMaxY = d->getBinX(d->histScanning2D.get(), posMaxY);

//  qDebug() << Q_FUNC_INFO << "X min" << binMinX << "X max" << binMaxX << "Y min" << binMinY <<"Y max" << binMaxY;
  TH1* profileX = d->histScanning2D->ProjectionX("_projX", binMinY, binMaxY);
  profileX->SetDirectory(nullptr);
  profileX->SetLineColor(kRed);
  profileX->SetLineWidth(2);
//  profileX->SetMarkerStyle(21);
//  profileX->SetMarkerSize(1.3);
  profileX->SetMarkerColor(kRed);
  TH1* profileY = d->histScanning2D->ProjectionY("_projY", binMinX, binMaxX);
  profileY->SetDirectory(nullptr);
  profileY->SetLineColor(kBlue);
  profileY->SetLineWidth(2);
//  profileY->SetMarkerStyle(21);
//  profileY->SetMarkerSize(1.3);
  profileY->SetMarkerColor(kBlue);

  profileX->Scale(1. / Double_t(binMaxY - binMinY + 1));
  profileY->Scale(1. / Double_t(binMaxX - binMinX + 1));
  Double_t maxX = profileX->GetMaximum();
  Double_t maxY = profileY->GetMaximum();
  d->histScanningHorizontalProfile.reset(profileX);
  d->histScanningVerticalProfile.reset(profileY);
  d->histScanningHorizontalProfile->SetStats(kFALSE);
  d->histScanningVerticalProfile->SetStats(kFALSE);

  if (maxX > maxY)
  {
    d->histScanningHorizontalProfile->Draw("PL");
    d->histScanningHorizontalProfile->SetTitle("Mean profile");
    d->histScanningHorizontalProfile->GetXaxis()->SetTitle("Position (mm)");
    d->histScanningHorizontalProfile->GetYaxis()->SetTitle("Intensity (%)");

    d->histScanningVerticalProfile->Draw("SAME");
  }
  else
  {
    d->histScanningVerticalProfile->Draw("PL");
    d->histScanningVerticalProfile->SetTitle("Mean profile");
    d->histScanningVerticalProfile->GetXaxis()->SetTitle("Position (mm)");
    d->histScanningVerticalProfile->GetYaxis()->SetTitle("Intensity (%)");
    d->histScanningHorizontalProfile->Draw("SAME");
  }

  if (d->legendScanningProfiles)
  {
    d->padScanningProfiles->GetListOfPrimitives()->Remove(d->legendScanningProfiles.get());
  }
  d->legendScanningProfiles.reset(new TLegend(0.05,0.9,0.35,1.0));
//  d->legendScanningProfiles->SetHeader("Mean scanning profiles","C"); // option "C" allows to center the header
  d->legendScanningProfiles->AddEntry(d->histScanningVerticalProfile.get(),"Vertical","lp");
  d->legendScanningProfiles->AddEntry(d->histScanningHorizontalProfile.get(),"Horizontal","lp");
  d->legendScanningProfiles->Draw();

  d->padScanningProfiles->Modified();
  d->padScanningProfiles->Update();
  d->updateScanningBorders();
}

QT_END_NAMESPACE
