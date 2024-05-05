/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <concurrent_queue.h>
#include "thread.hpp"
#include "cancellation_token.hpp"

class CMyTaskExecutor final
{
	CMyTaskExecutor();
	CMyTaskExecutor(const CMyTaskExecutor&) = delete;
	CMyTaskExecutor& operator =(const CMyTaskExecutor&) = delete;

	concurrency::concurrent_queue<std::function<void()>> m_taskQueue;	//const rethread::cancellation_token&
	rethread::thread	m_taskThread;
	volatile bool		m_bStopRequested;

	void ProcessTasksFunc();

public:
	~CMyTaskExecutor();

	static CMyTaskExecutor& Instance();

	void AddTask(std::function<void()> f);

	void Terminate();
};
