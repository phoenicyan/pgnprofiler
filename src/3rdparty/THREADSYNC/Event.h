/* ========================================================================
 *                            Event.h
 * ------------------------------------------------------------------------
 *  Interface file for the Event class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */
#pragma once

#include <windows.h>
#include <tchar.h>
#include <string>
using std::wstring;

class Event
{
	HANDLE       myHandle;
	wstring	 myName;
public:
	// constructors
	Event(bool bManualReset );
	Event(bool bManualReset, const _TCHAR *pszName );
	//    Event(a, bool bSet, bool bManualReset, const char *pszName );
	// destructor
	virtual ~Event();

	// accessors
	HANDLE             getHandle()        { return myHandle; }
	const wstring	   &getName()   const { return myName; }

	// manipulators
	DWORD    Wait();
	DWORD    Wait(DWORD TimeOut );
	void     Set();
	void     Reset();
	void     Pulse();
};


class ManualResetEvent : public Event
{
public:
	ManualResetEvent() : Event(true) {}
	ManualResetEvent(const _TCHAR *pszName )
	 : Event(true,pszName) {}
};


class AutoResetEvent : public Event
{
public:
	AutoResetEvent() : Event(false) {}
	AutoResetEvent(const _TCHAR *pszName )
	 : Event(false,pszName) {}
};
