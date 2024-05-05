/* ========================================================================
 *                            Mutex.cpp
 * ------------------------------------------------------------------------
 *  Implementation file for the Mutex class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#include "Mutex.h"
#include "string.h"



Mutex::Mutex() : myHandle(0), myName("")
 {
  myHandle = CreateMutex( NULL, FALSE, NULL );
 }



Mutex::Mutex( const char *pszName ) : myHandle(0), myName(pszName)
 {
  myHandle = CreateMutex( NULL, FALSE, myName.c_str() );
 }


//    Mutex::Mutex( a, bool bInitialOwned, const char *pszName )



Mutex::~Mutex()
 {
  CloseHandle( myHandle );
  myHandle = 0;
 }



DWORD    Mutex::Wait()
 {
  return WaitForSingleObject( myHandle, INFINITE );
 }




DWORD    Mutex::TimedWait( DWORD TimeOut )
 {
  return WaitForSingleObject( myHandle, TimeOut );
 }



void     Mutex::Release()
 {
  ReleaseMutex( myHandle );
 }
