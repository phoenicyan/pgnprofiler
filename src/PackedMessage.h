/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

class CLoggerItemBase;

class CPackedMessage
{
public:
#pragma pack(push, 1)
	struct
	{
		LONGLONG _offset : 48;
		LONGLONG _itemId : 16;
	}
	// __attribute__((packed))
	_v;
#pragma pack(pop)

	static CPackedMessage Make(CLoggerItemBase* pLoggerItem, LONGLONG offset);

private:
	CPackedMessage() = default;
};


class CLoggerTracker
{
	map<USHORT, CLoggerItemBase*> _loggersMap;

public:
	static CLoggerTracker& Instance();

	void AddLogger(CLoggerItemBase* pLoggerItem);
	
	void RemoveLogger(CLoggerItemBase* pLoggerItem);
	
	CLoggerItemBase* GetLogger(USHORT itemId);

private:
	CLoggerTracker() = default;
};
