#ifndef ROOT_THaDebugModule
#define ROOT_THaDebugModule

//////////////////////////////////////////////////////////////////////////
//
// THaDebugModule
//
//////////////////////////////////////////////////////////////////////////

#include "THaPhysicsModule.h"
#include "THaPrintOption.h"

class THaVar;

class THaDebugModule : public THaPhysicsModule {
  
public:
  THaDebugModule( const char* var_list );
  virtual ~THaDebugModule();
  
  virtual EStatus   Init( const TDatime& run_time );
  virtual Int_t     Process();

protected:

  enum EFlags { 
    kStop  = BIT(0),    // Wait for key press after every event
    kCount = BIT(1)     // Run for fCount events   
  };

  THaDebugModule() : fNvars(0), fVars(NULL), fFlags(kStop) {}
  THaDebugModule( const THaDebugModule& ) {}
  THaDebugModule& operator=( const THaDebugModule& ) { return *this; }

  Int_t           fNvars;     // Number of entries in fVars
  THaVar**        fVars;      // [fNvars] Array of pointers to variables 
  THaPrintOption  fVarString; // Set of strings with variable names
  Int_t           fFlags;     // Option flags
  Int_t           fCount;     // Event counter

  ClassDef(THaDebugModule,0)  // Physics module for debugging
};

#endif