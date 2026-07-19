// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <vector>
#include <cstdio>
#include <string>
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

	// Override the directory where the jitdump file (and its per-PID subdir)
	// gets written. Defaults to /tmp; call before the first JIT block compiles
	// to redirect (e.g. EmuFolders::Cache so /tmp doesn't fill up on tmpfs
	// systems). No-op on non-jitdump builds.
	void SetJitDumpDir(std::string dir);

	// Enable/disable the jitdump writer at runtime. Default false; call from
	// settings load with EmuConfig.Profiler.EnablePerfDump. Once disabled the
	// RegisterMethod calls early-return; nothing is written. Has no effect
	// on a file that's already been opened in this process.
	void SetJitDumpEnabled(bool enabled);

	extern Group any;
	extern Group ee;
	extern Group iop;
	extern Group vu0;
	extern Group vu1;
	extern Group vif;
} // namespace Perf
