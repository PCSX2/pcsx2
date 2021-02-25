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

#ifdef FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_0_ARG FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_1_ARG FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_2_ARG FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_3_ARG FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_4_ARG FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_5_ARG FUNCTION_SHIM_ANY_ARG
#define FUNCTION_SHIM_6_ARG FUNCTION_SHIM_ANY_ARG
#endif

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
//FUNCTION_SHIM_1_ARG(int, pcap_bufsize, pcap_t*)
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

#undef FUNCTION_SHIM_0_ARG
#undef FUNCTION_SHIM_1_ARG
#undef FUNCTION_SHIM_2_ARG
#undef FUNCTION_SHIM_3_ARG
#undef FUNCTION_SHIM_4_ARG
#undef FUNCTION_SHIM_5_ARG
#undef FUNCTION_SHIM_6_ARG

#ifdef FUNCTION_SHIM_ANY_ARG
#undef FUNCTION_SHIM_ANY_ARG
#endif
