
// log.c
extern void dbglog(const char *format, ...);
extern void wdbglog(const TCHAR *format, ...);

// default_volume.c
extern int monitor_registry(HKEY *g_property_store);

// trayicon.c
extern HWND create_tray_icon(HINSTANCE hInstance, HKEY *g_property_store);
