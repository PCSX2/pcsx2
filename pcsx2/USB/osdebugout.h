#pragma once

#include <cstdio>
#include <iostream>
#include <sstream>

#define USB_LOG __Log
void __Log(const char* fmt, ...);

#ifdef _WIN32

#include <vector>
void _OSDebugOut(const TCHAR *psz_fmt, ...);
std::wostream& operator<<(std::wostream& os, const std::string& s);

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) _OSDebugOut(TEXT("[USBqemu] [%" SFMTs "]:%d\t") psz_fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OSDebugOut_noprfx(psz_fmt, ...) _OSDebugOut(TEXT(psz_fmt), ##__VA_ARGS__)
#define OSDebugOutStream_noprfx(psz_str) do{ TSTDSTRINGSTREAM ss; ss << psz_str; _OSDebugOut(_T("%s\n"), ss.str().c_str()); }while(0)
#else
#define OSDebugOut(psz_fmt, ...) do{}while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{}while(0)
#define OSDebugOutStream_noprfx(str) do{}while(0)
#endif

#else //_WIN32

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) do{ fprintf(stderr, "[USBqemu] [%s]:%d\t" psz_fmt, __func__, __LINE__, ##__VA_ARGS__); }while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{ fprintf(stderr, psz_fmt, ##__VA_ARGS__); }while(0)
#define OSDebugOutStream_noprfx(str) do{ std::cerr << str << std::endl; }while(0)
#else
#define OSDebugOut(psz_fmt, ...) do{}while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{}while(0)
#define OSDebugOutStream_noprfx(str) do{}while(0)
#endif

#endif //_WIN32
