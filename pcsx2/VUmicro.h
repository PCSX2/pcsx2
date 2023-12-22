// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "VU.h"
#include "VUops.h"
#include "R5900.h"

static const uint VU0_MEMSIZE	= 0x1000;		// 4kb
static const uint VU0_PROGSIZE	= 0x1000;		// 4kb
static const uint VU1_MEMSIZE	= 0x4000;		// 16kb
static const uint VU1_PROGSIZE	= 0x4000;		// 16kb

static const uint VU0_MEMMASK	= VU0_MEMSIZE-1;
static const uint VU0_PROGMASK	= VU0_PROGSIZE-1;
static const uint VU1_MEMMASK	= VU1_MEMSIZE-1;
static const uint VU1_PROGMASK	= VU1_PROGSIZE-1;

#define vu1RunCycles (3000000) // mVU1 uses this for inf loop detection on dev builds


// --------------------------------------------------------------------------------------
//  BaseVUmicroCPU
// --------------------------------------------------------------------------------------
// Layer class for possible future implementation (currently is nothing more than a type-safe
// type define).
//
class BaseVUmicroCPU
{
public:
	int m_Idx = 0;

	// this boolean indicates to some generic logging facilities if the VU's registers
	// are valid for logging or not. (see DisVU1Micro.cpp, etc)  [kinda hacky, might
	// be removed in the future]
	bool	IsInterpreter;

public:
	BaseVUmicroCPU()
	{
		IsInterpreter = false;
	}

	virtual ~BaseVUmicroCPU() = default;

	virtual const char* GetShortName() const=0;
	virtual const char* GetLongName() const=0;

	// returns the number of bytes committed to the working caches for this CPU
	// provider (typically this refers to recompiled code caches, but could also refer
	// to other optional growable allocations).
	virtual size_t GetCommittedCache() const
	{
		return 0;
	}

	virtual void Shutdown()=0;
	virtual void Reset()=0;
	virtual void SetStartPC(u32 startPC)=0;
	virtual void Execute(u32 cycles)=0;

	virtual void Step()=0;
	virtual void Clear(u32 Addr, u32 Size)=0;

	// Executes a Block based on EE delta time (see VUmicro.cpp)
	void ExecuteBlock(bool startUp = 0);

	// C++ Calling Conventions are unstable, and some compilers don't even allow us to take the
	// address of C++ methods.  We need to use a wrapper function to invoke the ExecuteBlock from
	// recompiled code.
	static void ExecuteBlockJIT(BaseVUmicroCPU* cpu, bool interlocked);

	// VU1 sometimes needs to break execution on XGkick Path1 transfers if
	// there is another gif path 2/3 transfer already taking place.
	// Use this method to resume execution of VU1.
	virtual void ResumeXGkick() {}
};

// --------------------------------------------------------------------------------------
//  InterpVU0 / InterpVU1
// --------------------------------------------------------------------------------------
class InterpVU0 final : public BaseVUmicroCPU
{
public:
	InterpVU0();
	~InterpVU0() override { Shutdown(); }

	const char* GetShortName() const override { return "intVU0"; }
	const char* GetLongName() const override { return "VU0 Interpreter"; }

	void Shutdown() override {}
	void Reset() override;

	void Step() override;
	void SetStartPC(u32 startPC) override;
	void Execute(u32 cycles) override;
	void Clear(u32 addr, u32 size) override {}
};

class InterpVU1 final : public BaseVUmicroCPU
{
public:
	InterpVU1();
	~InterpVU1() override { Shutdown(); }

	const char* GetShortName() const override { return "intVU1"; }
	const char* GetLongName() const	override { return "VU1 Interpreter"; }

	void Shutdown() override {}
	void Reset() override;

	void SetStartPC(u32 startPC) override;
	void Step() override;
	void Execute(u32 cycles) override;
	void Clear(u32 addr, u32 size) override {}
	void ResumeXGkick() override {}
};

// --------------------------------------------------------------------------------------
//  recMicroVU0 / recMicroVU1
// --------------------------------------------------------------------------------------
class recMicroVU0 final : public BaseVUmicroCPU
{
public:
	recMicroVU0();
	~recMicroVU0() override { Shutdown(); }

	const char* GetShortName() const override { return "mVU0"; }
	const char* GetLongName() const override { return "microVU0 Recompiler"; }

	void Reserve();
	void Shutdown() override;

	void Reset() override;
	void Step() override;
	void SetStartPC(u32 startPC) override;
	void Execute(u32 cycles) override;
	void Clear(u32 addr, u32 size) override;
};

class recMicroVU1 final : public BaseVUmicroCPU
{
public:
	recMicroVU1();
	virtual ~recMicroVU1() { Shutdown(); }

	const char* GetShortName() const override { return "mVU1"; }
	const char* GetLongName() const override { return "microVU1 Recompiler"; }

	void Reserve();
	void Shutdown() override;
	void Reset() override;
	void Step() override;
	void SetStartPC(u32 startPC) override;
	void Execute(u32 cycles) override;
	void Clear(u32 addr, u32 size) override;
	void ResumeXGkick() override;
};

extern InterpVU0 CpuIntVU0;
extern InterpVU1 CpuIntVU1;

extern recMicroVU0 CpuMicroVU0;
extern recMicroVU1 CpuMicroVU1;

extern BaseVUmicroCPU* CpuVU0;
extern BaseVUmicroCPU* CpuVU1;


// VU0
extern void vu0ResetRegs();
extern void vu0ExecMicro(u32 addr);
extern void vu0Exec(VURegs* VU);
extern void _vu0FinishMicro();
extern void vu0Finish();

// VU1
extern void vu1Finish(bool add_cycles);
extern void vu1ResetRegs();
extern void vu1ExecMicro(u32 addr);
extern void vu1Exec(VURegs* VU);
extern void MTVUInterrupt();

#ifdef VUM_LOG

#define IdebugUPPER(VU) \
	VUM_LOG("(VU%d) %s", VU.IsVU1(), dis##VU##MicroUF(VU.code, VU.VI[REG_TPC].UL));
#define IdebugLOWER(VU) \
	VUM_LOG("(VU%d) %s", VU.IsVU1(), dis##VU##MicroLF(VU.code, VU.VI[REG_TPC].UL));
#define _vuExecMicroDebug(VU) \
	VUM_LOG("(VU%d) _vuExecMicro: %8.8x", VU.IsVU1(), VU.VI[REG_TPC].UL);

#else

#define IdebugUPPER(VU)
#define IdebugLOWER(VU)
#define _vuExecMicroDebug(VU)

#endif
