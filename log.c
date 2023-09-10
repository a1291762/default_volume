#include <windows.h>
#include <strsafe.h>

void dbglog(const char *format, ...) {
	char buffer[10000];
	va_list args;
	va_start(args, format);
	StringCbVPrintfA(buffer, sizeof(buffer), format, args);
	va_end(args);
	// DebugView
	OutputDebugStringA(buffer);
	// console (if compiled without -mwindows)
	printf("%s", buffer);
}

void wdbglog(const TCHAR *format, ...) {
	TCHAR buffer[10000];
	va_list args;
	va_start(args, format);
	StringCbVPrintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	// DebugView
	OutputDebugString(buffer);
	// console (if compiled without -mwindows)
	wprintf(L"%ls", buffer);
}
