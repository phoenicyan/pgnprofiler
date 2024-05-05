/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

static inline BOOL ClassMatches(HWND hwnd, LPCTSTR szClassType)
{
	if (!szClassType || !lstrlen(szClassType))
		return TRUE;
	
	TCHAR szClassName[40];
	::GetClassName(hwnd, szClassName, 40);

	ATLTRACE2(atlTraceDBProvider, 0, "::ClassMatches(x%x, %S) == %S\n", hwnd, szClassType, szClassName);

	return (lstrcmpi(szClassType, szClassName) == 0);
}

template<class HookMgrType>
class CHookMgr
{
public:
	virtual ~CHookMgr()
	{
		Release();
	}

protected:
	BOOL InitHooks(LPCTSTR szClassFilter = NULL)
	{
		Release();

		m_hCallWndHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, NULL, GetCurrentThreadId());
		m_hCbtHook = SetWindowsHookEx(WH_CBT, CbtProc, NULL, GetCurrentThreadId());

		m_sClassFilter = szClassFilter;

		return TRUE;
	}

	void Release()
	{
		UnhookWindowsHookEx(m_hCallWndHook);
		UnhookWindowsHookEx(m_hCbtHook);
		m_hCallWndHook = NULL;
		m_hCbtHook = NULL;
	}

protected:
	HHOOK m_hCallWndHook;
	HHOOK m_hCbtHook;
	WTL::CString m_sClassFilter;

protected:
	static HookMgrType& GetHookMgrInstance();

	CHookMgr()	// singleton
		: m_hCallWndHook(NULL)
		, m_hCbtHook(NULL)
	{}

	// derived classes override whatever they need
	virtual void OnCallWndProc(const MSG& msg) = 0;
	virtual BOOL OnCbt(int nCode, WPARAM wParam, LPARAM lParam) = 0;

	inline BOOL ClassMatches(HWND hwnd)
	{
		return ::ClassMatches(hwnd, m_sClassFilter);
	}

	// WH_CALLWNDPROC
	static LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HC_ACTION)
		{
			CWPSTRUCT* pwp = (CWPSTRUCT*)lParam;

			if (GetHookMgrInstance().ClassMatches(pwp->hwnd))
			{
				MSG msg = { pwp->hwnd, pwp->message, pwp->wParam, pwp->lParam, 0, { 0, 0 } };
				GetHookMgrInstance().OnCallWndProc(msg);
			}
		}
		
		return CallNextHookEx(GetHookMgrInstance().m_hCallWndHook, nCode, wParam, lParam);
	}

	// WH_CBT
	static LRESULT CALLBACK CbtProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HC_ACTION)
		{
			if (GetHookMgrInstance().OnCbt(nCode, wParam, lParam))
				return TRUE;
		}
		
		return CallNextHookEx(GetHookMgrInstance().m_hCbtHook, nCode, wParam, lParam);
	}
};
