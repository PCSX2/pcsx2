/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
} // namespace R5900