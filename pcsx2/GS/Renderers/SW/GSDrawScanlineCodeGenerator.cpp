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

#include "PrecompiledHeader.h"
#include "GSDrawScanlineCodeGenerator.h"
#include "GSDrawScanlineCodeGenerator.all.h"
#include "GSDrawScanline.h"
#include <fstream>
#include <map>
#include <mutex>

static std::map<u64, bool> s_use_c_draw_scanline;
static std::mutex s_use_c_draw_scanline_mutex;

static bool shouldUseCDrawScanline(u64 key)
{
	static const char* const fname = getenv("USE_C_DRAW_SCANLINE");
	if (!fname)
		return false;

	std::lock_guard<std::mutex> l(s_use_c_draw_scanline_mutex);

	if (s_use_c_draw_scanline.empty())
	{
		std::ifstream file(fname);
		if (file)
		{
			for (std::string str; std::getline(file, str);)
			{
				u64 key;
				char yn;
				if (sscanf(str.c_str(), "%llx %c", &key, &yn) == 2)
				{
					if (yn != 'Y' && yn != 'N' && yn != 'y' && yn != 'n')
						Console.Warning("Failed to parse %s: Not y/n", str.c_str());
					s_use_c_draw_scanline[key] = (yn == 'Y' || yn == 'y') ? true : false;
				}
				else
				{
					Console.Warning("Failed to process line %s", str.c_str());
				}
			}
		}
	}

	auto idx = s_use_c_draw_scanline.find(key);
	if (idx == s_use_c_draw_scanline.end())
	{
		s_use_c_draw_scanline[key] = false;
		// Rewrite file
		FILE* file = fopen(fname, "w");
		if (file)
		{
			for (const auto& pair : s_use_c_draw_scanline)
			{
				fprintf(file, "%016llX %c %s\n", pair.first, pair.second ? 'Y' : 'N', GSScanlineSelector(pair.first).to_string().c_str());
			}
			fclose(file);
		}
		else
		{
			Console.Warning("Failed to write C draw scanline usage config: %s", strerror(errno));
		}
		return false;
	}

	return idx->second;
}

GSDrawScanlineCodeGenerator::GSDrawScanlineCodeGenerator(void* param, u64 key, void* code, size_t maxsize)
	: GSCodeGenerator(code, maxsize)
	, m_local(*(GSScanlineLocalData*)param)
	, m_rip(false)
{
	m_sel.key = key;

	if (m_sel.breakpoint)
		db(0xCC);

	if (shouldUseCDrawScanline(key))
	{
#if defined(_WIN32)
		mov(r8, reinterpret_cast<size_t>(&m_local));
		push(ptr[r8 + offsetof(GSScanlineLocalData, gd)]);
		push(r8);
		sub(rsp, 32); // CC required shadow space
		call(reinterpret_cast<void*>(GSDrawScanline::CDrawScanline));
		ret(48);
#else
		mov(r8, reinterpret_cast<size_t>(&m_local));
		mov(r9, ptr[r8 + offsetof(GSScanlineLocalData, gd)]);
		jmp(reinterpret_cast<void*>(GSDrawScanline::CDrawScanline));
#endif
		return;
	}

	GSDrawScanlineCodeGenerator2(this, CPUInfo(m_cpu), (void*)&m_local, m_sel.key).Generate();
}
