#pragma once

// Annoying defines
// ---------------------------------------------------------------------
// make sure __POSIX__ is defined for all systems where we assume POSIX
// compliance
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__) || defined(__CYGWIN__) || defined(__LINUX__)
#if !defined(__POSIX__)
#define __POSIX__ 1
#endif
#endif

#ifdef _WIN32
#	define CALLBACK			__stdcall
#else
#	define CALLBACK			__attribute__((stdcall))
#endif

#ifndef EXPORT_C_
#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" type CALLBACK
#else
#define EXPORT_C_(type) extern "C" __attribute__((stdcall,externally_visible,visibility("default"))) type
//#define EXPORT_C_(type) extern "C" __attribute__((stdcall,visibility("default"))) type
#endif
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define wfopen _wfopen
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
#define TSTDSTRING std::wstring
#define TSTDSTRINGSTREAM std::wstringstream
#define TSTDTOSTRING std::to_wstring

#ifdef _MSC_VER
typedef SSIZE_T ssize_t;
#endif

//FIXME narrow string fmt
#define SFMTs "S"

#define __builtin_constant_p(p) false

void SysMessageW(const wchar_t *fmt, ...);
#define SysMessage SysMessageW

#ifndef _T
#define _T(x) L##x
#endif

#else //_WIN32

#define MAX_PATH PATH_MAX
#define __inline inline

//#ifndef TEXT
//#define TEXT(x) L##x
//#endif
//FIXME narrow string fmt
#define SFMTs "s"
#define TEXT(val) val
#define TCHAR char
#define wfopen fopen
#define TSTDSTRING std::string
#define TSTDSTRINGSTREAM std::stringstream
#define TSTDTOSTRING std::to_string

void SysMessage(const char *fmt, ...);

#endif //_WIN32

#if __MINGW32__

#define DBL_EPSILON 2.2204460492503131e-16
#define FLT_EPSILON 1.1920928955078125e-7f

template <size_t size>
errno_t mbstowcs_s(
	size_t *pReturnValue,
	wchar_t (&wcstr)[size],
	const char *mbstr,
	size_t count
)
{
	return mbstowcs_s(pReturnValue, wcstr, size, mbstr, count);
}

template <size_t size>
errno_t wcstombs_s(
	size_t *pReturnValue,
	char (&mbstr)[size],
	const wchar_t *wcstr,
	size_t count
)
{
	return wcstombs_s(pReturnValue, mbstr, size, wcstr, count);
}

#endif //__MINGW32__

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) ((sizeof(x) / sizeof((x)[0])))
#endif

#include <cstddef>

template <class T, std::size_t N>
constexpr std::size_t countof(const T (&)[N]) noexcept
{
    return N;
}

template <class T>
constexpr std::size_t countof(const T N)
{
    return N.size();
}

//TODO Idk, used only in desc.h and struct USBDescriptor should be already packed anyway
#if defined(_WIN32) && !defined(__MINGW32__)
#define PACK(def,name) __pragma( pack(push, 1) ) def name __pragma( pack(pop) )
#elif defined(__clang__)
#define PACK(def,name) def __attribute__((packed)) name
#else
#define PACK(def,name) def __attribute__((gcc_struct, packed)) name
#endif
