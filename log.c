#include <windows.h>
#include <strsafe.h>


// this variant is more convenient (doesn't need a wide format string)
void dbglog(const char *format, ...) {
	char buffer[10000];
	va_list args;
	va_start(args, format);
	HRESULT ok = StringCbVPrintfA(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (ok == S_OK || ok == STRSAFE_E_INSUFFICIENT_BUFFER) {
		// DebugView
		OutputDebugStringA(buffer);
		// console (if compiled without -mwindows)
		printf("%s", buffer);
	}
}


// I think this variant is needed to print wide strings?
void wdbglog(const TCHAR *format, ...) {
	TCHAR buffer[10000];
	va_list args;
	va_start(args, format);
	HRESULT ok = StringCbVPrintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (ok == S_OK || ok == STRSAFE_E_INSUFFICIENT_BUFFER) {
		// DebugView
		OutputDebugString(buffer);
		// console (if compiled without -mwindows)
		wprintf(L"%ls", buffer);
	}
}
