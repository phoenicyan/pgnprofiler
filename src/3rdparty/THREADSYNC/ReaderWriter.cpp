/* ========================================================================
 *                            ReaderWriter.cpp
 * ------------------------------------------------------------------------
 *  Implementation file for the ReaderWriter classes.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#include "ReaderWriter.h"
#include "CriticalSection.h"
#include "Event.h"
#include <string>
#include <windows.h>


ReadWriteProtector::ReadWriteProtector()
 : myNumReaders(0),myNumWriters(0),myName(_T("")),mypTurnMutex(0),
   mypResourceProtect(0), mypMoreReadsAllowed(0)
 {
  mypTurnMutex        = new CriticalSection;
  mypResourceProtect  = new AutoResetEvent;
  mypMoreReadsAllowed = new ManualResetEvent;
 }




ReadWriteProtector::ReadWriteProtector(const _TCHAR *name )
 : myNumReaders(0),myNumWriters(0),myName(name),mypTurnMutex(0),
   mypResourceProtect(0), mypMoreReadsAllowed(0)
 {
  mypTurnMutex        = new CriticalSection;
  mypResourceProtect  = new AutoResetEvent((myName+_T("Prot")).c_str() );
  mypMoreReadsAllowed = new ManualResetEvent((myName+_T("More")).c_str() );
 }




ReadWriteProtector::~ReadWriteProtector()
 {
  delete mypTurnMutex;
  mypTurnMutex        = 0;

  delete mypResourceProtect;
  mypResourceProtect  = 0;

  delete mypMoreReadsAllowed;
  mypMoreReadsAllowed = 0;
 }




void ReadWriteProtector::BeginRead()
 {
  mypMoreReadsAllowed->Wait();

  CriticalSectionLock  sync(*mypTurnMutex );

  if(myNumReaders++ == 0)
   {
    mypResourceProtect->Wait();
   }
 }




void ReadWriteProtector::EndRead()
 {
  CriticalSectionLock  sync(*mypTurnMutex );

  if(--myNumReaders == 0)
    mypResourceProtect->Set();
 }



void ReadWriteProtector::BeginWrite()
 {
   {
     CriticalSectionLock  sync(*mypTurnMutex );

	if(myNumWriters++ == 0)
       mypMoreReadsAllowed->Reset();
   }

  mypResourceProtect->Wait();
 }




void ReadWriteProtector::EndWrite()
 {
  mypResourceProtect->Set();

  CriticalSectionLock  sync(*mypTurnMutex );

  if(--myNumWriters == 0)
    mypMoreReadsAllowed->Set();
 }
