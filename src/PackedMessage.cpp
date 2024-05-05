/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "CriticalSection.h"
#include "PackedMessage.h"

CPackedMessage CPackedMessage::Make(CLoggerItemBase* pLoggerItem, LONGLONG offset)
{
	BUILD_BUG_ON(sizeof(CPackedMessage) != 8);

	CPackedMessage msg;

	msg._v._offset = offset;
	msg._v._itemId = (LONG_PTR)pLoggerItem;

	return msg;
}

static CLoggerTracker* _pTracker;
static CriticalSection _lock;

CLoggerTracker& CLoggerTracker::Instance()
{
	if (nullptr == _pTracker)
	{
		_pTracker = new CLoggerTracker();
	}

	return *_pTracker;
}

void CLoggerTracker::AddLogger(CLoggerItemBase* pLoggerItem)
{
	ATLTRACE2(atlTraceDBProvider, 0, "CLoggerTracker::AddLogger(item=%04x)\n", (USHORT)pLoggerItem);

	CriticalSectionLock lock(_lock);
	_pTracker->_loggersMap.insert(std::make_pair((USHORT)pLoggerItem, pLoggerItem));
}

void CLoggerTracker::RemoveLogger(CLoggerItemBase* pLoggerItem)
{
	ATLTRACE2(atlTraceDBProvider, 0, "CLoggerTracker::RemoveLogger(item=%04x)\n", (USHORT)pLoggerItem);

	CriticalSectionLock lock(_lock);
	auto it = _pTracker->_loggersMap.find((USHORT)pLoggerItem);

	if (it != _pTracker->_loggersMap.end())
		_pTracker->_loggersMap.erase(it);
}

CLoggerItemBase* CLoggerTracker::GetLogger(USHORT itemId)
{
	CriticalSectionLock lock(_lock);
	auto it = _pTracker->_loggersMap.find(itemId);

	ATLTRACE2(atlTraceDBProvider, 3, "CLoggerTracker::GetLogger(item=%04x) result: %s\n", itemId, (it != _pTracker->_loggersMap.end()) ? "found" : "not found");

	return (it != _pTracker->_loggersMap.end()) ? it->second : nullptr;
}
