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

#include "stdafx.h"
#include "GSdx.h"
#include "GS.h"
#include <fstream>

static void* s_hModule;

#ifdef _WIN32

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		s_hModule = hModule;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

bool GSdxApp::LoadResource(int id, std::vector<char>& buff, const char* type)
{
	buff.clear();
	HRSRC hRsrc = FindResource((HMODULE)s_hModule, MAKEINTRESOURCE(id), type != NULL ? type : RT_RCDATA);
	if(!hRsrc) return false;
	HGLOBAL hGlobal = ::LoadResource((HMODULE)s_hModule, hRsrc);
	if(!hGlobal) return false;
	DWORD size = SizeofResource((HMODULE)s_hModule, hRsrc);
	if(!size) return false;
	// On Linux resources are always NULL terminated
	// Add + 1 on size to do the same for compatibility sake (required by GSDeviceOGL)
	buff.resize(size + 1);
	memcpy(buff.data(), LockResource(hGlobal), size);
	return true;
}

#else

#include "GSdxResources.h"

bool GSdxApp::LoadResource(int id, std::vector<char>& buff, const char* type)
{
	std::string path;
	switch (id) {
		case IDR_COMMON_GLSL:
			path = "/GSdx/res/glsl/common_header.glsl";
			break;
		case IDR_CONVERT_GLSL:
			path = "/GSdx/res/glsl/convert.glsl";
			break;
		case IDR_FXAA_FX:
			path = "/GSdx/res/fxaa.fx";
			break;
		case IDR_INTERLACE_GLSL:
			path = "/GSdx/res/glsl/interlace.glsl";
			break;
		case IDR_MERGE_GLSL:
			path = "/GSdx/res/glsl/merge.glsl";
			break;
		case IDR_SHADEBOOST_GLSL:
			path = "/GSdx/res/glsl/shadeboost.glsl";
			break;
		case IDR_TFX_VGS_GLSL:
			path = "/GSdx/res/glsl/tfx_vgs.glsl";
			break;
		case IDR_TFX_FS_GLSL:
			path = "/GSdx/res/glsl/tfx_fs.glsl";
			break;
		case IDR_FONT_ROBOTO:
			path = "/GSdx/res/fonts-roboto/Roboto-Regular.ttf";
			break;
		default:
			printf("LoadResource not implemented for id %d\n", id);
			return false;
	}

	GBytes *bytes = g_resource_lookup_data(GSdx_res_get_resource(), path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);

	size_t size = 0;
	const void* data = g_bytes_get_data(bytes, &size);

	if (data == nullptr || size == 0) {
		printf("Failed to get data for resource: %d\n", id);
		return false;
	}

	buff.clear();
	buff.resize(size + 1);
	memcpy(buff.data(), data, size + 1);

	g_bytes_unref(bytes);

	return true;
}
#endif

size_t GSdxApp::GetIniString(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, size_t nSize, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string key(lpKeyName);
	std::string value = m_configuration_map[key];
	if (value.empty()) {
		// save the value for futur call
		m_configuration_map[key] = std::string(lpDefault);
		strcpy(lpReturnedString, lpDefault);
	} else
		strcpy(lpReturnedString, value.c_str());

    return 0;
}

bool GSdxApp::WriteIniString(const char* lpAppName, const char* lpKeyName, const char* pString, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string key(lpKeyName);
	std::string value(pString);
	m_configuration_map[key] = value;

	// Save config to a file
	FILE* f = px_fopen(lpFileName, "w");

	if (f == NULL) return false; // FIXME print a nice message

	// Maintain compatibility with GSDumpGUI/old Windows ini.
#ifdef _WIN32
	fprintf(f, "[Settings]\n");
#endif

	for (const auto& entry : m_configuration_map) {
		// Do not save the inifile key which is not an option
		if (entry.first.compare("inifile") == 0) continue;

		// Only keep option that have a default value (allow to purge old option of the GSdx.ini)
		if (!entry.second.empty() && m_default_configuration.find(entry.first) != m_default_configuration.end())
			fprintf(f, "%s = %s\n", entry.first.c_str(), entry.second.c_str());
	}
	fclose(f);

	return false;
}

int GSdxApp::GetIniInt(const char* lpAppName, const char* lpKeyName, int nDefault, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string value = m_configuration_map[std::string(lpKeyName)];
	if (value.empty()) {
		// save the value for futur call
		SetConfig(lpKeyName, nDefault);
		return nDefault;
	} else
		return atoi(value.c_str());
}

GSdxApp theApp;

GSdxApp::GSdxApp()
{
	// Empty constructor causes an illegal instruction exception on an SSE4.2 machine on Windows.
	// Non-empty doesn't, but raises a SIGILL signal when compiled against GCC 6.1.1.
	// So here's a compromise.
#ifdef _WIN32
	Init();
#endif
}

void GSdxApp::Init()
{
	static bool is_initialised = false;
	if (is_initialised)
		return;
	is_initialised = true;

	m_current_renderer_type = GSRendererType::Undefined;

	if (m_ini.empty())
		m_ini = "inis/GSdx.ini";
	m_section = "Settings";

#ifdef _WIN32
	m_gs_renderers.push_back(GSSetting(static_cast<uint32>(GSRendererType::DX1011_HW), "Direct3D 11", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<uint32>(GSRendererType::OGL_HW), "OpenGL", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<uint32>(GSRendererType::OGL_SW), "Software", ""));
#else // Linux
	m_gs_renderers.push_back(GSSetting(static_cast<uint32>(GSRendererType::OGL_HW), "OpenGL", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<uint32>(GSRendererType::OGL_SW), "Software", ""));
#endif

	// The null renderer goes third, it has use for benchmarking purposes in a release build
	m_gs_renderers.push_back(GSSetting(static_cast<uint32>(GSRendererType::Null), "Null", ""));

	m_gs_interlace.push_back(GSSetting(0, "None", ""));
	m_gs_interlace.push_back(GSSetting(1, "Weave tff", "saw-tooth"));
	m_gs_interlace.push_back(GSSetting(2, "Weave bff", "saw-tooth"));
	m_gs_interlace.push_back(GSSetting(3, "Bob tff", "use blend if shaking"));
	m_gs_interlace.push_back(GSSetting(4, "Bob bff", "use blend if shaking"));
	m_gs_interlace.push_back(GSSetting(5, "Blend tff", "slight blur, 1/2 fps"));
	m_gs_interlace.push_back(GSSetting(6, "Blend bff", "slight blur, 1/2 fps"));
	m_gs_interlace.push_back(GSSetting(7, "Automatic", "Default"));

	m_gs_aspectratio.push_back(GSSetting(0, "Stretch", ""));
	m_gs_aspectratio.push_back(GSSetting(1, "4:3", ""));
	m_gs_aspectratio.push_back(GSSetting(2, "16:9", ""));

	m_gs_upscale_multiplier.push_back(GSSetting(1, "Native", "PS2"));
	m_gs_upscale_multiplier.push_back(GSSetting(2, "2x Native", "~720p"));
	m_gs_upscale_multiplier.push_back(GSSetting(3, "3x Native", "~1080p"));
	m_gs_upscale_multiplier.push_back(GSSetting(4, "4x Native", "~1440p 2K"));
	m_gs_upscale_multiplier.push_back(GSSetting(5, "5x Native", "~1620p 3K"));
	m_gs_upscale_multiplier.push_back(GSSetting(6, "6x Native", "~2160p 4K"));
	m_gs_upscale_multiplier.push_back(GSSetting(8, "8x Native", "~2880p 5K"));

	m_gs_max_anisotropy.push_back(GSSetting(0, "Off", "Default"));
	m_gs_max_anisotropy.push_back(GSSetting(2, "2x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(4, "4x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(8, "8x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(16, "16x", ""));

	m_gs_dithering.push_back(GSSetting(0, "Off", ""));
	m_gs_dithering.push_back(GSSetting(1, "Scaled", ""));
	m_gs_dithering.push_back(GSSetting(2, "Unscaled", "Default"));

	m_gs_bifilter.push_back(GSSetting(static_cast<uint32>(BiFiltering::Nearest), "Nearest", ""));
	m_gs_bifilter.push_back(GSSetting(static_cast<uint32>(BiFiltering::Forced_But_Sprite), "Bilinear", "Forced excluding sprite"));
	m_gs_bifilter.push_back(GSSetting(static_cast<uint32>(BiFiltering::Forced), "Bilinear", "Forced"));
	m_gs_bifilter.push_back(GSSetting(static_cast<uint32>(BiFiltering::PS2), "Bilinear", "PS2"));

	m_gs_trifilter.push_back(GSSetting(static_cast<uint32>(TriFiltering::None), "None", "Default"));
	m_gs_trifilter.push_back(GSSetting(static_cast<uint32>(TriFiltering::PS2), "Trilinear", ""));
	m_gs_trifilter.push_back(GSSetting(static_cast<uint32>(TriFiltering::Forced), "Trilinear", "Ultra/Slow"));

	m_gs_generic_list.push_back(GSSetting(-1, "Automatic", "Default"));
	m_gs_generic_list.push_back(GSSetting(0, "Force-Disabled", ""));
	m_gs_generic_list.push_back(GSSetting(1, "Force-Enabled", ""));

	m_gs_hack.push_back(GSSetting(0, "Off", "Default"));
	m_gs_hack.push_back(GSSetting(1, "Half", ""));
	m_gs_hack.push_back(GSSetting(2, "Full", ""));

	m_gs_offset_hack.push_back(GSSetting(0, "Off", "Default"));
	m_gs_offset_hack.push_back(GSSetting(1, "Normal", "Vertex"));
	m_gs_offset_hack.push_back(GSSetting(2, "Special", "Texture"));
	m_gs_offset_hack.push_back(GSSetting(3, "Special", "Texture - aggressive"));

	m_gs_hw_mipmapping = {
		GSSetting(HWMipmapLevel::Automatic, "Automatic", "Default"),
		GSSetting(HWMipmapLevel::Off, "Off", ""),
		GSSetting(HWMipmapLevel::Basic, "Basic", "Fast"),
		GSSetting(HWMipmapLevel::Full, "Full", "Slow"),
	};

	m_gs_crc_level = {
		GSSetting(CRCHackLevel::Automatic, "Automatic", "Default"),
		GSSetting(CRCHackLevel::None, "None", "Debug"),
		GSSetting(CRCHackLevel::Minimum, "Minimum", "Debug"),
		GSSetting(CRCHackLevel::Partial, "Partial", "OpenGL"),
		GSSetting(CRCHackLevel::Full, "Full", "Direct3D"),
		GSSetting(CRCHackLevel::Aggressive, "Aggressive", ""),
	};

	m_gs_acc_date_level.push_back(GSSetting(0, "Off", ""));
	m_gs_acc_date_level.push_back(GSSetting(1, "Fast", "Default"));
	m_gs_acc_date_level.push_back(GSSetting(2, "Full", "Slow"));

	m_gs_acc_blend_level.push_back(GSSetting(0, "None", "Fastest"));
	m_gs_acc_blend_level.push_back(GSSetting(1, "Basic", "Recommended"));
	m_gs_acc_blend_level.push_back(GSSetting(2, "Medium", ""));
	m_gs_acc_blend_level.push_back(GSSetting(3, "High", ""));
	m_gs_acc_blend_level.push_back(GSSetting(4, "Full", "Very Slow"));
	m_gs_acc_blend_level.push_back(GSSetting(5, "Ultra", "Ultra Slow"));

	m_gs_acc_blend_level_d3d11.push_back(GSSetting(0, "None", "Fastest"));
	m_gs_acc_blend_level_d3d11.push_back(GSSetting(1, "Basic", "Recommended"));
	m_gs_acc_blend_level_d3d11.push_back(GSSetting(2, "Medium", "Debug"));
	m_gs_acc_blend_level_d3d11.push_back(GSSetting(3, "High", "Debug"));

	m_gs_tv_shaders.push_back(GSSetting(0, "None", ""));
	m_gs_tv_shaders.push_back(GSSetting(1, "Scanline filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(2, "Diagonal filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(3, "Triangular filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(4, "Wave filter", ""));

	// Avoid to clutter the ini file with useless options
#ifdef _WIN32
	// Per OS option.
	m_default_configuration["Adapter"]                                    = "default";
	m_default_configuration["CaptureFileName"]                            = "";
	m_default_configuration["CaptureVideoCodecDisplayName"]               = "";
	m_default_configuration["dx_break_on_severity"]                       = "0";
	// D3D Blending option
	m_default_configuration["accurate_blending_unit_d3d11"]               = "1";
#else
	m_default_configuration["linux_replay"]                               = "1";
#endif
	m_default_configuration["aa1"]                                        = "0";
	m_default_configuration["accurate_date"]                              = "1";
	m_default_configuration["accurate_blending_unit"]                     = "1";
	m_default_configuration["AspectRatio"]                                = "1";
	m_default_configuration["autoflush_sw"]                               = "1";
	m_default_configuration["capture_enabled"]                            = "0";
	m_default_configuration["capture_out_dir"]                            = "/tmp/GSdx_Capture";
	m_default_configuration["capture_threads"]                            = "4";
	m_default_configuration["CaptureHeight"]                              = "480";
	m_default_configuration["CaptureWidth"]                               = "640";
	m_default_configuration["clut_load_before_draw"]                      = "0";
	m_default_configuration["crc_hack_level"]                             = std::to_string(static_cast<int8>(CRCHackLevel::Automatic));
	m_default_configuration["CrcHacksExclusions"]                         = "";
	m_default_configuration["debug_glsl_shader"]                          = "0";
	m_default_configuration["debug_opengl"]                               = "0";
	m_default_configuration["disable_hw_gl_draw"]                         = "0";
	m_default_configuration["dithering_ps2"]                              = "2";
	m_default_configuration["dump"]                                       = "0";
	m_default_configuration["extrathreads"]                               = "2";
	m_default_configuration["extrathreads_height"]                        = "4";
	m_default_configuration["filter"]                                     = std::to_string(static_cast<int8>(BiFiltering::PS2));
	m_default_configuration["force_texture_clear"]                        = "0";
	m_default_configuration["fxaa"]                                       = "0";
	m_default_configuration["interlace"]                                  = "7";
	m_default_configuration["large_framebuffer"]                          = "0";
	m_default_configuration["linear_present"]                             = "1";
	m_default_configuration["MaxAnisotropy"]                              = "0";
	m_default_configuration["mipmap"]                                     = "1";
	m_default_configuration["mipmap_hw"]                                  = std::to_string(static_cast<int>(HWMipmapLevel::Automatic));
	m_default_configuration["ModeHeight"]                                 = "480";
	m_default_configuration["ModeWidth"]                                  = "640";
	m_default_configuration["NTSC_Saturation"]                            = "1";
#ifdef _WIN32
	m_default_configuration["osd_fontname"]                               = "C:\\Windows\\Fonts\\my_favorite_font_e_g_tahoma.ttf";
#else
	m_default_configuration["osd_fontname"]                               = "/usr/share/fonts/truetype/my_favorite_font_e_g_DejaVu Sans.ttf";
#endif
	m_default_configuration["osd_color_r"]                                = "0";
	m_default_configuration["osd_color_g"]                                = "160";
	m_default_configuration["osd_color_b"]                                = "255";
	m_default_configuration["osd_color_opacity"]                          = "100";
	m_default_configuration["osd_fontsize"]                               = "25";
	m_default_configuration["osd_log_enabled"]                            = "1";
	m_default_configuration["osd_log_timeout"]                            = "4";
	m_default_configuration["osd_monitor_enabled"]                        = "0";
	m_default_configuration["osd_max_log_messages"]                       = "2";
	m_default_configuration["override_geometry_shader"]                   = "-1";
	m_default_configuration["override_GL_ARB_compute_shader"]             = "-1";
	m_default_configuration["override_GL_ARB_copy_image"]                 = "-1";
	m_default_configuration["override_GL_ARB_clear_texture"]              = "-1";
	m_default_configuration["override_GL_ARB_clip_control"]               = "-1";
	m_default_configuration["override_GL_ARB_direct_state_access"]        = "-1";
	m_default_configuration["override_GL_ARB_draw_buffers_blend"]         = "-1";
	m_default_configuration["override_GL_ARB_get_texture_sub_image"]      = "-1";
	m_default_configuration["override_GL_ARB_gpu_shader5"]                = "-1";
	m_default_configuration["override_GL_ARB_multi_bind"]                 = "-1";
	m_default_configuration["override_GL_ARB_shader_image_load_store"]    = "-1";
	m_default_configuration["override_GL_ARB_shader_storage_buffer_object"] = "-1";
	m_default_configuration["override_GL_ARB_sparse_texture"]             = "-1";
	m_default_configuration["override_GL_ARB_sparse_texture2"]            = "-1";
	m_default_configuration["override_GL_ARB_texture_view"]               = "-1";
	m_default_configuration["override_GL_ARB_vertex_attrib_binding"]      = "-1";
	m_default_configuration["override_GL_ARB_texture_barrier"]            = "-1";
	m_default_configuration["paltex"]                                     = "0";
	m_default_configuration["png_compression_level"]                      = std::to_string(Z_BEST_SPEED);
	m_default_configuration["preload_frame_with_gs_data"]                 = "0";
	m_default_configuration["Renderer"]                                   = std::to_string(static_cast<int>(GSRendererType::Default));
	m_default_configuration["resx"]                                       = "1024";
	m_default_configuration["resy"]                                       = "1024";
	m_default_configuration["save"]                                       = "0";
	m_default_configuration["savef"]                                      = "0";
	m_default_configuration["savel"]                                      = "5000";
	m_default_configuration["saven"]                                      = "0";
	m_default_configuration["savet"]                                      = "0";
	m_default_configuration["savez"]                                      = "0";
	m_default_configuration["ShadeBoost"]                                 = "0";
	m_default_configuration["ShadeBoost_Brightness"]                      = "50";
	m_default_configuration["ShadeBoost_Contrast"]                        = "50";
	m_default_configuration["ShadeBoost_Saturation"]                      = "50";
	m_default_configuration["shaderfx"]                                   = "0";
	m_default_configuration["shaderfx_conf"]                              = "shaders/GSdx_FX_Settings.ini";
	m_default_configuration["shaderfx_glsl"]                              = "shaders/GSdx.fx";
	m_default_configuration["TVShader"]                                   = "0";
	m_default_configuration["upscale_multiplier"]                         = "1";
	m_default_configuration["UserHacks"]                                  = "0";
	m_default_configuration["UserHacks_align_sprite_X"]                   = "0";
	m_default_configuration["UserHacks_AutoFlush"]                        = "0";
	m_default_configuration["UserHacks_DisableDepthSupport"]              = "0";
	m_default_configuration["UserHacks_Disable_Safe_Features"]            = "0";
	m_default_configuration["UserHacks_DisablePartialInvalidation"]       = "0";
	m_default_configuration["UserHacks_CPU_FB_Conversion"]                = "0";
	m_default_configuration["UserHacks_Half_Bottom_Override"]             = "-1";
	m_default_configuration["UserHacks_HalfPixelOffset"]                  = "0";
	m_default_configuration["UserHacks_merge_pp_sprite"]                  = "0";
	m_default_configuration["UserHacks_round_sprite_offset"]              = "0";
	m_default_configuration["UserHacks_SkipDraw"]                         = "0";
	m_default_configuration["UserHacks_SkipDraw_Offset"]                  = "0";
	m_default_configuration["UserHacks_TCOffsetX"]                        = "0";
	m_default_configuration["UserHacks_TCOffsetY"]                        = "0";
	m_default_configuration["UserHacks_TextureInsideRt"]                  = "0";
	m_default_configuration["UserHacks_TriFilter"]                        = std::to_string(static_cast<int8>(TriFiltering::None));
	m_default_configuration["UserHacks_WildHack"]                         = "0";
	m_default_configuration["wrap_gs_mem"]                                = "0";
	m_default_configuration["vsync"]                                      = "0";
}

void GSdxApp::ReloadConfig()
{
	if (m_configuration_map.empty()) return;

	auto file = m_configuration_map.find("inifile");
	if (file == m_configuration_map.end()) return;

	// A map was built so reload it
	std::string filename = file->second;
	m_configuration_map.clear();
	BuildConfigurationMap(filename.c_str());
}

void GSdxApp::BuildConfigurationMap(const char* lpFileName)
{
	// Check if the map was already built
	std::string inifile_value(lpFileName);
	if ( inifile_value.compare(m_configuration_map["inifile"]) == 0 ) return;
	m_configuration_map["inifile"] = inifile_value;

	// Load config from file
#ifdef _WIN32
	std::ifstream file(convert_utf8_to_utf16(lpFileName));
#else
	std::ifstream file(lpFileName);
#endif
	if (!file.is_open())
		return;

	std::string line;
	while (std::getline(file, line)) {
		const auto separator = line.find('=');
		if (separator == std::string::npos)
			continue;

		std::string key = line.substr(0, separator);
		// Trim trailing whitespace
		key.erase(key.find_last_not_of(" \r\t") + 1);

		if (key.empty())
			continue;

		// Only keep options that have a default value so older, no longer used
		// ini options can be purged.
		if (m_default_configuration.find(key) == m_default_configuration.end())
			continue;

		std::string value = line.substr(separator + 1);
		// Trim leading whitespace
		value.erase(0, value.find_first_not_of(" \r\t"));

		m_configuration_map[key] = value;
	}
}

void* GSdxApp::GetModuleHandlePtr()
{
	return s_hModule;
}

void GSdxApp::SetConfigDir(const char* dir)
{
	if( dir == NULL )
	{
		m_ini = "inis/GSdx.ini";
	}
	else
	{
		m_ini = dir;

		if(m_ini[m_ini.length() - 1] != DIRECTORY_SEPARATOR)
		{
			m_ini += DIRECTORY_SEPARATOR;
		}

		m_ini += "GSdx.ini";
	}
}

std::string GSdxApp::GetConfigS(const char* entry)
{
	char buff[4096] = {0};
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end()) {
		GetIniString(m_section.c_str(), entry, def->second.c_str(), buff, countof(buff), m_ini.c_str());
	} else {
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
		GetIniString(m_section.c_str(), entry, "", buff, countof(buff), m_ini.c_str());
	}

	return {buff};
}

void GSdxApp::SetConfig(const char* entry, const char* value)
{
	WriteIniString(m_section.c_str(), entry, value, m_ini.c_str());
}

int GSdxApp::GetConfigI(const char* entry)
{
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end()) {
		return GetIniInt(m_section.c_str(), entry, std::stoi(def->second), m_ini.c_str());
	} else {
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
		return GetIniInt(m_section.c_str(), entry, 0, m_ini.c_str());
	}
}

bool GSdxApp::GetConfigB(const char* entry)
{
	return !!GetConfigI(entry);
}

void GSdxApp::SetConfig(const char* entry, int value)
{
	char buff[32] = {0};

	sprintf(buff, "%d", value);

	SetConfig(entry, buff);
}

void GSdxApp::SetCurrentRendererType(GSRendererType type)
{
	m_current_renderer_type = type;
}

GSRendererType GSdxApp::GetCurrentRendererType() const
{
	return m_current_renderer_type;
}
