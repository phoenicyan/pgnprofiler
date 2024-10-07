/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "Profiler\ProfilerDef.h"

#include <algorithm>
#include <string>
using std::string;
using std::wstring;
#include <vector>
using std::vector;

struct CMDPROFILE
{
	BOOL			_traceEnabled;
	LONG			_rows;

	LARGE_INTEGER	_begin;
	LARGE_INTEGER	_end;
	TRC_TIME		_elapsed;

	TRC_TIME		_timestamp;
	TRC_TIME		_parse;
	TRC_TIME		_prepare;
	TRC_TIME		_execute;
	TRC_TIME		_getdata;

	CURSOR_TYPE		_cursor;

	BOOL			_was_logged;		// TRUE if the info was already logged

	CMDPROFILE()
	{
		Reset();
	}

	inline void Start(void) 
	{
		QueryPerformanceCounter(&_begin);
	}

	inline TRC_TIME Stop(void)	
	{ 
		QueryPerformanceCounter(&_end);
		return (_elapsed = (TRC_TIME)(_end.QuadPart - _begin.QuadPart));
	}

	inline void Reset()
	{
		memset(this, 0, sizeof CMDPROFILE);
	}
};

BOOL IsTraceEnabled();

class CLogContext;

void LogClientSQL(CLogContext* logContext, int cmdType, const char* sClientSQL, const char* sExecutedSQL, CMDPROFILE& profile, LONG cmdId = 0);
void LogError(CLogContext* logContext, const char* sClientSQL, const char* sError, CMDPROFILE& profile, LONG cmdId);
extern "C"
void LogComment(CLogContext* logContext, COMMENT_LEVEL level, const char* sClientSQL, LONG cmdId, ...);
//void LogCommentEx(CLogContext* logContext, COMMENT_LEVEL level, const char* sClientSQL, CMDPROFILE& profile, LONG cmdId = 0);
void LogMsg(const BYTE* baseAddr);

string FormatClientSQL(const char* szSQL, DBPARAMS* pParams, DBCOUNTITEM cBindings, DBBINDING* rgBindings);
string FormatPreparedClientSQL(const char* szSQL, DBPARAMS* pParams, DBCOUNTITEM cBindings, DBBINDING* rgBindings);

string FormatPGSQL(const char* szSQL, int nParams, const unsigned int* paramTypes, char** paramValues, const int* paramLengths, const int* paramFormats, bool bIntegerDatetimes);
string StringFormat(const char* format, ...);

#define BEGIN_PROFILING										\
	CLogContext __logContext(nullptr, TRC_NONE, nullptr);	\
	CMDPROFILE __profile;									\
	__profile._traceEnabled = IsTraceEnabled();				\
	if (__profile._traceEnabled) {							\
		__profile.Start();									\
		__profile._timestamp = __profile._begin.QuadPart;	\
		}

#define BEGIN_PROFILING2(pSess, trcType, conn)				\
	CLogContext __logContext(pSess, trcType, conn);			\
	CMDPROFILE __profile;									\
	__profile._traceEnabled = IsTraceEnabled();				\
	if (__profile._traceEnabled) {							\
		__profile.Start();									\
		__profile._timestamp = __profile._begin.QuadPart;	\
		}

#define RESET_PROFILING										\
	__profile._rows = 0;									\
	__profile._parse = 0;									\
	__profile._prepare = 0;									\
	__profile._execute = 0;									\
	__profile._getdata = 0;									\
	__profile._was_logged = 0;								\
	if (__profile._traceEnabled) {							\
		__profile.Start();									\
		__profile._timestamp = __profile._begin.QuadPart;	\
	}

#define RESET_PROFILING2(pSess, trcType, conn)				\
	__logContext.UpdateDbName(conn);						\
	__profile._rows = 0;									\
	__profile._parse = 0;									\
	__profile._prepare = 0;									\
	__profile._execute = 0;									\
	__profile._getdata = 0;									\
	__profile._was_logged = 0;								\
	if (__profile._traceEnabled) {							\
		__profile.Start();									\
		__profile._timestamp = __profile._begin.QuadPart;	\
	}

#define INCREMENT_ROWSCOUNT	__profile._rows++;
#define SET_ROWSCOUNT(x)	__profile._rows = (DWORD)x;
#define SET_EXEC_TIME		if (__profile._traceEnabled) { __profile._execute = __profile.Stop(); __profile.Start(); }
#define ADD_EXEC_TIME		if (__profile._traceEnabled) { __profile._execute += __profile.Stop(); __profile.Start(); }
#define SET_GETDATA_TIME	if (__profile._traceEnabled) { __profile._getdata = __profile.Stop(); __profile.Start(); }
#define SET_CURSOR_TYPE(c)	__profile._cursor = c;

#define END_PROFILING_SQL(cmdType, clientsql, executedsql)																\
	if (__profile._traceEnabled) {																						\
		__profile._execute = __profile._execute == 0 ? __profile.Stop() : __profile._execute;							\
		LogClientSQL(&__logContext, cmdType, clientsql, executedsql, __profile);											\
	}

#define END_PROFILING_SQL2(cmdType, clientsql, executedsql, cmdId)														\
	if (__profile._traceEnabled) {																						\
		__profile._execute = __profile._execute == 0 ? __profile.Stop() : __profile._execute;							\
		LogClientSQL(&__logContext, cmdType, clientsql, executedsql, __profile, cmdId);									\
	}

#define END_PROFILING_ERROR(clientsql, error)								\
	if (__profile._traceEnabled) {											\
		LogError(&__logContext, clientsql, error, __profile, 0);				\
	}

//#define END_PROFILING_ERROR_P1(sess, clientsql, error, p1)					\
//	if (__profile._traceEnabled) {											\
//		LogError(&__logContext, clientsql, error, __profile, 0, p1);					\
//	}
//
//#define END_PROFILING_ERROR_P2(sess, clientsql, error, p1, p2)				\
//	if (__profile._traceEnabled) {											\
//		LogError(&__logContext, clientsql, error, __profile, 0, p1, p2);				\
//	}
//
//#define END_PROFILING_ERROR_P3(sess, clientsql, error, p1, p2, p3)			\
//	if (__profile._traceEnabled) {											\
//		LogError(&__logContext, clientsql, error, __profile, 0, p1, p2, p3);			\
//	}
//
//#define END_PROFILING_ERROR_P4(sess, clientsql, error, p1, p2, p3, p4)		\
//	if (__profile._traceEnabled) {											\
//		LogError(&__logContext, clientsql, error, __profile, 0, p1, p2, p3, p4);		\
//	}

#define END_PROFILING_ERROR2(clientsql, error, cmdId)						\
	if (__profile._traceEnabled) {											\
		LogError(&__logContext, clientsql, error, __profile, cmdId);			\
	}

//#define END_PROFILING_ERROR2_P1(sess, clientsql, error, cmdId, p1)			\
//	if (__profile._traceEnabled) {											\
//		LogError(&__logContext, clientsql, error, __profile, cmdId, p1);				\
//	}
//
//#define END_PROFILING_ERROR2_P2(sess, clientsql, error, cmdId, p1, p2)		\
//	if (__profile._traceEnabled) {											\
//		LogError(&__logContext, clientsql, error, __profile, cmdId, p1, p2);			\
//	}
//
//#define END_PROFILING_ERROR2_P3(sess, clientsql, error, cmdId, p1, p2, p3)		\
//	if (__profile._traceEnabled) {												\
//		LogError(&__logContext, clientsql, error, __profile, cmdId, p1, p2, p3);			\
//	}
//
//#define END_PROFILING_ERROR2_P4(sess, clientsql, error, cmdId, p1, p2, p3, p4)	\
//	if (__profile._traceEnabled) {												\
//		LogError(&__logContext, clientsql, error, __profile, cmdId, p1, p2, p3, p4);		\
//	}

#define END_PROFILING_COMMENT(level, comment, cmdId)							\
	if (__profile._traceEnabled) {												\
		LogComment(&__logContext, level, comment, cmdId);						\
	}

#define END_PROFILING_COMMENT_P1(level, comment, cmdId, p1)						\
	if (__profile._traceEnabled) {												\
		LogComment(&__logContext, level, comment, cmdId, p1);					\
	}

#define END_PROFILING_COMMENT_P2(level, comment, cmdId, p1, p2)					\
	if (__profile._traceEnabled) {												\
		LogComment(&__logContext, level, comment, cmdId, p1, p2);				\
	}

#define END_PROFILING_COMMENT_P3(level, comment, cmdId, p1, p2, p3)				\
	if (__profile._traceEnabled) {												\
		LogComment(&__logContext, level, comment, cmdId, p1, p2, p3);			\
	}

#define END_PROFILING_COMMENT_P4(level, comment, cmdId, p1, p2, p3, p4)			\
	if (__profile._traceEnabled) {												\
		LogComment(&__logContext, level, comment, cmdId, p1, p2, p3, p4);		\
	}

#define END_PROFILING_COMMENT_P5(level, comment, cmdId, p1, p2, p3, p4, p5)		\
	if (__profile._traceEnabled) {												\
		LogComment(&__logContext, level, comment, cmdId, p1, p2, p3, p4, p5);	\
	}

DWORD ProfilerClientInit();
DWORD ProfilerClientUninit();
