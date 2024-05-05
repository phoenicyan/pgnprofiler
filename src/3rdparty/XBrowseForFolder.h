#pragma once

extern BOOL XBrowseForFolder(HWND hWnd,
					  LPCTSTR lpszInitialFolder,
					  int nRootFolder,
					  LPCTSTR lpszCaption,
					  LPTSTR lpszBuf,
					  DWORD dwBufSize,
					  BOOL bEditBox /*= FALSE*/);
