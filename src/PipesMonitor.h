/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <algorithm>
#include <functional>
#include <set>
#include "thread.hpp"
#include "cancellation_token.hpp"

typedef std::function<void(int pid)> PipeCallback;

class CPipesMonitor
{
	std::set<int> m_appList;
public:
	CPipesMonitor(PipeCallback cbCreated, PipeCallback cbClosed);

	void start();

	void stop();

private:
	PipeCallback m_cbCreated, m_cbClosed;
	rethread::standalone_cancellation_token cancellation_token;
	rethread::thread* m_pth;
	void doMonitor();
	void checkIfProviderPipe(HANDLE hPipe);
};

