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
#include "Profiler/OneInstance.h"
#include "Profiler/ProfilerDef.h"

#include <chrono>
using std::chrono::system_clock;
using std::chrono::duration;

#include <thread>

#include <iostream>
#include <sstream>
#include "test.h"
using std::cout;
using std::endl;

#define MIN_EXECUTORS   (2)
#define MAX_EXECUTORS   (256)
#define DEFAULT_VERBOSITY (2)

auto USAGE = R"R(Usage:
    Test.exe [-p <path_wc>] [-r <number>] [-w] [-h]
Where:
    -h, --help
      Displays usage information and exits.

    -d <dir>, --directory
      Path to folder containing trace messages. Example: -p D:\data\.

    -f <file1,[file2,...]>, --files
      Comma-separated file names. Wildcards allowed, e.g. *.pgl.
      Only files with .pgl or .csv extension can be processed.

    -r <number>, --repeat
      Repeat everything <number> times.

    -v <number>, --verbose
      Sets verbosity level. 0 - display errors only, 1 - information messages,
      2 - profiling messages, 3 - debug messages. Default: 1.

    -w, --wait
      Wait for PGNProfiler to start before sending messages.

    -x <number>, --executors
      Launch specified <number> of executors (processes) to perform the job.
      Allowed values are from 2 to 256. Actual number of executors will not 
      exceed the number of test files.

    Stress test tool for PGNProfiler, 1.0. Copyright (c) 2024 Intellisoft LLC
)R";

char szPath[_MAX_PATH * 4], drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
int verbose = DEFAULT_VERBOSITY;

#define COUT_ERROR(X) cout << X
#define COUT_INFO(X) if (verbose) cout << X
#define COUT_METRICS(X) if (verbose > 1) cout << X
#define COUT_DEBUG(X) if (verbose > 2) cout << X

//
// Entry point to the test application.
//
int main(int argc, char** argv) {
    // check if command line parameters were provided; if not print Usage info
    CInputParser input(argc, argv);
    if (1 == argc || input.cmdOptionExists("-h") || input.cmdOptionExists("--help")) {
        COUT_ERROR(USAGE);
        return 0;
    }

    // validate command line parameters
    bool bWait = input.cmdOptionExists("-w") || input.cmdOptionExists("--wait");
    
    const string& sRepeat = input.getCmdOption("-r");
    int repeat = sRepeat.empty() ? 0 : std::stoi(sRepeat);
    if (repeat < 0 || repeat > 100'000)
        repeat = 0;

    const string& sVerbose = input.getCmdOption("-v");
    verbose = sVerbose.empty() ? DEFAULT_VERBOSITY : std::stoi(sVerbose);
    if (verbose < 0 || verbose > 3)
        verbose = DEFAULT_VERBOSITY;
    
    const string& sExecs = input.getCmdOption("-x");
    size_t executors = sExecs.empty() ? 0 : std::stoi(sExecs);
    if (executors < MIN_EXECUTORS || executors > MAX_EXECUTORS)
        executors = 0;
    
    string directory(input.getCmdOption("-d"));
    if (!directory.empty() && directory[directory.length() - 1] != '\\')
        directory.append(1, '\\');
    
    std::stringstream filesOption(input.getCmdOption("-f"));
    vector<string> paths;
    while (filesOption.good())
    {
        string fileName;
        getline(filesOption, fileName, ',');
        if (!fileName.empty())
            paths.push_back(directory + fileName);
    }

    // initialize messages Sender side
    ProfilerClientInit();
    COUT_DEBUG("Initialization finished.\n");

    // wait for PGNProfiler to start
    for (; bWait; Sleep(500)) {
        COneInstance guard(_T("Global\\")_T(PROFILERSGN));
        if (WAIT_TIMEOUT == WaitForSingleObject(guard.m_hInstanceGuard, 0)) {
            bWait = false;
        } else {
            COUT_DEBUG("Awaiting PGNProfiler start...\n");
        }
    }

    system_clock::time_point start = system_clock::now();
    size_t nFiles = 0, nMessages = 0;
    vector<string> fileNames;

    for_each(paths.begin(), paths.end(), [&](string& path) {
        COUT_DEBUG("Scanning " << path << "...\n");
        WIN32_FIND_DATAA findData;
        HANDLE hFind = ::FindFirstFileA(path.c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            _splitpath_s(path.c_str(), drive, dir, fname, ext);
            strcpy_s(szPath, drive);
            strcat_s(szPath, dir);

            do {
                if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    fileNames.push_back(findData.cFileName);
                }
            } while (::FindNextFileA(hFind, &findData) != 0);

            FindClose(hFind);
        }
    });

    if (0 == executors || fileNames.size() < MIN_EXECUTORS) {
        do {
            for_each(fileNames.begin(), fileNames.end(), [&](const string& fileName) {
                LogFile(fileName.c_str(), nFiles, nMessages);
            });
        } while (repeat--);
    }
    else {
        if (fileNames.size() <= executors)
            executors = fileNames.size();

        list<std::thread*> threads;
        for (size_t i = 0; i < executors; i++) {
            LogComment(0, COMMENT_1, (string("Launching executor ") + std::to_string(i)).c_str(), 0);
            threads.push_back(new std::thread([=, &nFiles, &nMessages] {
                    std::stringstream result(RunExecutor(concatIth(fileNames, i, executors), directory, repeat));
                    size_t loc_nFiles = 0, loc_nMessages = 0;
                    while (result.good())
                    {
                        string line;
                        getline(result, line, '\n');
                        if (!line.empty()) {
                            loc_nFiles++;
                            auto pos = line.find_last_of('=');
                            loc_nMessages += std::stoi(line.substr(pos+1, -1));
                        }
                    }

                    COUT_INFO("[" << i << " of " << executors << "] Files: " << loc_nFiles << "  Messages: " << loc_nMessages << endl);

                    InterlockedAdd64((LONG64*)&nFiles, loc_nFiles);
                    InterlockedAdd64((LONG64*)&nMessages, loc_nMessages);
                }));
        }

        for (auto t : threads) {
            if (t->joinable())
                t->join();
            delete t;
        }
    }

    duration<double> elapsed_seconds = system_clock::now() - start;
    COUT_METRICS("Files: " << nFiles << "  Messages: " << nMessages << "  Elapsed: " << elapsed_seconds.count() << endl);

    // let PGNProfiler digest the messages from named pipe before closing them
    Sleep(100);

    // close comm pipes
    ProfilerClientUninit();

    return 0;
}

void LogFile(const CHAR* cFileName, size_t& files, size_t& messages)
{
    CA2W lpFileName(cFileName);
    wstring lpFilePath((LPCTSTR)CA2W(szPath));
    lpFilePath.append((LPCTSTR)lpFileName);

    bool csv = strlen(cFileName) > 4 && strcmp(cFileName + strlen(cFileName) - 4, ".csv") == 0;

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
        COUT_INFO("## Processing file " << cFileName << "  numMessages=" << numMessages << "\n");

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
        COUT_ERROR("** Could not open and read file " << cFileName << endl);
    }
}
