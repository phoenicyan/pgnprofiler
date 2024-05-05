/* ========================================================================
 *                            Event.cpp
 * ------------------------------------------------------------------------
 *  Implementation file for the Event class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#include "Event.h"

Event::Event(bool bManualReset ) : myHandle(0), myName(_T(""))
 {
  myHandle = CreateEvent(NULL, bManualReset, TRUE, NULL );
 }



Event::Event(bool bManualReset, const _TCHAR *pszName )
 : myHandle(0), myName(pszName)
 {
  myHandle = CreateEvent(NULL, bManualReset, TRUE, myName.c_str() );
 }



//    Event::Event(a, bool bSet, bool bManualReset, const char *pszName );




Event::~Event()
 {
  CloseHandle(myHandle );
  myHandle = 0;
 }



DWORD    Event::Wait()
 {
  return WaitForSingleObject(myHandle, INFINITE );
 }



DWORD    Event::Wait(DWORD TimeOut )
 {
  return WaitForSingleObject(myHandle, TimeOut );
 }


void     Event::Set()
 {
  SetEvent(myHandle );
 }


void     Event::Reset()
 {
  ResetEvent(myHandle );
 }


void     Event::Pulse()
 {
  PulseEvent(myHandle );
 }
