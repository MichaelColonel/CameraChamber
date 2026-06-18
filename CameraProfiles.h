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

#include <QObject>

#include "typedefs.h"

QT_BEGIN_NAMESPACE

class AbstractCamera;
class TGraph;
class TH2;
class TH1;
class TLatex;

class CameraProfilesPrivate;

class CameraProfiles : public QObject {
  Q_OBJECT
public:
  explicit CameraProfiles(AbstractCamera* cam, QObject *parent = nullptr);
  virtual ~CameraProfiles();
  TH2* createAverageTimeStepProfile2D(int timeStepMs, IntegratorType integrType = IntegratorType::AB);
  TGraph* createTimeIntensityProfile(int pedMin, int pedMax, int sigMin, int sigMax,
    CameraProfileType profileType, ProfileRepresentationType representationType,
    IntegratorType integrType = IntegratorType::AB);
  TLatex* createBeamPathTable(int timeStepMs, int totalTimeMs, QDateTime& spillDateTime);

  static int GetBinX(TH2* h2, double posX);
  static int GetBinY(TH2* h2, double posY);
  static int GetProfileBin(TH1* h, double pos);

  static double GetNorm(TGraph* profile);
  static double GetMean(TGraph* profile, double norm);
  static double GetSigma(TGraph* profile, double mean, double norm);

  static void ReverseProfileValues(TH1* h);
  static void ReverseProfileValues(TGraph* h);

  static void ProcessBeamPathProfile(TGraph* profile);
  static BeamProfileArray GetProfilesParameters(TGraph* profileX, TGraph* profileY);
  static bool CheckNoBeamProfile(TGraph* profile);
  static TGraph* HistToGraph(TH1*);

protected:
  QScopedPointer< CameraProfilesPrivate > d_ptr;

private:
  Q_DECLARE_PRIVATE(CameraProfiles);
  Q_DISABLE_COPY(CameraProfiles);
};

QT_END_NAMESPACE
