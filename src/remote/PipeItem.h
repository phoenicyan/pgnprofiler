/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

using std::pair;

#define BUFF_SIZE_HINT 16384

enum PIPEITEM_STATE
{
	PIPESTATE_INITIAL,
	PIPESTATE_CONNECTED,
};

class CPipeItem;

typedef void(*ON_CLIENT_CONNECTED_TO_PIPE)(CPipeItem*);
typedef void(*ON_CLIENT_DISCONNECTED_FROM_PIPE)(CPipeItem*);

class CPipeItems;

// CPipeItem - intended for async pipe IO support.
class CPipeItem
{
	friend class CPipeItems;

	CRITICAL_SECTION m_pipeCS;	// controls access to the pipe and buffer below from multiple threads
	PIPEITEM_STATE	m_state;
	HANDLE			m_hPipe;
	HANDLE			m_evtStopPipeMonitor, m_evtPipeMonitorFinished, m_hCommThread;
	CPipeItems*		m_parent;
	OVERLAPPED		m_op;
	ATL::CString	m_availProcesses;	// list of processes available for profiling
	ON_CLIENT_CONNECTED_TO_PIPE m_OnClientConnected;
	ON_CLIENT_DISCONNECTED_FROM_PIPE m_OnClientDisconnected;
#if _DEBUG
	wstring sPipename;
#endif
	BYTE			m_inbuffer[BUFF_SIZE_HINT];

public:
	CPipeItem(CPipeItems* parent, ON_CLIENT_CONNECTED_TO_PIPE OnClientConnected, ON_CLIENT_DISCONNECTED_FROM_PIPE OnClientDisconnected)
		: m_state(PIPESTATE_INITIAL), m_parent(parent)
		, m_evtStopPipeMonitor(INVALID_HANDLE_VALUE), m_evtPipeMonitorFinished(INVALID_HANDLE_VALUE), m_hCommThread(INVALID_HANDLE_VALUE)
		, m_OnClientConnected(OnClientConnected), m_OnClientDisconnected(OnClientDisconnected)
	{
		InitializeCriticalSection(&m_pipeCS);

		memset(&m_op, 0, sizeof(m_op));
		//m_op.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

		memset(m_inbuffer, 0, sizeof(m_inbuffer));

		// the name of the pipe will be the name of the executable, which is based on the caller, so it should always be unique
		wchar_t path[_MAX_PATH * 2] = { 0 };
		GetModuleFileName(NULL, path, _countof(path));
		wchar_t* pC = wcsrchr(path, L'\\');
		if (NULL != pC)
			pC++; //get past backslash

		TCHAR pipename[_MAX_PATH];
		_tcscpy(pipename, L"\\\\.\\pipe\\");
		_tcscat(pipename, pC);
#if _DEBUG
		sPipename.assign(pipename);
#endif

		// Create instance of the named pipe and then wait for a client to connect to it. When the client 
		// connects, a thread is created to handle communications with that client, and the loop is repeated. 
		m_hPipe = CreateNamedPipe(
			pipename,             // pipe name 
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,       // async read/write access 
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			BUFF_SIZE_HINT,           // output buffer size 
			BUFF_SIZE_HINT,           // input buffer size 
			0,                        // client time-out 
			NULL);                    // default security attribute 

		if (INVALID_HANDLE_VALUE == m_hPipe)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to create pipe %s, error=%d.\n", pipename, GetLastError());

			//SetEvent(g_ServiceStopEvent);
		}
	}

	~CPipeItem()
	{
		if (IsCommActive())
			StopComm();

		CloseHandle(m_hPipe);
		m_hPipe = INVALID_HANDLE_VALUE;

		DeleteCriticalSection(&m_pipeCS);
	}

	void StartComm()
	{
		if (IsCommActive())
			return;		// already started

		ATLTRACE(atlTraceDBProvider, 2, _T("CPipeItem::StartComm()\n"));

		m_evtStopPipeMonitor = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_evtPipeMonitorFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_op.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		DWORD threadId;
		m_hCommThread = CreateThread(0, 0, PipeMonitorThread, this, CREATE_SUSPENDED, &threadId);
		ResumeThread(m_hCommThread);
	}

	void StopComm()
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("CPipeItem::StopComm()\n"));

		SetEvent(m_evtStopPipeMonitor);

		DWORD waitResult = WaitForSingleObject(m_evtPipeMonitorFinished, 50);
		if (waitResult != WAIT_OBJECT_0)
		{
			ATLTRACE(atlTraceDBProvider, 0, _T("** CPipeItem::StopComm: TerminateThread\n"));
			TerminateThread(m_hCommThread, 0);
		}

		CloseHandle(m_evtStopPipeMonitor);
		CloseHandle(m_evtPipeMonitorFinished);
		CloseHandle(m_op.hEvent);
		m_evtStopPipeMonitor = m_evtPipeMonitorFinished = m_op.hEvent = m_hCommThread = INVALID_HANDLE_VALUE;
	}

	inline HANDLE GetPipeHandle() const { return m_hPipe; }

	inline bool IsCommActive() const { return m_evtStopPipeMonitor != INVALID_HANDLE_VALUE; }

	inline PIPEITEM_STATE GetPipeState() const { return m_state; }

	inline ATL::CString GetAvailProcesses() const { return m_availProcesses; }

private:
	static DWORD WINAPI PipeMonitorThread(LPVOID pParam);
};


class CPipeItems
{
	CRITICAL_SECTION m_dhPipesCS;
	int m_availablePipes;
	map<HANDLE, CPipeItem*> m_pipes;
	//BYTE m_outbuffer[OUT_BUFSIZE];	// buffer data before sending to pipes

	CPipeItems(CPipeItems&);

public:
	CPipeItems() : m_availablePipes(0)
	{
		InitializeCriticalSection(&m_dhPipesCS);
	}

	~CPipeItems()
	{
		DeleteCriticalSection(&m_dhPipesCS);
	}

	CPipeItem* AddPipeInstance()
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("CPipeItems::AddPipeInstance()\n"));

		CPipeItem* pipe = new CPipeItem(this, OnClientConnected, OnClientDisconnected);

		EnterCriticalSection(&m_dhPipesCS);

		m_pipes.insert(pair<HANDLE, CPipeItem*>(pipe->GetPipeHandle(), pipe));

		m_availablePipes++;

		LeaveCriticalSection(&m_dhPipesCS);

		return pipe;
	}

	inline int GetAvailablePipes() const { return m_availablePipes; }

	inline int DecrementAvailablePipes() { if (m_availablePipes > 0) --m_availablePipes;  return m_availablePipes; }

	inline int IncrementAvailablePipes() { return ++m_availablePipes; }

	//inline BYTE* GetOutBuffer() { return &m_outbuffer[0]; }

	static void OnClientConnected(CPipeItem* pipe);

	static void OnClientDisconnected(CPipeItem* pipe);

	void DoSubscriptions();

	void Clear()
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("CPipeItems::Clear()\n"));

		EnterCriticalSection(&m_dhPipesCS);

		for_each(m_pipes.begin(), m_pipes.end(), [](pair<HANDLE, CPipeItem*> p)
		{
			p.second->StopComm();
		});

		m_pipes.clear();

		m_availablePipes = 0;

		LeaveCriticalSection(&m_dhPipesCS);
	}
};
