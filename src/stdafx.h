/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

// Change these values to use different versions
#define WINVER			0x0600
#define _WIN32_WINNT	0x0600
#define _WIN32_IE		0x0700
#define _RICHEDIT_VER	0x0200

#define _CRT_RAND_S
#include <stdlib.h>
#include <assert.h>

// cms070922: new for wtl8.0 to get atldlgs.h to compile.
#define _WTL_FORWARD_DECLARE_CSTRING

#define NOMINMAX
#include <algorithm>
using std::max;
using std::min;

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlcom.h>
#include <atlhost.h>
#include <atlwin.h>
#include <atlctl.h>

#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include <atlcrack.h>
//#include <atlcoll.h>
#include <atlstr.h>
//#include <atltypes.h>
#include <atlframe.h>
#include <atlmisc.h>
#include <atldlgs.h>
#include <atlsplit.h>
#include <atlctrls.h>
#include <shellapi.h>
#include <atlctrlw.h>
#include <atlctrlx.h>
#include <atlddx.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <xmemory>
using std::map;
using std::list;
using std::string;
using std::wstring;
using std::vector;

#include <psapi.h>		// for GetProcessImageFileName

#include "../auto_ptr_array.h"
#include "../CommonCfg.h"
#include "3rdparty/CppProperty.h"
#include "ProfilerDef.h"

typedef CImageList CImageListHandle;

#define PGL_HEADER_SIZE	12
#define PGL_HEADER		"PGNProfiler\1"				// Log file header, \1 is a file version number.
#define PGL_HEADER_WORK	"PGNProfiler\2"				// Work file header
#define CSV_HEADER		"AbsTime,SQLType,ClientSQL,ExecutedSQL,Parse,Prepare,Execute,GetRows,Rows,Database,UserName,PID,SessId,CmdId,CursorMode,Application,CmdType\r\n"
#define CSV_HEADER_SIZE	(_countof(CSV_HEADER)-1)

#define PGL_FILE_EXT	L"pgl"
#define CSV_FILE_EXT	L"csv"
#define JSON_FILE_EXT	L"json"
#define REG_PATH		L"SOFTWARE\\INTELLISOFT\\PGNP\\PGNPROFILER"
#define PROFILERSGN		"PGNProfiler"

#define SMALLEST_FONT		(-1)
#define LARGEST_FONT		(-100)

#define PGNPROF_SERVICE_CLASS	L"PGNPROF_SERVICE_CLASS"
#define PGNPROF_VREADER_CLASS	L"PGNPROF_VREADER_CLASS"
#define PGNPROF_VDSL_CLASS		L"PGNPROF_VDSL_CLASS"

#define MYMSG_DATA_READ			(WM_USER + 33)

#define MYMSG_PROFILE_ALL_APPS	(WM_USER + 35)
#define MYMSG_SAVESETTINGS		(WM_USER + 36)
#define MYMSG_LOAD_OPTIMIZED	(WM_USER + 37)
#define MYMSG_DRAGDROP_FILE		(WM_USER + 38)
#define MYMSG_HOST_CONNECTED	(WM_USER + 39)

#define MYMSG_UI_UPDATE			(WM_USER + 41)
#define MYMSG_START_SINGLE		(WM_USER + 42)
#define MYWM_THEME_CHANGE		(WM_USER + 43)
#define MYWM_DESTMOVE			(WM_USER + 44)

#define PGNPTITLE		L"PGNP Profiler"
#define PGNPTITLE_FULL	L"PGNP Profiler for PostgreSQL v%s"

// Checking sizeof at compile time. From the linux kernel: 
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#define MAX_LOGFILE_SIZE (1LL << 30)		// .pgl file cannot be larger than this value (can be changed in Registry)

enum OPTIONS_TYPE
{
	OT_GENERAL = 1,
	OT_COLUI = 2,
	OT_DETAILSFONT = 4,
	OT_EXPLORERFONT = 8,
	OT_FILTERFONT = 16,
};

enum FILTER_PROGRESS
{
	FILTER_START = -1,
	FILTER_FINISH = 101,
};

// Constants from PQNP. Query Types (for parser)
enum SQL_QUERY_TYPE
{
	QT_NONE,
	QT_SELECT,
	QT_INSERT,
	QT_UPDATE,
	QT_DELETE,
	QT_CREATE_DATABASE,
	QT_CREATE_TABLE,
	QT_CREATE_VIEW,
	QT_CREATE_INDEX,
	QT_CREATE_FUNCTION,
	QT_ALTER,

	QT_SET,
	QT_SHOW,

	// see <drop> productions in sql.grm
	QT_DROP_DATABASE,
	QT_DROP_TABLE,
	QT_DROP_VIEW,
	QT_DROP_INDEX,
	QT_DROP_FUNCTION,

	QT_PROCEDURE,
	QT_INTERNAL_PROC,		// internal stored procedure

	QT_START_TRANS,
	QT_COMMIT,
	QT_ROLLBACK,

	QT_NOTIFY,

	QT_COPY,
};

enum QCLR : short
{
	QCLR_FOCUS,		// focused/selected line
	QCLR_SELECT,
	QCLR_INSERT,
	QCLR_UPDATE,
	QCLR_DELETE,
	QCLR_CREATEDROPALTER,
	QCLR_PROCEDURES,
	QCLR_SYSSCHEMA,
	QCLR_SYSTEM,
	QCLR_ERROR,
	QCLR_NOTIFY,
	QCLR_NONE,
	QCLR_COMMENT,

	QCLR_SIZE		// enum size; should always be the last
};

struct LVCOLUMN_EX
{
	// LVCOLUMN members
    UINT mask;
    int fmt;
    int cx;
    LPWSTR pszText;
    int cchTextMax;
    int iSubItem;
#if (_WIN32_IE >= 0x0300)
    int iImage;
    int iOrder;
#endif

	// extention members
	LPWSTR pszDescription;
	bool bMayBeInvis;		// false if the column must always be shown (visible); or true if column may be invisible.
	LPWSTR pszAltText;		// Alternative column name
};

extern LONGLONG g_qpcFreq;

/////////////////////////////////////////////////////////////////////////////
// A mananged ImageList control

class CImageListCtrl : public CImageList
{
public:
   ~CImageListCtrl()
   {
      Destroy();
   }
};

extern LONG AlterWindowFont(ATL::CWindow window, LONG delta);

constexpr unsigned int str2hash(const char* str, int h = 0)
{
	unsigned int rez = !str[h] ? 5381 : (str2hash(str, h + 1) * 33) ^ str[h];

	if (0 == h)
	{
		ATLTRACE("case 0x%08X: /* %s */\r\n", rez, str);
	}

	return rez;
}
