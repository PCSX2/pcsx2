// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 recompiler-toolchain smoke test (Phase 0.6).
//
// Proves the full JIT path end-to-end on Apple Silicon: allocate executable
// (MAP_JIT) memory, emit a tiny ARM64 function through the same VIXL
// MacroAssembler lifecycle the recompilers use (armSetAsmPtr / armStartBlock /
// armEndBlock, which toggle pthread_jit_write_protect and flush the I-cache),
// then call into it and check the result. If this passes, the emission +
// execution toolchain is sound and Phase 1 (the EE skeleton) can build on it.

#include "common/Pcsx2Defs.h"

#if defined(__aarch64__)

#include "arm64/AsmHelpers.h"

#include <gtest/gtest.h>

#include <sys/mman.h>

using namespace vixl::aarch64;

namespace
{
	// RAII executable buffer, allocated exactly like PCSX2's macOS JIT regions
	// (SharedMemoryMappingArea::Create with jit=true): MAP_JIT so that
	// pthread_jit_write_protect_np can flip it between writable and executable.
	class JitBuffer
	{
	public:
		explicit JitBuffer(size_t size)
			: m_size(size)
		{
			int flags = MAP_ANONYMOUS | MAP_PRIVATE;
#ifdef __APPLE__
			flags |= MAP_JIT;
#endif
			void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);
			m_ptr = (ptr == MAP_FAILED) ? nullptr : ptr;
		}

		~JitBuffer()
		{
			if (m_ptr)
				munmap(m_ptr, m_size);
		}

		JitBuffer(const JitBuffer&) = delete;
		JitBuffer& operator=(const JitBuffer&) = delete;

		void* ptr() const { return m_ptr; }
		size_t size() const { return m_size; }

	private:
		void* m_ptr = nullptr;
		size_t m_size;
	};
} // namespace

// int add(int a, int b) { return a + b; }
// Args arrive in w0/w1, return value in w0 (AAPCS64).
TEST(Arm64Emit, AddTwoArgs)
{
	JitBuffer buf(4096);
	ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed (JIT entitlement / hardened runtime?)";

	armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
	u8* const code = armStartBlock();
	armAsm->Add(w0, w0, w1);
	armAsm->Ret();
	armEndBlock();

	using AddFn = int (*)(int, int);
	AddFn fn = reinterpret_cast<AddFn>(code);
	EXPECT_EQ(fn(2, 3), 5);
	EXPECT_EQ(fn(-10, 7), -3);
	EXPECT_EQ(fn(1000000, 2345678), 3345678);
}

// u64 f() { return 0x1234567890ABCDEF; }
// Exercises 64-bit immediate materialization (multi-instruction MOV/MOVK).
TEST(Arm64Emit, ReturnConstant64)
{
	JitBuffer buf(4096);
	ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

	armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
	u8* const code = armStartBlock();
	armAsm->Mov(x0, 0x1234567890ABCDEFULL);
	armAsm->Ret();
	armEndBlock();

	using Fn = u64 (*)();
	Fn fn = reinterpret_cast<Fn>(code);
	EXPECT_EQ(fn(), 0x1234567890ABCDEFULL);
}

// Loads/stores through a pointer argument, mirroring how the recompiler will
// touch guest state in memory: void store(u64* p, u64 v) { *p = v + 1; }
TEST(Arm64Emit, LoadStoreThroughPointer)
{
	JitBuffer buf(4096);
	ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

	armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
	u8* const code = armStartBlock();
	armAsm->Add(x1, x1, 1);
	armAsm->Str(x1, MemOperand(x0));
	armAsm->Ret();
	armEndBlock();

	using StoreFn = void (*)(u64*, u64);
	StoreFn fn = reinterpret_cast<StoreFn>(code);
	u64 slot = 0;
	fn(&slot, 41);
	EXPECT_EQ(slot, 42u);
}

#endif // __aarch64__
