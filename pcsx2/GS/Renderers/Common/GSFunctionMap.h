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

#include "GS/GSExtra.h"
#include "GS/Renderers/SW/GSScanlineEnvironment.h"
#include "VirtualMemory.h"
#include "common/emitter/tools.h"

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

	void PrintStats()
	{
		u64 totalTicks = 0;

		for (const auto& i : m_map_active)
		{
			ActivePtr* p = i.second;
			totalTicks += p->ticks;
		}

		double tick_us = 1.0 / GetTickFrequency();
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

// --------------------------------------------------------------------------------------
//  GSCodeReserve
// --------------------------------------------------------------------------------------
// Stores code buffers for the GS software JIT.
//
class GSCodeReserve : public RecompiledCodeReserve
{
public:
	GSCodeReserve();
	~GSCodeReserve();

	static GSCodeReserve& GetInstance();

	size_t GetMemoryUsed() const { return m_memory_used; }

	void Assign(VirtualMemoryManagerPtr allocator);
	void Reset();

	u8* Reserve(size_t size);
	void Commit(size_t size);

private:
	size_t m_memory_used = 0;
};

template <class CG, class KEY, class VALUE>
class GSCodeGeneratorFunctionMap : public GSFunctionMap<KEY, VALUE>
{
	std::string m_name;
	std::unordered_map<u64, VALUE> m_cgmap;

	enum { MAX_SIZE = 8192 };

public:
	GSCodeGeneratorFunctionMap(std::string name)
		: m_name(name)
	{
	}

	~GSCodeGeneratorFunctionMap() = default;

	void Clear()
	{
		m_cgmap.clear();
	}

	VALUE GetDefaultFunction(KEY key)
	{
		VALUE ret = nullptr;

		auto i = m_cgmap.find(key);

		if (i != m_cgmap.end())
		{
			ret = i->second;
		}
		else
		{
			u8* code_ptr = GSCodeReserve::GetInstance().Reserve(MAX_SIZE);
			CG cg(key, code_ptr, MAX_SIZE);
			ASSERT(cg.getSize() < MAX_SIZE);

#if 0
			fprintf(stderr, "%s Location:%p Size:%zu Key:%llx\n", m_name.c_str(), code_ptr, cg.getSize(), (u64)key);
			GSScanlineSelector sel(key);
			sel.Print();
#endif

			GSCodeReserve::GetInstance().Commit(cg.getSize());

			ret = (VALUE)cg.getCode();

			m_cgmap[key] = ret;
		}

		return ret;
	}
};
