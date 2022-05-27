/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#if defined(_WIN32) && defined(_MSC_VER)

#include "pcsx2/VMManager.h"

#include "common/RedtapeWindows.h"

// The problem with AVX2 builds on Windows, is that MSVC generates AVX instructions for zeroing memory,
// which is pretty common in our global object constructors. So, we have to use a special object which
// gets initialized before all other global objects, that does the hardware check, and terminates the
// process before main() or any of the other objects are constructed (which would subsequently crash).
struct EarlyHardwareCheckObject
{
#pragma optimize("", off)
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
#pragma optimize("", on)
};
#pragma warning(disable : 4075) // warning C4075: initializers put in unrecognized initialization area
#pragma init_seg(".CRT$XCT")
EarlyHardwareCheckObject s_hardware_checker;

#endif
