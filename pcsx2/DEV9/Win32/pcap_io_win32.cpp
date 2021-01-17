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

#include "../pcap_io.h"

HMODULE hpcap = nullptr;

#define LOAD_FUNCTION(name)                                  \
	fp_##name = (fp_##name##_t)GetProcAddress(hpcap, #name); \
	if (fp_##name == nullptr)                                \
	{                                                        \
		FreeLibrary(hpcap);                                  \
		Console.Error("%s not found", #name);                \
		hpcap = nullptr;                                     \
		return false;                                        \
	}

#define FUNCTION_SHIM_0_ARG(retType, name) \
	typedef retType (*fp_##name##_t)();    \
	fp_##name##_t fp_##name;               \
	retType name()                         \
	{                                      \
		return fp_##name();                \
	}
#define FUNCTION_SHIM_1_ARG(retType, name, type1) \
	typedef retType (*fp_##name##_t)(type1);      \
	fp_##name##_t fp_##name;                      \
	retType name(type1 a1)                        \
	{                                             \
		return fp_##name(a1);                     \
	}
#define FUNCTION_SHIM_2_ARG(retType, name, type1, type2) \
	typedef retType (*fp_##name##_t)(type1, type2);      \
	fp_##name##_t fp_##name;                             \
	retType name(type1 a1, type2 a2)                     \
	{                                                    \
		return fp_##name(a1, a2);                        \
	}
#define FUNCTION_SHIM_3_ARG(retType, name, type1, type2, type3) \
	typedef retType (*fp_##name##_t)(type1, type2, type3);      \
	fp_##name##_t fp_##name;                                    \
	retType name(type1 a1, type2 a2, type3 a3)                  \
	{                                                           \
		return fp_##name(a1, a2, a3);                           \
	}
#define FUNCTION_SHIM_4_ARG(retType, name, type1, type2, type3, type4) \
	typedef retType (*fp_##name##_t)(type1, type2, type3, type4);      \
	fp_##name##_t fp_##name;                                           \
	retType name(type1 a1, type2 a2, type3 a3, type4 a4)               \
	{                                                                  \
		return fp_##name(a1, a2, a3, a4);                              \
	}
#define FUNCTION_SHIM_5_ARG(retType, name, type1, type2, type3, type4, type5) \
	typedef retType (*fp_##name##_t)(type1, type2, type3, type4, type5);      \
	fp_##name##_t fp_##name;                                                  \
	retType name(type1 a1, type2 a2, type3 a3, type4 a4, type5 a5)            \
	{                                                                         \
		return fp_##name(a1, a2, a3, a4, a5);                                 \
	}
#define FUNCTION_SHIM_6_ARG(retType, name, type1, type2, type3, type4, type5, type6) \
	typedef retType (*fp_##name##_t)(type1, type2, type3, type4, type5, type6);      \
	fp_##name##_t fp_##name;                                                         \
	retType name(type1 a1, type2 a2, type3 a3, type4 a4, type5 a5, type6 a6)         \
	{                                                                                \
		return fp_##name(a1, a2, a3, a4, a5, a6);                                    \
	}

FUNCTION_SHIM_5_ARG(pcap_t*, pcap_open_live, const char*, int, int, int, char*)
FUNCTION_SHIM_2_ARG(pcap_t*, pcap_open_dead, int, int)
//FUNCTION_SHIM_3_ARG(pcap_t*, pcap_open_dead_with_tstamp_precision, int, int, u_int)
//FUNCTION_SHIM_3_ARG(pcap_t*, pcap_open_offline_with_tstamp_precision, const char*, u_int, char*)
FUNCTION_SHIM_2_ARG(pcap_t*, pcap_open_offline, const char*, char*)
#ifdef _WIN32
//FUNCTION_SHIM_3_ARG(pcap_t*, pcap_hopen_offline_with_tstamp_precision, intptr_t, u_int, char*)
FUNCTION_SHIM_2_ARG(pcap_t*, pcap_hopen_offline, intptr_t, char*)
#else
pcap_t* pcap_fopen_offline_with_tstamp_precision(FILE*, u_int, char*);
pcap_t* pcap_fopen_offline(FILE*, char*);
#endif
//
FUNCTION_SHIM_1_ARG(void, pcap_close, pcap_t*)
FUNCTION_SHIM_4_ARG(int, pcap_loop, pcap_t*, int, pcap_handler, u_char*)
FUNCTION_SHIM_4_ARG(int, pcap_dispatch, pcap_t*, int, pcap_handler, u_char*)
FUNCTION_SHIM_2_ARG(const u_char*, pcap_next, pcap_t*, struct pcap_pkthdr*)
FUNCTION_SHIM_3_ARG(int, pcap_next_ex, pcap_t*, struct pcap_pkthdr**, const u_char**)
FUNCTION_SHIM_1_ARG(void, pcap_breakloop, pcap_t*)
FUNCTION_SHIM_2_ARG(int, pcap_stats, pcap_t*, struct pcap_stat*)
FUNCTION_SHIM_2_ARG(int, pcap_setfilter, pcap_t*, struct bpf_program*)
FUNCTION_SHIM_2_ARG(int, pcap_setdirection, pcap_t*, pcap_direction_t)
FUNCTION_SHIM_2_ARG(int, pcap_getnonblock, pcap_t*, char*)
FUNCTION_SHIM_3_ARG(int, pcap_setnonblock, pcap_t*, int, char*)
//FUNCTION_SHIM_3_ARG(int, pcap_inject, pcap_t*, const void*, size_t)
FUNCTION_SHIM_3_ARG(int, pcap_sendpacket, pcap_t*, const u_char*, int)
//FUNCTION_SHIM_1_ARG(const char*, pcap_statustostr, int)
FUNCTION_SHIM_1_ARG(const char*, pcap_strerror, int)
FUNCTION_SHIM_1_ARG(char*, pcap_geterr, pcap_t*)
FUNCTION_SHIM_2_ARG(void, pcap_perror, pcap_t*, const char*)
FUNCTION_SHIM_5_ARG(int, pcap_compile, pcap_t*, struct bpf_program*, const char*, int, bpf_u_int32)
FUNCTION_SHIM_6_ARG(int, pcap_compile_nopcap, int, int, struct bpf_program*, const char*, int, bpf_u_int32)
FUNCTION_SHIM_1_ARG(void, pcap_freecode, struct bpf_program*)
FUNCTION_SHIM_3_ARG(int, pcap_offline_filter, const struct bpf_program*, const struct pcap_pkthdr*, const u_char*)
FUNCTION_SHIM_1_ARG(int, pcap_datalink, pcap_t*)
//FUNCTION_SHIM_1_ARG(int, pcap_datalink_ext, pcap_t*)
FUNCTION_SHIM_2_ARG(int, pcap_list_datalinks, pcap_t*, int**)
FUNCTION_SHIM_2_ARG(int, pcap_set_datalink, pcap_t*, int)
FUNCTION_SHIM_1_ARG(void, pcap_free_datalinks, int*)
FUNCTION_SHIM_1_ARG(int, pcap_datalink_name_to_val, const char*)
FUNCTION_SHIM_1_ARG(const char*, pcap_datalink_val_to_name, int)
FUNCTION_SHIM_1_ARG(const char*, pcap_datalink_val_to_description, int)
//FUNCTION_SHIM_1_ARG(const char*, pcap_datalink_val_to_description_or_dlt, int)
FUNCTION_SHIM_1_ARG(int, pcap_snapshot, pcap_t*)
FUNCTION_SHIM_1_ARG(int, pcap_is_swapped, pcap_t*)
FUNCTION_SHIM_1_ARG(int, pcap_major_version, pcap_t*)
FUNCTION_SHIM_1_ARG(int, pcap_minor_version, pcap_t*)
FUNCTION_SHIM_1_ARG(int, pcap_bufsize, pcap_t*)
//
FUNCTION_SHIM_1_ARG(FILE*, pcap_file, pcap_t*)
FUNCTION_SHIM_1_ARG(int, pcap_fileno, pcap_t*)
//
#ifdef _WIN32
//FUNCTION_SHIM_0_ARG(int, pcap_wsockinit)
#endif
//
FUNCTION_SHIM_2_ARG(pcap_dumper_t*, pcap_dump_open, pcap_t*, const char*)
#ifdef _WIN32
//FUNCTION_SHIM_2_ARG(pcap_dumper_t*, pcap_dump_hopen, pcap_t*, intptr_t)
#else
//pcap_dumper_t* pcap_dump_fopen(pcap_t*, FILE* fp);
#endif
//FUNCTION_SHIM_2_ARG(pcap_dumper_t*, pcap_dump_open_append, pcap_t*, const char*)
FUNCTION_SHIM_1_ARG(FILE*, pcap_dump_file, pcap_dumper_t*)
FUNCTION_SHIM_1_ARG(long, pcap_dump_ftell, pcap_dumper_t*)
//FUNCTION_SHIM_1_ARG(int64_t, pcap_dump_ftell64, pcap_dumper_t*)
FUNCTION_SHIM_1_ARG(int, pcap_dump_flush, pcap_dumper_t*)
FUNCTION_SHIM_1_ARG(void, pcap_dump_close, pcap_dumper_t*)
FUNCTION_SHIM_3_ARG(void, pcap_dump, u_char*, const struct pcap_pkthdr*, const u_char*)
//
FUNCTION_SHIM_2_ARG(int, pcap_findalldevs, pcap_if_t**, char*)
FUNCTION_SHIM_1_ARG(void, pcap_freealldevs, pcap_if_t*)
//
FUNCTION_SHIM_0_ARG(const char*, pcap_lib_version)

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
			delete oldDllDir;
			return false;
		}
	}

	//Set DllDirectory, to allow us to load Npcap
	SetDllDirectory(L"C:\\Windows\\System32\\Npcap");

	//Try to load npcap or pcap
	hpcap = LoadLibrary(L"wpcap.dll");

	//Reset DllDirectory
	SetDllDirectory(oldDllDir);
	delete oldDllDir;

	//Did we succeed?
	if (hpcap == nullptr)
		return false;

	//Load all the functions we need
	//pcap.h
	LOAD_FUNCTION(pcap_open_live);
	LOAD_FUNCTION(pcap_open_dead);
	//LOAD_FUNCTION(pcap_open_dead_with_tstamp_precision);
	//LOAD_FUNCTION(pcap_open_offline_with_tstamp_precision);
	LOAD_FUNCTION(pcap_open_offline);
#ifdef _WIN32
	//LOAD_FUNCTION(pcap_hopen_offline_with_tstamp_precision);
	LOAD_FUNCTION(pcap_hopen_offline);
#else
	//pcap_fopen_offline_with_tstamp_precision
	//pcap_fopen_offline
#endif
	//
	LOAD_FUNCTION(pcap_close);
	LOAD_FUNCTION(pcap_loop);
	LOAD_FUNCTION(pcap_dispatch);
	LOAD_FUNCTION(pcap_next);
	LOAD_FUNCTION(pcap_next_ex);
	LOAD_FUNCTION(pcap_breakloop);
	LOAD_FUNCTION(pcap_stats);
	LOAD_FUNCTION(pcap_setfilter);
	LOAD_FUNCTION(pcap_setdirection);
	LOAD_FUNCTION(pcap_getnonblock);
	LOAD_FUNCTION(pcap_setnonblock);
	//pcap_inject is in winPcap's headers, but not exported
	//LOAD_FUNCTION(pcap_inject);
	LOAD_FUNCTION(pcap_sendpacket);
	//pcap_statustostr is in winPcap's headers, but not exported
	//LOAD_FUNCTION(pcap_statustostr);
	LOAD_FUNCTION(pcap_strerror);
	LOAD_FUNCTION(pcap_geterr);
	LOAD_FUNCTION(pcap_perror);
	LOAD_FUNCTION(pcap_compile);
	LOAD_FUNCTION(pcap_compile_nopcap);
	LOAD_FUNCTION(pcap_freecode);
	LOAD_FUNCTION(pcap_offline_filter);
	LOAD_FUNCTION(pcap_datalink);
	//pcap_datalink_ext is in winPcap's headers, but not exported
	//LOAD_FUNCTION(pcap_datalink_ext);
	LOAD_FUNCTION(pcap_list_datalinks);
	LOAD_FUNCTION(pcap_set_datalink);
	LOAD_FUNCTION(pcap_free_datalinks);
	LOAD_FUNCTION(pcap_datalink_name_to_val);
	LOAD_FUNCTION(pcap_datalink_val_to_name);
	LOAD_FUNCTION(pcap_datalink_val_to_description);
	//LOAD_FUNCTION(pcap_datalink_val_to_description_or_dlt);
	LOAD_FUNCTION(pcap_snapshot);
	LOAD_FUNCTION(pcap_is_swapped);
	LOAD_FUNCTION(pcap_major_version);
	LOAD_FUNCTION(pcap_minor_version);
	//LOAD_FUNCTION(pcap_bufsize);
	//
	LOAD_FUNCTION(pcap_file);
	LOAD_FUNCTION(pcap_fileno);
	//
#ifdef _WIN32
	//LOAD_FUNCTION(pcap_wsockinit);
#endif

	LOAD_FUNCTION(pcap_dump_open);
#ifdef _WIN32
	//LOAD_FUNCTION(pcap_dump_hopen);
#else
	//ppcap_dump_fopen
#endif
	//LOAD_FUNCTION(pcap_dump_open_append);
	LOAD_FUNCTION(pcap_dump_file);
	LOAD_FUNCTION(pcap_dump_ftell);
	//LOAD_FUNCTION(pcap_dump_ftell64);
	LOAD_FUNCTION(pcap_dump_flush);
	LOAD_FUNCTION(pcap_dump_close);
	LOAD_FUNCTION(pcap_dump);
	//
	LOAD_FUNCTION(pcap_findalldevs);
	LOAD_FUNCTION(pcap_freealldevs);
	//
	LOAD_FUNCTION(pcap_lib_version);

	return true;
}

void unload_pcap()
{
	FreeLibrary(hpcap);
	hpcap = nullptr;
}
