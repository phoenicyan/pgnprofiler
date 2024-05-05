/* ========================================================================
 *                            CriticalSection.cpp
 * ------------------------------------------------------------------------
 *  Implementation file for the CriticalSection class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#include "CriticalSection.h"



CriticalSection::CriticalSection()
 {
  InitializeCriticalSection(&myCriticalSection );
 }






CriticalSection::~CriticalSection()
 {
  DeleteCriticalSection(&myCriticalSection );
 }





void   CriticalSection::Wait()
 {
  EnterCriticalSection(&myCriticalSection );
 }



#if __NEVER_
BOOL    CriticalSection::Try()
 {
  return TryEnterCriticalSection(&myCriticalSection );
 }
#endif



void     CriticalSection::Release()
 {
  LeaveCriticalSection(&myCriticalSection );
 }
