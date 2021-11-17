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
#include "Host.h"

#include <map>

#ifdef None
	// X11 seems to like to define this, not fun
	#undef None
#endif

enum class GSRendererType : int8_t
{
	Undefined = -1,
	NO_RENDERER = 0,
	DX1011_HW = 3,
	Null = 11,
	OGL_HW = 12,
	OGL_SW = 13,

#ifdef _WIN32
	Default = Undefined
#else
	// Use ogl renderer as default otherwise it crash at startup
	// GSRenderOGL only GSDeviceOGL (not GSDeviceNULL)
	Default = OGL_HW
#endif
};

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

// Ordering was done to keep compatibility with older ini file.
enum class BiFiltering : u8
{
	Nearest,
	Forced,
	PS2,
	Forced_But_Sprite,
};

enum class TriFiltering : u8
{
	None,
	PS2,
	Forced,
};

enum class HWMipmapLevel : int
{
	Automatic = -1,
	Off,
	Basic,
	Full
};

enum class CRCHackLevel : s8
{
	Automatic = -1,
	None,
	Minimum,
	Partial,
	Full,
	Aggressive
};

void GSsetBaseMem(u8* mem);
int GSinit();
void GSshutdown();
void GSclose();
int _GSopen(const WindowInfo& wi, const char* title, GSRendererType renderer, int threads);
void GSosdLog(const char* utf8, u32 color);
void GSosdMonitor(const char* key, const char* value, u32 color);
int GSopen2(const WindowInfo & wi, u32 flags);
void GSreset();
void GSgifSoftReset(u32 mask);
void GSwriteCSR(u32 csr);
void GSinitReadFIFO(u8* mem);
void GSreadFIFO(u8* mem);
void GSinitReadFIFO2(u8* mem, u32 size);
void GSreadFIFO2(u8* mem, u32 size);
void GSgifTransfer(const u8* mem, u32 size);
void GSgifTransfer1(u8* mem, u32 addr);
void GSgifTransfer2(u8* mem, u32 size);
void GSgifTransfer3(u8* mem, u32 size);
void GSvsync(int field);
u32 GSmakeSnapshot(char* path);
void GSkeyEvent(const HostKeyEvent& e);
int GSfreeze(FreezeAction mode, freezeData* data);
void GSconfigure();
int GStest();
bool GSsetupRecording(std::string& filename);
void GSendRecording();
void GSsetGameCRC(u32 crc, int options);
void GSgetTitleInfo2(char* dest, size_t length);
void GSsetFrameSkip(int frameskip);
void GSsetVsync(int vsync);
void GSsetExclusive(int enabled);

#ifndef PCSX2_CORE
// Needed for window resizing in wx. Can be safely called from the UI thread.
void GSResizeWindow(int width, int height);
bool GSCheckForWindowResize(int* new_width, int* new_height);
#endif

class GSApp
{
	std::string m_section;
	std::map<std::string, std::string> m_default_configuration;
	std::map<std::string, std::string> m_configuration_map;
	GSRendererType m_current_renderer_type;

public:
	std::string m_ini;
	GSApp();

	void Init();
	void* GetModuleHandlePtr();

#ifdef _WIN32
	HMODULE GetModuleHandle()
	{
		return (HMODULE)GetModuleHandlePtr();
	}
#endif

	void BuildConfigurationMap(const char* lpFileName);
	void ReloadConfig();

	size_t GetIniString(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, size_t nSize, const char* lpFileName);
	bool WriteIniString(const char* lpAppName, const char* lpKeyName, const char* pString, const char* lpFileName);
	int GetIniInt(const char* lpAppName, const char* lpKeyName, int nDefault, const char* lpFileName);

#ifdef _WIN32
	bool LoadResource(int id, std::vector<char>& buff, const wchar_t* type = nullptr);
#else
	bool LoadResource(int id, std::vector<char>& buff, const char* type = nullptr);
#endif

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

	void SetCurrentRendererType(GSRendererType type);
	GSRendererType GetCurrentRendererType() const;

	void SetConfigDir();

	std::vector<GSSetting> m_gs_renderers;
	std::vector<GSSetting> m_gs_interlace;
	std::vector<GSSetting> m_gs_upscale_multiplier;
	std::vector<GSSetting> m_gs_max_anisotropy;
	std::vector<GSSetting> m_gs_dithering;
	std::vector<GSSetting> m_gs_bifilter;
	std::vector<GSSetting> m_gs_trifilter;
	std::vector<GSSetting> m_gs_hack;
	std::vector<GSSetting> m_gs_generic_list;
	std::vector<GSSetting> m_gs_offset_hack;
	std::vector<GSSetting> m_gs_hw_mipmapping;
	std::vector<GSSetting> m_gs_crc_level;
	std::vector<GSSetting> m_gs_acc_blend_level;
	std::vector<GSSetting> m_gs_acc_blend_level_d3d11;
	std::vector<GSSetting> m_gs_tv_shaders;
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

extern bool gsopen_done;
