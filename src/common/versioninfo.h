/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <string>
#include <sstream>
#include <iomanip>

#ifdef _UNICODE
	using std::wstring;
	typedef wstring			tstring;

	using std::wstringstream;
	typedef wstringstream	tstringstream;
#else
	using std::string;
	typedef string			tstring;

	using std::stringstream;
	typedef stringstream	tstringstream;
#endif

using namespace std;

class VersionInfo
{
public:

   VersionInfo (const LPTSTR inFilename );
   VersionInfo (HMODULE hModule = NULL);

   ~VersionInfo();

   bool hasInfo () const
	{
		return theVersionInfo != 0;
   }

   unsigned short	majorVersion () const;
   unsigned short	minorVersion () const;
   unsigned short	build () const;
   unsigned short	subBuild () const;

   unsigned short	productMajorVersion () const;
   unsigned short	productMinorVersion () const;
   unsigned short	productBuild () const;
   unsigned short	productSubBuild () const;

   // tstring properties from VS_FIXEDFILEINFO

   tstring			version_str () const;
   tstring			productVersion_str () const;

   // tstring properties from unicode tstring block

   tstring			CompanyName () const;
   tstring			FileDescription () const;
   tstring			OriginalFilename () const;

protected:
	void			LoadVersionInfo(const LPTSTR inFileName);

private:

   void *theVersionInfo;
   void *theFixedInfo;
   WORD	*langInfo;
}; // end VersionInfo class declaration



// implementation

//--------------------------------------------------------------------------------
inline VersionInfo::VersionInfo (const LPTSTR inFileName)
   : theVersionInfo (NULL), theFixedInfo (NULL)
{
	LoadVersionInfo(inFileName);
} // end constructor

inline VersionInfo::VersionInfo (HMODULE hModule)
   : theVersionInfo (NULL), theFixedInfo (NULL)
{
	TCHAR inFileName[MAX_PATH];
	GetModuleFileName(hModule, inFileName, MAX_PATH);

	LoadVersionInfo(inFileName);
} // end constructor



//--------------------------------------------------------------------------------
inline VersionInfo::~VersionInfo ()
{
	delete theVersionInfo;
} // end destructor

inline void VersionInfo::LoadVersionInfo(const LPTSTR inFileName)
{
	unsigned long aVersionInfoSize = GetFileVersionInfoSize ( const_cast<LPTSTR> (inFileName), &aVersionInfoSize);
	
	if (aVersionInfoSize)
	{
		theVersionInfo = new char [aVersionInfoSize];
		if (!GetFileVersionInfo ( const_cast<LPTSTR> (inFileName)
								, 0
								, aVersionInfoSize
								, theVersionInfo
								) )
		{
			return;
		} // endif

		unsigned int aSize = 0;
		if (!VerQueryValue( theVersionInfo
							, _T("\\")
							, &theFixedInfo
							, &aSize))
		{
			return;
		} // endif

	    //First, to get tstring information, we need to get
		//language information.
		UINT cbLang;
		VerQueryValue(theVersionInfo, _T("\\VarFileInfo\\Translation"), (LPVOID*)&langInfo, &cbLang);

	} // endif
}


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::majorVersion () const
{
	if (!theFixedInfo) 
	{
		return 0;
	}
	
	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	
	return HIWORD(aInfo -> dwFileVersionMS);
} // end VersionInfo::majorVersion () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::minorVersion () const
{
	if (!theFixedInfo)
	{
		return 0;
	}
	
	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	
	return LOWORD(aInfo -> dwFileVersionMS);
} // end VersionInfo::minorVersion () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::build () const
{
	if (!theFixedInfo) 
	{
		return 0;
	}

	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	return LOWORD(aInfo -> dwFileVersionLS);
} // end VersionInfo::build () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::subBuild () const
{
	if (!theFixedInfo) return 0;
	
	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	return HIWORD(aInfo -> dwFileVersionLS);
} // end VersionInfo::subBuild () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::productMajorVersion () const
{
	if (!theFixedInfo) return 0;
	
	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	return HIWORD(aInfo -> dwProductVersionMS);
} // end VersionInfo::productMajorVersion () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::productMinorVersion () const
{
	if (!theFixedInfo) return 0;
	
	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	return LOWORD(aInfo -> dwProductVersionMS);
} // end VersionInfo::productMinorVersion () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::productBuild () const
{
	if (!theFixedInfo) return 0;

	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	return LOWORD(aInfo -> dwProductVersionLS);
} // end VersionInfo::productBuild () const


//--------------------------------------------------------------------------------
inline unsigned short VersionInfo::productSubBuild () const
{
	if (!theFixedInfo) return 1;
	
	VS_FIXEDFILEINFO * aInfo = (VS_FIXEDFILEINFO *) theFixedInfo;
	return HIWORD(aInfo -> dwProductVersionLS);
} // end VersionInfo::productSubBuild () const

inline tstring VersionInfo::version_str () const
{
	if (!theFixedInfo) return _T("");

	tstringstream ver;

	ver << (ULONG)majorVersion();
	ver << _T(".") << (ULONG)minorVersion();
	ver << _T(".") << (ULONG)subBuild();
	ver << _T(".") << (ULONG)build();
	
	return ver.str();
}

inline tstring VersionInfo::productVersion_str () const
{
	if (!theFixedInfo) return _T("");

	tstringstream ver;

	ver << (ULONG)productMajorVersion();
	ver << _T(".") << (ULONG)productMinorVersion();
	ver << _T(".") << (ULONG)productSubBuild();
	ver << _T(".") << (ULONG)productBuild();
	
	return ver.str();
}

inline tstring VersionInfo::CompanyName () const
{
	TCHAR tszVerStrName[128];
	LPVOID lpt;
	UINT cbBufSize;

	//Prepare the label -- default lang is bytes 0 & 1
    //of langInfo
    wsprintf(tszVerStrName, _T("\\StringFileInfo\\%04x%04x\\%s"),
                langInfo[0], langInfo[1], _T("CompanyName"));
    //Get the tstring from the resource data
    if (VerQueryValue(theVersionInfo, tszVerStrName, &lpt, &cbBufSize))
	{
        return (LPTSTR)lpt;    
	}

	return _T("");
}

inline tstring VersionInfo::FileDescription () const
{
	TCHAR tszVerStrName[128];
	LPVOID lpt;
	UINT cbBufSize;

	//Prepare the label -- default lang is bytes 0 & 1
    //of langInfo
    wsprintf(tszVerStrName, _T("\\StringFileInfo\\%04x%04x\\%s"),
                langInfo[0], langInfo[1], _T("FileDescription"));
    //Get the tstring from the resource data
    if (VerQueryValue(theVersionInfo, tszVerStrName, &lpt, &cbBufSize))
	{
        return (LPTSTR)lpt;    
	}

	return _T("");
}

inline tstring VersionInfo::OriginalFilename() const
{
	TCHAR tszVerStrName[128];
	LPVOID lpt;
	UINT cbBufSize;

	//Prepare the label -- default lang is bytes 0 & 1
	//of langInfo
	wsprintf(tszVerStrName, _T("\\StringFileInfo\\%04x%04x\\%s"),
		langInfo[0], langInfo[1], _T("OriginalFilename"));
	//Get the tstring from the resource data
	if (VerQueryValue(theVersionInfo, tszVerStrName, &lpt, &cbBufSize))
	{
		return (LPTSTR)lpt;
	}

	return _T("");
}

#pragma comment(lib, "version.lib")