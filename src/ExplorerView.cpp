/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "resource.h"
#include "ExplorerView.h"

BOOL CExplorerView::PreTranslateMessage(MSG* pMsg)
{
   pMsg;
   return FALSE;
}

LRESULT CExplorerView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	LRESULT lResult = DefWindowProc(uMsg, wParam, lParam);
	ModifyStyle(0, TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_DISABLEDRAGDROP);

	// Insert localhost node
	int iImage = m_rootLogger.GetImage();

	HTREEITEM hItem = InsertItem(TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM, 
				m_rootLogger.GetName(), 
				iImage, iImage, 
				0, 0,
				(LPARAM) &m_rootLogger, 
				TVI_ROOT, 
				TVI_LAST);
	
	SetItemState(hItem, TVIS_EXPANDED, TVIS_EXPANDED);

	return lResult;
}

// Close connections to remote hosts
LRESULT CExplorerView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	for (list<CLoggerItemBase*>::iterator it = m_remoteHosts.begin(); it != m_remoteHosts.end(); it++)
	{
		delete (*it);
	}

	m_remoteHosts.clear();

	for (auto logger : m_rootLogger.GetChildren())
	{
		logger->CloseLogFile();
	}

	bHandled = FALSE;
	return 1;
}

LRESULT CExplorerView::OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	
	// If an item clicked then select it.
	UINT flags = 0;
	HTREEITEM hNewItem = HitTest(pt, &flags);
	//if (flags & TVHT_ONITEMBUTTON)
	{
		SelectItem(hNewItem);
	}

	ClientToScreen(&pt);
	HMENU hMenu = ::LoadMenu(_Module.GetResourceInstance(),(LPCTSTR)IDR_EXPLORER_MENU);
	if (hMenu != NULL)
	{
		int nPos = 2;	// when clicked on empty space in treeview
		CLoggerItemBase* pLogger = nullptr;
		if (hNewItem != 0)
		{
			pLogger = (CLoggerItemBase*)GetItemData(hNewItem);
			nPos = pLogger->GetType() == LT_LOGFILE ? 1 /*for log file*/ : 0 /*for host or process*/;
		}

		HMENU hSubMenu = ::GetSubMenu(hMenu, nPos);
		if (hSubMenu != NULL)
		{
			if (pLogger && pLogger->GetState() != LS_TERMINATED)	// if there is a selected item in "Applications" pane and the item is not in terminated state, then enable Capture menu item.
				::EnableMenuItem(hSubMenu, ID_CAPTURE, MF_ENABLED);

			if (!nPos)
			{	// set check for process/host profiling level
				HMENU hTraceLevel = ::GetSubMenu(hSubMenu, 3);
				::CheckMenuItem(hTraceLevel, pLogger->GetTraceLevel(), MF_BYPOSITION | MF_CHECKED);
			}

			::TrackPopupMenu(hSubMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);

			::PostMessage(m_hWnd, WM_NULL, 0, 0); 
		}

		::DestroyMenu(hMenu);
	}

	return 0;
}

LRESULT CExplorerView::OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if ((MK_CONTROL & wParam) != 0)
	{
		LONG delta = (int)GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? 2 : -2;

		AlterWindowFont(*this, delta);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_EXPLORERFONT, 0);

		return 0;
	}

	bHandled = FALSE;
	return 1;
}

LRESULT CExplorerView::OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == '.' || wParam == '>' || wParam == '+' || wParam == '=')
	{
		AlterWindowFont(*this, -2);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_EXPLORERFONT, 0);
		return 1;
	}

	if (wParam == ',' || wParam == '<' || wParam == '-' || wParam == '_')
	{
		AlterWindowFont(*this, 2);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_EXPLORERFONT, 0);
		return 1;
	}

	bHandled = FALSE;
	return 0;
}

LRESULT CExplorerView::OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return DLGC_WANTALLKEYS;
}

LRESULT CExplorerView::OnSendCmdToMainWnd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	GetParent().SendMessage(WM_COMMAND, MAKEWPARAM((WORD)wID, BN_CLICKED), (LPARAM)m_hWnd);

	return 0;
}

LRESULT CExplorerView::OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	this->PostMessage(WM_CHAR, '.', 1);
	return 0;
}

LRESULT CExplorerView::OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	this->PostMessage(WM_CHAR, ',', 1);
	return 0;
}

DWORD CExplorerView::OnItemPrePaint(int idCtrl, LPNMCUSTOMDRAW lpNMCustomDraw)
{
	bool bSelected = lpNMCustomDraw->uItemState & CDIS_SELECTED;
	NMTVCUSTOMDRAW* pnmtv = reinterpret_cast<NMTVCUSTOMDRAW*>(lpNMCustomDraw);
	pnmtv->clrText = bSelected ? White : (COLORREF)m_palit.CrText;
	pnmtv->clrTextBk = bSelected ? (COLORREF)m_palit.CrBackGndSel : (COLORREF)m_palit.CrBackGnd;

	return CDRF_DODEFAULT;
}

int CExplorerView::AddLogFile(CLoggerItemBase* pLogFile)
{
	m_rootLogger.Lock();

	m_logFiles.push_back(pLogFile);

	m_rootLogger.Unlock();

	return 0;//success
}

void CExplorerView::RemoveClearedLogFiles()
{
	m_rootLogger.Lock();

	for (list<CLoggerItemBase*>::iterator it=m_logFiles.begin(); it != m_logFiles.end(); )
	{
		CLoggerItemBase* pLogFile = *it;
		if (pLogFile->IsFilterActive() || pLogFile->GetNumMessages() != 0)
		{
			// don't delete logger if filter is turned on, or there are some messages in the log
			it++;
			continue;
		}

		if (m_pCurLogger == pLogFile)
			m_pCurLogger = &m_rootLogger;

		pLogFile->CloseLogFile();
		it = m_logFiles.erase(it);
	}

	m_rootLogger.Unlock();
}

bool CExplorerView::IsLogFileOpen(LPCTSTR lpFileName)
{
	return FindLogger<CLoggerItemBase>(m_logFiles, lpFileName) != nullptr;
}

int CExplorerView::AddHost(CHostLoggerItem* pHost)
{
	m_rootLogger.Lock();

	m_remoteHosts.push_back(pHost);

	m_rootLogger.Unlock();

	return 0;//success
}

bool CExplorerView::IsHostConnected(LPCTSTR lpHostName)
{
	return FindLogger<CLoggerItemBase>(m_remoteHosts, lpHostName) != nullptr;
}

CHostLoggerItem* CExplorerView::FindHost(HANDLE hPipe)
{
	for (list<CLoggerItemBase*>::const_iterator it = m_remoteHosts.begin(); it != m_remoteHosts.end(); it++)
	{
		if (hPipe == ((CHostLoggerItem*)*it)->GetRemotePipe())
		{
			return (CHostLoggerItem*)*it;
		}
	}

	return NULL;
}

void CExplorerView::CancelFilters()
{
	for_each(m_logFiles.begin(), m_logFiles.end(), [](CLoggerItemBase* logger) { logger->SetFilterActive(false); });

	for_each(m_remoteHosts.begin(), m_remoteHosts.end(), [](CLoggerItemBase* logger) { logger->SetFilterActive(false); });

	m_rootLogger.SetFilterActive(false);
}

template<class T>
T* CExplorerView::FindLogger(const list<T*>& loggers, const WCHAR* strText)
{
	T* pLogger = NULL;
	for (auto it=loggers.cbegin(); it != loggers.cend(); it++)
	{
		if (0 == wcscmp((*it)->GetName(), strText))
		{
			pLogger = *it;
			break;
		}
	}
	return pLogger;
}

// For 'host' nodes: delete all sub-items that do not have associated logger (in other words - delete items corresponding to the closed processes).
// For 'file' nodes: delete node unconditionally; it can be re-added later easily.
// Remarks: Preserves selected item.
// Return: true if item was deleted.
bool CExplorerView::_CleanupItem(HTREEITEM hItem, bool& bLoggerSelected)
{
	CLoggerItemBase* pLogger = (CLoggerItemBase*)GetItemData(hItem);
	if (!pLogger)
	{
		return false;
	}

	if (pLogger->GetType() == LT_LOGFILE)
	{
		DeleteItem(hItem);
		return true;
	}

	ATLASSERT(pLogger->GetType() == LT_LOGROOT);

	bool bExpanded = GetItemState(hItem, TVIS_EXPANDED) != 0;

	const auto& loggers = pLogger->GetChildren();
	const int nLen = 256;
	WCHAR strText[nLen];
	list<wstring> procNames;

	HTREEITEM hChildItem = GetChildItem(hItem);
	while (hChildItem != NULL)
	{
		HTREEITEM hTmp = GetNextSiblingItem(hChildItem);
		if (GetItemText(hChildItem, strText, nLen))
		{
			procNames.push_back(strText);

			if (!FindLogger<CProcessLoggerItem>(loggers, strText))
				DeleteItem(hChildItem);
		}
		hChildItem = hTmp;
	}

	if (m_pCurLogger == pLogger)
	{
		bLoggerSelected = true;
		SelectItem(hItem);
	}

	// Add host items back.
	for (auto it = loggers.cbegin(); it != loggers.cend(); it++)
	{
		if (find(procNames.begin(), procNames.end(), (*it)->GetName()) == procNames.end())
			_PopulateTree(*it, hItem);
		else
			_UpdateHostIcon(*it, hItem);
	}

	if (bExpanded)
		Expand(hItem);

	if (m_pCurLogger != 0 && !bLoggerSelected)
	{
		HTREEITEM hChildItem = GetChildItem(hItem);
		while (hChildItem)
		{
			if (m_pCurLogger == (CLoggerItemBase*)GetItemData(hChildItem))
			{
				bLoggerSelected = true;
				SelectItem(hChildItem);
				break;
			}
			hChildItem = GetNextSiblingItem(hChildItem);
		}
	}

	// Update logger image
	int iImage = pLogger->GetImage();
	SetItemImage(hItem, iImage, iImage);

	return false;
}

BOOL CExplorerView::Populate()
{
	SetRedraw(FALSE);

	// Remember a selected item data element.
	bool bLoggerSelected = false;
	list<CLoggerItemBase*> remoteHosts(m_remoteHosts);		// hosts to add
	HTREEITEM hItem = GetChildItem(TVI_ROOT);
	do
	{
		HTREEITEM hSibling = GetNextSiblingItem(hItem);

		if (!_CleanupItem(hItem, bLoggerSelected))
		{
			CLoggerItemBase* pLogger = (CLoggerItemBase*)GetItemData(hItem);
			list<CLoggerItemBase*>::iterator it = std::find(remoteHosts.begin(), remoteHosts.end(), pLogger);
			if (it != remoteHosts.end())
			{
				remoteHosts.erase(it);
			}
			else if (pLogger != &m_rootLogger)
			{
				DeleteItem(hItem);
			}
		}

		hItem = hSibling;
	} while (hItem != NULL);

	// Add new hosts
	for (list<CLoggerItemBase*>::iterator it = remoteHosts.begin(); it != remoteHosts.end(); it++)
	{
		HTREEITEM hHostItem = _PopulateTree(*it, TVI_ROOT);

		SetItemState(hHostItem, TVIS_EXPANDED, TVIS_EXPANDED);
	}

	// Add Log File items back.
	for (list<CLoggerItemBase*>::iterator it=m_logFiles.begin(); it != m_logFiles.end(); it++)
	{
		HTREEITEM hFileItem = _PopulateTree(*it, TVI_ROOT);

		if (m_pCurLogger == *it)
		{
			bLoggerSelected = true;
			SelectItem(hFileItem);
		}
	}

	// Select root item if selected item was removed
	if (!bLoggerSelected)
	{
		m_pCurLogger = &m_rootLogger;
		SelectItem(GetChildItem(TVI_ROOT));
	}

	SetRedraw(TRUE);

	return TRUE;
}

void CExplorerView::_UpdateHostIcon(const CLoggerItemBase* pBlock, HTREEITEM hParent)
{
	const int nLen = 256;
	WCHAR strText[nLen];
	HTREEITEM hItem = GetChildItem(hParent);
	while (hItem != NULL)
	{
		HTREEITEM hTmp = GetNextSiblingItem(hItem);
		if (GetItemText(hItem, strText, nLen) && 0 == wcscmp(pBlock->GetName(), strText))
		{
			int iImage = pBlock->GetImage();
			SetItemImage(hItem, iImage, iImage);
			break;
		}
		hItem = hTmp;
	}
}

// Insert given logger item and recursivelly insert all of its children
HTREEITEM CExplorerView::_PopulateTree(const CLoggerItemBase* pBlock, HTREEITEM hParent)
{
	int iImage = pBlock->GetImage();

	HTREEITEM hItem = InsertItem(TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
				pBlock->GetName(), 
				iImage, iImage, 
				0, 0, 
				(LPARAM) pBlock, 
				hParent, 
				TVI_LAST);
	
	const auto& loggers = const_cast<CLoggerItemBase*>(pBlock)->GetChildren();
	for (auto it=loggers.cbegin(); it != loggers.cend(); it++)
	{
		_PopulateTree(*it, hItem);
	}

	return hItem;
}
