/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

using std::pair;

class CLoggerItemBase
{
protected:
	// Data Members
	wstring				m_name;
	string				m_nameA;
	wstring				m_user, m_pwd;
	DWORD				m_dwTraceLevel;

	list<CLoggerItemBase*> m_children;
	CLoggerItemBase* m_pParent;
	FILETIME			m_initialTime;
	wstring				m_filterStr;
	bool				m_filterActive;
	bool				m_bROmode;			// true for read-only file

	const wstring* m_psWorkPath;		// points to MainFrame variable containing WorkPath option

public:
	// Constructor / destructor
	CLoggerItemBase();
	CLoggerItemBase(LPCTSTR pstrName, CLoggerItemBase* pParent);
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

	const CLoggerItemBase* GetParent() const { return m_pParent; }
	const list<CLoggerItemBase*>& GetChildren() const { return m_children; }

	BOOL DeleteChildren();
	BOOL AddChild(CLoggerItemBase* pBlock);

	// Implementation
	//inline int GetImage() const { return m_type | (m_state << 2); }

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

	virtual void SetFilterActive(bool isActive, std::function<void(int)> fnProgressCallback = [](int) {});

	virtual void FilterLoggerItems() = 0;

	virtual void ClearLog() = 0;

	//static void Lock()
	//{
	//	_lock.Wait();
	//}

	//static void Unlock()
	//{
	//	_lock.Release();
	//}

protected:
	//static CriticalSection _lock;

	//static void SetVariables(Driver& driver, const BYTE* baseAddr, LONGLONG initialTime, DWORD pid, LPCSTR appName);
};
