// stdafx.h : include file for standard system include files,
//	or project specific include files that are used frequently, but
//		are changed infrequently
//


#define VC_EXTRALEAN		// Windows ヘッダーから使用されていない部分を除外します。
#define WINVER 0x0501 //XP
#define _WIN32_WINNT 0x0501 //XP
#define _WIN32_WINDOWS 0x0410 // これを Windows Me またはそれ以降のバージョン向けに適切な値に変更してください。
#define _WIN32_IE 0x0600	// これを IE の他のバージョン向けに適切な値に変更してください。

#define ISOLATION_AWARE_ENABLED 1
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#include <afxwin.h>         // MFC のコアおよび標準コンポーネント
#include <afxext.h>         // MFC の拡張部分


//#include <afxdisp.h>        // MFC オートメーション クラス


#ifndef _AFX_NO_OLE_SUPPORT
//#include <afxdtctl.h>		// MFC の Internet Explorer 4 コモン コントロール サポート
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC の Windows コモン コントロール サポート
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxpriv.h>

#define _WTL_FORWARD_DECLARE_CSTRING 
#include <atlcoll.h>
#include <atlstr.h>
#include <atlbase.h>
#include <atlwin.h>

#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlapp.h>
#include <atldlgs.h>
#include <atlgdi.h>//CDCHandle
#include <atlctrls.h>//CComboBox
#include <atlframe.h>//COwnerDraw
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlddx.h>
#include <atlsplit.h>

#include <shlobj.h>

#include "..\cmos.h"

#include <imagehlp.h>

namespace BZ { // TODO:HACK!! avoid conflict with atl
#include "MemDC.h"
}

#include <crtdbg.h>

/*
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
*/

#ifdef BZ_SCRIPT_API
#define BZ_SCRIPT_DLLEXTERN __declspec(dllexport)
#else
#define BZ_SCRIPT_DLLEXTERN __declspec(dllimport)
#endif
