/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

#include "common/WindowInfo.h"
#include "Window/GSSetting.h"
#include "SaveState.h"
#include "pcsx2/Config.h"
#include "pcsx2/GS/config.h"

#include <map>

#ifdef None
	// X11 seems to like to define this, not fun
	#undef None
#endif

// ST_WRITE is defined in libc, avoid this
enum stateType
{
	SAVE_WRITE,
	SAVE_TRANSFER,
	SAVE_VSYNC
};

enum class GSVideoMode : u8
{
	Unknown,
	NTSC,
	PAL,
	VESA,
	SDTV_480P,
	HDTV_720P,
	HDTV_1080I
};

extern Pcsx2Config::GSOptions GSConfig;

struct HostKeyEvent;
class HostDisplay;

int GSinit();
void GSinitConfig();
void GSshutdown();
bool GSopen(const Pcsx2Config::GSOptions& config, GSRendererType renderer, u8* basemem);
bool GSreopen(bool recreate_display);
void GSreset(bool hardware_reset);
void GSclose();
void GSgifSoftReset(u32 mask);
void GSwriteCSR(u32 csr);
void GSInitAndReadFIFO(u8* mem, u32 size);
void GSReadLocalMemoryUnsync(u8* mem, u32 qwc, u64 BITBLITBUF, u64 TRXPOS, u64 TRXREG);
void GSgifTransfer(const u8* mem, u32 size);
void GSgifTransfer1(u8* mem, u32 addr);
void GSgifTransfer2(u8* mem, u32 size);
void GSgifTransfer3(u8* mem, u32 size);
void GSvsync(u32 field, bool registers_written);
int GSfreeze(FreezeAction mode, freezeData* data);
void GSQueueSnapshot(const std::string& path, u32 gsdump_frames = 0);
void GSStopGSDump();
#ifndef PCSX2_CORE
void GSkeyEvent(const HostKeyEvent& e);
void GSconfigure();
int GStest();
bool GSsetupRecording(std::string& filename);
void GSendRecording();
#endif
void GSsetGameCRC(u32 crc, int options);
void GSsetFrameSkip(int frameskip);

GSVideoMode GSgetDisplayMode();
void GSgetInternalResolution(int* width, int* height);
void GSgetStats(std::string& info);
void GSgetTitleStats(std::string& info);

void GSUpdateConfig(const Pcsx2Config::GSOptions& new_config);
void GSSwitchRenderer(GSRendererType new_renderer);
void GSResetAPIState();
void GSRestoreAPIState();
bool GSSaveSnapshotToMemory(u32 width, u32 height, std::vector<u32>* pixels);

class GSApp
{
	std::string m_section;
	std::map<std::string, std::string> m_default_configuration;
	std::map<std::string, std::string> m_configuration_map;

public:
	std::string m_ini;
	GSApp();

	void Init();

#ifndef PCSX2_CORE
	void BuildConfigurationMap(const char* lpFileName);
	void ReloadConfig();
	int GetIniInt(const char* lpAppName, const char* lpKeyName, int nDefault, const char* lpFileName);
#endif

	size_t GetIniString(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, size_t nSize, const char* lpFileName);
	bool WriteIniString(const char* lpAppName, const char* lpKeyName, const char* pString, const char* lpFileName);

	void SetConfig(const char* entry, const char* value);
	void SetConfig(const char* entry, int value);
	// Avoid issue with overloading
	template <typename T>
	T GetConfigT(const char* entry)
	{
		return static_cast<T>(GetConfigI(entry));
	}
	int GetConfigI(const char* entry);
	bool GetConfigB(const char* entry);
	std::string GetConfigS(const char* entry);

	void SetConfigDir();

	std::vector<GSSetting> m_gs_renderers;
	std::vector<GSSetting> m_gs_deinterlace;
	std::vector<GSSetting> m_gs_upscale_multiplier;
	std::vector<GSSetting> m_gs_max_anisotropy;
	std::vector<GSSetting> m_gs_dithering;
	std::vector<GSSetting> m_gs_bifilter;
	std::vector<GSSetting> m_gs_trifilter;
	std::vector<GSSetting> m_gs_texture_preloading;
	std::vector<GSSetting> m_gs_hack;
	std::vector<GSSetting> m_gs_generic_list;
	std::vector<GSSetting> m_gs_offset_hack;
	std::vector<GSSetting> m_gs_hw_mipmapping;
	std::vector<GSSetting> m_gs_crc_level;
	std::vector<GSSetting> m_gs_acc_blend_level;
	std::vector<GSSetting> m_gs_tv_shaders;
	std::vector<GSSetting> m_gs_dump_compression;
};

struct GSError
{
};
struct GSRecoverableError : GSError
{
};
struct GSErrorGlVertexArrayTooSmall : GSError
{
};

extern GSApp theApp;
