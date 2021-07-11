/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "DEV9/pcap_io.h"

HMODULE hpcap = nullptr;

#define FUNCTION_SHIM_HEAD_NO_ARGS(retType, name) \
	typedef retType (*fp_##name##_t)();           \
	fp_##name##_t fp_##name;                      \
	retType name()

#define FUNCTION_SHIM_BODY_NO_ARGS(retType, name) \
	{                                             \
		return fp_##name();                       \
	}

#define FUNCTION_SHIM_HEAD_ARGS(retType, name, ...) \
	typedef retType (*fp_##name##_t)(__VA_ARGS__);  \
	fp_##name##_t fp_##name;                        \
	retType name(__VA_ARGS__)

#define FUNCTION_SHIM_BODY_ARGS(retType, name, ...) \
	{                                               \
		return fp_##name(__VA_ARGS__);              \
	}


#define FUNCTION_SHIM_0_ARG(retType, name)    \
	FUNCTION_SHIM_HEAD_NO_ARGS(retType, name) \
	FUNCTION_SHIM_BODY_NO_ARGS(retType, name)

#define FUNCTION_SHIM_1_ARG(retType, name, type1)    \
	FUNCTION_SHIM_HEAD_ARGS(retType, name, type1 a1) \
	FUNCTION_SHIM_BODY_ARGS(retType, name, a1)

#define FUNCTION_SHIM_2_ARG(retType, name, type1, type2)       \
	FUNCTION_SHIM_HEAD_ARGS(retType, name, type1 a1, type2 a2) \
	FUNCTION_SHIM_BODY_ARGS(retType, name, a1, a2)

#define FUNCTION_SHIM_3_ARG(retType, name, type1, type2, type3)          \
	FUNCTION_SHIM_HEAD_ARGS(retType, name, type1 a1, type2 a2, type3 a3) \
	FUNCTION_SHIM_BODY_ARGS(retType, name, a1, a2, a3)

#define FUNCTION_SHIM_4_ARG(retType, name, type1, type2, type3, type4)             \
	FUNCTION_SHIM_HEAD_ARGS(retType, name, type1 a1, type2 a2, type3 a3, type4 a4) \
	FUNCTION_SHIM_BODY_ARGS(retType, name, a1, a2, a3, a4)

#define FUNCTION_SHIM_5_ARG(retType, name, type1, type2, type3, type4, type5)                \
	FUNCTION_SHIM_HEAD_ARGS(retType, name, type1 a1, type2 a2, type3 a3, type4 a4, type5 a5) \
	FUNCTION_SHIM_BODY_ARGS(retType, name, a1, a2, a3, a4, a5)

#define FUNCTION_SHIM_6_ARG(retType, name, type1, type2, type3, type4, type5, type6)                   \
	FUNCTION_SHIM_HEAD_ARGS(retType, name, type1 a1, type2 a2, type3 a3, type4 a4, type5 a5, type6 a6) \
	FUNCTION_SHIM_BODY_ARGS(retType, name, a1, a2, a3, a4, a5, a6)

#include "pcap_io_win32_funcs.h"

//
bool load_pcap()
{
	if (hpcap != nullptr)
		return true;

	//Store old Search Dir
	wchar_t* oldDllDir;
	int len = GetDllDirectory(0, nullptr);
	if (len == 0)
		return false;

	oldDllDir = new wchar_t[len];

	if (len == 1)
		oldDllDir[0] = 0;
	else
	{
		len = GetDllDirectory(len, oldDllDir);
		if (len == 0)
		{
			delete[] oldDllDir;
			return false;
		}
	}

	//Set DllDirectory, to allow us to load Npcap
	SetDllDirectory(L"C:\\Windows\\System32\\Npcap");

	//Try to load npcap or pcap
	hpcap = LoadLibrary(L"wpcap.dll");

	//Reset DllDirectory
	SetDllDirectory(oldDllDir);
	delete[] oldDllDir;

	//Did we succeed?
	if (hpcap == nullptr)
		return false;

#define LOAD_FUNCTION(name)                                  \
	fp_##name = (fp_##name##_t)GetProcAddress(hpcap, #name); \
	if (fp_##name == nullptr)                                \
	{                                                        \
		FreeLibrary(hpcap);                                  \
		Console.Error("DEV9: %s not found", #name);          \
		hpcap = nullptr;                                     \
		return false;                                        \
	}

		//Load all the functions we need
#define FUNCTION_SHIM_ANY_ARG(retType, name, ...) LOAD_FUNCTION(name)

#include "pcap_io_win32_funcs.h"

	return true;
}

void unload_pcap()
{
	FreeLibrary(hpcap);
	hpcap = nullptr;
}
