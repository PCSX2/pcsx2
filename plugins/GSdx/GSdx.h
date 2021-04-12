/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "Window/GSSetting.h"
#include "GS.h"

class GSdxApp
{
	std::string m_ini;
	std::string m_section;
	std::map<std::string, std::string> m_default_configuration;
	std::map<std::string, std::string> m_configuration_map;
	GSRendererType m_current_renderer_type;

public:
	GSdxApp();

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

	void SetConfigDir(const char* dir);

	std::vector<GSSetting> m_gs_renderers;
	std::vector<GSSetting> m_gs_interlace;
	std::vector<GSSetting> m_gs_aspectratio;
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

struct GSDXError
{
};
struct GSDXRecoverableError : GSDXError
{
};
struct GSDXErrorGlVertexArrayTooSmall : GSDXError
{
};

extern GSdxApp theApp;
