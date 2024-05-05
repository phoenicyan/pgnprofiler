/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "MyTaskExecutor.h"

rethread::standalone_cancellation_token g_ct;

CMyTaskExecutor::CMyTaskExecutor()
	: m_taskThread(std::bind(&CMyTaskExecutor::ProcessTasksFunc, this))
	, m_bStopRequested(false)
{}

CMyTaskExecutor& CMyTaskExecutor::Instance()
{
	static CMyTaskExecutor* _inst = nullptr;

	if (_inst == nullptr)
	{
		_inst = new CMyTaskExecutor();
	}

	return *_inst;
}

CMyTaskExecutor::~CMyTaskExecutor()
{}

void CMyTaskExecutor::AddTask(std::function<void()> f)
{
	m_taskQueue.push(f);
}

void CMyTaskExecutor::Terminate()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CMyTaskExecutor::Terminate()\n");

	m_bStopRequested = true;

	if (m_taskThread.joinable())
	{
		m_taskThread.join();
	}
}

void CMyTaskExecutor::ProcessTasksFunc()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CMyTaskExecutor::ProcessTasksFunc(): enter\n");

	while (!m_bStopRequested)
	{
		std::function<void()> f;
		if (m_taskQueue.try_pop(f))
			f();

		Sleep(50);	// get some rest before taking on next task
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"CMyTaskExecutor::ProcessTasksFunc(): exit\n");
}
