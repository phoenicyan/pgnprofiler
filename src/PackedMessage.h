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
		ULONGLONG _itemId : 16;
	}
	// __attribute__((packed))
	_v;
#pragma pack(pop)

	static inline CPackedMessage Make(USHORT itemId, LONGLONG offset) noexcept
	{
		BUILD_BUG_ON(sizeof(CPackedMessage) != 8);

		return { offset, itemId };
	}

private:
	CPackedMessage() = default;
};


class CLoggerTracker
{
	map<USHORT, CLoggerItemBase*> _loggersMap;

public:
	static CLoggerTracker& Instance();

	void AddLogger(USHORT itemId, CLoggerItemBase* pLoggerItem);
	
	void RemoveLogger(CLoggerItemBase* pLoggerItem);
	
	CLoggerItemBase* GetLogger(USHORT itemId);

private:
	CLoggerTracker() noexcept = default;
};
