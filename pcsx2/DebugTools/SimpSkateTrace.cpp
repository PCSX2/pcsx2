// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugTools/SimpSkateTrace.h"

#include "Memory.h"
#include "R5900.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace SimpSkateTrace
{
namespace
{
static std::mutex s_lock;
static bool s_init = false;
static bool s_enabled = false;
static std::FILE* s_fp = nullptr;
static std::array<u32, 32> s_tracepoints = {};
static size_t s_tracepoint_count = 0;
static u64 s_seq = 0;

static std::string Trim(std::string s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
	return s;
}

static std::string JsonEscape(const std::string_view& text)
{
	std::string out;
	out.reserve(text.size() + 8);
	for (const unsigned char ch : text)
	{
		switch (ch)
		{
			case '\"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (ch < 0x20)
				{
					out += '?';
				}
				else
				{
					out.push_back(static_cast<char>(ch));
				}
				break;
		}
	}
	return out;
}

static std::string DefaultOutputPath()
{
	const char* out_env = std::getenv("PCSX2_SIMPTRACE_OUT");
	if (out_env && out_env[0] != '\0')
		return out_env;

	const char* temp_env = std::getenv("TEMP");
	if (temp_env && temp_env[0] != '\0')
		return Path::Combine(temp_env, "pcsx2_simpskate_trace.jsonl");

	return "pcsx2_simpskate_trace.jsonl";
}

static void ParseTracepoints(const std::string_view raw)
{
	s_tracepoint_count = 0;
	std::string current;
	for (const char ch : raw)
	{
		if (ch == ',')
		{
			current = Trim(current);
			if (!current.empty() && s_tracepoint_count < s_tracepoints.size())
			{
				char* end = nullptr;
				const u32 addr = static_cast<u32>(std::strtoul(current.c_str(), &end, 0));
				if (end != nullptr && end != current.c_str())
					s_tracepoints[s_tracepoint_count++] = addr;
			}
			current.clear();
			continue;
		}
		current.push_back(ch);
	}

	current = Trim(current);
	if (!current.empty() && s_tracepoint_count < s_tracepoints.size())
	{
		char* end = nullptr;
		const u32 addr = static_cast<u32>(std::strtoul(current.c_str(), &end, 0));
		if (end != nullptr && end != current.c_str())
			s_tracepoints[s_tracepoint_count++] = addr;
	}
}

static void InitLocked()
{
	if (s_init)
		return;
	s_init = true;

	const char* enabled_env = std::getenv("PCSX2_SIMPTRACE");
	if (!enabled_env || enabled_env[0] == '\0' || std::strcmp(enabled_env, "0") == 0 ||
		StringUtil::compareNoCase(enabled_env, "false"))
	{
		s_enabled = false;
		return;
	}

	std::string point_list;
	if (const char* points_env = std::getenv("PCSX2_SIMPTRACE_EE_FUNCS");
		points_env && points_env[0] != '\0')
	{
		point_list = points_env;
	}
	else
	{
		// Loader/dispatch conversion chain from current Simpsons Skateboarding reverse work.
		point_list = "0x00131410,0x00131510,0x0013e510,0x0011c4d8,0x002d5150,0x0030fd00";
	}
	ParseTracepoints(point_list);

	const std::string out_path = DefaultOutputPath();
	s_fp = FileSystem::OpenCFile(out_path.c_str(), "ab");
	if (!s_fp)
	{
		Console.Error("SimpSkateTrace: failed to open '%s' (errno=%d)", out_path.c_str(), errno);
		s_enabled = false;
		return;
	}

	s_enabled = true;
	std::ostringstream os;
	os << "{\"type\":\"trace_start\",\"note\":\"simpskate\",\"ee_tracepoints\":[";
	for (size_t i = 0; i < s_tracepoint_count; i++)
	{
		if (i != 0)
			os << ',';
		os << s_tracepoints[i];
	}
	os << "]}\n";
	const std::string line = os.str();
	std::fwrite(line.data(), 1, line.size(), s_fp);
	std::fflush(s_fp);
	Console.WriteLn(Color_StrongGreen, "SimpSkateTrace enabled -> %s", out_path.c_str());
}

static void LogLineLocked(const std::string& line)
{
	if (!s_enabled || !s_fp)
		return;
	std::fwrite(line.data(), 1, line.size(), s_fp);
	std::fwrite("\n", 1, 1, s_fp);
	std::fflush(s_fp);
}

static std::string ReadEEString(u32 ee_addr, size_t max_len = 160)
{
	std::string out;
	if (ee_addr == 0)
		return out;

	out.reserve(max_len);
	for (size_t i = 0; i < max_len; i++)
	{
		const u8* ptr = reinterpret_cast<const u8*>(PSM(ee_addr + static_cast<u32>(i)));
		if (!ptr)
			break;
		const unsigned char ch = *ptr;
		if (ch == 0)
			break;
		out.push_back((ch >= 32 && ch < 127) ? static_cast<char>(ch) : '?');
	}
	return out;
}

static bool ReadEEU32(u32 ee_addr, u32* out)
{
	const u8* ptr = reinterpret_cast<const u8*>(PSM(ee_addr));
	if (!ptr)
		return false;
	std::memcpy(out, ptr, sizeof(u32));
	return true;
}

static bool ReadEEF32(u32 ee_addr, float* out)
{
	u32 word = 0;
	if (!ReadEEU32(ee_addr, &word))
		return false;
	std::memcpy(out, &word, sizeof(float));
	return std::isfinite(*out);
}
} // namespace

bool IsEnabled()
{
	std::lock_guard<std::mutex> lock(s_lock);
	InitLocked();
	return s_enabled;
}

bool IsEETracepoint(u32 pc)
{
	std::lock_guard<std::mutex> lock(s_lock);
	InitLocked();
	if (!s_enabled)
		return false;

	for (size_t i = 0; i < s_tracepoint_count; i++)
	{
		if (s_tracepoints[i] == pc)
			return true;
	}
	return false;
}

void OnEETracepoint(u32 pc)
{
	std::lock_guard<std::mutex> lock(s_lock);
	InitLocked();
	if (!s_enabled)
		return;

	const u32 a0 = cpuRegs.GPR.n.a0.UL[0];
	const u32 a1 = cpuRegs.GPR.n.a1.UL[0];
	const u32 a2 = cpuRegs.GPR.n.a2.UL[0];
	const u32 a3 = cpuRegs.GPR.n.a3.UL[0];

	const std::string a1_text = ReadEEString(a1);
	const std::string a0_text = ReadEEString(a0);

	std::ostringstream os;
	os << "{\"type\":\"ee_trace\""
	   << ",\"seq\":" << ++s_seq
	   << ",\"cycle\":" << cpuRegs.cycle
	   << ",\"pc\":" << pc
	   << ",\"a0\":" << a0
	   << ",\"a1\":" << a1
	   << ",\"a2\":" << a2
	   << ",\"a3\":" << a3
	   << ",\"a0_str\":\"" << JsonEscape(a0_text) << "\""
	   << ",\"a1_str\":\"" << JsonEscape(a1_text) << "\"";

	// DAT object post-conversion record candidate.
	if (pc == 0x0013E510 && a1 != 0)
	{
		u32 status = 0;
		u32 unique_id = 0;
		u32 flags = 0;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float rx = 0.0f;
		float ry = 0.0f;
		float rz = 0.0f;
		const bool ok_status = ReadEEU32(a1 + 0x32, &status);
		const bool ok_uid = ReadEEU32(a1 + 0x34, &unique_id);
		const bool ok_flags = ReadEEU32(a1 + 0x38, &flags);
		const bool ok_x = ReadEEF32(a1 + 0x3C, &x);
		const bool ok_y = ReadEEF32(a1 + 0x40, &y);
		const bool ok_z = ReadEEF32(a1 + 0x44, &z);
		const bool ok_rx = ReadEEF32(a1 + 0x48, &rx);
		const bool ok_ry = ReadEEF32(a1 + 0x4C, &ry);
		const bool ok_rz = ReadEEF32(a1 + 0x50, &rz);
		const std::string custom = ReadEEString(a1 + 0x54, 192);

		os << ",\"obj\":{"
		   << "\"valid\":" << ((ok_status || ok_uid || ok_flags || ok_x || ok_y || ok_z || ok_rx || ok_ry || ok_rz) ? "true" : "false")
		   << ",\"status\":" << status
		   << ",\"unique_id\":" << unique_id
		   << ",\"flags\":" << flags
		   << ",\"x\":" << x
		   << ",\"y\":" << y
		   << ",\"z\":" << z
		   << ",\"rx\":" << rx
		   << ",\"ry\":" << ry
		   << ",\"rz\":" << rz
		   << ",\"custom\":\"" << JsonEscape(custom) << "\""
		   << "}";
	}

	os << "}";
	LogLineLocked(os.str());
}

void OnIsoOpen(const std::string_view& iso_path)
{
	std::lock_guard<std::mutex> lock(s_lock);
	InitLocked();
	if (!s_enabled)
		return;

	std::ostringstream os;
	os << "{\"type\":\"iso_open\",\"path\":\"" << JsonEscape(std::string(iso_path)) << "\"}";
	LogLineLocked(os.str());
}

void OnIsoMapBuilt(size_t file_count, bool has_assets_blt, u32 assets_blt_lsn, u32 assets_blt_size)
{
	std::lock_guard<std::mutex> lock(s_lock);
	InitLocked();
	if (!s_enabled)
		return;

	std::ostringstream os;
	os << "{\"type\":\"iso_map\",\"files\":" << file_count
	   << ",\"has_assets_blt\":" << (has_assets_blt ? "true" : "false")
	   << ",\"assets_blt_lsn\":" << assets_blt_lsn
	   << ",\"assets_blt_size\":" << assets_blt_size
	   << "}";
	LogLineLocked(os.str());
}

void OnIsoReadRun(
	u32 start_lsn,
	u32 sector_count,
	int mode,
	u32 ee_pc,
	u32 iop_pc,
	const std::string_view& owner_path,
	u32 owner_offset,
	u32 owner_size)
{
	std::lock_guard<std::mutex> lock(s_lock);
	InitLocked();
	if (!s_enabled)
		return;

	std::ostringstream os;
	os << "{\"type\":\"iso_read\""
	   << ",\"start_lsn\":" << start_lsn
	   << ",\"sector_count\":" << sector_count
	   << ",\"mode\":" << mode
	   << ",\"ee_pc\":" << ee_pc
	   << ",\"iop_pc\":" << iop_pc
	   << ",\"owner\":\"" << JsonEscape(std::string(owner_path)) << "\""
	   << ",\"owner_offset\":" << owner_offset
	   << ",\"owner_size\":" << owner_size
	   << "}";
	LogLineLocked(os.str());
}
} // namespace SimpSkateTrace
