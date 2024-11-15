/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"

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

	int	iReturnVal = -1;
	char* Buffer = NULL;

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

		try
		{
			if (CreateProcess(NULL, (LPWSTR)cmdLine.c_str(), &sa, &sa, true, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
			{
				try
				{
					while (::WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0)
					{
						Sleep(50);
					}

					int DataSize = 0;

					if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, (LPDWORD)&DataSize, NULL))
					{
						if (DataSize)
						{
							int BufferSize = 2000;
							if (DataSize > BufferSize)
								BufferSize = DataSize;
							Buffer = (char*)malloc(BufferSize + 1);
							memset(Buffer, 0, sizeof(char) * (BufferSize + 1));

							int BytesRead = 0;
							do
							{
								ReadFile(hReadPipe, Buffer, BufferSize, (LPDWORD)&BytesRead, NULL);
								Buffer[BytesRead] = 0;
							} while ((int)BytesRead > BufferSize && BufferSize > 0);
						}

						GetExitCodeProcess(pi.hProcess, (LPDWORD)&iReturnVal);
					}
				}
				catch (...)
				{
				}

				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
			CloseHandle(hReadPipe);
			CloseHandle(hWritePipe);
		}
		catch (...)
		{
			CloseHandle(hReadPipe);
			CloseHandle(hWritePipe);
		}
	}

	string tmp(Buffer ? Buffer : "");

	free(Buffer);

	return tmp;
}
