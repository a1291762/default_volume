#include <windows.h>
#include <strsafe.h>

void dbglog(const char *format, ...) {
	va_list args;
	va_start(args, format);
	char buffer[10000];
	StringCbVPrintfA(buffer, sizeof(buffer), format, args);
	OutputDebugStringA(buffer);
	va_end(args);
}

void wdbglog(const TCHAR *format, ...) {
	va_list args;
	va_start(args, format);
	TCHAR buffer[10000];
	StringCbVPrintf(buffer, sizeof(buffer), format, args);
	OutputDebugString(buffer);
	va_end(args);
}
