// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if defined(_WIN32) && defined(_MSC_VER)

#include "pcsx2/VMManager.h"

#include "common/RedtapeWindows.h"

#pragma optimize("", off)

// The problem with AVX2 builds on Windows, is that MSVC generates AVX instructions for zeroing memory,
// which is pretty common in our global object constructors. So, we have to use a special object which
// gets initialized before all other global objects, that does the hardware check, and terminates the
// process before main() or any of the other objects are constructed (which would subsequently crash).
struct EarlyHardwareCheckObject
{
	EarlyHardwareCheckObject()
	{
		const char* error;
		if (VMManager::PerformEarlyHardwareChecks(&error))
			return;

		// we can't use StringUtil::UTF8StringToWideString because *that* constructor uses AVX..
		const int error_len = static_cast<int>(std::strlen(error));
		int wlen = MultiByteToWideChar(CP_UTF8, 0, error, error_len, nullptr, 0);
		if (wlen > 0)
		{
			wchar_t* werror = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * (error_len + 1)));
			if (werror && (wlen = MultiByteToWideChar(CP_UTF8, 0, error, error_len, werror, wlen)) > 0)
			{
				werror[wlen] = 0;
				MessageBoxW(NULL, werror, L"Hardware Check Failed", MB_ICONERROR);
				HeapFree(GetProcessHeap(), 0, werror);
			}
		}

		TerminateProcess(GetCurrentProcess(), 0xFFFFFFFF);
	}
};
#pragma warning(disable : 4075) // warning C4075: initializers put in unrecognized initialization area
#pragma init_seg(".CRT$XCT")
EarlyHardwareCheckObject s_hardware_checker;

#pragma optimize("", on)

#endif
