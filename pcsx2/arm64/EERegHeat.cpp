// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/EERegHeat.h"

#include "VMManager.h"

#include "common/Console.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

#ifdef _WIN32
#include <process.h>
#define EEREGHEAT_GETPID _getpid
#else
#include <unistd.h>
#define EEREGHEAT_GETPID getpid
#endif

namespace EERegHeat
{
	namespace
	{
		struct Record
		{
			u32 startpc;
			u32 insns;
			u64 exec;
			u16 r64[32];
			u16 w64[32];
			u16 r128[32];
			u16 w128[32];
		};

		// Deque: element addresses stay stable across growth — emitted code
		// holds raw pointers to Record::exec until the next DumpAndReset,
		// which only happens at recResetRaw/recShutdown, after which the
		// pointing code is gone too.
		std::deque<Record> s_records;
		Record* s_open = nullptr;

		// Runaway-SMC backstop; ~272 B/record. Hit only by pathological
		// recompile churn; dumps keep draining the arena at every reset.
		constexpr size_t kMaxRecords = 1u << 20;
		bool s_cap_warned = false;

		bool s_dir_resolved = false;
		std::string s_dir;

		void ResolveDir()
		{
			if (s_dir_resolved)
				return;
			s_dir_resolved = true;
			const char* env = std::getenv("PCSX2_EE_REGHEAT_DIR");
			s_dir = env ? env : "";
		}
	} // namespace

	bool IsEnabled()
	{
		ResolveDir();
		return !s_dir.empty();
	}

	u64* BeginBlock(u32 startpc)
	{
		if (!IsEnabled())
		{
			s_open = nullptr;
			return nullptr;
		}

		if (s_records.size() >= kMaxRecords)
		{
			if (!s_cap_warned)
			{
				s_cap_warned = true;
				Console.Warning("EERegHeat: record cap reached (%zu); dropping further blocks until next dump", kMaxRecords);
			}
			s_open = nullptr;
			return nullptr;
		}

		s_records.push_back(Record());
		s_open = &s_records.back();
		std::memset(s_open, 0, sizeof(Record));
		s_open->startpc = startpc;
		return &s_open->exec;
	}

	void Ref(u32 gpr, bool write, bool is128)
	{
		Record* r = s_open;
		// HI/LO arrive as XMMGPR_HI/LO (32/33) from the MFHI/MTHI/PMFHL
		// backprop paths. They are not pinnable GPRs (HI/LO stay
		// memory-direct under EE-SRA), so exclude rather than alias them
		// onto $zero/$at.
		if (!r || gpr >= 32)
			return;
		u16* arr = is128 ? (write ? r->w128 : r->r128) : (write ? r->w64 : r->r64);
		if (arr[gpr] != 0xFFFF)
			arr[gpr]++;
	}

	void EndBlock(u32 insns)
	{
		if (!s_open)
			return;
		s_open->insns = insns;
		s_open = nullptr;
	}

	void DumpAndReset(const char* reason)
	{
		s_open = nullptr;
		if (s_records.empty())
			return;
		if (!IsEnabled())
		{
			// Disabled mid-flight (test override removed): drop silently.
			s_records.clear();
			return;
		}

		const std::string path = s_dir + "/eeregheat-" + std::to_string(EEREGHEAT_GETPID()) + ".csv";
		std::FILE* f = std::fopen(path.c_str(), "ab");
		if (!f)
		{
			Console.Error("EERegHeat: cannot open '%s' for append; dropping %zu records", path.c_str(), s_records.size());
			s_records.clear();
			return;
		}

		std::fprintf(f, "# eeregheat v1 reason=%s serial=%s crc=%08X records=%zu\n",
			reason ? reason : "?", VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(),
			s_records.size());
		std::fprintf(f, "# B,startpc,insns,exec,r64[0..31],w64[0..31],r128[0..31],w128[0..31]\n");
		for (const Record& r : s_records)
		{
			std::fprintf(f, "B,%08X,%u,%llu", r.startpc, r.insns, (unsigned long long)r.exec);
			for (int i = 0; i < 32; i++)
				std::fprintf(f, ",%u", r.r64[i]);
			for (int i = 0; i < 32; i++)
				std::fprintf(f, ",%u", r.w64[i]);
			for (int i = 0; i < 32; i++)
				std::fprintf(f, ",%u", r.r128[i]);
			for (int i = 0; i < 32; i++)
				std::fprintf(f, ",%u", r.w128[i]);
			std::fputc('\n', f);
		}
		std::fclose(f);
		Console.WriteLn("EERegHeat: appended %zu block records to %s (%s)", s_records.size(), path.c_str(), reason ? reason : "?");
		s_records.clear();
		s_cap_warned = false;
	}

	void OverrideDirForTesting(const char* dir)
	{
		s_open = nullptr;
		if (dir)
		{
			s_dir_resolved = true;
			s_dir = dir;
		}
		else
		{
			s_dir_resolved = false;
			s_dir.clear();
			ResolveDir();
		}
	}

	bool FindRecordForTesting(u32 startpc, RecordView* out)
	{
		for (auto it = s_records.rbegin(); it != s_records.rend(); ++it)
		{
			if (it->startpc != startpc)
				continue;
			out->startpc = it->startpc;
			out->insns = it->insns;
			out->exec = it->exec;
			std::memcpy(out->r64, it->r64, sizeof(out->r64));
			std::memcpy(out->w64, it->w64, sizeof(out->w64));
			std::memcpy(out->r128, it->r128, sizeof(out->r128));
			std::memcpy(out->w128, it->w128, sizeof(out->w128));
			return true;
		}
		return false;
	}
} // namespace EERegHeat
