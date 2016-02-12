// routine++
// Copyright (c) 2012-2016 Henry++

#include "rapp.h"

rapp::rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// initialize controls
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof (icex);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

	InitCommonControlsEx (&icex);

	// general information
	StringCchCopy (app_name, _countof (app_name), name);
	StringCchCopy (app_name_short, _countof (app_name_short), short_name);
	StringCchCopy (app_version, _countof (app_version), version);
	StringCchCopy (app_copyright, _countof (app_copyright), copyright);

	// get hinstance
	app_hinstance = GetModuleHandle (nullptr);

	// get current directory
	GetModuleFileName (nullptr, app_directory, _countof (app_directory));
	PathRemoveFileSpec (app_directory);

	// get configuration path
	StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_directory, app_name_short);

	if (!_r_fs_exists (app_config_path))
	{
		ExpandEnvironmentStrings (_r_fmt (L"%%APPDATA%%\\%s\\%s", _APP_AUTHOR, app_name), app_profile_directory, _countof (app_profile_directory));
		StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_profile_directory, app_name_short);
	}
	else
	{
		StringCchCopy (app_profile_directory, _countof (app_profile_directory), app_directory);
	}

	HDC h = GetDC (nullptr);

	// get dpi
	dpi_percent = DOUBLE (GetDeviceCaps (h, LOGPIXELSX)) / 96.0f;

#ifndef _APP_NO_ABOUT
	// create window class
	if (!GetClassInfoEx (GetHINSTANCE (), _APP_ABOUT_CLASS, nullptr))
	{
		WNDCLASSEX wcx = {0};

		wcx.cbSize = sizeof (WNDCLASSEX);
		wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wcx.lpfnWndProc = &AboutWndProc;
		wcx.hInstance = GetHINSTANCE ();
		wcx.lpszClassName = _APP_ABOUT_CLASS;
		wcx.hbrBackground = GetSysColorBrush (COLOR_3DFACE);

		RegisterClassEx (&wcx);
	}

	// create dialog font
	LOGFONT lf = {0};

	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FF_DONTCARE;
	lf.lfWeight = FW_NORMAL;
	lf.lfHeight = -MulDiv (9, GetDeviceCaps (h, LOGPIXELSY), 72);

	StringCchCopy (lf.lfFaceName, _countof (lf.lfFaceName), _r_sys_validversion (6, 0) ? L"MS Shell Dlg 2" : L"MS Shell Dlg");

	app_font = CreateFontIndirect (&lf);

	// get logo
#ifdef IDI_MAIN
	app_logo = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (IDI_MAIN), GetDPI (64));
#endif // IDI_MAIN

#endif // _APP_NO_ABOUT

	ReleaseDC (nullptr, h);

	// load settings
	ConfigInit ();
}

rapp::~rapp ()
{
	if (app_callback)
	{
		app_callback (app_hwnd, _RM_UNINITIALIZE, nullptr, nullptr);
	}

	if (app_mutex)
	{
		CloseHandle (app_mutex);
		app_mutex = nullptr;
	}

#ifndef _APP_NO_ABOUT
	if (app_logo)
	{
		DestroyIcon (app_logo);
		app_logo = nullptr;
	}

	if (app_font)
	{
		DeleteObject (app_font);
		app_font = nullptr;
	}

	UnregisterClass (_APP_ABOUT_CLASS, GetHINSTANCE ());
#endif // _APP_NO_ABOUT

#ifndef _APP_NO_SETTINGS
	ClearSettingsPage ();
#endif // _APP_NO_SETTINGS
}

BOOL rapp::Initialize ()
{
	// check mutex
	if (app_mutex)
		CloseHandle (app_mutex);

	app_mutex = CreateMutex (nullptr, FALSE, app_name_short);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		HWND h = FindWindowEx (nullptr, nullptr, nullptr, app_name);

		if (h)
		{
			_r_wnd_toggle (h, TRUE);
			return FALSE;
		}

		CloseHandle (app_mutex);
		app_mutex = nullptr;
	}

#ifdef _APP_NO_UAC
	if (_r_sys_uacstate () && SkipUacRun ())
	{
		return FALSE;
	}
#endif // _APP_NO_UAC

#ifndef _WIN64
	if (_r_sys_iswow64 ())
	{
		_r_msg (nullptr, MB_OK | MB_ICONEXCLAMATION, app_name, L"WARNING! 32-bit executable may incompatible with 64-bit operating system version!");
	}
#endif // _WIN64

	return TRUE;
}

VOID rapp::AutorunCreate (BOOL is_remove)
{
	HKEY key = nullptr;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS)
	{
		if (is_remove)
		{
			RegDeleteValue (key, app_name);
		}
		else
		{
			WCHAR buffer[MAX_PATH] = {0};

			GetModuleFileName (nullptr, buffer, _countof (buffer));
			PathQuoteSpaces (buffer);

			StringCchCat (buffer, _countof (buffer), L" ");
			StringCchCat (buffer, _countof (buffer), L"/minimized");

			RegSetValueEx (key, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((wcslen (buffer) + 1) * sizeof (WCHAR)));
		}

		RegCloseKey (key);
	}
}

BOOL rapp::AutorunIsPresent ()
{
	HKEY key = nullptr;
	BOOL result = FALSE;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		WCHAR path1[MAX_PATH] = {0};

		DWORD size = MAX_PATH;

		result = (RegQueryValueEx (key, app_name, nullptr, nullptr, (LPBYTE)path1, &size) == ERROR_SUCCESS);

		if (result)
		{
			WCHAR path2[MAX_PATH] = {0};

			PathRemoveArgs (path1);
			PathUnquoteSpaces (path1);

			GetModuleFileName (GetHINSTANCE (), path2, _countof (path2));

			// check path is to current module
			result = _wcsicmp (path1, path2) == 0;
		}

		RegCloseKey (key);
	}

	return result;
}

#ifndef _APP_NO_UPDATES
VOID rapp::CheckForUpdates (BOOL is_periodical)
{
	if (is_periodical)
	{
		if (!ConfigGet (L"CheckUpdates", 1).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", 0).AsLonglong ()) <= _APP_UPDATE_PERIOD)
		{
			return;
		}
	}

	is_update_forced = is_periodical;

	_beginthreadex (nullptr, 0, &CheckForUpdatesProc, (LPVOID)this, 0, nullptr);
}
#endif // _APP_NO_UPDATES

VOID rapp::ConfigInit ()
{
	app_config_array.clear (); // reset

	ParseINI (app_config_path, &app_config_array);

	LocaleInit ();
}

rstring rapp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%d", def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name)
{
	rstring result;

	if (!name)
	{
		name = app_name_short;
	}

	// check key is exists
	if (app_config_array.find (name) != app_config_array.end () && app_config_array[name].find (key) != app_config_array[name].end ())
	{
		result = app_config_array[name][key];
	}

	if (result.IsEmpty ())
	{
		result = def;
	}

	return result;
}

BOOL rapp::ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	if (!_r_fs_exists (app_profile_directory))
	{
		_r_fs_mkdir (app_profile_directory);
	}

	if (!name)
	{
		name = app_name_short;
	}

	// update hash value
	app_config_array[name][key] = val;

	return WritePrivateProfileString (name, key, val, app_config_path);
}

BOOL rapp::ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%lld", val), name);
}

#ifndef _APP_NO_ABOUT
VOID rapp::CreateAboutWindow ()
{
#ifdef _WIN64
	const INT architecture = 64;
#else
	const INT architecture = 32;
#endif // _WIN64

	MSG msg = {0};
	RECT rc = {0};

	rc.right = GetDPI (374);
	rc.bottom = GetDPI (114);

	AdjustWindowRectEx (&rc, WS_SYSMENU | WS_BORDER, FALSE, WS_EX_DLGMODALFRAME | WS_EX_TOPMOST);

	HWND hwnd = CreateWindowEx (WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, _APP_ABOUT_CLASS, I18N (this, IDS_ABOUT, 0), WS_SYSMENU | WS_BORDER, CW_USEDEFAULT, CW_USEDEFAULT, rc.right, rc.bottom, GetHWND (), nullptr, GetHINSTANCE (), nullptr);

	if (hwnd)
	{
		HMENU menu = GetSystemMenu (hwnd, FALSE);

		DeleteMenu (menu, SC_RESTORE, MF_BYCOMMAND);
		DeleteMenu (menu, SC_SIZE, MF_BYCOMMAND);
		DeleteMenu (menu, SC_MINIMIZE, MF_BYCOMMAND);
		DeleteMenu (menu, SC_MAXIMIZE, MF_BYCOMMAND);
		DeleteMenu (menu, 1, MF_BYPOSITION); // divider

		// create controls
		HWND hctrl = CreateWindowEx (0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ICON, GetDPI (12), GetDPI (12), GetDPI (64), GetDPI (64), hwnd, nullptr, nullptr, nullptr);
		SendMessage (hctrl, STM_SETIMAGE, IMAGE_ICON, (LPARAM)app_logo);

		hctrl = CreateWindowEx (0, WC_STATIC, _r_fmt (L"%s %s (%d-bit)", app_name, app_version, architecture), WS_CHILD | WS_VISIBLE, GetDPI (88), GetDPI (14), GetDPI (270), GetDPI (16), hwnd, nullptr, nullptr, nullptr);
		SendMessage (hctrl, WM_SETFONT, (WPARAM)app_font, TRUE);

		hctrl = CreateWindowEx (0, WC_STATIC, app_copyright, WS_CHILD | WS_VISIBLE, GetDPI (88), GetDPI (36), GetDPI (270), GetDPI (16), hwnd, nullptr, nullptr, nullptr);
		SendMessage (hctrl, WM_SETFONT, (WPARAM)app_font, TRUE);

		hctrl = CreateWindowEx (0, WC_LINK, _r_fmt (L"<a href=\"%s\">Website</a> | <a href=\"%s\">GitHub</a> | <a href=\"%s/%s/blob/master/LICENSE\">License agreement</a>", _APP_WEBSITE_URL, _APP_GITHUB_URL, _APP_GITHUB_URL, app_name_short), WS_CHILD | WS_VISIBLE, GetDPI (88), GetDPI (58), GetDPI (270), GetDPI (16), hwnd, nullptr, nullptr, nullptr);
		SendMessage (hctrl, WM_SETFONT, (WPARAM)app_font, TRUE);

		ShowWindow (hwnd, SW_SHOW);

		while (GetMessage (&msg, nullptr, 0, 0) > 0)
		{
			if (!IsDialogMessage (hwnd, &msg))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}
	}
}
#endif // _APP_NO_ABOUT

BOOL rapp::CreateMainWindow (DLGPROC proc, APPLICATION_CALLBACK callback)
{
	BOOL result = FALSE;

	if (Initialize ())
	{
		// create window
#ifdef IDD_MAIN
		app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (IDD_MAIN), nullptr, proc);
#endif // IDD_MAIN

		if (app_hwnd)
		{
			if (callback)
			{
				app_callback = callback;

				app_callback (app_hwnd, _RM_INITIALIZE, nullptr, nullptr);
			}

			// set title
			SetWindowText (app_hwnd, app_name);

			// set on top
			_r_wnd_top (app_hwnd, ConfigGet (L"AlwaysOnTop", 0).AsBool ());

			// set icons
#ifdef IDI_MAIN
			SendMessage (app_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (IDI_MAIN), GetSystemMetrics (SM_CXSMICON)));
			SendMessage (app_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (IDI_MAIN), GetSystemMetrics (SM_CXICON)));
#endif // IDI_MAIN

			// check for updates
#ifndef _APP_NO_UPDATES
			CheckForUpdates (TRUE);
#endif // _APP_NO_UPDATES

			result = TRUE;
		}
		else
		{
			result = FALSE;
		}
	}

	return result;
}

#ifndef _APP_NO_SETTINGS
VOID rapp::CreateSettingsWindow ()
{
	static bool is_opened = false;

	if (!is_opened)
	{
		is_opened = true;

#ifdef IDD_SETTINGS
		DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), GetHWND (), &SettingsWndProc, (LPARAM)this);
#endif // IDD_SETTINGS
	}

	is_opened = false;
}

VOID rapp::AddSettingsPage (HINSTANCE h, UINT dlg_id, LPCWSTR title, APPLICATION_CALLBACK callback)
{
	PAPPLICATION_PAGE ptr = new APPLICATION_PAGE;

	if (ptr)
	{
		ptr->h = h;
		ptr->dlg_id = dlg_id;
		ptr->title = title;
		ptr->callback = callback;

		app_settings_pages.push_back (ptr);
	}
}

VOID rapp::ClearSettingsPage ()
{
	for (size_t i = 0; i < app_settings_pages.size (); i++)
	{
		PAPPLICATION_PAGE ptr = app_settings_pages.at (i);

		delete ptr;
	}

	app_settings_pages.clear ();
}
#endif // _APP_NO_SETTINGS

rstring rapp::GetDirectory () const
{
	return app_directory;
}

rstring rapp::GetProfileDirectory () const
{
	return app_profile_directory;
}

rstring rapp::GetUserAgent () const
{
	return _r_fmt (L"%s/%s (+%s)", app_name, app_version, _APP_WEBSITE_URL);
}

INT rapp::GetDPI (INT v) const
{
	return (INT)ceil (static_cast<DOUBLE>(v) * dpi_percent);
}

HINSTANCE rapp::GetHINSTANCE () const
{
	return app_hinstance;
}

HWND rapp::GetHWND () const
{
	return app_hwnd;
}

VOID rapp::SetHWND (HWND hwnd)
{
	app_hwnd = hwnd;
}

#ifndef _APP_NO_SETTINGS
VOID rapp::LocaleEnum (HWND hwnd, INT ctrl_id)
{
	WIN32_FIND_DATA wfd = {0};
	HANDLE h = FindFirstFile (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\*.ini", app_directory), &wfd);

	if (h != INVALID_HANDLE_VALUE)
	{
		INT count = max (0, (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCOUNT, 0, 0));
		rstring def = ConfigGet (L"Language", nullptr);

		do
		{
			LPWSTR fname = wfd.cFileName;
			PathRemoveExtension (fname);

			SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, count, (LPARAM)fname);

			if (def.CompareNoCase (fname) == 0)
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, count, 0);
			}

			count += 1;
		}
		while (FindNextFile (h, &wfd));

		FindClose (h);
	}
	else
	{
		EnableWindow (GetDlgItem (hwnd, ctrl_id), FALSE);
	}
}
#endif // _APP_NO_SETTINGS

VOID rapp::LocaleInit ()
{
	rstring name = ConfigGet (L"Language", nullptr);

	app_locale_array.clear (); // clear
	is_localized = FALSE;

	if (!name.IsEmpty ())
	{
		is_localized = ParseINI (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\%s.ini", GetDirectory (), name), &app_locale_array);
	}
}

rstring rapp::LocaleString (HINSTANCE h, UINT uid, LPCWSTR name)
{
	rstring result;

	if (is_localized)
	{
		// check key is exists
		if (app_locale_array[_APP_I18N_SECTION].find (name) != app_locale_array[_APP_I18N_SECTION].end ())
		{
			result = app_locale_array[_APP_I18N_SECTION][name];

			result.Replace (L"\\t", L"\t");
			result.Replace (L"\\r", L"\r");
			result.Replace (L"\\n", L"\n");
		}
	}

	if (result.IsEmpty ())
	{
		if (!h)
		{
			h = GetHINSTANCE ();
		}

		LoadString (h, uid, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH);
		result.ReleaseBuffer ();
	}

	return result;
}

VOID rapp::LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position) const
{
	if (text)
	{
		MENUITEMINFO mif = {0};

		mif.cbSize = sizeof (MENUITEMINFO);
		mif.fMask = MIIM_STRING;
		mif.dwTypeData = (LPWSTR)text;

		SetMenuItemInfo (menu, item, by_position, &mif);
	}
}

#ifndef _APP_NO_ABOUT
LRESULT CALLBACK rapp::AboutWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static HWND hparent = nullptr;

	switch (msg)
	{
		case WM_CREATE:
		{
			hparent = GetWindow (hwnd, GW_OWNER);

			EnableWindow (hparent, FALSE);

			_r_wnd_center (hwnd);

			break;
		}

		case WM_DESTROY:
		{
			if (!GetWindow (hparent, GW_ENABLEDPOPUP))
			{
				EnableWindow (hparent, TRUE);
				SetActiveWindow (hparent);
			}

			PostQuitMessage (0);

			break;
		}

		case WM_CLOSE:
		case WM_LBUTTONDBLCLK:
		{
			DestroyWindow (hwnd);
			break;
		}

		case WM_LBUTTONDOWN:
		{
			SendMessage (hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
			break;
		}

		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
		case WM_CAPTURECHANGED:
		{
			LONG_PTR exstyle = GetWindowLongPtr (hwnd, GWL_EXSTYLE);

			if ((exstyle & WS_EX_LAYERED) == 0) { SetWindowLongPtr (hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED); }

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 100 : 255, LWA_ALPHA);
			SetCursor (LoadCursor (nullptr, (msg == WM_ENTERSIZEMOVE) ? IDC_SIZEALL : IDC_ARROW));

			break;
		}

		case WM_NOTIFY:
		{
			switch (LPNMHDR (lparam)->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					ShellExecute (hwnd, nullptr, PNMLINK (lparam)->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
				case IDCANCEL: // process Esc key
				{
					DestroyWindow (hwnd);
					break;
				}
			}

			break;
		}

		default:
		{
			return DefWindowProc (hwnd, msg, wparam, lparam);
		}
	}

	return FALSE;
}

#endif // _APP_NO_ABOUT

#ifndef _APP_NO_SETTINGS
INT_PTR CALLBACK rapp::SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = reinterpret_cast<rapp*>(lparam);
			break;
		}

		case WM_COMMAND:
		case WM_CONTEXTMENU:
		case WM_NOTIFY:
		case WM_MOUSEMOVE:
		case WM_LBUTTONUP:
		{
			MSG wmsg = {0};

			wmsg.message = msg;
			wmsg.wParam = wparam;
			wmsg.lParam = lparam;

			return this_ptr->app_settings_pages.at (this_ptr->app_settings_page)->callback (hwnd, _RM_MESSAGE, &wmsg, this_ptr->app_settings_pages.at (this_ptr->app_settings_page));
		}
	}

	return FALSE;
}

INT_PTR CALLBACK rapp::SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = reinterpret_cast<rapp*>(lparam);

			// localize
			SetWindowText (hwnd, I18N (this_ptr, IDS_SETTINGS, 0));

			SetDlgItemText (hwnd, IDC_APPLY, I18N (this_ptr, IDS_APPLY, 0));
			SetDlgItemText (hwnd, IDC_CLOSE, I18N (this_ptr, IDS_CLOSE, 0));

			// configure window
			_r_wnd_center (hwnd);

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, GetSystemMetrics (SM_CYSMICON));

			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				this_ptr->app_settings_pages.at (i)->hwnd = CreateDialogParam (this_ptr->app_settings_pages.at (i)->h, MAKEINTRESOURCE (this_ptr->app_settings_pages.at (i)->dlg_id), hwnd, &this_ptr->SettingsPagesProc, (LPARAM)this_ptr);

				HTREEITEM item = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->app_settings_pages.at (i)->title, nullptr, -1, (LPARAM)i);

				if (this_ptr->ConfigGet (L"SettingsLastPage", 0).AsSizeT () == i)
				{
					SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)item);
				}

				this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, _RM_INITIALIZE, nullptr, this_ptr->app_settings_pages.at (i));
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;

			if (lphdr->idFrom == IDC_NAV)
			{
				switch (lphdr->code)
				{
					case TVN_SELCHANGED:
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

						ShowWindow (this_ptr->app_settings_pages.at (size_t (pnmtv->itemOld.lParam))->hwnd, SW_HIDE);
						ShowWindow (this_ptr->app_settings_pages.at (size_t (pnmtv->itemNew.lParam))->hwnd, SW_SHOW);

						this_ptr->ConfigSet (L"SettingsLastPage", INT (pnmtv->itemNew.lParam));

						this_ptr->app_settings_page = size_t (pnmtv->itemNew.lParam);

						break;
					}
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
				case IDOK: // process Enter key
				case IDC_APPLY:
				{
					BOOL is_restart = FALSE;

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
					{
						if (this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, _RM_SETTINGS, nullptr, this_ptr->app_settings_pages.at (i)))
						{
							is_restart = TRUE;
						}
					}

					this_ptr->ConfigInit (); // reload settings

					// reinitialization
					if (this_ptr->app_callback)
					{
						this_ptr->app_callback (this_ptr->app_hwnd, _RM_UNINITIALIZE, nullptr, nullptr);
						this_ptr->app_callback (this_ptr->app_hwnd, _RM_INITIALIZE, nullptr, nullptr);
					}

					if (is_restart) { SendMessage (hwnd, WM_CLOSE, 0, 0);}

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
					{
						this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, _RM_UNINITIALIZE, nullptr, this_ptr->app_settings_pages.at (i)); // call closed state
					}

					_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", 0).AsBool ());

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}
#endif // _APP_NO_SETTINGS

#ifndef _APP_NO_UPDATES
UINT WINAPI rapp::CheckForUpdatesProc (LPVOID lparam)
{
	rapp* this_ptr = static_cast<rapp*>(lparam);

	if (this_ptr)
	{
		BOOL result = FALSE;
		HINTERNET internet = nullptr, connect = nullptr;

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_DISABLED);
#endif // IDM_CHECKUPDATES

		internet = InternetOpen (this_ptr->GetUserAgent (), INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);

		if (internet)
		{
			connect = InternetOpenUrl (internet, _r_fmt (L"%s/update.php?product=%s", _APP_WEBSITE_URL, this_ptr->app_name_short), nullptr, 0, INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_NO_COOKIES, 0);

			if (connect)
			{
				DWORD dwStatus = 0, dwStatusSize = sizeof (dwStatus);
				HttpQueryInfo (connect, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, nullptr);

				if (dwStatus == HTTP_STATUS_OK)
				{
					DWORD out = 0;

					CHAR buffer[_R_BUFFER_LENGTH] = {0};

					rstring bufferw;

					while (1)
					{
						if (!InternetReadFile (connect, buffer, _R_BUFFER_LENGTH - 1, &out) || !out)
						{
							break;
						}

						buffer[out] = 0;

						bufferw.Append (buffer);
					}

					bufferw.Trim (L" \r\n");

					if (_r_str_versioncompare (this_ptr->app_version, bufferw) == -1)
					{
						if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, I18N (this_ptr, IDS_UPDATE_YES, 0), bufferw) == IDYES)
						{
							ShellExecute (nullptr, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
						}

						result = TRUE;
					}
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", DWORD (_r_unixtime_now ()));
			}
		}

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);
#endif // IDM_CHECKUPDATES

		if (!result && !this_ptr->is_update_forced)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, I18N (this_ptr, IDS_UPDATE_NO, 0));
		}

		InternetCloseHandle (connect);
		InternetCloseHandle (internet);
	}

	return ERROR_SUCCESS;
}
#endif // _APP_NO_UPDATES

BOOL rapp::ParseINI (LPCWSTR path, rstring::map_two* map)
{
	BOOL result = FALSE;

	if (map && _r_fs_exists (path))
	{
		rstring section_ptr;
		rstring value_ptr;

		size_t length = 0, out_length = 0;
		size_t delimeter = 0;

		map->clear (); // clear first

		// get sections
		do
		{
			length += _R_BUFFER_LENGTH;

			out_length = GetPrivateProfileSectionNames (section_ptr.GetBuffer (length), static_cast<DWORD>(length), path);
		}
		while (out_length == (length - 1));

		section_ptr.SetLength (out_length);

		LPCWSTR section = section_ptr.GetString ();

		while (*section)
		{
			// get values
			length = 0;

			do
			{
				length += _R_BUFFER_LENGTH;

				out_length = GetPrivateProfileSection (section, value_ptr.GetBuffer (length), static_cast<DWORD>(length), path);
			}
			while (out_length == (length - 1));

			value_ptr.SetLength (out_length);

			LPCWSTR value = value_ptr.GetString ();

			while (*value)
			{
				rstring parser = value;

				delimeter = parser.Find (L'=');

				if (delimeter != rstring::npos)
				{
					(*map)[section][parser.Mid (0, delimeter)] = parser.Mid (delimeter + 1); // set
				}

				value += wcslen (value) + 1; // go next item
			}

			section += wcslen (section) + 1; // go next section
		}

		result = TRUE;
	}

	return result;
}

#ifdef _APP_NO_UAC
BOOL rapp::SkipUacCreate (BOOL is_remove)
{
	BOOL result = FALSE;
	BOOL action_result = FALSE;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	ITaskDefinition* task = nullptr;
	IRegistrationInfo* reginfo = nullptr;
	IPrincipal* principal = nullptr;
	ITaskSettings* settings = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;
	IRegisteredTask* registered_task = nullptr;

	rstring name;
	name.Format (_APP_TASKSCHD_NAME, app_name_short);

	if (_r_sys_validversion (6, 0))
	{
		CoInitializeEx (nullptr, COINIT_MULTITHREADED);
		CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

		if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
		{
			if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
			{
				if (SUCCEEDED (service->GetFolder (L"\\", &folder)))
				{
					if (is_remove)
					{
						result = (folder->DeleteTask (name.GetBuffer (), 0) == S_OK);
					}
					else
					{
						if (SUCCEEDED (service->NewTask (0, &task)))
						{
							if (SUCCEEDED (task->get_RegistrationInfo (&reginfo)))
							{
								reginfo->put_Author (_APP_AUTHOR);
								reginfo->Release ();
							}

							if (SUCCEEDED (task->get_Principal (&principal)))
							{
								principal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
								principal->Release ();
							}

							if (SUCCEEDED (task->get_Settings (&settings)))
							{
								settings->put_StartWhenAvailable (VARIANT_BOOL (FALSE));
								settings->put_DisallowStartIfOnBatteries (VARIANT_BOOL (FALSE));
								settings->put_StopIfGoingOnBatteries (VARIANT_BOOL (FALSE));
								settings->put_MultipleInstances (TASK_INSTANCES_PARALLEL);
								settings->put_ExecutionTimeLimit (L"PT0S");
								settings->Release ();
							}

							if (SUCCEEDED (task->get_Actions (&action_collection)))
							{
								if (SUCCEEDED (action_collection->Create (TASK_ACTION_EXEC, &action)))
								{
									if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
									{
										WCHAR path[MAX_PATH] = {0};

										GetModuleFileName (nullptr, path, _countof (path));
										PathQuoteSpaces (path);

										if (SUCCEEDED (exec_action->put_Path (path)) && SUCCEEDED (exec_action->put_Arguments (L"$(Arg0)")))
										{
											action_result = TRUE;
										}

										exec_action->Release ();
									}

									action->Release ();
								}

								action_collection->Release ();
							}

							if (action_result && SUCCEEDED (folder->RegisterTaskDefinition (name.GetBuffer (), task, TASK_CREATE_OR_UPDATE, _variant_t (), _variant_t (), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t (), &registered_task)))
							{
								result = TRUE;
								registered_task->Release ();
							}

							task->Release ();
						}
					}

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();
	}

	return result;
}

BOOL rapp::SkipUacIsPresent (BOOL is_run)
{
	BOOL result = FALSE;

	rstring name;
	name.Format (_APP_TASKSCHD_NAME, app_name_short);

	if (_r_sys_validversion (6, 0))
	{
		ITaskService* service = nullptr;
		ITaskFolder* folder = nullptr;
		IRegisteredTask* registered_task = nullptr;

		ITaskDefinition* task = nullptr;
		IActionCollection* action_collection = nullptr;
		IAction* action = nullptr;
		IExecAction* exec_action = nullptr;

		IRunningTask* running_task = nullptr;

		CoInitializeEx (nullptr, COINIT_MULTITHREADED);
		CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

		if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
		{
			if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
			{
				if (SUCCEEDED (service->GetFolder (L"\\", &folder)))
				{
					if (SUCCEEDED (folder->GetTask (name.GetBuffer (), &registered_task)))
					{
						if (SUCCEEDED (registered_task->get_Definition (&task)))
						{
							if (SUCCEEDED (task->get_Actions (&action_collection)))
							{
								if (SUCCEEDED (action_collection->get_Item (1, &action)))
								{
									if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
									{
										BSTR path1 = nullptr;
										WCHAR path2[MAX_PATH] = {0};

										exec_action->get_Path (&path1);
										PathUnquoteSpaces (path1);

										GetModuleFileName (GetHINSTANCE (), path2, _countof (path2));

										// check path is to current module
										if (_wcsicmp (path1, path2) == 0)
										{
											if (is_run)
											{
												INT numargs = 0;
												LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);
												rstring args;

												for (INT i = 1; i < numargs; i++)
												{
													args.Append (arga[i]);
													args.Append (L" ");
												}

												LocalFree (arga);

												variant_t ticker = args.Trim (L" ");

												if (SUCCEEDED (registered_task->RunEx (ticker, TASK_RUN_AS_SELF, 0, nullptr, &running_task)) && running_task)
												{
													TASK_STATE state;
													INT count = 5; // try count

													do
													{
														Sleep (500);

														running_task->Refresh ();
														running_task->get_State (&state);

														if (state == TASK_STATE_RUNNING || state == TASK_STATE_DISABLED)
														{
															if (state == TASK_STATE_RUNNING) { result = TRUE; }
															break;
														}
													}
													while (count--);

													running_task->Release ();
												}
											}
											else
											{
												result = TRUE;
											}
										}

										exec_action->Release ();
									}

									action->Release ();
								}

								action_collection->Release ();
							}

							task->Release ();
						}

						registered_task->Release ();
					}

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();
	}

	return result;
}

BOOL rapp::SkipUacRun ()
{
	if (_r_sys_uacstate ())
	{
		CloseHandle (app_mutex);
		app_mutex = nullptr;

		if (SkipUacIsPresent (TRUE))
		{
			return TRUE;
		}
		else
		{
			WCHAR buffer[MAX_PATH] = {0};

			SHELLEXECUTEINFO shex = {0};

			shex.cbSize = sizeof (shex);
			shex.fMask = SEE_MASK_UNICODE | SEE_MASK_FLAG_NO_UI;
			shex.lpVerb = L"runas";
			shex.nShow = SW_NORMAL;

			GetModuleFileName (nullptr, buffer, _countof (buffer));
			shex.lpFile = buffer;

			if (ShellExecuteEx (&shex))
			{
				return TRUE;
			}
		}

		app_mutex = CreateMutex (nullptr, FALSE, app_name_short);
	}
	else
	{
		return TRUE;
	}

	return FALSE;
}
#endif // _APP_NO_UAC