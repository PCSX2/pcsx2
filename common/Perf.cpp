// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Perf.h"
#include "common/Pcsx2Defs.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"

#ifdef ENABLE_VTUNE
#include "jitprofiling.h"
#endif

#include <array>
#include <cstring>

#ifdef __linux__
#include <atomic>
#include <ctime>
#include <mutex>
#include <elf.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#endif

//#define ProfileWithPerf
//#define ProfileWithPerfJitDump

#if defined(ENABLE_VTUNE) && defined(_WIN32)
#pragma comment(lib, "jitprofiling.lib")
#endif

namespace Perf
{
	Group any("");
	Group ee("EE");
	Group iop("IOP");
	Group vu0("VU0");
	Group vu1("VU1");
	Group vif("VIF");

// Perf is only supported on linux
#if defined(__linux__) && defined(ProfileWithPerf)
	static std::FILE* s_map_file = nullptr;
	static bool s_map_file_opened = false;
	static std::mutex s_mutex;
	static void RegisterMethod(const void* ptr, size_t size, const char* symbol)
	{
		std::unique_lock lock(s_mutex);

		if (!s_map_file)
		{
			if (s_map_file_opened)
				return;

			char file[256];
			snprintf(file, std::size(file), "/tmp/perf-%d.map", getpid());
			s_map_file = std::fopen(file, "wb");
			s_map_file_opened = true;
			if (!s_map_file)
				return;
		}

		std::fprintf(s_map_file, "%" PRIx64 " %zx %s\n", static_cast<u64>(reinterpret_cast<uintptr_t>(ptr)), size, symbol);
		std::fflush(s_map_file);
	}
#elif defined(__linux__) && defined(ProfileWithPerfJitDump)
	enum : u32
	{
		JIT_CODE_LOAD = 0,
		JIT_CODE_MOVE = 1,
		JIT_CODE_DEBUG_INFO = 2,
		JIT_CODE_CLOSE = 3,
		JIT_CODE_UNWINDING_INFO = 4
	};

#pragma pack(push, 1)
	struct JITDUMP_HEADER
	{
		u32 magic = 0x4A695444; // JiTD
		u32 version = 1;
		u32 header_size = sizeof(JITDUMP_HEADER);
		u32 elf_mach;
		u32 pad1 = 0;
		u32 pid;
		u64 timestamp;
		u64 flags = 0;
	};
	struct JITDUMP_RECORD_HEADER
	{
		u32 id;
		u32 total_size;
		u64 timestamp;
	};
	struct JITDUMP_CODE_LOAD
	{
		JITDUMP_RECORD_HEADER header;
		u32 pid;
		u32 tid;
		u64 vma;
		u64 code_addr;
		u64 code_size;
		u64 code_index;
		// name
	};
#pragma pack(pop)

	static u64 JitDumpTimestamp()
	{
		struct timespec ts = {};
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (static_cast<u64>(ts.tv_sec) * 1000000000ULL) + static_cast<u64>(ts.tv_nsec);
	}

	static FILE* s_jitdump_file = nullptr;
	static bool s_jitdump_file_opened = false;
	static std::mutex s_jitdump_mutex;
	static u32 s_jitdump_record_id;

	static void RegisterMethod(const void* ptr, size_t size, const char* symbol)
	{
		const u32 namelen = std::strlen(symbol) + 1;

		std::unique_lock lock(s_jitdump_mutex);
		if (!s_jitdump_file)
		{
			if (!s_jitdump_file_opened)
			{
				char file[256];
				snprintf(file, std::size(file), "jit-%d.dump", getpid());
				s_jitdump_file = fopen(file, "w+b");
				s_jitdump_file_opened = true;
				if (!s_jitdump_file)
					return;
			}

			void* perf_marker = mmap(nullptr, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE, fileno(s_jitdump_file), 0);
			pxAssertRel(perf_marker != MAP_FAILED, "Map perf marker");

			JITDUMP_HEADER jh = {};
#if defined(_M_X86)
			jh.elf_mach = EM_X86_64;
#elif defined(_M_ARM64)
			jh.elf_mach = EM_AARCH64;
#else
#error Unhandled architecture.
#endif
			jh.pid = getpid();
			jh.timestamp = JitDumpTimestamp();
			std::fwrite(&jh, sizeof(jh), 1, s_jitdump_file);
		}

		JITDUMP_CODE_LOAD cl = {};
		cl.header.id = JIT_CODE_LOAD;
		cl.header.total_size = sizeof(cl) + namelen + static_cast<u32>(size);
		cl.header.timestamp = JitDumpTimestamp();
		cl.pid = getpid();
		cl.tid = syscall(SYS_gettid);
		cl.vma = 0;
		cl.code_addr = static_cast<u64>(reinterpret_cast<uintptr_t>(ptr));
		cl.code_size = static_cast<u64>(size);
		cl.code_index = s_jitdump_record_id++;
		std::fwrite(&cl, sizeof(cl), 1, s_jitdump_file);
		std::fwrite(symbol, namelen, 1, s_jitdump_file);
		std::fwrite(ptr, size, 1, s_jitdump_file);
		std::fflush(s_jitdump_file);
	}
#elif defined(ENABLE_VTUNE)
	static void RegisterMethod(const void* ptr, size_t size, const char* symbol)
	{
		iJIT_Method_Load_V2 ml = {};
		ml.method_id = iJIT_GetNewMethodID();
		ml.method_name = const_cast<char*>(symbol);
		ml.method_load_address = const_cast<void*>(ptr);
		ml.method_size = static_cast<unsigned int>(size);
		iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, &ml);
	}
#endif

#if (defined(__linux__) && (defined(ProfileWithPerf) || defined(ProfileWithPerfJitDump))) || defined(ENABLE_VTUNE)
	void Group::Register(const void* ptr, size_t size, const char* symbol)
	{
		char full_symbol[128];
		if (HasPrefix())
			std::snprintf(full_symbol, std::size(full_symbol), "%s_%s", m_prefix, symbol);
		else
			StringUtil::Strlcpy(full_symbol, symbol, std::size(full_symbol));
		RegisterMethod(ptr, size, full_symbol);
	}

	void Group::RegisterPC(const void* ptr, size_t size, u32 pc)
	{
		char full_symbol[128];
		if (HasPrefix())
			std::snprintf(full_symbol, std::size(full_symbol), "%s_%08X", m_prefix, pc);
		else
			std::snprintf(full_symbol, std::size(full_symbol), "%08X", pc);
		RegisterMethod(ptr, size, full_symbol);
	}

	void Group::RegisterKey(const void* ptr, size_t size, const char* prefix, u64 key)
	{
		char full_symbol[128];
		if (HasPrefix())
			std::snprintf(full_symbol, std::size(full_symbol), "%s_%s%016" PRIX64, m_prefix, prefix, key);
		else
			std::snprintf(full_symbol, std::size(full_symbol), "%s%016" PRIX64, prefix, key);
		RegisterMethod(ptr, size, full_symbol);
	}
#else
	void Group::Register(const void* ptr, size_t size, const char* symbol) {}
	void Group::RegisterPC(const void* ptr, size_t size, u32 pc) {}
	void Group::RegisterKey(const void* ptr, size_t size, const char* prefix, u64 key) {}
#endif
} // namespace Perf
