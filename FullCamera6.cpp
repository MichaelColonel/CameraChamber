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

#include "FullCamera6.h"

#include <QDebug>

#include <TH1.h>
#include <TH2.h>
#include <TGraphErrors.h>
#include <TPad.h>

#include <array>

QT_BEGIN_NAMESPACE

namespace {

constexpr std::array< int, 16 > RowP{ 49,	50,	51,	52,	53,	54,	55,	56,	217,	218,	219,	220,	221,	222,	223,	224 };
constexpr std::array< int, 16 > RowO{ 57,	58,	59,	60,	61,	62,	63,	64,	209,	210,	211,	212,	213,	214,	215,	216 };
constexpr std::array< int, 16 > RowN{ 33,	34,	35,	36,	37,	38,	39,	40,	201,	202,	203,	204,	205,	206,	207,	208 };
constexpr std::array< int, 16 > RowM{ 41,	42,	43,	44,	45,	46,	47,	48,	193,	194,	195,	196,	197,	198,	199,	200 };
constexpr std::array< int, 16 > RowL{ 81,	82,	83,	84,	85,	86,	87,	88,	185,	186,	187,	188,	189,	190,	191,	192 };
constexpr std::array< int, 16 > RowK{ 89,	90,	91,	92,	93,	94,	95,	96,	177,	178,	179,	180,	181,	182,	183,	184 };
constexpr std::array< int, 16 > RowJ{ 65,	66,	67,	68,	69,	70,	71,	72,	169,	170,	171,	172,	173,	174,	175,	176 };
constexpr std::array< int, 16 > RowI{ 73,	74,	75,	76,	77,	78,	79,	80,	161,	162,	163,	164,	165,	166,	167,	168 };
constexpr std::array< int, 16 > RowH{ 9,	10,	11,	12,	13,	14,	15,	16,	225,	226,	227,	228,	229,	230,	231,	232 };
constexpr std::array< int, 16 > RowG{ 1,	2,	3,	4,	5,	6,	7,	8,	233,	234,	235,	236,	237,	238,	239,	240 };
constexpr std::array< int, 16 > RowF{ 25,	26,	27,	28,	29,	30,	31,	32,	241,	242,	243,	244,	245,	246,	247,	248 };
constexpr std::array< int, 16 > RowE{ 17,	18,	19,	20,	21,	22,	23,	24,	249,	250,	251,	252,	253,	254,	255,	256 };
constexpr std::array< int, 16 > RowD{ 105,	106,	107,	108,	109,	110,	111,	112,	129,	130,	131,	132,	133,	134,	135,	136 };
constexpr std::array< int, 16 > RowC{ 97,	98,	99,	100,	101,	102,	103,	104,	137,	138,	139,	140,	141,	142,	143,	144 };
constexpr std::array< int, 16 > RowB{ 121,	122,	123,	124,	125,	126,	127,	128,	145,	146,	147,	148,	149,	150,	151,	152 };
constexpr std::array< int, 16 > RowA{ 113,	114,	115,	116,	117,	118,	119,	120,	153,	154,	155,	156,	157,	158,	159,	160 };

constexpr std::array< std::array< int, 16 >, 16 > MATRIX{ RowA, RowB, RowC, RowD, RowE, RowF, RowG, RowH, RowI, RowJ, RowK, RowL, RowM, RowN, RowO, RowP };
}

class FullCamera6Private
{
  Q_DECLARE_PUBLIC(FullCamera6);
protected:
  FullCamera6* const q_ptr;
public:
  FullCamera6Private(FullCamera6 &object);
  virtual ~FullCamera6Private();
};

FullCamera6Private::FullCamera6Private(FullCamera6 &object)
  :
  q_ptr(&object)
{
}

FullCamera6Private::~FullCamera6Private()
{
}

FullCamera6::FullCamera6(const AbstractCamera::CameraDeviceData& data, QObject *parent)
  :
  FullCamera4(data, parent),
  d_ptr(new FullCamera6Private(*this))
{
  Q_D(FullCamera6);
}

FullCamera6::~FullCamera6()
{

}

TGraph* FullCamera6::createProfile(CameraProfileType profileType, bool withErrors)
{
  Q_D(FullCamera6);

  const std::vector< double >& vertProfStrips = this->getVerticalProfileStripsNumbers();
  const std::vector< double >& horizProfStrips = this->getHorizontalProfileStripsNumbers();
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  TGraph* profile = nullptr;
  switch (profileType)
  {
  case CameraProfileType::PROFILE_VERTICAL:
    {
      if (withErrors)
      {
        profile = new TGraphErrors(vertProfData.size());
      }
      else
      {
        profile = new TGraph(vertProfData.size());
      }
//      double halfSizeVert = *std::max_element(vertProfStrips.begin(), vertProfStrips.end()) / 2.;
//      for (Int_t i = 0; i < vertProfData.size(); ++i)
//      {
//        profile->SetPoint(i, Double_t(vertProfStrips[i] - halfSizeVert), Double_t(vertProfData[i]));
//      }
      for (Int_t i = 0; i < vertProfData.size(); ++i)
      {
        profile->SetPoint(i, Double_t(i), Double_t(vertProfData[i]));
        qDebug() << Q_FUNC_INFO << "Vertical profile:" << i << Double_t(vertProfData[i]);
      }
    }
    break;
  case CameraProfileType::PROFILE_HORIZONTAL:
    {
      if (withErrors)
      {
        profile = new TGraphErrors(horizProfData.size());
      }
      else
      {
        profile = new TGraph(horizProfData.size());
      }
//      double halfSizeHoriz = *std::max_element(horizProfStrips.begin(), horizProfStrips.end()) / 2.;
//      for (Int_t i = 0; i < horizProfData.size(); ++i)
//      {
//        profile->SetPoint(i, Double_t(horizProfStrips[i] - halfSizeHoriz), Double_t(horizProfData[i]));
//      }
      for (Int_t i = 0; i < horizProfData.size(); ++i)
      {
        profile->SetPoint(i, Double_t(i), Double_t(horizProfData[i]));
        qDebug() << Q_FUNC_INFO << "Horizontal profile:" << i << Double_t(horizProfData[i]);
      }
    }
    break;
  default:
    break;
  }
  return profile;
}

void FullCamera6::updateProfiles(TGraph* vertProfile, TGraph* horizProfile, bool withErrors)
{
  Q_D(FullCamera6);
  TGraphErrors* vertProf = nullptr;
  TGraphErrors* horizProf = nullptr;
  if (withErrors)
  {
    vertProf = dynamic_cast< TGraphErrors* >(vertProfile);
    horizProf = dynamic_cast< TGraphErrors* >(horizProfile);
  }
  withErrors = (vertProf && horizProf);

  const std::vector< double >& vertProfStrips = this->getVerticalProfileStripsNumbers();
  const std::vector< double >& horizProfStrips = this->getHorizontalProfileStripsNumbers();
  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

  if (vertProfile)
  {
    double halfSizeVert = *std::max_element(vertProfStrips.begin(), vertProfStrips.end()) / 2.;
    std::vector< double > vertProfDataCopy(vertProfData);
    std::reverse(vertProfDataCopy.begin(), vertProfDataCopy.end());
    for (Int_t i = 0; i < vertProfData.size(); ++i)
    {
///      vertProfile->SetPoint(i, Double_t(vertProfDataCopy[i]), Double_t(vertProfStrips[i] - halfSizeVert));
///      vertProfile->SetPoint(i, Double_t(vertProfStrips[i] - halfSizeVert), Double_t(vertProfDataCopy[i]));
      vertProfile->SetPoint(i, Double_t(i), Double_t(vertProfDataCopy[i]));
//      qDebug() << Q_FUNC_INFO << "Vertical profile:" << i << Double_t(vertProfDataCopy[i]);
    }
  }

  if (horizProf)
  {
    double halfSizeHoriz = *std::max_element(horizProfStrips.begin(), horizProfStrips.end()) / 2.;
    std::vector< double > horizProfDataCopy(horizProfData);
    std::reverse(horizProfDataCopy.begin(), horizProfDataCopy.end());
    for (Int_t i = 0; i < horizProfData.size(); ++i)
    {
///      horizProfile->SetPoint(i, Double_t(horizProfStrips[i] - halfSizeHoriz), Double_t(horizProfDataCopy[i]));
      horizProfile->SetPoint(i, Double_t(i), Double_t(horizProfDataCopy[i]));
//      qDebug() << Q_FUNC_INFO << "Horizontal profile:" << i << Double_t(horizProfDataCopy[i]);
    }
  }
}

void FullCamera6::updateProfiles2D(TH2* pseudo2D, TH2* integPseudo2D)
{
  Q_D(FullCamera6);

  if (!pseudo2D && !integPseudo2D)
  {
    return;
  }
//  const std::vector< double >& vertProfData = this->getVerticalProfile();
  const std::vector< double >& horizProfData = this->getHorizontalProfile();

//  std::vector< double > horizProfDataCopy(horizProfData);
///  std::reverse(horizProfDataCopy.begin(), horizProfDataCopy.end());
  if (pseudo2D)
  {
    pseudo2D->Reset();
    for (Int_t column = 0; column < 16; ++column)
    {
      Double_t centerX = pseudo2D->GetXaxis()->GetBinCenter(column + 1);
      const std::array< int, 16 >& rowArray = MATRIX[column];
      for (Int_t row = 0; row < 16; ++row)
      {
        Double_t centerY = pseudo2D->GetYaxis()->GetBinCenter(row + 1);
        int horizProfileIndex = rowArray[row];
        double pixelValue = horizProfData[horizProfileIndex - 1];
        pseudo2D->Fill(centerX, centerY, pixelValue);
      }
    }
  }
}

QT_END_NAMESPACE
