/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

class CMyFileDialog : public CFileDialogImpl<CMyFileDialog>
{
public:
    // Construction
    CMyFileDialog ( BOOL bOpenFileDialog,
                    _U_STRINGorID szDefExt = 0U,
                    _U_STRINGorID szFileName = 0U, 
                    DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    _U_STRINGorID szFilter = 0U,
                    HWND hwndParent = NULL );

    // Maps
    BEGIN_MSG_MAP(CMyFileDialog)
        CHAIN_MSG_MAP(CFileDialogImpl<CMyFileDialog>)
    END_MSG_MAP()

    // Overrides
    void OnInitDone ( LPOFNOTIFY lpon );

protected:
    ATL::CString m_sDefExt, m_sFileName, m_sFilter;

    LPCTSTR PrepFilterString(ATL::CString& sFilter);
};


class CMyFolderDialog : public CFolderDialogImpl<CMyFolderDialog>
{
public:
    // Construction
    CMyFolderDialog ( HWND hWndParent = NULL,
                      _U_STRINGorID szTitle = 0U,
                      UINT uFlags = BIF_RETURNONLYFSDIRS );

    // Overrides
    void OnInitialized();

    // Data
    ATL::CString m_sInitialDir;

protected:
	ATL::CString m_sTitle;
};
