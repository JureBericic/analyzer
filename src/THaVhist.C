//*-- AUTHOR :    Robert Michaels   05/08/2002

//////////////////////////////////////////////////////////////////////////
//
// THaVhist
//
// Vector of histograms; of course can be just 1 histogram.
// This object uses TH1 class of ROOT to form a histogram
// used by THaOutput.
// The X and Y axis, as well as cut conditions, can be
// formulas of global variables.  
// They can also be arrays of variables, or vector formula,
// though certain rules apply about the dimensions; see
// THaOutput documentation for those rules.
//
//
// author:  R. Michaels    May 2003
//
//////////////////////////////////////////////////////////////////////////

#define CHECKOUT 1

#include "THaVhist.h"
#include "THaVform.h"
#include "TObject.h"
#include "THaFormula.h"
#include "THaVarList.h"
#include "THaVar.h"
#include "THaGlobals.h"
#include "THaCut.h"
#include "TH1.h"
#include "TH2.h"
#include "TTree.h"
#include "TFile.h"
#include "TRegexp.h"
#include "TError.h"
#include <algorithm>
#include <fstream>
#include <cstring>
#include <iostream>

using namespace std;

//_____________________________________________________________________________
THaVhist::~THaVhist() 
{
  if (fFormX) delete fFormX;
  if (fFormY) delete fFormY;
  if (fCut) delete fCut;
  for (std::vector<TH1*>::iterator ith = fH1.begin();
       ith != fH1.end(); ith++) delete *ith;
  fH1.clear();
}

//_____________________________________________________________________________
void THaVhist::CheckValidity( ) 
{
  fProc = kTRUE;
  if (fEye == 0 && fSize != fH1.size()) {
     fProc = kFALSE;
     ErrPrint();
     cerr << "THaVhist:ERROR:: Inconsistent sizes."<<endl;
  }
  if (fFormX == 0) {
     fProc = kFALSE;
     ErrPrint();
     cerr << "THaVhist:ERROR:: No X axis defined."<<endl;
  }
  if (fInitStat != 0) {
     fProc = kFALSE;
     ErrPrint();
     cerr << "THaVhist:ERROR:: Improperly initialized."<<endl;
  }
} 

//_____________________________________________________________________________
void THaVhist::Clear( ) 
{
  fVarX     = "";
  fVarY     = "";
  fScut     = "";
  fNbinX    = 0;
  fNbinY    = 0;
  fScaler   = 0;
  fEye      = 0;
  fSize     = 0;
  fInitStat = 0;
  fXlo      = 0;
  fXhi      = 0;
  fYlo      = 0;
  fYhi      = 0;
  fFormX    = 0;
  fFormY    = 0;
  fCut      = 0;
  fFirst    = kTRUE;
  fProc     = kTRUE;
}

//_____________________________________________________________________________
Int_t THaVhist::Init( ) 
{
  // Must be called once at beginning of execution.
  // Initializes this vector of histograms.
  // Sets up the X, Y, and cuts as THaVforms.
  // Checks the dimensionality of this object and
  // calls BookHisto() to instantiate the histograms.
  // fInitStat is returned.
  // fInitStat ==  0  --> all ok
  // fInitStat !=0 --> various errors, interpreted 
  //                   with ErrPrint();

  for (std::vector<TH1*>::iterator ith = fH1.begin();
       ith != fH1.end(); ith++) delete *ith;
  fH1.clear();
  fInitStat = 0;
  Int_t status;
  string sname;
  if (fNbinX == 0) {
     cerr << "THaVhist:ERROR:: Histogram "<<fName<<" has no bins."<<endl;
     fInitStat = kNoBinX;
     return fInitStat;
  }
  if (fFormX == 0) {
     if (fVarX == "") {
       cerr << "THaVhist:WARNING:: Empty X formula."<<endl;
     }
     sname = fName+"X";
     if (FindEye(fVarX)) {
       fFormX = new THaVform("eye",sname.c_str(),"[0]");
     } else {
       fFormX = new THaVform("formula",sname.c_str(),fVarX.c_str());
     }
  }
  status = fFormX->Init();
  if (status != 0) {
     fFormX->ErrPrint(status);
     fInitStat = kIllFox;
     return fInitStat;
  }
  if (fNbinY != 0 && fFormY == 0) {
     if (fVarY == "") {
       cerr << "THaVhist:WARNING:: Empty Y formula."<<endl;
     }
     sname = fName+"Y";
     if (FindEye(fVarY)) {
       fFormY = new THaVform("eye",sname.c_str(),"[0]");
     } else {
       fFormY = new THaVform("formula",sname.c_str(),fVarY.c_str());
     }
  }
  if (fFormY) {
     status = fFormY->Init();
     if (status != 0) {
       fFormY->ErrPrint(status);
       fInitStat = kIllFoy;
       return fInitStat;
     }
  }
  if (fCut == 0 && HasCut()) {
     sname = fName+"Cut";
     fCut = new THaVform("cut",sname.c_str(),fScut.c_str());
  }
  if (fCut) {
     status = fCut->Init();
     if (status != 0) {
       fCut->ErrPrint(status);
       fInitStat = kIllCut;
       return fInitStat;
     }
  }

  fSize = FindVarSize();

  if (fSize < 0) {
    switch (fSize) {
      case -1:
        fInitStat = kNoX;
        return fInitStat;

      case -2:
        fInitStat = kAxiSiz;
        return fInitStat;

      case -3:
        fInitStat = kCutSix;
        return fInitStat;

      case -4:
        fInitStat = kCutSiy;
        return fInitStat;

      default:
        fInitStat = kUnk;
        return fInitStat;
    }
  }

  BookHisto(0, fSize);

}

//_____________________________________________________________________________
void THaVhist::ReAttach( ) 
{
  if (fFormX) fFormX->ReAttach();
  if (fFormY) fFormY->ReAttach();
  if (fCut) fCut->ReAttach();
  return;
}


//_____________________________________________________________________________
Bool_t THaVhist::FindEye(THaString& var) {
// If the variable is "[I]" it is an "Eye" variable.
// This means we will simply use I = 0,1,2, to fill that axis.
  Int_t pos, pos1,pos2;
  THaString eye = "[I]";
  pos1 = var.find(eye.ToUpper(),0);
  pos2 = var.find(eye.ToLower(),0);
  pos = string::npos;
  if (pos1 != string::npos) pos = pos1;
  if (pos2 != string::npos) pos = pos2;
  if (pos  != string::npos) {
    if (var.length() == eye.length()) {
      return kTRUE;
    } 
  }
  return kFALSE;
}

//_____________________________________________________________________________
Int_t THaVhist::FindVarSize()
{
 // Find the size of this object, according to this logic:
 // 1) If one of the variables is an "eye" ("[I]") then  
 //    the other variable to determines the eye's size.
 // 2) If the cut is a scaler, and if one of X or Y is 
 //    a scaler, this histo will be a scaler (i.e. one element).
 // 3) If neither 1) nor 2), then the size is the size of the 
 //    X component.
 // The sizes of X,Y,cuts must obey the rules encoded below.
 // Return code:
 //      1 = size is 1, i.e. a scaler.
 //    > 1 = size of this vector.
 //    < 0 = Rules were violated, as follows:
 //     -1 = No X defined.  Must have at least an X.
 //     -2 = Inconsistent size of X and Y.
 //     -3 = Cut size inconsistent with X.
 //     -4 = Cut size inconsistent with Y.

  Int_t sizex,sizey,sizec;  
  if (!fFormX) return -2;  // Error, must have at least an X.
  sizex = 0;
  sizey = 0;
  sizec = 0;
  if ( fFormX->IsEye() ) {
    fEye = 1;
  } else {
    sizex = fFormX->GetSize();
  }
  if (fFormY) { 
    sizey = fFormY->GetSize();
    if ( fFormY->IsEye() ) {
       fEye = 1;
       sizey = sizex;
    } else {
       if (fFormX->IsEye()) sizex = sizey;
    }
  }
  if (fCut) sizec = fCut->GetSize();
  if ( (sizex != 0 && sizey != 0) && (sizex != sizey) ) { 
    cerr<< "THaVhist::ERROR: inconsistent axis sizes"<<endl;
    return -2;
  }
  if ( (sizex != 0 && sizec > 1) && (sizex != sizec) ) {
    cerr<< "THaVhist::ERROR: inconsistent cut size"<<endl;
    return -3;
  }
  if ( (sizey != 0 && sizec > 1) && (sizey != sizec) ) {
    cerr<< "THaVhist::ERROR: inconsistent cut size"<<endl;
    return -4;
  }
  if ( (sizec <= 1) && (sizex <=1 || sizey <= 1) ) {
    fScaler = 1;  // This object is a scaler.
    return 1;  
  }            
  fScaler = 0;    // This object is a vector.
  return sizex;
}


//_____________________________________________________________________________
Int_t THaVhist::BookHisto(Int_t hfirst, Int_t hlast) 
{ 
  // Using ROOT Histogram classes, set up the vector
  // of histograms of type fType (th1f, th1d, th2f, th2d) 
  // for vector indices that run from hfirst to hlast.
  fSize = hlast;
  if (hfirst > hlast) return -1;
  if (fSize > fgVHIST_HUGE) {
    // fSize cannot be infinite.  
    // The following probably indicates a usage error.
    cerr << "THaVhist::WARNING: Asking for a too-huge";
    cerr << " number of histograms !!" << endl;
    cerr << "Truncating at " << fgVHIST_HUGE << endl;
    fSize = fgVHIST_HUGE;
  }
  // Booking a histogram with zero bins is suspicious.
  if (fNbinX == 0) cerr << "THaVhist:WARNING: fNbinX = 0."<<endl;

  string sname = fName;
  for (Int_t i = hfirst; i < fSize; i++) {
    if (fEye == 0 && fSize > 1) 
       sname = fName + Form("%d",i); 
    if (fEye == 1 && i > hfirst) continue;
    if (fType.CmpNoCase("th1f") == 0) {
       fH1.push_back(new TH1F(sname.c_str(), 
	     fTitle.c_str(), fNbinX, fXlo, fXhi));
    }
    if (fType.CmpNoCase("th1d") == 0) {
       fH1.push_back(new TH1D(sname.c_str(), 
	     fTitle.c_str(), fNbinX, fXlo, fXhi));
    }
    if (fType.CmpNoCase("th2f") == 0) { 
      fH1.push_back(new TH2F(sname.c_str(), fTitle.c_str(), 
       	  fNbinX, fXlo, fXhi, fNbinY, fYlo, fYhi));
      if (fNbinY == 0) {
      // Booking a 2D histo with zero Y bins is suspicious.
       cerr << "THaVhist:WARNING:: ";
       cerr << "2D histo with fNbiny = 0 ?"<<endl;
      }
    }
    if (fType.CmpNoCase("th2d") == 0) {
      fH1.push_back(new TH2D(sname.c_str(), fTitle.c_str(), 
	  fNbinX, fXlo, fXhi, fNbinY, fYlo, fYhi));
      if (fNbinY == 0) {
        cerr << "THaVhist:WARNING:: ";
        cerr << "2D histo with fNbiny = 0 ?"<<endl;
      }
    }
  }
  return 0;
}
 
//_____________________________________________________________________________
Int_t THaVhist::Process() 
{
  // Every event must Process() to fill histograms.
  // Two cases: This object is either a scaler or a vector.
  // If it's a scaler, then we fill one histogram, and if one 
  // of the variables is an array then all its elements are put 
  // into that histogram.  
  // If it's a vector, then (as previously verified) 2 or more 
  // from among [x, y, cuts] are vectors of same size.  Hence 
  // fill a vector of histograms with indices running in parallel.
  // Also, a vector of histograms can grow in size (up to a 
  // sensible limit) if the inputs grow in size.

  Int_t i,oldsize,size,sizey;

  // Check validity just once for efficiency.  
  if (fFirst) {
    fFirst = kFALSE;  
    CheckValidity();
  }
  if ( !IsValid() ) return -1;

  fFormX->Process();
  if (fFormY) fFormY->Process();
  if (fCut) fCut->Process();

  if ( IsScaler() ) {  
    sizey = 0;
    if (fFormY) sizey = fFormY->GetSize();
    if (sizey == 0) {  // Y is a scaler.  
     for (i = 0; i < fFormX->GetSize(); i++) {
        if ( CheckCut()==0 ) continue; 
        if (fFormY) {
          fH1[0]->Fill(fFormX->GetData(i), fFormY->GetData());
	} else {
          fH1[0]->Fill(fFormX->GetData(i));
	}
      }
    } else {   // Y is a vector 
      for (i = 0; i < fFormY->GetSize(); i++) {
        if ( CheckCut()==0 ) continue; 
        fH1[0]->Fill(fFormX->GetData(), fFormY->GetData(i));
      }
    }

  } else { // Vector histogram.  

// Expand if the size has changed.
    size = FindVarSize();
    if (size < 0) return -1;
    if (size > fSize) {
      oldsize = fSize;
      fSize = size;
      BookHisto(oldsize, fSize);
    }    

    for (Int_t i = 0; i < fSize; i++) {
      if ( CheckCut(i)==0 ) continue; 
      if (fFormY) {
        if (fEye == 1) {
          fH1[0]->Fill(fFormX->GetData(i), fFormY->GetData(i));
	} else {
          fH1[i]->Fill(fFormX->GetData(i), fFormY->GetData(i));
	}
      } else {
        if (fEye == 1) {
          fH1[0]->Fill(fFormX->GetData(i));
	} else {
          fH1[i]->Fill(fFormX->GetData(i));
	}
      }
    }
  }

  return 0;
}

//_____________________________________________________________________________
Int_t THaVhist::CheckCut(Int_t index) 
{ 
 // Check the cut.  Returns 1 if cut condition passed.
   if ( !fCut ) return 1;
   return fCut->GetData(index);
}

//_____________________________________________________________________________
Int_t THaVhist::End() 
{
  for (vector<TH1* >::iterator ith = fH1.begin(); 
      ith != fH1.end(); ith++ ) (*ith)->Write();
  return 0;
}


//_____________________________________________________________________________
void THaVhist::ErrPrint() const
{
  cerr << "THaVhist::ERROR:: Invalid histogram."<<endl;
  cerr << "Offending line:"<<endl;
  cerr << fType << "  " << fName << "  '" << fTitle<< "'  ";
  cerr << fNbinX << "  " << GetVarX();
  cerr << "  " << fXlo << "  " << fXhi;
  if (fNbinY != 0) {
    cerr << fNbinY << "  " << GetVarY();
    cerr << "  " << fYlo << "  " << fYhi;
  }
  if (HasCut()) cerr << " "<<fScut;
  cerr << endl;
  switch (fInitStat) {
    case kNoBinX:
      cerr << "Number of bins in X is zero."<<endl;
      cerr << "This must be a typo error."<<endl;
      break;
    case kIllFox:
      cerr << "Illegal formula on X axis."<<endl;
      if (fFormX) fFormX->ErrPrint(fFormX->Init());
      break;
    case kIllFoy:
      cerr << "Illegal formula on Y axis."<<endl;
      if (fFormY) fFormY->ErrPrint(fFormY->Init());
      break;
    case kIllCut:
      cerr << "Illegal formula for Cut."<<endl;
      if (fCut) fCut->ErrPrint(fCut->Init());
      break;
    case kNoX:
      cerr << "No X axis defined.  Must at least have an X."<<endl;
      break;
    case kAxiSiz:
      cerr << "Inconsistent size of X and Y axes."<<endl;
      break;
    case kCutSix:
      cerr << "Size of Cut inconsistent with size of X."<<endl;
      break;
    case kCutSiy:
      cerr << "Size of Cut inconsistent with size of Y."<<endl;
      break;
    case kOK:
    case kUnk:
    default:
      cerr << "Unknown error."<<endl;  
      break;
  }
}

//_____________________________________________________________________________
void THaVhist::Print() const
{
  cout << "  Histo "<<fType<<" : "<<fName;
  cout << "  Size : "<<fSize;
  cout << "  Title : '"<<fTitle<<"' "<<endl;
  cout << "  X : "<<GetVarX()<<"  nbins "<<fNbinX;
  cout << "  xlo "<<fXlo<<"  xhi "<<fXhi<<endl;
  if (fNbinY != 0) {
    cout << "  Y : "<<GetVarY()<<"  nbins "<<fNbinY;
    cout << "  ylo "<<fYlo<<"  yhi "<<fYhi<<endl;
  }
  if (HasCut()) cout << "  Cut : "<<fScut<<endl;
  if (fInitStat != kOK) ErrPrint();
}

//_____________________________________________________________________________
ClassImp(THaVhist)