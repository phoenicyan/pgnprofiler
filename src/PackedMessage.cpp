/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "CriticalSection.h"
#include "PackedMessage.h"

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

void CLoggerTracker::AddLogger(USHORT itemId, CLoggerItemBase* pLoggerItem)
{
	ATLTRACE2(atlTraceDBProvider, 0, "CLoggerTracker::AddLogger(item=%d)\n", itemId);

	CriticalSectionLock lock(_lock);
	ATLASSERT(_pTracker->_loggersMap.find(itemId) == _pTracker->_loggersMap.end());
	_pTracker->_loggersMap.insert(std::make_pair(itemId, pLoggerItem));
}

void CLoggerTracker::RemoveLogger(CLoggerItemBase* pLoggerItem)
{
	ATLTRACE2(atlTraceDBProvider, 0, "CLoggerTracker::RemoveLogger(%p)\n", pLoggerItem);

	CriticalSectionLock lock(_lock);
	for (auto it = _pTracker->_loggersMap.begin(); it != _pTracker->_loggersMap.end(); )
	{
		if (pLoggerItem == it->second)
			it = _pTracker->_loggersMap.erase(it);
		else
			it++;
	}
}

CLoggerItemBase* CLoggerTracker::GetLogger(USHORT itemId)
{
	CriticalSectionLock lock(_lock);
	auto it = _pTracker->_loggersMap.find(itemId);

	ATLTRACE2(atlTraceDBProvider, 3, "CLoggerTracker::GetLogger(item=%04x) result: %s\n", itemId, (it != _pTracker->_loggersMap.end()) ? "found" : "not found");

	return (it != _pTracker->_loggersMap.end()) ? it->second : nullptr;
}
