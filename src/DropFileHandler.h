#pragma once

template <class T> class CDropFilesHandler
{
	public:
	// For frame windows:
	// You might turn drop handling on or off, depending, for instance, on user's choice.
	void RegisterDropHandler(BOOL bTurnOn = TRUE)
	{
        T* pT = static_cast<T*>(this);
		ATLASSERT(::IsWindow(pT->m_hWnd));
		::DragAcceptFiles(pT->m_hWnd, bTurnOn);
	}

	protected:
		UINT m_nFiles; // Number of dropped files, in case you want to display some kind of progress.

		// We'll use a message map here, so future implementations can, if necessary, handle
		// more than one message.
		BEGIN_MSG_MAP(CDropFilesHandler<T>)
			MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles) 
		END_MSG_MAP()

		// WM_DROPFILES handler: it calls a T function, with the path to one dropped file each time.
		LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"*** Got WM_DROPFILE! *******\n");
			T* pT = static_cast<T*>(this);
			// Your class should implement a boolean function. It should return TRUE only
			// if initialization (whatever is tried) was successful.
			if (!pT->IsReadyForDrop())
			{
				bHandled = FALSE;
				return 0;
			}

			CHDrop<TCHAR> cd(wParam);

			// Your class should have a public member of type HWND (like CWindow does).
			if (cd.IsInClientRect(pT->m_hWnd))
			{
				m_nFiles = cd.GetNumFiles();
				if (0 < m_nFiles)
				{
					for (UINT i = 0; i < m_nFiles; i++)
					{
						if (0 < cd.DragQueryFile(i))
						{
							ATLTRACE2(atlTraceDBProvider, 0, L"   Dropped one! %s\n", (LPCTSTR)cd);
							// Your class should return TRUE unless no more files are wanted.
							if (!pT->HandleDroppedFile((LPCTSTR) cd))
								i = m_nFiles + 10; // Break the loop
						}
					} // for
					pT->EndDropFiles();
				}
			}
			else 
				bHandled = FALSE;
			m_nFiles = 0;
			return 0;
		}
	private:
		// Helper class for CDropFilesHandler, or any other handler for WM_DROPFILES.
		// This one is a template to avoid compiling when not used.
		// It is a wrapper for a HDROP.
		template <class CHAR> class CHDrop
		{
			HDROP m_hd;
			bool bHandled;             // Checks if resources should be released.
			CHAR m_buff[MAX_PATH + 5]; // DragQueryFile() wants LPTSTR.
		public:
			// Constructor, destructor
			CHDrop(WPARAM wParam)
			{
				m_hd = (HDROP) wParam; 
				bHandled = false;
				m_buff[0] = '\0';
			}
			~CHDrop()
			{
				if (bHandled)
					::DragFinish(m_hd);
			}
			// Helper function, detects if the message is meant for 'this' window.
			BOOL IsInClientRect(HWND hw)
			{
				ATLASSERT(::IsWindow(hw));
				POINT p;
				::DragQueryPoint(m_hd, &p);
				RECT rc;
				::GetClientRect(hw, &rc);
				return ::PtInRect(&rc, p);
			}
			// This function returns the number of files dropped on the window by the current operation.
			UINT GetNumFiles(void)
			{
				return ::DragQueryFile(m_hd, 0xffffFFFF, NULL, 0);
			}
			// This function gets the whole file path for a file, given its ordinal number.
			UINT DragQueryFile(UINT iFile)
			{
				bHandled = true;
				return ::DragQueryFile(m_hd, iFile, m_buff, MAX_PATH);
			}
#ifdef _WTL_USE_CSTRING
			// WTL::CString overload for DragQueryFile (not used here, might be used by a handler
			// which is implemented outside CDropFilesHandler<T>.
			UINT DragQueryFile(UINT iFile, WTL::CString &cs)
			{
				bHandled = true;
				UINT ret = ::DragQueryFile(m_hd, iFile, m_buff, MAX_PATH);
				cs = m_buff;
				return ret;
			}
			inline operator WTL::CString() const { return WTL::CString(m_buff); }
#endif
			// Other string overloads (such as std::string) might come handy...
			
			// This class can 'be' the currently held file's path.
			inline operator const CHAR *() const { return m_buff; }
		}; // class CHDrop
};
