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
#include "GSUtil.h"

#ifdef _WIN32
#include "Renderers/DX11/GSDevice11.h"
#include <VersionHelpers.h>
#include "svnrev.h"
#else
#define SVN_REV 0
#define SVN_MODS 0
#endif

Xbyak::util::Cpu g_cpu;

const char* GSUtil::GetLibName()
{
	// The following ifdef mess is courtesy of "static string str;"
	// being optimised by GCC to be unusable by older CPUs. Enjoy!
	static char name[255];

#if _M_SSE < 0x501
	const char* sw_sse = g_cpu.has(Xbyak::util::Cpu::tAVX) ? "AVX" :
		g_cpu.has(Xbyak::util::Cpu::tSSE41) ? "SSE41" :
		g_cpu.has(Xbyak::util::Cpu::tSSSE3) ? "SSSE3" : "SSE2";
#endif

	snprintf(name, sizeof(name), "GSdx "

#ifdef _WIN32
		"%lld "
#endif
#ifdef _M_AMD64
		"64-bit "
#endif
#ifdef __INTEL_COMPILER
		"(Intel C++ %d.%02d %s/%s)",
#elif _MSC_VER
		"(MSVC %d.%02d %s/%s)",
#elif __clang__
		"(clang %d.%d.%d %s/%s)",
#elif __GNUC__
		"(GCC %d.%d.%d %s/%s)",
#else
		"(%s/%s)",
#endif
#ifdef _WIN32
		SVN_REV,
#endif
#ifdef __INTEL_COMPILER
		__INTEL_COMPILER / 100, __INTEL_COMPILER % 100,
#elif _MSC_VER
		_MSC_VER / 100, _MSC_VER % 100,
#elif __clang__
		__clang_major__, __clang_minor__, __clang_patchlevel__,
#elif __GNUC__
		__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
#endif

#if _M_SSE >= 0x501
		"AVX2", "AVX2"
#elif _M_SSE >= 0x500
		"AVX", sw_sse
#elif _M_SSE >= 0x401
		"SSE4.1", sw_sse
#elif _M_SSE >= 0x301
		"SSSE3", sw_sse
#elif _M_SSE >= 0x200
		"SSE2", sw_sse
#endif
	);

	return name;
}

static class GSUtilMaps
{
public:
	uint8 PrimClassField[8];
	uint8 VertexCountField[8];
	uint8 ClassVertexCountField[4];
	uint32 CompatibleBitsField[64][2];
	uint32 SharedBitsField[64][2];

	// Defer init to avoid AVX2 illegal instructions
	void Init()
	{
		PrimClassField[GS_POINTLIST] = GS_POINT_CLASS;
		PrimClassField[GS_LINELIST] = GS_LINE_CLASS;
		PrimClassField[GS_LINESTRIP] = GS_LINE_CLASS;
		PrimClassField[GS_TRIANGLELIST] = GS_TRIANGLE_CLASS;
		PrimClassField[GS_TRIANGLESTRIP] = GS_TRIANGLE_CLASS;
		PrimClassField[GS_TRIANGLEFAN] = GS_TRIANGLE_CLASS;
		PrimClassField[GS_SPRITE] = GS_SPRITE_CLASS;
		PrimClassField[GS_INVALID] = GS_INVALID_CLASS;

		VertexCountField[GS_POINTLIST] = 1;
		VertexCountField[GS_LINELIST] = 2;
		VertexCountField[GS_LINESTRIP] = 2;
		VertexCountField[GS_TRIANGLELIST] = 3;
		VertexCountField[GS_TRIANGLESTRIP] = 3;
		VertexCountField[GS_TRIANGLEFAN] = 3;
		VertexCountField[GS_SPRITE] = 2;
		VertexCountField[GS_INVALID] = 1;

		ClassVertexCountField[GS_POINT_CLASS] = 1;
		ClassVertexCountField[GS_LINE_CLASS] = 2;
		ClassVertexCountField[GS_TRIANGLE_CLASS] = 3;
		ClassVertexCountField[GS_SPRITE_CLASS] = 2;

		memset(CompatibleBitsField, 0, sizeof(CompatibleBitsField));

		for(int i = 0; i < 64; i++)
		{
			CompatibleBitsField[i][i >> 5] |= 1 << (i & 0x1f);
		}

		CompatibleBitsField[PSM_PSMCT32][PSM_PSMCT24 >> 5] |= 1 << (PSM_PSMCT24 & 0x1f);
		CompatibleBitsField[PSM_PSMCT24][PSM_PSMCT32 >> 5] |= 1 << (PSM_PSMCT32 & 0x1f);
		CompatibleBitsField[PSM_PSMCT16][PSM_PSMCT16S >> 5] |= 1 << (PSM_PSMCT16S & 0x1f);
		CompatibleBitsField[PSM_PSMCT16S][PSM_PSMCT16 >> 5] |= 1 << (PSM_PSMCT16 & 0x1f);
		CompatibleBitsField[PSM_PSMZ32][PSM_PSMZ24 >> 5] |= 1 << (PSM_PSMZ24 & 0x1f);
		CompatibleBitsField[PSM_PSMZ24][PSM_PSMZ32 >> 5] |= 1 << (PSM_PSMZ32 & 0x1f);
		CompatibleBitsField[PSM_PSMZ16][PSM_PSMZ16S >> 5] |= 1 << (PSM_PSMZ16S & 0x1f);
		CompatibleBitsField[PSM_PSMZ16S][PSM_PSMZ16 >> 5] |= 1 << (PSM_PSMZ16 & 0x1f);

		memset(SharedBitsField, 0, sizeof(SharedBitsField));

		SharedBitsField[PSM_PSMCT24][PSM_PSMT8H >> 5] |= 1 << (PSM_PSMT8H & 0x1f);
		SharedBitsField[PSM_PSMCT24][PSM_PSMT4HL >> 5] |= 1 << (PSM_PSMT4HL & 0x1f);
		SharedBitsField[PSM_PSMCT24][PSM_PSMT4HH >> 5] |= 1 << (PSM_PSMT4HH & 0x1f);
		SharedBitsField[PSM_PSMZ24][PSM_PSMT8H >> 5] |= 1 << (PSM_PSMT8H & 0x1f);
		SharedBitsField[PSM_PSMZ24][PSM_PSMT4HL >> 5] |= 1 << (PSM_PSMT4HL & 0x1f);
		SharedBitsField[PSM_PSMZ24][PSM_PSMT4HH >> 5] |= 1 << (PSM_PSMT4HH & 0x1f);
		SharedBitsField[PSM_PSMT8H][PSM_PSMCT24 >> 5] |= 1 << (PSM_PSMCT24 & 0x1f);
		SharedBitsField[PSM_PSMT8H][PSM_PSMZ24 >> 5] |= 1 << (PSM_PSMZ24 & 0x1f);
		SharedBitsField[PSM_PSMT4HL][PSM_PSMCT24 >> 5] |= 1 << (PSM_PSMCT24 & 0x1f);
		SharedBitsField[PSM_PSMT4HL][PSM_PSMZ24 >> 5] |= 1 << (PSM_PSMZ24 & 0x1f);
		SharedBitsField[PSM_PSMT4HL][PSM_PSMT4HH >> 5] |= 1 << (PSM_PSMT4HH & 0x1f);
		SharedBitsField[PSM_PSMT4HH][PSM_PSMCT24 >> 5] |= 1 << (PSM_PSMCT24 & 0x1f);
		SharedBitsField[PSM_PSMT4HH][PSM_PSMZ24 >> 5] |= 1 << (PSM_PSMZ24 & 0x1f);
		SharedBitsField[PSM_PSMT4HH][PSM_PSMT4HL >> 5] |= 1 << (PSM_PSMT4HL & 0x1f);
	}

} s_maps;

void GSUtil::Init()
{
	s_maps.Init();
}

GS_PRIM_CLASS GSUtil::GetPrimClass(uint32 prim)
{
	return (GS_PRIM_CLASS)s_maps.PrimClassField[prim];
}

int GSUtil::GetVertexCount(uint32 prim)
{
	return s_maps.VertexCountField[prim];
}

int GSUtil::GetClassVertexCount(uint32 primclass)
{
	return s_maps.ClassVertexCountField[primclass];
}

const uint32* GSUtil::HasSharedBitsPtr(uint32 dpsm)
{
	return s_maps.SharedBitsField[dpsm];
}

bool GSUtil::HasSharedBits(uint32 spsm, const uint32* RESTRICT ptr)
{
	return (ptr[spsm >> 5] & (1 << (spsm & 0x1f))) == 0;
}

bool GSUtil::HasSharedBits(uint32 spsm, uint32 dpsm)
{
	return (s_maps.SharedBitsField[dpsm][spsm >> 5] & (1 << (spsm & 0x1f))) == 0;
}

bool GSUtil::HasSharedBits(uint32 sbp, uint32 spsm, uint32 dbp, uint32 dpsm)
{
	return ((sbp ^ dbp) | (s_maps.SharedBitsField[dpsm][spsm >> 5] & (1 << (spsm & 0x1f)))) == 0;
}

bool GSUtil::HasCompatibleBits(uint32 spsm, uint32 dpsm)
{
	return (s_maps.CompatibleBitsField[spsm][dpsm >> 5] & (1 << (dpsm & 0x1f))) != 0;
}

bool GSUtil::CheckSSE()
{
	bool status = true;

	struct ISA {
		Xbyak::util::Cpu::Type type;
		const char* name;
	};

	ISA checks[] = {
		{Xbyak::util::Cpu::tSSE2, "SSE2"},
#if _M_SSE >= 0x301
		{Xbyak::util::Cpu::tSSSE3, "SSSE3"},
#endif
#if _M_SSE >= 0x401
		{Xbyak::util::Cpu::tSSE41, "SSE41"},
#endif
#if _M_SSE >= 0x500
		{Xbyak::util::Cpu::tAVX, "AVX1"},
#endif
#if _M_SSE >= 0x501
		{Xbyak::util::Cpu::tAVX2, "AVX2"},
		{Xbyak::util::Cpu::tBMI1, "BMI1"},
		{Xbyak::util::Cpu::tBMI2, "BMI2"},
#endif
	};

	for (size_t i = 0; i < countof(checks); i++) {
		if(!g_cpu.has(checks[i].type)) {
			fprintf(stderr, "This CPU does not support %s\n", checks[i].name);

			status = false;
		}
	}

	return status;
}

CRCHackLevel GSUtil::GetRecommendedCRCHackLevel(GSRendererType type)
{
	return type == GSRendererType::OGL_HW ? CRCHackLevel::Partial : CRCHackLevel::Full;
}

#ifdef _WIN32
// ---------------------------------------------------------------------------------
//  DX11 Detection (includes DXGI detection and dynamic library method bindings)
// ---------------------------------------------------------------------------------
//  Code 'Borrowed' from Microsoft's DXGI sources -- Modified to suit our needs. --air
//  Stripped down because of unnecessary complexity and false positives
//  e.g. (d3d11_beta.dll would fail at device creation time) --pseudonym

static int s_DXGI;
static int s_D3D11;

bool GSUtil::CheckDXGI()
{
	if (0 == s_DXGI)
	{
		HMODULE hmod = LoadLibrary("dxgi.dll");
		s_DXGI = hmod ? 1 : -1;
		if (hmod)
			FreeLibrary(hmod);
	}

	return s_DXGI > 0;
}

bool GSUtil::CheckD3D11()
{
	if (!CheckDXGI())
		return false;

	if (0 == s_D3D11)
	{
		HMODULE hmod = LoadLibrary("d3d11.dll");
		s_D3D11 = hmod ? 1 : -1;
		if (hmod)
			FreeLibrary(hmod);
	}

	return s_D3D11 > 0;
}

D3D_FEATURE_LEVEL GSUtil::CheckDirect3D11Level(IDXGIAdapter *adapter, D3D_DRIVER_TYPE type)
{
	HRESULT hr;
	D3D_FEATURE_LEVEL level;

	if(!CheckD3D11())
		return (D3D_FEATURE_LEVEL)0;

	hr = D3D11CreateDevice(adapter, type, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, &level, NULL);

	return SUCCEEDED(hr) ? level : (D3D_FEATURE_LEVEL)0;
}

GSRendererType GSUtil::GetBestRenderer()
{
	CComPtr<IDXGIFactory1> dxgi_factory;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory))))
	{
		CComPtr<IDXGIAdapter1> adapter;
		if (SUCCEEDED(dxgi_factory->EnumAdapters1(0, &adapter)))
		{
			DXGI_ADAPTER_DESC1 desc;
			if (SUCCEEDED(adapter->GetDesc1(&desc)))
			{
				D3D_FEATURE_LEVEL level = GSUtil::CheckDirect3D11Level();
				// Check for Nvidia VendorID. Latest OpenGL features need at least DX11 level GPU
				if (desc.VendorId == 0x10DE && level >= D3D_FEATURE_LEVEL_11_0)
					return GSRendererType::OGL_HW;
			}
		}
	}
	return GSRendererType::DX1011_HW;
}

#endif

void GSmkdir(const char* dir)
{
#ifdef _WIN32
	if (!CreateDirectory(dir, nullptr)) {
		DWORD errorID = ::GetLastError();
		if (errorID != ERROR_ALREADY_EXISTS) {
			fprintf(stderr, "Failed to create directory: %s error %u\n", dir, errorID);
		}
	}
#else
	int err = mkdir(dir, 0777);
	if (!err && errno != EEXIST)
		fprintf(stderr, "Failed to create directory: %s\n", dir);
#endif
}

std::string GStempdir()
{
#ifdef _WIN32
	char path[MAX_PATH + 1];
	GetTempPath(MAX_PATH, path);
	return {path};
#else
	return "/tmp";
#endif
}

const char* psm_str(int psm)
{
	switch(psm) {
		// Normal color
		case PSM_PSMCT32:  return "C_32";
		case PSM_PSMCT24:  return "C_24";
		case PSM_PSMCT16:  return "C_16";
		case PSM_PSMCT16S: return "C_16S";

		// Palette color
		case PSM_PSMT8:    return "P_8";
		case PSM_PSMT4:    return "P_4";
		case PSM_PSMT8H:   return "P_8H";
		case PSM_PSMT4HL:  return "P_4HL";
		case PSM_PSMT4HH:  return "P_4HH";

		// Depth
		case PSM_PSMZ32:   return "Z_32";
		case PSM_PSMZ24:   return "Z_24";
		case PSM_PSMZ16:   return "Z_16";
		case PSM_PSMZ16S:  return "Z_16S";

		case PSM_PSGPU24:     return "PS24";

		default:break;
	}
	return "BAD_PSM";
}
