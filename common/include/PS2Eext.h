/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PS2EEXT_H_INCLUDED
#define PS2EEXT_H_INCLUDED

#include <stdio.h>
#include <string>
#include <cstdarg>

#include <mutex>
#include <condition_variable>

#if defined(_MSC_VER)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#define EXPORT_C_(type) extern "C" type CALLBACK

#elif defined(__unix__)

#include <cstring>
#include <wx/msgdlg.h>
#include <wx/app.h>

#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type

#else

#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type

#endif

#include <PluginCompatibility.h>
//#include "PS2Edefs.h"

#if !defined(_MSC_VER) || !defined(UNICODE)
static void SysMessage(const char* fmt, ...);
static void __forceinline PluginNullConfigure(std::string desc, s32& log);
#else
static void SysMessage(const wchar_t* fmt, ...);
static void __forceinline PluginNullConfigure(std::wstring desc, s32& log);
#endif

enum FileMode
{
	READ_FILE = 0,
	WRITE_FILE
};

struct PluginLog
{
	bool WriteToFile, WriteToConsole;
	FILE* LogFile;

	bool Open(std::string logname)
	{
		LogFile = px_fopen(logname, "w");

		if (LogFile)
		{
			setvbuf(LogFile, NULL, _IONBF, 0);
			return true;
		}
		return false;
	}

	void Close()
	{
		if (LogFile)
		{
			fclose(LogFile);
			LogFile = NULL;
		}
	}

	void Write(const char* fmt, ...)
	{
		if (LogFile == NULL)
			return;

		va_list list;
		if (WriteToFile)
		{
			va_start(list, fmt);
			vfprintf(LogFile, fmt, list);
			va_end(list);
		}
		if (WriteToConsole)
		{
			va_start(list, fmt);
			vfprintf(stdout, fmt, list);
			va_end(list);
		}
	}

	void WriteLn(const char* fmt, ...)
	{
		if (LogFile == NULL)
			return;

		va_list list;
		if (WriteToFile)
		{
			va_start(list, fmt);
			vfprintf(LogFile, fmt, list);
			va_end(list);
			fprintf(LogFile, "\n");
		}
		if (WriteToConsole)
		{
			va_start(list, fmt);
			vfprintf(stdout, fmt, list);
			va_end(list);
			fprintf(stdout, "\n");
		}
	}

#if !defined(_MSC_VER) || !defined(UNICODE)
	void Message(const char* fmt, ...)
	{
		va_list list;
		char buf[256];

		if (LogFile == NULL)
			return;

		va_start(list, fmt);
		vsprintf(buf, fmt, list);
		va_end(list);

		SysMessage(buf);
	}
#else
	void Message(const wchar_t* fmt, ...)
	{
		va_list list;
		wchar_t buf[256];

		if (LogFile == NULL)
			return;

		va_start(list, fmt);
		vswprintf(buf, 256, fmt, list);
		va_end(list);

		SysMessage(buf);
	}
#endif
};

struct PluginConf
{
	FILE* ConfFile;
	char* PluginName;

	bool Open(std::string name, FileMode mode = READ_FILE)
	{
		if (mode == READ_FILE)
		{
			ConfFile = px_fopen(name, "r");
		}
		else
		{
			ConfFile = px_fopen(name, "w");
		}

		if (ConfFile == NULL)
			return false;

		return true;
	}

	void Close()
	{
		if (ConfFile)
		{
			fclose(ConfFile);
			ConfFile = NULL;
		}
	}

	int ReadInt(const std::string& item, int defval)
	{
		int value = defval;
		std::string buf = item + " = %d\n";

		if (ConfFile)
			if (fscanf(ConfFile, buf.c_str(), &value) < 0)
				fprintf(stderr, "Error reading %s\n", item.c_str());

		return value;
	}

	void WriteInt(std::string item, int value)
	{
		std::string buf = item + " = %d\n";

		if (ConfFile)
			fprintf(ConfFile, buf.c_str(), value);
	}
};

#if defined(__unix__)
static void SysMessage(const char* fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = 0;

	if (!wxIsMainThread())
	{
		std::mutex dialogMutex;
		std::condition_variable dialogCV;
		bool dialogClosed = false;

		wxTheApp->CallAfter([&] {
			wxMessageDialog dialog(nullptr, msg, "Info", wxOK);
			dialog.ShowModal();

			{
				std::lock_guard dialogLock1(dialogMutex);
				dialogClosed = true;
			}
			dialogCV.notify_all();
		});
		//Block until done
		std::unique_lock dialogLock2(dialogMutex);
		dialogCV.wait(dialogLock2, [&] { return dialogClosed; });
	}
	else
	{
		wxMessageDialog dialog(nullptr, msg, "Info", wxOK);
		dialog.ShowModal();
	}
}

#define ENTRY_POINT /* We don't need no stinkin' entry point! */


#elif defined(__WXMAC__) || defined(__APPLE__)

static void SysMessage(const char* fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = 0;

	// TODO OSX can we use WX MessageBox here or should Cocoa MessageBox used?
}

static void SysMessage(const wchar_t* fmt, ...)
{
	va_list list;
	wchar_t msg[512];

	va_start(list, fmt);
	//vsprintf(msg, fmt, list);
	va_end(list);

	// TODO OSX can we use WX MessageBox here or should Cocoa MessageBox used?
}

static void __forceinline PluginNullConfigure(std::string desc, int& log)
{
	SysMessage("This space is intentionally left blank.");
}

#define ENTRY_POINT /* We don't need no stinkin' entry point! */ // TODO OSX WTF is this anyway?


#else

#define usleep(x) Sleep(x / 1000)

#ifndef UNICODE

static void __forceinline SysMessage(const char* fmt, ...)
{
	va_list list;
	char tmp[512];
	va_start(list, fmt);
	vsprintf(tmp, fmt, list);
	va_end(list);
	MessageBox(GetActiveWindow(), tmp, "Message", MB_SETFOREGROUND | MB_OK);
}

static void __forceinline PluginNullConfigure(std::string desc, s32& log)
{
	/* To do: Write a dialog box that displays a dialog box with the text in desc,
	   and a check box that says "Logging", checked if log !=0, and set log to
	   1 if it is checked on return, and 0 if it isn't. */
	SysMessage("This space is intentionally left blank.");
}

#else

static void __forceinline SysMessage(const wchar_t* fmt, ...)
{
	va_list list;
	wchar_t tmp[512];
	va_start(list, fmt);
	vswprintf(tmp, 512, fmt, list);
	va_end(list);
	MessageBox(GetActiveWindow(), tmp, L"Message", MB_SETFOREGROUND | MB_OK);
}

static void __forceinline PluginNullConfigure(std::string desc, s32& log)
{
	/* To do: Write a dialog box that displays a dialog box with the text in desc,
	and a check box that says "Logging", checked if log !=0, and set log to
	1 if it is checked on return, and 0 if it isn't. */
	SysMessage(L"This space is intentionally left blank.");
}

#endif

#define ENTRY_POINT                                     \
	HINSTANCE hInst;                                    \
                                                        \
	BOOL APIENTRY DllMain(HANDLE hModule, /* DLL INIT*/ \
						  DWORD dwReason,               \
						  LPVOID lpReserved)            \
	{                                                   \
		hInst = (HINSTANCE)hModule;                     \
		return TRUE; /* very quick :)*/                 \
	}

#endif
#endif // PS2EEXT_H_INCLUDED
