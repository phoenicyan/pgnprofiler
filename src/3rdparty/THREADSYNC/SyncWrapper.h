/* ========================================================================
 *                                SYNCWRAP.H
 * ------------------------------------------------------------------------
 *  Interface file for the SyncWrapper template class.
 *
 *  Copyright 1997 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */

#pragma once

#pragma warning(disable : 4512)		// assignment operator could not be generated

template <class T>
class SyncWrapper
{
	T         &mySync;
	void  (T::*mypEnd)();
	DWORD      myStatus;

public:
	SyncWrapper( T &sync, DWORD (T::*pmfBegin)(), void (T::*pmfEnd)() )
	 : mySync(sync), mypEnd(pmfEnd)
	{
		myStatus = (mySync.*pmfBegin)();
	}

	SyncWrapper( T &sync, DWORD (T::*pmfBegin)( DWORD ), void (T::*pmfEnd)(),
		     DWORD timeOut )
	 : mySync(sync), mypEnd(pmfEnd)
	{
		myStatus = (mySync.*pmfBegin)( timeOut );
	}

	SyncWrapper( T &sync, void (T::*pmfBegin)(), void (T::*pmfEnd)() )
	 : mySync(sync), mypEnd(pmfEnd)
	{
		(mySync.*pmfBegin)();
		myStatus = WAIT_OBJECT_0;
	}
	~SyncWrapper()
	{
		(mySync.*mypEnd)();
	}

	DWORD   getStatus() const { return myStatus; }
};
