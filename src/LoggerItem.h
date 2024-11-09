/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <functional>
#include <boost/signals2.hpp>
#include "driver.h"
#include "CriticalSection.h"
#include "MyTaskExecutor.h"
#include "PackedMessage.h"
#include "Remote/RemoteApi.h"

using namespace bisparser;

#define PIPEBUFFERSIZE	(32*1024*1024)
#define MMFGROWSIZE		(16*1024*1024)

enum LOGGER_TYPE
{
	LT_LOGROOT,		// LogRoot
	LT_PROCESS,		// Process, e.g. "ADO Explorer.exe (PID 3688)
	LT_PROCESS64,
	LT_LOGFILE,		// Log file
	//LT_MGSSRC,		// Message source, e.g. "SQL Log", "Error Log", "All Messages", or in future "User Log X"
};

enum LOGGER_STATE
{
	LS_PAUSED,
	LS_RUN,
	LS_TERMINATED,	// when there are log entries but process terminated
};

class CProcessLoggerItem;

/////////////////////////////////////////////////////////////////////////////
// CLoggerItemBase - Abstract base class for Explorer nodes.
/////////////////////////////////////////////////////////////////////////////
class CLoggerItemBase
{
protected:
	// Data Members
	wstring				m_name;
	string				m_nameA;
	wstring				m_user, m_pwd;
	LOGGER_TYPE			m_type;
	LOGGER_STATE		m_state;
	DWORD				m_dwTraceLevel;

	list<CProcessLoggerItem*> m_children;
	HWND				m_mainWindow;
	CLoggerItemBase*	m_pParent;
	FILETIME			m_initialTime;
	wstring				m_filterStr;
	bool				m_filterActive;
	bool				m_bROmode;			// true for read-only file
	Driver				m_driver;

	const wstring*		m_psWorkPath;		// points to MainFrame variable containing WorkPath option

	int					m_completionCode;	// 0 - no errors, 1 - thread execution was cancelled, 2 - expression parsing error
	rethread::standalone_cancellation_token m_ct;
	boost::signals2::signal<void(int)> m_filterProgressSignal;

public:
	// Constructor / destructor
	CLoggerItemBase();
	CLoggerItemBase(LPCTSTR pstrName, CLoggerItemBase* pParent, LOGGER_TYPE type, LOGGER_STATE state = LS_PAUSED);
	virtual ~CLoggerItemBase();

	// Operations

	virtual void SetName(LPCTSTR pstrName) { m_name.assign(pstrName); }

	virtual LPCTSTR GetName() const { return m_name.c_str(); }
	virtual LPCTSTR GetDisplayName() const { return m_name.c_str(); }		// for customizations in child classes
	virtual LPCSTR GetNameA() { if (m_nameA.empty()) m_nameA = CW2A(m_name.c_str()); return m_nameA.c_str(); }

	virtual void SetWorkPath(const wstring* psWorkPath) { m_psWorkPath = psWorkPath; }
	virtual const wstring* GetWorkPath() const { return m_psWorkPath; }

	virtual void SetTraceLevel(DWORD dwTraceLevel) { m_dwTraceLevel = dwTraceLevel; }
	virtual DWORD GetTraceLevel() const { return m_dwTraceLevel; }

	virtual void SetParamFormat(ULONG dwParamFormat) {}

	//virtual void SetFilterProgressCallback(std::function<void(int)>& fnProgressCallback);
	int GetCompletionCode() const { return m_completionCode; }

	const CLoggerItemBase* GetParent() const { return m_pParent; }
	list<CProcessLoggerItem*>& GetChildren() { return m_children; }

	BOOL DeleteChildren();
	BOOL AddChild(CProcessLoggerItem* pBlock);

	inline HWND GetMainWindow() const
	{
		return m_mainWindow;
	}

	inline void SetMainWindow(HWND mainWindow)
	{
		m_mainWindow = mainWindow;
	}

	// Implementation

	inline LOGGER_TYPE GetType() const { return m_type; }
	inline void SetType(LOGGER_TYPE type) { m_type = type; }

	inline LOGGER_STATE GetState() const { return m_state; }
	inline void SetState(LOGGER_STATE state) { m_state = state; }

	inline int GetImage() const { return m_type | (m_state << 2); }

	inline LONGLONG* GetInitialTimePtr() { return (LONGLONG*)&m_initialTime; }

	inline bool IsFilterActive() const { return m_filterActive; }

	inline bool IsROmode() const { return m_bROmode; }
	void SetROmode(bool bROmode) { m_bROmode = bROmode; }

	inline wstring GetFilterStr() const { return m_filterStr; }
	inline void SetFilterStr(const wstring& filterStr) { m_filterStr = filterStr; }

	// overrides
	virtual int OpenLogFile(DWORD dwReserved = 0);

	virtual void CloseLogFile();

	virtual map<DWORD, wstring>* GetLogger2AppnameMap();

	virtual size_t GetNumErrors() const = 0;

	virtual size_t GetNumMessages() const = 0;

	virtual const BYTE* GetMessageData(size_t i) const = 0;

	virtual void SetFilterActive(bool isActive, std::function<void(int)> fnProgressCallback = [](int){});

	virtual void FilterLoggerItems() = 0;

	virtual void ClearLog() = 0;

	static void Lock()
	{
		_lock.Wait();
	}

	static void Unlock()
	{
		_lock.Release();
	}

protected:
	static CriticalSection _lock;

	static void SetVariables(Driver& driver, const BYTE* baseAddr, LONGLONG initialTime, DWORD pid, LPCSTR appName);
};

class CRemoteApi;

/////////////////////////////////////////////////////////////////////////////
// CHostLoggerItem - Host/Machine node
/////////////////////////////////////////////////////////////////////////////
class CHostLoggerItem : public CLoggerItemBase
{
	vector<CPackedMessage> m_rgPackedMessages;	// HI short - id of CProcessLoggerItem; LO LONGLONG:48 - offset from beginning of mapped view;
	vector<CPackedMessage> m_rgFilteredMessages;
	wstring			m_remoteName;			// used for building pipe name; contains '.' for the local computer, or 'remote system name' for the remote host
	CRemoteApi*		m_pRemote;
	OVERLAPPED		m_op;
	BYTE			m_buffer[16384];

public:
	CHostLoggerItem(const wstring* psWorkPath);
	~CHostLoggerItem();

	/// Overrides
	void SetName(/*in*/LPCTSTR hostName) override;
	size_t GetNumErrors() const override;
	size_t GetNumMessages() const override;
	const BYTE* GetMessageData(size_t i) const override;
	//void SetFilterActive(bool isActive) override;
	void FilterLoggerItems() override;
	void ClearLog() override;
	void SetTraceLevel(DWORD dwTraceLevel) override;

	/// Misc
	void SetCredentials(/*in*/LPCTSTR sUser, /*in*/LPCTSTR sPwd);
	LPCTSTR GetRemoteName() const { return m_remoteName.c_str(); }
	void AddPackedMessage(CPackedMessage pMessage);

	inline vector<CPackedMessage>& GetPackedMessages() { return m_rgPackedMessages; }
	inline vector<CPackedMessage>& GetFilteredMessages() { return m_rgFilteredMessages; }

	CRemoteApi*	GetRemoteObj() { return m_pRemote; }

	int ListProcessesOnRemoteHost(/*out*/ATL::CString& processes);

	int SubscribeForEvents();

	inline OVERLAPPED& GetOverlapped() { return m_op; }

	inline HANDLE GetRemotePipe() const { return m_pRemote ? m_pRemote->GetPipe() : NULL; }

	void CloseRemotePipe();

	inline BYTE* GetIObuffer(int* maxSize) { *maxSize = sizeof(m_buffer); return &m_buffer[0]; }
};


/////////////////////////////////////////////////////////////////////////////
// CProcessLoggerItem - Process node
/////////////////////////////////////////////////////////////////////////////
class CProcessLoggerItem : public CLoggerItemBase
{
	HANDLE			m_hFile;		// pipe
	DWORD			m_pid;
	DWORD			m_dwDeleteOnClose;
	mutable wstring	m_displayName;		// this name contains "host\" prefix
	WCHAR			m_pipeName[128];	// the global buffer is OK because this is a single threaded app?

public:
	vector<LONG> m_rgMessageOffsets;
	vector<LONG> m_rgFilteredMessages;
	size_t m_errorCnt;
	LONGLONG m_llMaxLogFileSize;

	OVERLAPPED m_op;
	DWORD m_dwMMFsize;

	HANDLE m_hLogFile, m_hMapping;
	BYTE *m_logStart, *m_logWritePos;

	DWORD m_pendingBytes;			// bytes left to complete a message (zero if there is no incomplete message)
	BYTE *m_pendingWritePos;		// points to the beginning of the pending message
	bool m_pendingLengthUnknown;	// true when m_pendingBytes cannot be interpretted as payload length

	BYTE m_tszBuffer[PIPEBUFFERSIZE];

	// ctor/dtor
	CProcessLoggerItem(LPCTSTR pstrName, CLoggerItemBase* pParent, LOGGER_STATE state = LS_PAUSED, LONGLONG llMaxLogFileSize = MAX_LOGFILE_SIZE, BOOL is64 = FALSE);

	~CProcessLoggerItem();

	virtual LPCTSTR GetDisplayName() const;

	// methods
	inline HANDLE GetPipe() const { return m_hFile; }
	inline void SetPipe(HANDLE hFile) { m_hFile = hFile; }

	const WCHAR* GetPipeName();

	void ClosePipe();

	inline void SetPID(DWORD pid) { m_pid = pid; }
	inline const DWORD GetPID() const { return m_pid; }

	int OpenLogFile(DWORD dwDeleteOnClose = 0) override;

	int GrowLogFile(DWORD size);

	void CloseLogFile() override;

	bool AddMessageFromWritePos();

	size_t GetNumErrors() const override;

	size_t GetNumMessages() const override;

	const BYTE* GetMessageData(size_t i) const override;

	void FilterLoggerItems() override;

	/// Overrides
	void ClearLog() override;
	void SetTraceLevel(DWORD dwTraceLevel) override;
	void SetParamFormat(ULONG dwParamFormat) override;

protected:
	void AppendLogFileName(WCHAR* path);
	int InternalOpenLogFile();
	void WriteHeader();
};

/////////////////////////////////////////////////////////////////////////////
// CFileLoggerItem - Log File node (read-only)
/////////////////////////////////////////////////////////////////////////////
class CFileLoggerItem : public CLoggerItemBase
{
public:
	// data members
	HANDLE m_hLogFile, m_hMapping;
	BYTE *m_logStart;

	vector<LONG> m_rgMessageOffsets;
	vector<LONG> m_rgFilteredMessages;
	size_t m_errorCnt;

	wstring m_path;

	map<DWORD, wstring> m_mapLogger2Appname;

	// ctor/dtor
	CFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath);

	~CFileLoggerItem();

	// methods
	//inline void SetPID(DWORD pid) { m_pid = pid; }
	//inline const DWORD GetPID() const { return m_pid; }

	int OpenLogFile(DWORD dwReserved = 0) override;

	void CloseLogFile() override;

	virtual map<DWORD, wstring>* GetLogger2AppnameMap();

	size_t GetNumErrors() const override;

	size_t GetNumMessages() const override;

	const BYTE* GetMessageData(size_t i) const override;

	void FilterLoggerItems() override;

	void ClearLog() override;

protected:
	void ApplyFilter(const rethread::cancellation_token& loc_ct);
};

/////////////////////////////////////////////////////////////////////////////
// CCsvFileLoggerItem - Csv Log File node (read-only)
/////////////////////////////////////////////////////////////////////////////
class CCsvFileLoggerItem : public CLoggerItemBase
{
public:
	// data members
	vector<BYTE*> m_rgMessages;
	vector<BYTE*> m_rgFilteredMessages;
	size_t m_errorCnt;

	wstring m_path;

	map<DWORD, wstring> m_mapLogger2Appname;

	// ctor/dtor
	CCsvFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath);

	~CCsvFileLoggerItem();

	virtual int OpenLogFile();

	virtual map<DWORD, wstring>* GetLogger2AppnameMap();

	size_t GetNumErrors() const override;

	size_t GetNumMessages() const override;

	const BYTE* GetMessageData(size_t i) const override;

	void FilterLoggerItems() override;

	void ClearLog() override;
};
