/* ========================================================================
 *                            ReaderWriter.h
 * ------------------------------------------------------------------------
 *  Interface file for the ReaderWriter classes, ReadWriteProtector,
 *    RWPReader, and RWPWriter.
 *
 *  Copyright 1997-1998 by G. Wade Johnson (Telescan, Inc.)
 *   Use of this code is placed in the public domain as long as the
 *   copyright notice is retained.
 */
#pragma once

#pragma warning(disable: 4251)	// disables dll-interface export warning C4251 for members

#include <windows.h>
#include <tchar.h>
#include "SyncWrapper.h"

#include <string>
using std::wstring;

class  CriticalSection;
class  ManualResetEvent;
class  AutoResetEvent;

class ReadWriteProtector
{
	short             myNumReaders;
	short             myNumWriters;
	wstring           myName;
	CriticalSection   *mypTurnMutex;
	AutoResetEvent    *mypResourceProtect;
	ManualResetEvent  *mypMoreReadsAllowed;

public:
	ReadWriteProtector();                   // create Mutexes/Events
	ReadWriteProtector(const _TCHAR *name); // create Mutexes/Events
	~ReadWriteProtector();                  // close Mutexes/Events

	const wstring& getName() const { return myName; }

	void    BeginRead();
	void    EndRead();

	void    BeginWrite();
	void    EndWrite();
};


class RWPReader : public SyncWrapper<ReadWriteProtector>
{
public:
	RWPReader(ReadWriteProtector &rw )
	 : SyncWrapper<ReadWriteProtector>(rw,
               &ReadWriteProtector::BeginRead, &ReadWriteProtector::EndRead)
	{ }

	~RWPReader() { }

	RWPReader & operator=( const RWPReader & ) {}
};


class RWPWriter : public SyncWrapper<ReadWriteProtector>
{
public:
	RWPWriter(ReadWriteProtector &rw )
	 : SyncWrapper<ReadWriteProtector>(rw,
               &ReadWriteProtector::BeginWrite,&ReadWriteProtector::EndWrite)
	{ }
	
	~RWPWriter() { }

	RWPWriter & operator=( const RWPWriter & ) {}
};
