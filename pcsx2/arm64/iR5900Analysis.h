// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 wrapper for the shared instruction analysis pass.
// Routes to ARM64-specific headers instead of x86 iR5900.h/iCore.h.

#pragma once

#include "arm64/iR5900-arm64.h"
#include "arm64/iCore-arm64.h"

// Re-export the shared analysis classes and functions from x86/iR5900Analysis.h
namespace R5900
{
	class AnalysisPass
	{
	public:
		AnalysisPass();
		virtual ~AnalysisPass();

		virtual void Run(u32 start, u32 end, EEINST* inst_cache);

	protected:
		template <class F>
		void ForEachInstruction(u32 start, u32 end, EEINST* inst_cache, const F& func);

		template <class F>
		void DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache, const F& func);
	};

	class COP2FlagHackPass final : public AnalysisPass
	{
	public:
		COP2FlagHackPass();
		~COP2FlagHackPass();

		void Run(u32 start, u32 end, EEINST* inst_cache) override;

	private:
		void DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache);

		void CommitStatusFlag();
		void CommitMACFlag();
		void CommitClipFlag();
		void CommitAllFlags();

		bool m_status_denormalized = false;
		EEINST* m_last_status_write = nullptr;
		EEINST* m_last_mac_write = nullptr;
		EEINST* m_last_clip_write = nullptr;

		u32 m_cfc2_pc = 0;
	};

	class COP2MicroFinishPass final : public AnalysisPass
	{
	public:
		COP2MicroFinishPass();
		~COP2MicroFinishPass();

		void Run(u32 start, u32 end, EEINST* inst_cache) override;
	};
} // namespace R5900

void recBackpropBSC(u32 code, EEINST* prev, EEINST* pinst);
