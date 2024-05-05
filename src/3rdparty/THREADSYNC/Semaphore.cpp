/* ========================================================================
 *                            Semaphore.cpp
 * ------------------------------------------------------------------------
 *  Implementation file for the Semaphore class.
 *
 *  Copyright 1997 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#include "Semaphore.h"
#include "string.h"



Semaphore::Semaphore( int maxsize )
 : myHandle(0), myInitial(0), myMax(maxsize), myName("")
 {
  myHandle = CreateSemaphore( NULL, myInitial, myMax, NULL );
 }



Semaphore::Semaphore( int init, int maxsize )
 : myHandle(0), myInitial(init), myMax(maxsize), myName("")
 {
  myHandle = CreateSemaphore( NULL, myInitial, myMax, NULL );
 }



Semaphore::Semaphore( int init, int maxsize, const char *pszName )
 : myHandle(0), myInitial(init), myMax(maxsize), myName(pszName)
 {
  myHandle = CreateSemaphore( NULL, myInitial, myMax, myName.c_str() );
 }




//    Semaphore( a, int init, int maxsize, const char *pszName );



Semaphore::~Semaphore()
 {
  CloseHandle( myHandle );
  myHandle = 0;
 }


    // manipulators
DWORD    Semaphore::Wait()
 {
  return WaitForSingleObject( myHandle, INFINITE );
 }



DWORD    Semaphore::TimedWait( DWORD TimeOut )
 {
  return WaitForSingleObject( myHandle, TimeOut );
 }



void     Semaphore::Release()
 {
  ReleaseSemaphore( myHandle, 1, NULL );
 }



void     Semaphore::Release( int incr )
 {
  ReleaseSemaphore( myHandle, incr, NULL );
 }
