/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "DebugHelper.h"

string concatIth(const vector<string>& names, size_t i, size_t span)
{
	string result;

	for (; i < names.size(); i += span)
	{
		if (!result.empty())
			result.append(1, ',');

		auto sp = names[i].find_first_of(" \t") != string::npos;
		if (sp)
			result.append(1, '"');

		result.append(names[i]);

		if (sp)
			result.append(1, '"');
	}

	return result;
}

void ReadProcessOutput(HANDLE hReadPipe, /*out*/ string& messages)
{
	for (int dataSize = 0; PeekNamedPipe(hReadPipe, NULL, 0, NULL, (LPDWORD)&dataSize, NULL) && dataSize; dataSize = 0)
	{
		char* buf = (char*)calloc(dataSize + 1, sizeof(char));
		int bytesRead = 0;
		ReadFile(hReadPipe, buf, dataSize, (LPDWORD)&bytesRead, NULL);
		buf[bytesRead] = 0;

		messages.append(buf);
		free(buf);
	}
}

string RunExecutor(const string& fileNames, const string& directory, int repeat)
{
	ATLTRACE2(atlTraceDBProvider, 0, "RunExecutor(%s)\n", fileNames.c_str());

	TCHAR fullPathToExe[MAX_PATH];
	GetModuleFileName(NULL, fullPathToExe, _countof(fullPathToExe));

	wstring cmdLine(fullPathToExe);
	cmdLine.append(_T(" -d "));
	cmdLine.append((LPCTSTR)CA2W(directory.c_str()));
	cmdLine.append(_T(" -f "));
	cmdLine.append((LPCTSTR)CA2W(fileNames.c_str()));
	cmdLine.append(_T(" -v 1"));

	if (repeat > 0)
	{
		cmdLine.append(_T(" -r "));
		cmdLine.append(std::to_wstring(repeat));
	}

	string messages;

	HANDLE hReadPipe = NULL, hWritePipe = NULL;

	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;

	if (CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
	{
		PROCESS_INFORMATION pi = { 0 };
		STARTUPINFO si = { 0 };
		si.cb = sizeof(si);
		si.hStdOutput = hWritePipe;
		si.hStdInput = hReadPipe;
		si.dwFlags = STARTF_USESTDHANDLES + STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		if (CreateProcess(NULL, (LPWSTR)cmdLine.c_str(), &sa, &sa, true, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
		{
			LogComment(0, COMMENT_1, (string("Started executor ") + std::to_string(GetProcessId(pi.hProcess))).c_str(), 0);

			while (::WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0)
			{
				ReadProcessOutput(hReadPipe, messages);
				Sleep(50);
			}

			ReadProcessOutput(hReadPipe, messages);

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}

		CloseHandle(hReadPipe);
		CloseHandle(hWritePipe);
	}

	//ATLTRACE2(atlTraceDBProvider, 0, "<<<- RunExecutor: %s\n", messages.c_str());

	return messages;
}
