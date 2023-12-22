// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <vector>
#include <cstdio>
#include "common/Pcsx2Types.h"

namespace Perf
{
	class Group
	{
		const char* m_prefix;

	public:
		constexpr Group(const char* prefix) : m_prefix(prefix) {}
		bool HasPrefix() const { return (m_prefix && m_prefix[0]); }

		void Register(const void* ptr, size_t size, const char* symbol);
		void RegisterPC(const void* ptr, size_t size, u32 pc);
		void RegisterKey(const void* ptr, size_t size, const char* prefix, u64 key);
	};

	extern Group any;
	extern Group ee;
	extern Group iop;
	extern Group vu0;
	extern Group vu1;
	extern Group vif;
} // namespace Perf
