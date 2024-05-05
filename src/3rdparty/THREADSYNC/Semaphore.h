/* ========================================================================
 *                            Semaphore.h
 * ------------------------------------------------------------------------
 *  Interface file for the Semaphore class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#if !defined( INCLUDED_SEMAPHORE )
  #define INCLUDED_SEMAPHORE

  #if !defined( INCLUDED_WINDOWS )
     #include <windows.h>
     #define INCLUDED_WINDOWS
  #endif
  #if !defined( INCLUDED_SYNCWRAPPER )
     #include "SyncWrapper.h"
  #endif
  #if !defined( INCLUDED_STRING )
    #include <string>
    #define INCLUDED_STRING
  #endif

  class Semaphore
   {
    HANDLE       myHandle;
    int          myInitial;
    int          myMax;
    std::string  myName;
  public:
    // constructors
    Semaphore( int maxsize );
    Semaphore( int init, int maxsize );
    Semaphore( int init, int maxsize, const char *pszName );
//    Semaphore( a, int init, int maxsize, const char *pszName );
    // destructor
    ~Semaphore();

    // accessors
    HANDLE        getHandle()        { return myHandle; }
    int           getInitial() const { return myInitial; }
    int           getMax()     const { return myMax; }
    const string &getName()    const { return myName; }

    // manipulators
    DWORD    Wait();
    DWORD    TimedWait( DWORD TimeOut );

    void     Release();
    void     Release( int incr );
   } ;


  class SemaphoreLock : public SyncWrapper<Semaphore>
   {
   public:
    SemaphoreLock( Semaphore &s )
     : SyncWrapper<Semaphore>(s,Semaphore::Wait,&Semaphore::Release)
     { }
    SemaphoreLock( Semaphore &s, DWORD TimeOut )
     : SyncWrapper<Semaphore>(s,Semaphore::TimedWait,&Semaphore::Release,TimeOut)
     { }
    ~SemaphoreLock() { }
   } ;

#endif
