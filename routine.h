// routine++
// Copyright (c) 2013-2015 Henry++
//
// lastmod: Oct 15, 2015

#pragma once

#include <windows.h>
#include <uxtheme.h>
#include <unordered_map>
#include <time.h>
#include <math.h>
#include <lm.h>
#include <commctrl.h>
#include <atlstr.h>
#include <strsafe.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

#define ROUTINE_BUFFER_LENGTH 4096

#define ROUTINE_PERCENT_OF(val, total) (INT)ceil((double(val) / double(total)) * 100.0)
#define ROUTINE_PERCENT_VAL(val, total) (double(total) * double(val) / 100.0)

/*
	CString Map (simple string associative array)
*/

struct CStringHash
{
	size_t operator()(const CString& k) const
	{
		ULONG hash = 5381;
		INT c = 0;

		LPWSTR str = (LPWSTR)k.GetString ();

		while (c = *str++)
		{
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
		}

		return hash;
	}
};

struct CStringEqul
{
	BOOL operator()(const CString& lhs, const CString& rhs) const
	{
		return lhs == rhs;
	}
};

typedef std::unordered_map<CString, CString, CStringHash, CStringEqul> CStringMap;

/*
	Write debug log to console
*/

#define WDEBUG1(a) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, L"%s", a)
#define WDEBUG2(a, ...) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, __VA_ARGS__)

VOID _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...);

/*
	Format strings, dates, numbers
*/

CString _r_fmt (LPCWSTR format, ...);

CString _r_fmt_date (LPFILETIME ft, const DWORD flags = FDTF_DEFAULT); // see SHFormatDateTime flags definition
CString _r_fmt_date (__time64_t ut, const DWORD flags = FDTF_DEFAULT);

CString _r_fmt_size64 (DWORDLONG size);

/*
	System messages
*/

#define WMSG1(a) _r_msg (nullptr, 0, nullptr, L"%s", a)
#define WMSG2(a, ...) _r_msg (nullptr, 0, nullptr, a, __VA_ARGS__)

INT _r_msg (HWND hwnd, UINT type, LPCWSTR title, LPCWSTR format, ...);
HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR ref);

/*
	Clipboard operations
*/

CString _r_clipboard_get (HWND hwnd);
VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length);

/*
	System information
*/

BOOL _r_system_adminstate ();
BOOL _r_system_iswow64 ();
BOOL _r_system_setprivilege (LPCWSTR privilege, BOOL enable);
BOOL _r_system_uacstate ();
BOOL _r_system_validversion (DWORD major, DWORD minor);

/*
	Window management
*/

VOID _r_windowcenter (HWND hwnd);
VOID _r_windowtoggle (HWND hwnd, BOOL show);
VOID _r_windowtotop (HWND hwnd, BOOL enable);

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, INT ctrl, LPCWSTR text, INT width, INT subitem, INT fmt);
INT _r_listview_addgroup (HWND hwnd, INT ctrl, INT group_id, LPCWSTR text, UINT align = 0, UINT state = 0);
INT _r_listview_additem (HWND hwnd, INT ctrl, LPCWSTR text, INT item, INT subitem, INT image = -1, INT group_id = -1, LPARAM lparam = 0);
INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl, INT column);
LPARAM _r_listview_getlparam (HWND hwnd, INT ctrl, INT item);
CString _r_listview_gettext (HWND hwnd, INT ctrl, INT item, INT subitem);
DWORD _r_listview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle);

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, INT ctrl, LPCWSTR text, INT image = -1, LPARAM lparam = 0);
LPARAM _r_treeview_getlparam (HWND hwnd, INT ctrl, HTREEITEM item);
DWORD _r_treeview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle, INT height);

/*
	Control: statusbar
*/

BOOL _r_status_settext (HWND hwnd, INT ctrl, INT part, LPCWSTR text);
VOID _r_status_setstyle (HWND hwnd, INT ctrl, INT height);

HWND _r_setcontroltip (HWND hwnd, INT ctrl, LPWSTR text);
BOOL _r_seteditbaloontip (HWND hwnd, INT ctrl, LPCWSTR title, LPCWSTR text, INT icon);

BOOL _r_file_is_exists (LPCWSTR path);
DWORD64 _r_file_size (HANDLE h);


__time64_t _r_unixtime_now ();
VOID _r_unixtime_to_filetime (__time64_t t, LPFILETIME pft);
VOID _r_unixtime_to_systemtime (__time64_t t, LPSYSTEMTIME pst);

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT width, INT height);
INT _r_string_versioncompare (LPCWSTR v1, LPCWSTR v2);

/*
BOOL _r_skipuac_run ();
BOOL _r_skipuac_is_present (BOOL checkandrun);
BOOL _r_skipuac_cancer (BOOL remove);
*/

/*
	Exported function definitions
*/

typedef BOOL (WINAPI *IW64P) (HANDLE, PBOOL); // IsWow64Process
typedef HRESULT (WINAPI *LIWSD) (HINSTANCE, PCWSTR, INT, INT, HICON*); // LoadIconWithScaleDown
typedef VOID (WINAPI *TDI) (TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect