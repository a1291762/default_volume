#include <windows.h>
#include <stdio.h>

extern void dbglog(const char *format, ...);
extern void wdbglog(const TCHAR *format, ...);
extern int monitor_registry(HKEY *g_property_store);
extern HWND create_tray_icon(HINSTANCE hInstance, HKEY *g_property_store);

static HKEY property_store;

DWORD WINAPI MyThreadFunction(LPVOID lpParam) {
	// This needs to be called once when using COM
	HRESULT hr;
	hr = CoInitialize(NULL);
	if (hr != S_OK) {
		dbglog("Failed to CoInitialize %lx\n", hr);
		goto cleanup;
	}

	int ret = monitor_registry(&property_store);

cleanup:
	CoUninitialize();

	// close the tray icon
	HWND hwnd = (HWND)lpParam;
	PostMessage(hwnd, WM_CLOSE, 0, 0);

	return ret;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PTSTR lpCmdLine, int nCmdShow) {
	// the tray icon lives on the main thread
	HWND hwnd = create_tray_icon(hInstance, &property_store);

	// the registry listener runs in a separate thread
	HANDLE thread = CreateThread(NULL, 0, MyThreadFunction, hwnd, 0, NULL);

	// run the message loop
	MSG msg = {0};
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	WaitForMultipleObjects(1, &thread, TRUE, INFINITE);
	DWORD exitCode;
	GetExitCodeThread(thread, &exitCode);
	CloseHandle(&thread);

	return exitCode;
}
