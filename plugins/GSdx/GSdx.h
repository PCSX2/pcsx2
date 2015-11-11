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

#include "GSSetting.h"

class GSdxApp
{
	std::string m_ini;
	std::string m_section;
#ifdef __linux__
	std::map< std::string, std::string > m_configuration_map;
#endif

public:
	GSdxApp();

    void* GetModuleHandlePtr();

#ifdef _WINDOWS
 	HMODULE GetModuleHandle() {return (HMODULE)GetModuleHandlePtr();}
#endif

#ifdef __linux__
	void BuildConfigurationMap(const char* lpFileName);
	void ReloadConfig();

	size_t GetPrivateProfileString(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, size_t nSize, const char* lpFileName);
	bool WritePrivateProfileString(const char* lpAppName, const char* lpKeyName, const char* pString, const char* lpFileName);
	int GetPrivateProfileInt(const char* lpAppName, const char* lpKeyName, int nDefault, const char* lpFileName);
#endif

	bool LoadResource(int id, vector<unsigned char>& buff, const char* type = NULL);

	template<typename T> T GetConfig(const char* entry, T value);
	string GetConfig(const char* entry, const char* value);
	template<typename T> void SetConfig(const char* entry, T value);
	void SetConfigDir(const char* dir);

	vector<GSSetting> m_gs_renderers;
	vector<GSSetting> m_gs_interlace;
	vector<GSSetting> m_gs_aspectratio;
	vector<GSSetting> m_gs_upscale_multiplier;
	vector<GSSetting> m_gs_max_anisotropy;
	vector<GSSetting> m_gs_filter;
	vector<GSSetting> m_gs_gl_ext;
	vector<GSSetting> m_gs_hack;
	vector<GSSetting> m_gs_crc_level;
	vector<GSSetting> m_gs_acc_blend_level;
	vector<GSSetting> m_gs_tv_shaders;

	vector<GSSetting> m_gpu_renderers;
	vector<GSSetting> m_gpu_filter;
	vector<GSSetting> m_gpu_dithering;
	vector<GSSetting> m_gpu_aspectratio;
	vector<GSSetting> m_gpu_scale;
};

// Generic methods need to move to the header to allow definition on demand
template< typename T >
T GSdxApp::GetConfig(const char* entry, T value)
{
	return static_cast<T>(GetConfig(entry, static_cast<int>(value)));
}

template<typename T>
void GSdxApp::SetConfig(const char* entry, T value)
{
	SetConfig(entry, static_cast<int>(value));
}

template<> string GSdxApp::GetConfig<string>(const char* entry, string value);
template<> int GSdxApp::GetConfig<int>(const char* entry, int value);
template<> bool GSdxApp::GetConfig<bool>(const char* entry, bool value);

template<> void GSdxApp::SetConfig<const char*>(const char* entry, const char* value);
template<> void GSdxApp::SetConfig<char*>(const char* entry, char* value);
template<> void GSdxApp::SetConfig<int>(const char* entry, int value);
template<> void GSdxApp::SetConfig<bool>(const char* entry, bool value);

struct GSDXError {};
struct GSDXRecoverableError : GSDXError {};

extern GSdxApp theApp;
