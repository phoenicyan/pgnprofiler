// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change these values to use different versions
#define WINVER			0x0600
#define _WIN32_WINNT	0x0600
#define _WIN32_IE		0x0700
#define _RICHEDIT_VER	0x0200

#define PGL_HEADER_SIZE	12
#define PGL_HEADER		"PGNProfiler\1"				// Log file header, \1 is a file version number.
#define PGL_HEADER_WORK	"PGNProfiler\2"				// Work file header
#define CSV_HEADER		"AbsTime,SQLType,ClientSQL,ExecutedSQL,Parse,Prepare,Execute,GetRows,Rows,Database,UserName,PID,SessId,CmdId,CursorMode,Application,CmdType\r\n"
#define CSV_HEADER_SIZE	(_countof(CSV_HEADER)-1)

#define PGL_FILE_EXT	L"pgl"
#define CSV_FILE_EXT	L"csv"
#define JSON_FILE_EXT	L"json"

#define PROFILERSGN		"PGNProfiler"
#define GLB_LICCHIP		0x55						// todo: cleanup

// cms070922: new for wtl8.0 to get atldlgs.h to compile.
#define _WTL_FORWARD_DECLARE_CSTRING

#define NOMINMAX
#include <algorithm>
using std::max;
using std::min;

#define CTRAYNOTIFYICON_USE_WTL_STRING

#include <atlbase.h>
//#include <atlapp.h>

//extern CAppModule _Module;

//#include <atlcom.h>
//#include <atlhost.h>
//#include <atlwin.h>
//#include <atlctl.h>

#include <atlstr.h>
//#include <atldb.h>
#include <comdef.h>

#include <shellapi.h>
#include <comutil.h>

//#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <vector>
using std::map;
using std::list;
using std::string;
using std::wstring;
using std::vector;

typedef wstring _tstring;

#include <oledb.h>
#include <Sddl.h>

// Supported Postgres types (originated from pg_type.h)
enum
{
	PGTYPE_BOOL = 16,
	PGTYPE_BYTEA = 17,
	PGTYPE_NAME = 19,
	PGTYPE_INT8 = 20,
	PGTYPE_INT2 = 21,
	PGTYPE_INT4 = 23,
	PGTYPE_TEXT = 25,
	PGTYPE_OID = 26,
	PGTYPE_XID = 28,
	PGTYPE_XML = 142,
	PGTYPE_XML_ARRAY = 143,
	PGTYPE_UI1 = 444,		// this is the only exception; the artificial value that allows us to support UI1 (hope 444 is not used in PG)
	PGTYPE_FLOAT4 = 700,
	PGTYPE_FLOAT8 = 701,
	PGTYPE_UNKNOWN = 705,
	PGTYPE_MONEY = 790,
	PGTYPE_TEXTRG = 1009,
	PGTYPE_BPCHAR = 1042,
	PGTYPE_VARCHAR = 1043,
	PGTYPE_DBDATE = 1082,
	PGTYPE_TIME = 1083,
	PGTYPE_DATE = 1114,		// defined same as PGTYPE_TIMESTAMP
	PGTYPE_TIMESTAMP = 1114,
	PGTYPE_TIMESTAMPTZ = 1184,
	PGTYPE_INTERVAL = 1186,
	PGTYPE_TIMETZ = 1266,
	PGTYPE_NUMERIC = 1700,
	PGTYPE_RECORD = 2249,
	PGTYPE_VOID = 2278,
	PGTYPE_UUID = 2950,
	PGTYPE_UUID_ARRAY = 2951,
	PGTYPE_JSONB = 3802,
	PGTYPE_JSONB_ARRAY = 3807,

	PGTYPE_BOOL_ARRAY = 1000, /* bool[] */
	PGTYPE_BYTEA_ARRAY = 1001, /* bytea[] */
	PGTYPE_CHAR_ARRAY = 1002, /* char[] */
	PGTYPE_NAME_ARRAY = 1003, /* name[] */
	PGTYPE_INT2_ARRAY = 1005, /* int2[] */
	PGTYPE_INT2VECTOR_ARRAY = 1006, /* int2vector[] */
	PGTYPE_INT4_ARRAY = 1007, /* int4[] */
	PGTYPE_REGPROG_ARRAY = 1008, /* regproc[] */
	PGTYPE_OID_ARRAY = 1028, /* oid[] */
	PGTYPE_TID_ARRAY = 1010, /* tid[] */
	PGTYPE_XID_ARRAY = 1011, /* xid[] */
	PGTYPE_CID_ARRAY = 1012, /* cid[] */
	PGTYPE_OIDVECTOR_ARRAY = 1013, /* oidvector[] */
	PGTYPE_BPCHAR_ARRAY = 1014, /* bpchar[] */
	PGTYPE_VARCHAR_ARRAY = 1015, /* varchar[] */
	PGTYPE_INT8_ARRAY = 1016, /* int8[] */
	PGTYPE_POINT_ARRAY = 1017, /* point[] */
	PGTYPE_LSEG_ARRAY = 1018, /* lseg[] */
	PGTYPE_PATH_ARRAY = 1019, /* path[] */
	PGTYPE_BOX_ARRAY = 1020, /* box[] */
	PGTYPE_FLOAT4_ARRAY = 1021, /* float4[] */
	PGTYPE_FLOAT8_ARRAY = 1022, /* float8[] */
	PGTYPE_ABSTIME_ARRAY = 1023, /* abstime[] */
	PGTYPE_RELTIME_ARRAY = 1024, /* reltime[] */
	PGTYPE_TINTERVAL_ARRAY = 1025, /* tinterval[] */
	PGTYPE_POLYGON_ARRAY = 1027, /* polygon[] */
	PGTYPE_ACLITEM = 1033, /* aclitem */
	PGTYPE_ACLITEM_ARRAY = 1034, /* aclitem[] */
	PGTYPE_MACADDR_ARRAY = 1040, /* macaddr[] */
	PGTYPE_INET_ARRAY = 1041, /* inet[] */
	PGTYPE_CIDR_ARRAY = 651, /* cidr[] */
	PGTYPE_CSTRING_ARRAY = 1263, /* cstring[] */
	PGTYPE_TIMESTAMP_ARRAY = 1115, /* timestamp[] */
	PGTYPE_DATE_ARRAY = 1182, /* date[] */
	PGTYPE_TIME_ARRAY = 1183, /* time[] */
	PGTYPE_TIMESTAMPTZ_ARRAY = 1185, /* timestamptz[] */
	PGTYPE_INTERVAL_ARRAY = 1187, /* interval[] */
	PGTYPE_NUMERIC_ARRAY = 1231, /* numeric[] */
	PGTYPE_TIMETZ_ARRAY = 1270, /* timetz[] */
	PGTYPE_BIT = 1560, /* bit */
	PGTYPE_BIT_ARRAY = 1561, /* bit[] */
	PGTYPE_VARBIT = 1562, /* varbit */
	PGTYPE_VARBIT_ARRAY = 1563, /* varbit[] */
	PGTYPE_REFCURSOR = 1790, /* refcursor */
	PGTYPE_REFCURSOR_ARRAY = 2201, /* refcursor[] */
	PGTYPE_POSTGIS_GEOMETRY = 1'000'001,	/* our made-up geometry */
};

#define  TYPTYPE_BASE		'b' /* base type (ordinary scalar type) */
#define  TYPTYPE_COMPOSITE	'c' /* composite (e.g., table's rowtype) */
#define  TYPTYPE_DOMAIN		'd' /* domain over another type */
#define  TYPTYPE_ENUM		'e' /* enumerated type */
#define  TYPTYPE_PSEUDO		'p' /* pseudo-type */

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

// PG Type Name: timestamp [ (p) ] [ with time zone ]
// Storage Size: 8 bytes
// Description:  both date and time[, with time zone ]
// Low Value:    4713 BC 
// High Value:   5874897 AD
// Resolution:   1 microsecond / 14 digits
typedef __int64 PGTimeStamp;

// PG Type Name: date
// Storage Size: 4 bytes
// Description:  dates only
// Low Value:    4713 BC 
// High Value:   5874897 AD
// Resolution:   1 day
typedef int	PGDBDate;

#define BILLIONTH_TO_MILLIONTH(fraction)		(fraction >= 0 ? (fraction + 500) / 1000 : (fraction - 500) / 1000)
#define MILLIONTH_TO_BILLIONTH(fraction)		(fraction * 1000)

#define JULIAN_MINYEAR (-4713)
#define JULIAN_MINMONTH (11)
#define JULIAN_MINDAY (24)
#define JULIAN_MAXYEAR (5874898)

#define IS_VALID_JULIAN(y,m,d) ((((y) > JULIAN_MINYEAR) \
  || (((y) == JULIAN_MINYEAR) && (((m) > JULIAN_MINMONTH) \
  || (((m) == JULIAN_MINMONTH) && ((d) >= JULIAN_MINDAY))))) \
 && ((y) < JULIAN_MAXYEAR))

#define POSTGRES_EPOCH_JDATE 2451545 // date2j(2000,1,1)

#define DAYS_PER_YEAR	365.25	/* assumes leap year every four years */
#define MONTHS_PER_YEAR 12

#define DAYS_PER_MONTH	30		/* assumes exactly 30 days per month */
#define HOURS_PER_DAY	24		/* assume no daylight savings time changes */

#define SECS_PER_YEAR	(36525 * 864)	/* avoid floating-point computation */
#define SECS_PER_DAY	86400
#define SECS_PER_HOUR	3600
#define SECS_PER_MINUTE 60
#define MINS_PER_HOUR 60

typedef enum DBTYPEENUM EOledbTypes;
#define DBTYPE_XML               ((EOledbTypes) 141) // introduced in SQL 2005
#define DBTYPE_TABLE             ((EOledbTypes) 143) // introduced in SQL 2008
#define DBTYPE_SQLVARIANT        ((EOledbTypes) 144) // introduced in MDAC 2.5
#define DBTYPE_DBTIME2           ((EOledbTypes) 145) // introduced in SQL 2008
#define DBTYPE_DBTIMESTAMPOFFSET ((EOledbTypes) 146) // introduced in SQL 2008

#include <pshpack8.h>    // 8-byte structure packing

typedef struct tagDBTIME2
{
	USHORT hour;
	USHORT minute;
	USHORT second;
	ULONG fraction;
} 	DBTIME2;

typedef struct tagDBTIMESTAMPOFFSET
{
	SHORT year;
	USHORT month;
	USHORT day;
	USHORT hour;
	USHORT minute;
	USHORT second;
	ULONG fraction;
	SHORT timezone_hour;
	SHORT timezone_minute;
} 	DBTIMESTAMPOFFSET;

#include <poppack.h>     // restore original structure packing

