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

#include <TApplication.h>
#include <TROOT.h>

#include <TRandom.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TGraphErrors.h>
#include <TFrame.h>
#include <TFile.h>
#include <TH1.h>
#include <TH1I.h>
#include <TH2.h>
#include <TF1.h>
#include <TMath.h>
#include <TVirtualFitter.h>
#include <TLegend.h>
#include <TImage.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>
#include <TBranch.h>
#include <TKey.h>
#include <TROOT.h>

#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <climits>
#include <map>


namespace
{

const int BEAM_PATH_CUSTOM_COLORS_PALETTE = 20;

const std::array< Int_t, BEAM_PATH_CUSTOM_COLORS_PALETTE > CalibrationScanningPalette{
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

void profile_offline(const char* fileName)
{
  TCanvas* canvas = new TCanvas("canvas", "Graph", 200, 10, 900, 900);
  canvas->GetFrame()->SetBorderSize(6);
  canvas->GetFrame()->SetBorderMode(-1);
//  canvas->Divide(1, 2);

  std::unique_ptr< TFile > rootFile(TFile::Open(fileName));
/*
  canvas->cd(1);
  TPad* pad1 = new TPad("pad1", "Grid", 0., 0., 1., 1.);
  pad1->Divide(2, 1);
  pad1->Draw();

  pad1->cd(1);
  TGraph* vertProf = dynamic_cast< TGraph* >(rootFile->Get("vp"));
  if (vertProf)
  {
    vertProf->Draw();
  }

  pad1->cd(2);
  TGraph* horizProf = dynamic_cast< TGraph* >(rootFile->Get("hp"));
  if (horizProf)
  {
    horizProf->Draw();
  }
  canvas->cd(2);
*/
  canvas->cd();
  TPad* pad2 = new TPad("pad2", "Grid", 0., 0., 1., 1.);
  pad2->SetGrid();
  pad2->Draw();
  pad2->cd();


  Int_t* ptr = const_cast< Int_t* >(CalibrationScanningPalette.data());
  Int_t SIZE = CalibrationScanningPalette.size();
  gStyle->SetPalette(SIZE, ptr);

//  TH2* h2p = dynamic_cast< TH2* >(rootFile->Get("hist2"));
  TH2* h2p = dynamic_cast< TH2* >(rootFile->Get("hist2_cloned"));
  if (h2p)
  {
/*
    h2p->Scale(100. / h2p->GetMaximum());
    for (Int_t row = 1; row <= h2p->GetYaxis()->GetNbins(); ++row)
    {
      for (Int_t column = 1; column <= h2p->GetXaxis()->GetNbins(); ++column)
      {
        Double_t value = h2p->GetBinContent(column, row);
        if (value < 3.)
        {
          h2p->SetBinContent(column, row, 0.);
        }
      }
    }
*/
    h2p->SetStats(kFALSE);
    h2p->Draw("COLZ");
  }
//  pad1->Modified();
//  pad1->Update();

  pad2->Modified();
  pad2->Update();

  std::unique_ptr< TImage > img(TImage::Create());
  img->FromPad(canvas);
  img->WriteImage("/tmp/profile2.png");
}

} // namespace

int main(int argc, char **argv)
{
  TApplication theApp("ROOT application", &argc, argv);

  if (theApp.Argc() == 1)
  {
    std::cerr << "No input ROOT file." << '\n';
    std::cerr << "Usage: profile_offline profile_file.root" << '\n';
    return EXIT_FAILURE;
  }

  gROOT->SetBatch(kTRUE);
  profile_offline(theApp.Argv(1));
  gROOT->SetBatch(kFALSE);

//  theApp.Run();
  return EXIT_SUCCESS;
}
