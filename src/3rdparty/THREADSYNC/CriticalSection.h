/* ========================================================================
 *                         CriticalSection.h
 * ------------------------------------------------------------------------
 *  Interface file for the CriticalSection class.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */
#pragma once

#include <windows.h>
#include "SyncWrapper.h"
#include <string>
using std::wstring;

class CriticalSection
{
	CRITICAL_SECTION    myCriticalSection;

public:
	// constructors
	CriticalSection();
	// destructor
	~CriticalSection();

	// manipulators
	void     Wait();
	//    BOOL     Try();
	void     Release();
};

class CriticalSectionLock : public SyncWrapper<CriticalSection>
{
public:
	CriticalSectionLock(CriticalSection &c )
	 : SyncWrapper<CriticalSection>(c,&CriticalSection::Wait,&CriticalSection::Release)
	 { }
	~CriticalSectionLock() { }
};

//struct Guard {
//	CriticalSection& cs;
//	Guard(CriticalSection& cs)
//		: cs(cs)
//	{
//		EnterCriticalSection(cs);
//	}
//	~Guard() {
//		LeaveCriticalSection(cs);
//	}
//	Guard(const Guard&) = delete;
//	Guard& operator = (const Guard&) = delete;
//};
