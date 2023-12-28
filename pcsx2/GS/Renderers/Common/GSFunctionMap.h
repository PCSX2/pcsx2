// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GSExtra.h"
#include "GS/Renderers/SW/GSScanlineEnvironment.h"

#include "common/HostSys.h"

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

			pxAssert(m_active->total >= m_active->actual);
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
namespace GSCodeReserve
{
	void ResetMemory();

	size_t GetMemoryUsed();

	u8* ReserveMemory(size_t size);
	void CommitMemory(size_t size);
}

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
			u8* code_ptr = GSCodeReserve::ReserveMemory(MAX_SIZE);
			CG cg(key, code_ptr, MAX_SIZE);
			pxAssert(cg.getSize() < MAX_SIZE);

#if 0
			fprintf(stderr, "%s Location:%p Size:%zu Key:%llx\n", m_name.c_str(), code_ptr, cg.getSize(), (u64)key);
			GSScanlineSelector sel(key);
			sel.Print();
#endif

			GSCodeReserve::CommitMemory(cg.getSize());

			ret = (VALUE)cg.getCode();

			m_cgmap[key] = ret;
		}

		return ret;
	}
};
