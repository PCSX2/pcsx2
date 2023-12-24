// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "iR5900.h"
#include "iCore.h"

namespace R5900
{
	class AnalysisPass
	{
	public:
		AnalysisPass();
		virtual ~AnalysisPass();

		/// Runs the actual pass.
		virtual void Run(u32 start, u32 end, EEINST* inst_cache);

	protected:
		/// Takes a functor of bool(pc, EEINST*), returning false if iteration should stop.
		template <class F>
		void ForEachInstruction(u32 start, u32 end, EEINST* inst_cache, const F& func);

		/// Dumps the block to the console, calling the functor void(pc, EEINST*, std::string&) for each instruction.
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
