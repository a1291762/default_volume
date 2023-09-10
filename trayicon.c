#include <windows.h>
#include <stdio.h>
#include <shellapi.h>
#include <winuser.h>

#include <assert.h>

extern void dbglog(const char *format, ...);
extern void wdbglog(const TCHAR *format, ...);

#define IDI_TRAYICON 2
#define WM_USER_SHELLICON (WM_USER+1)

static HKEY *g_property_store;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// dbglog("WindowProc uMsg %d wParam %lld, lParam %lld\n", uMsg, wParam, lParam);
	switch (uMsg) {
	case WM_USER_SHELLICON:
		switch (lParam) {
		case WM_LBUTTONDBLCLK:
			// user double-clicked on the tray icon

			// delete the tray icon
			NOTIFYICONDATA data = {0};
			data.cbSize = sizeof(NOTIFYICONDATA);
			data.hWnd = hwnd;
			data.uID = IDI_TRAYICON;
			Shell_NotifyIcon(NIM_DELETE, &data);

			HKEY property_store = *g_property_store;
			if (property_store) {
				// stop the worker thread
				LSTATUS err = RegCloseKey(property_store);
				*g_property_store = NULL;
				if (err != ERROR_SUCCESS) {
					dbglog("Failed to RegCloseKey from WindowProc\n");
				}
			}

			// exit the event loop
			PostQuitMessage(0);
			break;
		}
		break;
	case WM_CLOSE:
		// the worker thread sends this when it exits

		// exit the event loop
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND create_tray_icon(HINSTANCE hInstance, HKEY *_property_store) {
	g_property_store = _property_store;
	const wchar_t CLASS_NAME[]  = L"Dummy Window";
	WNDCLASS wc = {0};
	wc.lpfnWndProc   = WindowProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);
	HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Not Shown",
		WS_DISABLED | WS_MINIMIZE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	if (!hwnd) {
		DWORD error = GetLastError();
		dbglog("got error %lx\n", error);
		return NULL;
	}

	NOTIFYICONDATA data = {0};
	data.cbSize = sizeof(NOTIFYICONDATA);
	data.hWnd = hwnd;
	data.uID = IDI_TRAYICON;
	data.uFlags = NIF_ICON | NIF_MESSAGE;
	data.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYICON));
	data.uCallbackMessage = WM_USER_SHELLICON;
	if (!Shell_NotifyIcon(NIM_ADD, &data)) {
		dbglog("failed to add tray icon\n");
		// FIXME deallocate the window?
		return NULL;
	}

	return hwnd;
}
