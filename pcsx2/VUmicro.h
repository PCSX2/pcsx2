/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "VU.h"
#include "VUops.h"
#include "R5900.h"

#include "common/Exceptions.h"

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
//  BaseCpuProvider
// --------------------------------------------------------------------------------------
//
// Design Note: This class is only partial C++ style.  It still relies on Alloc and Shutdown
// calls for memory and resource management.  This is because the underlying implementations
// of our CPU emulators don't have properly encapsulated objects yet -- if we allocate ram
// in a constructor, it won't get free'd if an exception occurs during object construction.
// Once we've resolved all the 'dangling pointers' and stuff in the recompilers, Alloc
// and Shutdown can be removed in favor of constructor/destructor syntax.
//
class BaseCpuProvider
{
protected:
	// allocation counter for multiple calls to Reserve.  Most implementations should utilize
	// this variable for sake of robustness.
	std::atomic<int>		m_Reserved;

public:
	// this boolean indicates to some generic logging facilities if the VU's registers
	// are valid for logging or not. (see DisVU1Micro.cpp, etc)  [kinda hacky, might
	// be removed in the future]
	bool	IsInterpreter;

public:
	BaseCpuProvider()
	{
		m_Reserved = 0;
		IsInterpreter = false;
	}

	virtual ~BaseCpuProvider()
	{
		try {
			if( m_Reserved != 0 )
				Console.Warning( "Cleanup miscount detected on CPU provider.  Count=%d", m_Reserved.load() );
		}
		DESTRUCTOR_CATCHALL
	}

	virtual const char* GetShortName() const=0;
	virtual const char* GetLongName() const=0;

	// returns the number of bytes committed to the working caches for this CPU
	// provider (typically this refers to recompiled code caches, but could also refer
	// to other optional growable allocations).
	virtual size_t GetCommittedCache() const
	{
		return 0;
	}

	virtual void Reserve()=0;
	virtual void Shutdown()=0;
	virtual void Reset()=0;
	virtual void SetStartPC(u32 startPC)=0;
	virtual void Execute(u32 cycles)=0;
	virtual void ExecuteBlock(bool startUp)=0;

	virtual void Step()=0;
	virtual void Clear(u32 Addr, u32 Size)=0;

	// C++ Calling Conventions are unstable, and some compilers don't even allow us to take the
	// address of C++ methods.  We need to use a wrapper function to invoke the ExecuteBlock from
	// recompiled code.
	static void ExecuteBlockJIT( BaseCpuProvider* cpu )
	{
		cpu->Execute(1024);
	}

	// Gets the current cache reserve allocated to this CPU (value returned in megabytes)
	virtual uint GetCacheReserve() const=0;
	
	// Specifies the maximum cache reserve amount for this CPU (value in megabytes).
	// CPU providers are allowed to reset their reserves (recompiler resets, etc) if such is
	// needed to conform to the new amount requested.
	virtual void SetCacheReserve( uint reserveInMegs ) const=0;

};

// --------------------------------------------------------------------------------------
//  BaseVUmicroCPU
// --------------------------------------------------------------------------------------
// Layer class for possible future implementation (currently is nothing more than a type-safe
// type define).
//
class BaseVUmicroCPU : public BaseCpuProvider {
public:
	int m_Idx;

	BaseVUmicroCPU() {
		m_Idx		   = 0;
	}
	virtual ~BaseVUmicroCPU() = default;

	virtual void Step() {
		// Ideally this would fall back on interpretation for executing single instructions
		// for all CPU types, but due to VU complexities and large discrepancies between
		// clamping in recs and ints, it's not really worth bothering with yet.
	}

	// Executes a Block based on EE delta time (see VUmicro.cpp)
	virtual void ExecuteBlock(bool startUp=0);

	static void ExecuteBlockJIT(BaseVUmicroCPU* cpu, bool interlocked);

	// VU1 sometimes needs to break execution on XGkick Path1 transfers if
	// there is another gif path 2/3 transfer already taking place.
	// Use this method to resume execution of VU1.
	virtual void ResumeXGkick() {}
};


// --------------------------------------------------------------------------------------
//  InterpVU0 / InterpVU1
// --------------------------------------------------------------------------------------
class InterpVU0 : public BaseVUmicroCPU
{
public:
	InterpVU0();
	virtual ~InterpVU0() { Shutdown(); }

	const char* GetShortName() const	{ return "intVU0"; }
	const char* GetLongName() const		{ return "VU0 Interpreter"; }

	void Reserve() { }
	void Shutdown() noexcept { }
	void Reset();

	void Step();
	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size) {}

	uint GetCacheReserve() const { return 0; }
	void SetCacheReserve( uint reserveInMegs ) const {}
};

class InterpVU1 : public BaseVUmicroCPU
{
public:
	InterpVU1();
	virtual ~InterpVU1() { Shutdown(); }

	const char* GetShortName() const	{ return "intVU1"; }
	const char* GetLongName() const		{ return "VU1 Interpreter"; }

	void Reserve() { }
	void Shutdown() noexcept;
	void Reset();

	void SetStartPC(u32 startPC);
	void Step();
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size) {}
	void ResumeXGkick() {}

	uint GetCacheReserve() const { return 0; }
	void SetCacheReserve( uint reserveInMegs ) const {}
};

// --------------------------------------------------------------------------------------
//  recMicroVU0 / recMicroVU1
// --------------------------------------------------------------------------------------
class recMicroVU0 : public BaseVUmicroCPU
{
public:
	recMicroVU0();
	virtual ~recMicroVU0() { Shutdown(); }

	const char* GetShortName() const	{ return "mVU0"; }
	const char* GetLongName() const		{ return "microVU0 Recompiler"; }

	void Reserve();
	void Shutdown() noexcept;

	void Reset();
	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size);

	uint GetCacheReserve() const;
	void SetCacheReserve( uint reserveInMegs ) const;
};

class recMicroVU1 : public BaseVUmicroCPU
{
public:
	recMicroVU1();
	virtual ~recMicroVU1() { Shutdown(); }

	const char* GetShortName() const	{ return "mVU1"; }
	const char* GetLongName() const		{ return "microVU1 Recompiler"; }

	void Reserve();
	void Shutdown() noexcept;
	void Reset();
	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size);
	void ResumeXGkick();

	uint GetCacheReserve() const;
	void SetCacheReserve( uint reserveInMegs ) const;
};

extern BaseVUmicroCPU* CpuVU0;
extern BaseVUmicroCPU* CpuVU1;


// VU0
extern void vu0ResetRegs();
extern void vu0ExecMicro(u32 addr);
extern void vu0Exec(VURegs* VU);
extern void _vu0FinishMicro();
extern void vu0Finish();
extern void iDumpVU0Registers();

// VU1
extern void vu1Finish(bool add_cycles);
extern void vu1ResetRegs();
extern void vu1ExecMicro(u32 addr);
extern void vu1Exec(VURegs* VU);
extern void iDumpVU1Registers();

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
