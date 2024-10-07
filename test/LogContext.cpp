/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "Profiler\ProfilerDef.h"
#include "LogContext.h"

CLogContext::CLogContext() : _sessionId(-1), _trcType(TRC_NONE)
{}

CLogContext::CLogContext(CPqSession* pSession, SHORT trcType, void* pgconn) : _sessionId(-1), _trcType(trcType)
{
	//if (pgconn != nullptr)
	//	_sDbName.assign(PQdb((PGconn*)pgconn));
	//else if (pSession != nullptr)
	//	_sDbName.assign(CW2A(pSession->GetDbName()));

	//if (pSession != nullptr)
	//{
	//	_sUser.assign(CW2A(pSession->GetUser()));
	//	_sessionId = pSession->GetSessionId();
	//}
}

void CLogContext::UpdateDbName(void* pgconn)
{
	//if (pgconn != nullptr)
	//	_sDbName.assign(PQdb((PGconn*)pgconn));
}
