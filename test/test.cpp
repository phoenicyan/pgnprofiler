/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"

#include "CsvFileLoggerItem.h"
#include "DebugHelper.h"
#include "FileLoggerItem.h"
#include "InputParser.h"
#include "Profiler\ProfilerDef.h"

#include <chrono>
using std::chrono::system_clock;
using std::chrono::duration;

#include <iostream>
using std::cout;
using std::endl;

auto USAGE = R"R(Usage:
    Test.exe [-p <path>] [-w] [-h]
Where:
    -p, --path
      Path to folder containing trace messages. Example: -p D:\data\*.pgl.
      Only files with .pgl or .csv extension can be processed.

    -w, --wait
      Wait for PGNProfiler to start before sending messages.

    -h, --help
      Displays usage information and exits.

    Stress test tool for PGNProfiler. Copyright (c) 2024 Intellisoft LLC
)R";

char szPath[_MAX_PATH * 4], drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];

//
// Entry point to the test application.
//
int main(int argc, char** argv) {
    CInputParser input(argc, argv);
    if (1 == argc || input.cmdOptionExists("-h") || input.cmdOptionExists("--help")) {
        cout << USAGE;
        return 0;
    }

    bool bWait = input.cmdOptionExists("-w") || input.cmdOptionExists("--wait");

    ProfilerClientInit();
    Sleep(100);

    system_clock::time_point start = system_clock::now();
    size_t files = 0, messages = 0;
    const std::string& path = input.getCmdOption("-p");
    if (!path.empty()) {
        cout << "Scanning " << path << "...\n";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = ::FindFirstFileA(path.c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            _splitpath_s(path.c_str(), drive, dir, fname, ext);
            strcpy_s(szPath, drive);
            strcat_s(szPath, dir);

            bool csv = false;

            do
            {
                if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    CA2W lpFileName(findData.cFileName);
                    wstring lpFilePath((LPCTSTR)CA2W(szPath));
                    lpFilePath.append((LPCTSTR)lpFileName);

                    CLoggerItemBase* pLogFile;
                    if (csv)
                    {
                        pLogFile = new CCsvFileLoggerItem(lpFileName, lpFilePath.c_str());
                    }
                    else
                    {
                        pLogFile = new CFileLoggerItem(lpFileName, lpFilePath.c_str());
                    }

                    int err = pLogFile->OpenLogFile();
                    if (0 == err)
                    {
                        size_t numMessages = pLogFile->GetNumMessages();
                        cout << "## Processing file " << findData.cFileName << "  numMessages=" << numMessages << "\n";

                        files++;
                        messages += numMessages;

                        for (size_t i = 0; i < numMessages; i++)
                        {
                            LogMsg(pLogFile->GetMessageData(i));
                        }

                        pLogFile->CloseLogFile();
                    }
                    else
                    {
                        cout << "** Could not open and read file " << findData.cFileName << endl;
                    }

                    //HANDLE hSrcFile = CreateFileA((std::string(szPath) + findData.cFileName).c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                    //if (hSrcFile != INVALID_HANDLE_VALUE)
                    //{
                    //    CloseHandle(hSrcFile);
                    //}

                    //if (10 == ++processed)
                    //    break;
                }
            } while (::FindNextFileA(hFind, &findData) != 0);

            FindClose(hFind);
        }
    }

    duration<double> elapsed_seconds = system_clock::now() - start;
    cout << "Files: " << files << "  Messages: " << messages << "  Elapsed: " << elapsed_seconds.count() << endl;

    Sleep(1000);

    ProfilerClientUninit();

    return 0;
}
