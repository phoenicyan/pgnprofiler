/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

class CPqSession;

class CLogContext
{
public:
	string _sDbName;
	string _sUser;
	SHORT _sessionId;
	SHORT _trcType;

	CLogContext();

	CLogContext(CPqSession* pSession, SHORT trcType, void* pgconn);

	inline const string& GetDbName() const { return _sDbName; }

	inline const string& GetUser() const { return _sUser; }

	inline SHORT GetSessionId() const { return _sessionId; }

	void UpdateDbName(void* pgconn);
};
