/* ========================================================================
 *                            Mutex.h
 * ------------------------------------------------------------------------
 *  Interface file for the Mutex class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#if !defined( INCLUDED_MUTEX )
  #define INCLUDED_MUTEX

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

  class Mutex
   {
    HANDLE       myHandle;
    std::string  myName;
  public:
    // constructors
    Mutex();
    Mutex( const char *pszName );
//    Mutex( a, bool bInitialOwned, const char *pszName );
    // destructor
    ~Mutex();

    // accessors
    HANDLE              getHandle()        { return myHandle; }
    const std::string  &getName()   const  { return myName; }

    // manipulators
    DWORD    Wait();
    DWORD    TimedWait( DWORD TimeOut );
    void     Release();
   } ;

  class MutexLock : public SyncWrapper<Mutex>
   {
   public:
    MutexLock( Mutex &m )
     : SyncWrapper<Mutex>(m,Mutex::Wait,Mutex::Release)
     { }
    MutexLock( Mutex &m, DWORD TimeOut )
     : SyncWrapper<Mutex>(m,Mutex::TimedWait,Mutex::Release,TimeOut)
     { }
    ~MutexLock() { }
   } ;


#endif
