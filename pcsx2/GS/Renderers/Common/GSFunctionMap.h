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

#include "GS/GSCodeBuffer.h"
#include "GS/GSExtra.h"
#include "GS/Renderers/SW/GSScanlineEnvironment.h"
#include "common/emitter/tools.h"

#include <xbyak/xbyak_util.h>

template <class KEY, class VALUE>
class GSFunctionMap
{
protected:
	struct ActivePtr
	{
		u64 frame, frames, prims;
		u64 ticks, actual, total;
		VALUE f;
	};

	std::unordered_map<KEY, ActivePtr*> m_map_active;

	ActivePtr* m_active;

	virtual VALUE GetDefaultFunction(KEY key) = 0;

public:
	GSFunctionMap()
		: m_active(NULL)
	{
	}

	virtual ~GSFunctionMap()
	{
		for (auto& i : m_map_active)
			delete i.second;
	}

	VALUE operator[](KEY key)
	{
		m_active = NULL;

		auto it = m_map_active.find(key);

		if (it != m_map_active.end())
		{
			m_active = it->second;
		}
		else
		{
			ActivePtr* p = new ActivePtr();

			memset(p, 0, sizeof(*p));

			p->frame = (u64)-1;

			p->f = GetDefaultFunction(key);

			m_map_active[key] = p;

			m_active = p;
		}

		return m_active->f;
	}

	void UpdateStats(u64 frame, u64 ticks, int actual, int total, int prims)
	{
		if (m_active)
		{
			if (m_active->frame != frame)
			{
				m_active->frame = frame;
				m_active->frames++;
			}

			m_active->prims += prims;
			m_active->ticks += ticks;
			m_active->actual += actual;
			m_active->total += total;

			ASSERT(m_active->total >= m_active->actual);
		}
	}

	virtual void PrintStats()
	{
		u64 totalTicks = 0;

		for (const auto& i : m_map_active)
		{
			ActivePtr* p = i.second;
			totalTicks += p->ticks;
		}

		double tick_us = 1.0 / x86capabilities::CachedMHz();
		double tick_ms = tick_us / 1000;
		double tick_ns = tick_us * 1000;

		printf("GS stats\n");

		printf("       key       | frames | prims |       runtime       |          pixels\n");
		printf("                 |        |  #/f  |   pct   ms/f  ns/px |    #/f   #/prim overdraw\n");

		std::vector<std::pair<KEY, ActivePtr*>> sorted(std::begin(m_map_active), std::end(m_map_active));
		std::sort(std::begin(sorted), std::end(sorted), [](const auto& l, const auto& r){ return l.second->ticks > r.second->ticks; });

		for (const auto& i : sorted)
		{
			KEY key = i.first;
			ActivePtr* p = i.second;

			if (p->frames && p->actual)
			{
				u64 tpf = p->ticks / p->frames;

				printf("%016llx | %6llu | %5llu | %5.2f%% %5.1f %6.1f | %8llu %6llu %5.2f%%\n",
					(u64)key,
					p->frames,
					p->prims / p->frames,
					(double)(p->ticks * 100) / totalTicks,
					tpf * tick_ms,
					(p->ticks * tick_ns) / p->actual,
					p->actual / p->frames,
					p->actual / (p->prims ? p->prims : 1),
					(double)((p->total - p->actual) * 100) / p->total);
			}
		}
	}
};

class GSCodeGenerator : public Xbyak::CodeGenerator
{
protected:
	Xbyak::util::Cpu m_cpu;

public:
	GSCodeGenerator(void* code, size_t maxsize)
		: Xbyak::CodeGenerator(maxsize, code)
	{
	}
};

template <class CG, class KEY, class VALUE>
class GSCodeGeneratorFunctionMap : public GSFunctionMap<KEY, VALUE>
{
	std::string m_name;
	void* m_param;
	std::unordered_map<u64, VALUE> m_cgmap;
	GSCodeBuffer m_cb;
	size_t m_total_code_size;

	enum { MAX_SIZE = 8192 };

public:
	GSCodeGeneratorFunctionMap(const char* name, void* param)
		: m_name(name)
		, m_param(param)
		, m_total_code_size(0)
	{
	}

	~GSCodeGeneratorFunctionMap()
	{
#ifdef _DEBUG
		fprintf(stderr, "%s generated %zu bytes of instruction\n", m_name.c_str(), m_total_code_size);
#endif
	}

	VALUE GetDefaultFunction(KEY key)
	{
		VALUE ret = NULL;

		auto i = m_cgmap.find(key);

		if (i != m_cgmap.end())
		{
			ret = i->second;
		}
		else
		{
			void* code_ptr = m_cb.GetBuffer(MAX_SIZE);

			CG* cg = new CG(m_param, key, code_ptr, MAX_SIZE);
			ASSERT(cg->getSize() < MAX_SIZE);

#if 0
			fprintf(stderr, "%s Location:%p Size:%zu Key:%llx\n", m_name.c_str(), code_ptr, cg->getSize(), (u64)key);
			GSScanlineSelector sel(key);
			sel.Print();
#endif

			m_total_code_size += cg->getSize();

			m_cb.ReleaseBuffer(cg->getSize());

			ret = (VALUE)cg->getCode();

			m_cgmap[key] = ret;

#ifdef ENABLE_VTUNE

			// vtune method registration

			// if(iJIT_IsProfilingActive()) // always > 0
			{
				std::string name = format("%s<%016llx>()", m_name.c_str(), (u64)key);

				iJIT_Method_Load ml;

				memset(&ml, 0, sizeof(ml));

				ml.method_id = iJIT_GetNewMethodID();
				ml.method_name = (char*)name.c_str();
				ml.method_load_address = (void*)cg->getCode();
				ml.method_size = (unsigned int)cg->getSize();

				iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &ml);
/*
				name = format("c:/temp1/%s_%016llx.bin", m_name.c_str(), (u64)key);

				if(FILE* fp = fopen(name.c_str(), "wb"))
				{
					fputc(0x0F, fp); fputc(0x0B, fp);
					fputc(0xBB, fp); fputc(0x6F, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
					fputc(0x64, fp); fputc(0x67, fp); fputc(0x90, fp);

					fwrite(cg->getCode(), cg->getSize(), 1, fp);

					fputc(0xBB, fp); fputc(0xDE, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
					fputc(0x64, fp); fputc(0x67, fp); fputc(0x90, fp);
					fputc(0x0F, fp); fputc(0x0B, fp);

					fclose(fp);
				}
*/
			}

#endif

			delete cg;
		}

		return ret;
	}
};
