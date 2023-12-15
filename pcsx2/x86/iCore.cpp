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


#include "PrecompiledHeader.h"

#include "System.h"
#include "iR5900.h"
#include "Vif.h"
#include "VU.h"
#include "R3000A.h"

using namespace x86Emitter;

thread_local u8* j8Ptr[32];
thread_local u32* j32Ptr[32];

u16 g_x86AllocCounter = 0;
u16 g_xmmAllocCounter = 0;

EEINST* g_pCurInstInfo = NULL;

_xmmregs xmmregs[iREGCNT_XMM], s_saveXMMregs[iREGCNT_XMM];

// X86 caching
_x86regs x86regs[iREGCNT_GPR], s_saveX86regs[iREGCNT_GPR];

// Clear current register mapping structure
// Clear allocation counter
void _initXMMregs()
{
	std::memset(xmmregs, 0, sizeof(xmmregs));
	g_xmmAllocCounter = 0;
}

bool _isAllocatableX86reg(int x86reg)
{
	// we use rax, rcx and rdx as scratch (they have special purposes...)
	if (x86reg <= 2)
		return false;

	// We keep the first two argument registers free.
	// On windows, this is ecx/edx, and it's taken care of above, but on Linux, it uses rsi/rdi.
	// The issue is when we do a load/store, the address register overlaps a cached register.
	// TODO(Stenzek): Rework loadstores to handle this and allow caching.
	if (x86reg == arg1reg.GetId() || x86reg == arg2reg.GetId())
		return false;

	// arg3reg is also used for dispatching without fastmem
	if (!CHECK_FASTMEM && x86reg == arg3reg.GetId())
		return false;

	// rbp is used as the fastmem base
	if (CHECK_FASTMEM && x86reg == 5)
		return false;

#ifdef ENABLE_VTUNE
	// vtune needs ebp...
	if (!CHECK_FASTMEM && x86reg == 5)
		return false;
#endif

	// rsp is never allocatable..
	if (x86reg == 4)
		return false;

	return true;
}

bool _hasX86reg(int type, int reg, int required_mode /*= 0*/)
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse && x86regs[i].type == type && x86regs[i].reg == reg)
		{
			return ((x86regs[i].mode & required_mode) == required_mode);
		}
	}

	return false;
}

// Get the index of a free register
// Step1: check any available register (inuse == 0)
// Step2: check registers that are not live (both EEINST_LIVE* are cleared)
// Step3: check registers that won't use SSE in the future (likely broken as EEINST_XMM isn't set properly)
// Step4: take a randome register
//
// Note: I don't understand why we don't check register that aren't useful anymore
// (i.e EEINST_USED is cleared)
int _getFreeXMMreg(u32 maxreg)
{
	int i, tempi;
	u32 bestcount = 0x10000;

	// check for free registers
	for (i = 0; (uint)i < maxreg; i++)
	{
		if (!xmmregs[i].inuse)
			return i;
	}

	// check for dead regs
	tempi = -1;
	bestcount = 0xffff;
	for (i = 0; (uint)i < maxreg; i++)
	{
		pxAssert(xmmregs[i].inuse);
		if (xmmregs[i].needed)
			continue;

		// temps should be needed
		pxAssert(xmmregs[i].type != XMMTYPE_TEMP);

		if (xmmregs[i].counter < bestcount)
		{
			switch (xmmregs[i].type)
			{
				case XMMTYPE_GPRREG:
				{
					if (EEINST_USEDTEST(xmmregs[i].reg))
						continue;
				}
				break;

				case XMMTYPE_FPREG:
				{
					if (FPUINST_USEDTEST(xmmregs[i].reg))
						continue;
				}
				break;

				case XMMTYPE_VFREG:
				{
					if (EEINST_VFUSEDTEST(xmmregs[i].reg))
						continue;
				}
				break;
			}

			tempi = i;
			bestcount = xmmregs[i].counter;
		}
	}
	if (tempi != -1)
	{
		_freeXMMreg(tempi);
		return tempi;
	}

	// lastly, try without the used check
	bestcount = 0xffff;
	for (i = 0; (uint)i < maxreg; i++)
	{
		pxAssert(xmmregs[i].inuse);
		if (xmmregs[i].needed)
			continue;

		if (xmmregs[i].counter < bestcount)
		{
			tempi = i;
			bestcount = xmmregs[i].counter;
		}
	}

	if (tempi != -1)
	{
		_freeXMMreg(tempi);
		return tempi;
	}

	pxFailRel("*PCSX2*: XMM Reg Allocation Error in _getFreeXMMreg()!");
	return -1;
}

// Reserve a XMM register for temporary operation.
int _allocTempXMMreg(XMMSSEType type)
{
	const int xmmreg = _getFreeXMMreg();
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_TEMP;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;
	g_xmmtypes[xmmreg] = type;
	return xmmreg;
}

// Search register "reg" of type "type" which is inuse
// If register doesn't have the read flag but mode is read
// then populate the register from the memory
// Note: There is a special HALF mode (to handle low 64 bits copy) but it seems to be unused
//
// So basically it is mostly used to set the mode of the register, and load value if we need to read it
int _checkXMMreg(int type, int reg, int mode)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && (xmmregs[i].type == (type & 0xff)) && (xmmregs[i].reg == reg))
		{
			// shouldn't have dirty constants...
			pxAssert(type != XMMTYPE_GPRREG || !GPR_IS_DIRTY_CONST(reg));

			if (type == XMMTYPE_GPRREG && !(xmmregs[i].mode & (MODE_READ | MODE_WRITE)) && (mode & MODE_READ))
				pxFailRel("Somehow ended up with an allocated xmm without mode");

			if (type == XMMTYPE_GPRREG && (mode & MODE_WRITE))
			{
				// go through the alloc path instead, because we might need to invalidate a gpr.
				return _allocGPRtoXMMreg(reg, mode);
			}

			xmmregs[i].mode |= mode;
			xmmregs[i].counter = g_xmmAllocCounter++; // update counter
			xmmregs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

bool _hasXMMreg(int type, int reg, int required_mode /*= 0*/)
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == type && xmmregs[i].reg == reg)
		{
			return ((xmmregs[i].mode & required_mode) == required_mode);
		}
	}

	return false;
}

// Fully allocate a FPU register
// first trial:
//     search an already reserved reg then populate it if we read it
// Second trial:
//     reserve a new reg, then populate it if we read it
//
// Note: FPU are always in XMM register
int _allocFPtoXMMreg(int fpreg, int mode)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;
		if (xmmregs[i].type != XMMTYPE_FPREG)
			continue;
		if (xmmregs[i].reg != fpreg)
			continue;

		if (!(xmmregs[i].mode & MODE_READ) && (mode & MODE_READ))
		{
			xMOVSSZX(xRegisterSSE(i), ptr[&fpuRegs.fpr[fpreg].f]);
			xmmregs[i].mode |= MODE_READ;
		}

		g_xmmtypes[i] = XMMT_FPS;
		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode |= mode;
		return i;
	}

	const int xmmreg = _getFreeXMMreg();

	g_xmmtypes[xmmreg] = XMMT_FPS;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_FPREG;
	xmmregs[xmmreg].reg = fpreg;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;

	if (mode & MODE_READ)
		xMOVSSZX(xRegisterSSE(xmmreg), ptr[&fpuRegs.fpr[fpreg].f]);

	return xmmreg;
}

int _allocGPRtoXMMreg(int gprreg, int mode)
{
#define MODE_STRING(x) ((((x) & MODE_READ)) ? (((x)&MODE_WRITE) ? "readwrite" : "read") : "write")

	// is this already in a gpr?
	const int hostx86reg = _checkX86reg(X86TYPE_GPR, gprreg, MODE_READ);

	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (!xmmregs[i].inuse || xmmregs[i].type != XMMTYPE_GPRREG || xmmregs[i].reg != gprreg)
			continue;

		if (!(xmmregs[i].mode & (MODE_READ | MODE_WRITE)) && (mode & MODE_READ))
			pxFailRel("Somehow ended up with an allocated register without mode");

		if (mode & MODE_WRITE && hostx86reg >= 0)
		{
			RALOG("Invalidating cached guest GPR reg %d in host reg GPR %d due to XMM transition\n", gprreg, hostx86reg);
			x86regs[hostx86reg].inuse = 0;
		}

		if (mode & MODE_WRITE)
		{
			if (GPR_IS_CONST1(gprreg))
			{
				RALOG("Clearing constant value for guest GPR reg %d on XMM reconfig\n", gprreg);
				GPR_DEL_CONST(gprreg);
			}
			if (hostx86reg >= 0)
			{
				// x86 register should be up to date, because if it was written, it should've been invalidated
				pxAssert(!(x86regs[hostx86reg].mode & MODE_WRITE));
				_freeX86regWithoutWriteback(hostx86reg);
			}
		}

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = true;
		xmmregs[i].mode |= mode;
		return i;
	}

	const int xmmreg = _getFreeXMMreg();
	RALOG("Allocating host XMM %d to guest GPR %d in %s mode\n", xmmreg, gprreg, GetModeString(mode));

	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_GPRREG;
	xmmregs[xmmreg].reg = gprreg;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;

	if (mode & MODE_READ)
	{
		if (gprreg == 0)
		{
			xPXOR(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg));
		}
		else
		{
			if (GPR_IS_CONST1(gprreg))
			{
				RALOG("Writing constant value %lld from guest reg %d to host XMM reg %d\n", g_cpuConstRegs[gprreg].SD[0], gprreg, xmmreg);

				// load lower+upper, replace lower
				xMOVDQA(xRegisterSSE(xmmreg), ptr128[&cpuRegs.GPR.r[gprreg].UQ]);
				xMOV64(rax, g_cpuConstRegs[gprreg].SD[0]);
				xPINSR.Q(xRegisterSSE(xmmreg), rax, 0);
				xmmregs[xmmreg].mode |= MODE_WRITE; // reg is dirty
				g_cpuFlushedConstReg |= (1u << gprreg);

				// kill any gpr allocation which is dirty, since it's a constant value
				if (hostx86reg >= 0)
				{
					RALOG("Invalidating guest reg %d in GPR %d due to constant value write to XMM %d\n", gprreg, hostx86reg, xmmreg);
					x86regs[hostx86reg].inuse = 0;
				}
			}
			else if (hostx86reg >= 0)
			{
				RALOG("Copying (for guest reg %d) host GPR %d to XMM %d\n", gprreg, hostx86reg, xmmreg);

				// load lower+upper, replace lower if dirty
				xMOVDQA(xRegisterSSE(xmmreg), ptr128[&cpuRegs.GPR.r[gprreg].UQ]);

				// if the gpr was written to (dirty), we need to invalidate it
				if (x86regs[hostx86reg].mode & MODE_WRITE)
				{
					RALOG("Moving dirty guest reg %d from GPR %d to XMM %d\n", gprreg, hostx86reg, xmmreg);
					xPINSR.Q(xRegisterSSE(xmmreg), xRegister64(hostx86reg), 0);
					_freeX86regWithoutWriteback(hostx86reg);
					xmmregs[xmmreg].mode |= MODE_WRITE;
				}
			}
			else
			{
				// not loaded
				RALOG("Loading guest reg %d to host FPR %d\n", gprreg, xmmreg);
				xMOVDQA(xRegisterSSE(xmmreg), ptr128[&cpuRegs.GPR.r[gprreg].UQ]);
			}
		}
	}

	if (mode & MODE_WRITE && gprreg < 32 && GPR_IS_CONST1(gprreg))
	{
		RALOG("Clearing constant value for guest GPR reg %d on XMM alloc\n", gprreg);
		GPR_DEL_CONST(gprreg);
	}
	if (mode & MODE_WRITE && hostx86reg >= 0)
	{
		RALOG("Invalidating cached guest GPR reg %d in host reg GPR %d due to XMM transition\n", gprreg, hostx86reg);
		_freeX86regWithoutWriteback(hostx86reg);
	}

	return xmmreg;
#undef MODE_STRING
}

// Same code as _allocFPtoXMMreg but for the FPU ACC register
// (seriously boy you could have factorized it)
int _allocFPACCtoXMMreg(int mode)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;
		if (xmmregs[i].type != XMMTYPE_FPACC)
			continue;

		if (!(xmmregs[i].mode & MODE_READ) && (mode & MODE_READ))
		{
			xMOVSSZX(xRegisterSSE(i), ptr[&fpuRegs.ACC.f]);
			xmmregs[i].mode |= MODE_READ;
		}

		g_xmmtypes[i] = XMMT_FPS;
		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode |= mode;
		return i;
	}

	const int xmmreg = _getFreeXMMreg();

	g_xmmtypes[xmmreg] = XMMT_FPS;
	xmmregs[xmmreg].inuse = 1;
	xmmregs[xmmreg].type = XMMTYPE_FPACC;
	xmmregs[xmmreg].mode = mode;
	xmmregs[xmmreg].needed = 1;
	xmmregs[xmmreg].reg = 0;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;

	if (mode & MODE_READ)
	{
		xMOVSSZX(xRegisterSSE(xmmreg), ptr[&fpuRegs.ACC.f]);
	}

	return xmmreg;
}

void _reallocateXMMreg(int xmmreg, int newtype, int newreg, int newmode, bool writeback /*= true*/)
{
	pxAssert(xmmreg >= 0 && xmmreg <= static_cast<int>(iREGCNT_XMM));
	_xmmregs& xr = xmmregs[xmmreg];
	if (writeback)
		_freeXMMreg(xmmreg);

	xr.inuse = true;
	xr.type = newtype;
	xr.reg = newreg;
	xr.mode = newmode;
	xr.needed = true;
}

// Mark reserved GPR reg as needed. It won't be evicted anymore.
// You must use _clearNeededXMMregs to clear the flag
void _addNeededGPRtoX86reg(int gprreg)
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse == 0)
			continue;
		if (x86regs[i].type != X86TYPE_GPR)
			continue;
		if (x86regs[i].reg != gprreg)
			continue;

		x86regs[i].counter = g_x86AllocCounter++; // update counter
		x86regs[i].needed = 1;
		break;
	}
}

void _addNeededPSXtoX86reg(int gprreg)
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse == 0)
			continue;
		if (x86regs[i].type != X86TYPE_PSX)
			continue;
		if (x86regs[i].reg != gprreg)
			continue;

		x86regs[i].counter = g_x86AllocCounter++; // update counter
		x86regs[i].needed = 1;
		break;
	}
}

void _addNeededGPRtoXMMreg(int gprreg)
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;
		if (xmmregs[i].type != XMMTYPE_GPRREG)
			continue;
		if (xmmregs[i].reg != gprreg)
			continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

// Mark reserved FPU reg as needed. It won't be evicted anymore.
// You must use _clearNeededXMMregs to clear the flag
void _addNeededFPtoXMMreg(int fpreg)
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;
		if (xmmregs[i].type != XMMTYPE_FPREG)
			continue;
		if (xmmregs[i].reg != fpreg)
			continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

// Mark reserved FPU ACC reg as needed. It won't be evicted anymore.
// You must use _clearNeededXMMregs to clear the flag
void _addNeededFPACCtoXMMreg()
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;
		if (xmmregs[i].type != XMMTYPE_FPACC)
			continue;

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		break;
	}
}

// Clear needed flags of all registers
// Written register will set MODE_READ (aka data is valid, no need to load it)
void _clearNeededXMMregs()
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{

		if (xmmregs[i].needed)
		{

			// setup read to any just written regs
			if (xmmregs[i].inuse && (xmmregs[i].mode & MODE_WRITE))
				xmmregs[i].mode |= MODE_READ;
			xmmregs[i].needed = 0;
		}

		if (xmmregs[i].inuse)
		{
			pxAssert(xmmregs[i].type != XMMTYPE_TEMP);
		}
	}
}

// Flush is 0: _freeXMMreg. Flush in memory if MODE_WRITE. Clear inuse
// Flush is 1: Flush in memory. But register is still valid
// Flush is 2: like 0 ...
// Flush is 3: drop register content
void _deleteGPRtoX86reg(int reg, int flush)
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{
		if (x86regs[i].inuse && x86regs[i].type == X86TYPE_GPR && x86regs[i].reg == reg)
		{
			switch (flush)
			{
				case DELETE_REG_FREE:
					_freeX86reg(i);
					break;
				case DELETE_REG_FLUSH:
				case DELETE_REG_FLUSH_AND_FREE:
					if (x86regs[i].mode & MODE_WRITE)
					{
						pxAssert(reg != 0);
						xMOV(ptr64[&cpuRegs.GPR.r[reg].UL[0]], xRegister64(i));

						// get rid of MODE_WRITE since don't want to flush again
						x86regs[i].mode &= ~MODE_WRITE;
						x86regs[i].mode |= MODE_READ;
					}

					if (flush == DELETE_REG_FLUSH_AND_FREE)
						x86regs[i].inuse = 0;
					break;

				case DELETE_REG_FREE_NO_WRITEBACK:
					x86regs[i].inuse = 0;
					break;
			}

			return;
		}
	}
}

void _deletePSXtoX86reg(int reg, int flush)
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse && x86regs[i].type == X86TYPE_PSX && x86regs[i].reg == reg)
		{

			switch (flush)
			{
				case DELETE_REG_FREE:
					_freeX86reg(i);
					break;
				case DELETE_REG_FLUSH:
				case DELETE_REG_FLUSH_AND_FREE:
					if (x86regs[i].mode & MODE_WRITE)
					{
						pxAssert(reg != 0);
						xMOV(ptr32[&psxRegs.GPR.r[reg]], xRegister32(i));

						// get rid of MODE_WRITE since don't want to flush again
						x86regs[i].mode &= ~MODE_WRITE;
						x86regs[i].mode |= MODE_READ;

						RALOG("Writing back X86 reg %d for guest PSX reg %d P2\n", i, x86regs[i].reg);
					}

					if (flush == 2)
						x86regs[i].inuse = 0;
					break;

				case DELETE_REG_FREE_NO_WRITEBACK:
					x86regs[i].inuse = 0;
					break;
			}

			return;
		}
	}
}

void _deleteGPRtoXMMreg(int reg, int flush)
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{

		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_GPRREG && xmmregs[i].reg == reg)
		{

			switch (flush)
			{
				case DELETE_REG_FREE:
					_freeXMMreg(i);
					break;
				case DELETE_REG_FLUSH:
				case DELETE_REG_FLUSH_AND_FREE:
					if (xmmregs[i].mode & MODE_WRITE)
					{
						pxAssert(reg != 0);

						//pxAssert( g_xmmtypes[i] == XMMT_INT );
						xMOVDQA(ptr[&cpuRegs.GPR.r[reg].UL[0]], xRegisterSSE(i));

						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}

					if (flush == DELETE_REG_FLUSH_AND_FREE)
						xmmregs[i].inuse = 0;
					break;

				case DELETE_REG_FREE_NO_WRITEBACK:
					xmmregs[i].inuse = 0;
					break;
			}

			return;
		}
	}
}

// Flush is 0: _freeXMMreg. Flush in memory if MODE_WRITE. Clear inuse
// Flush is 1: Flush in memory. But register is still valid
// Flush is 2: drop register content
void _deleteFPtoXMMreg(int reg, int flush)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_FPREG && xmmregs[i].reg == reg)
		{
			switch (flush)
			{
				case DELETE_REG_FREE:
				case DELETE_REG_FLUSH_AND_FREE:
					_freeXMMreg(i);
					return;

				case DELETE_REG_FLUSH:
					if (xmmregs[i].mode & MODE_WRITE)
					{
						xMOVSS(ptr[&fpuRegs.fpr[reg].UL], xRegisterSSE(i));
						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}
					return;

				case DELETE_REG_FREE_NO_WRITEBACK:
					xmmregs[i].inuse = 0;
					return;
			}
		}
	}
}

void _writebackXMMreg(int xmmreg)
{
	switch (xmmregs[xmmreg].type)
	{
		case XMMTYPE_VFREG:
		{
			if (xmmregs[xmmreg].reg == 33)
				xMOVSS(ptr[&VU0.VI[REG_I].F], xRegisterSSE(xmmreg));
			else if (xmmregs[xmmreg].reg == 32)
				xMOVAPS(ptr[VU0.ACC.F], xRegisterSSE(xmmreg));
			else if (xmmregs[xmmreg].reg > 0)
				xMOVAPS(ptr[VU0.VF[xmmregs[xmmreg].reg].F], xRegisterSSE(xmmreg));
		}
		break;

		case XMMTYPE_GPRREG:
			pxAssert(xmmregs[xmmreg].reg != 0);
			xMOVDQA(ptr[&cpuRegs.GPR.r[xmmregs[xmmreg].reg].UL[0]], xRegisterSSE(xmmreg));
			break;

		case XMMTYPE_FPREG:
			xMOVSS(ptr[&fpuRegs.fpr[xmmregs[xmmreg].reg]], xRegisterSSE(xmmreg));
			break;

		case XMMTYPE_FPACC:
			xMOVSS(ptr[&fpuRegs.ACC.f], xRegisterSSE(xmmreg));
			break;

		default:
			break;
	}
}

// Free cached register
// Step 1: flush content in memory if MODE_WRITE
// Step 2: clear 'inuse' field
void _freeXMMreg(int xmmreg)
{
	pxAssert(static_cast<uint>(xmmreg) < iREGCNT_XMM);
	if (!xmmregs[xmmreg].inuse)
		return;

	if (xmmregs[xmmreg].mode & MODE_WRITE)
		_writebackXMMreg(xmmreg);

	xmmregs[xmmreg].mode = 0;
	xmmregs[xmmreg].inuse = 0;

	if (xmmregs[xmmreg].type == XMMTYPE_VFREG)
		mVUFreeCOP2XMMreg(xmmreg);
}

void _freeXMMregWithoutWriteback(int xmmreg)
{
	pxAssert(static_cast<uint>(xmmreg) < iREGCNT_XMM);
	if (!xmmregs[xmmreg].inuse)
		return;

	xmmregs[xmmreg].mode = 0;
	xmmregs[xmmreg].inuse = 0;

	if (xmmregs[xmmreg].type == XMMTYPE_VFREG)
		mVUFreeCOP2XMMreg(xmmreg);
}

int _allocVFtoXMMreg(int vfreg, int mode)
{
	// mode == 0 is called by the microvu side, and we don't want to clash with its temps...
	if (mode != 0)
	{
		for (uint i = 0; i < iREGCNT_XMM; i++)
		{
			if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_VFREG && xmmregs[i].reg == vfreg)
			{
				pxAssert(mode == 0 || xmmregs[i].mode != 0);
				xmmregs[i].counter = g_xmmAllocCounter++;
				xmmregs[i].mode |= mode;
				return i;
			}
		}
	}

	// -1 here because we don't want to allocate PQ.
	const int xmmreg = _getFreeXMMreg(iREGCNT_XMM - 1);
	xmmregs[xmmreg].inuse = true;
	xmmregs[xmmreg].type = XMMTYPE_VFREG;
	xmmregs[xmmreg].counter = g_xmmAllocCounter++;
	xmmregs[xmmreg].needed = true;
	xmmregs[xmmreg].reg = vfreg;
	xmmregs[xmmreg].mode = mode;

	if (mode & MODE_READ)
	{
		if (vfreg == 33)
			xMOVSSZX(xRegisterSSE(xmmreg), ptr[&VU0.VI[REG_I].F]);
		else if (vfreg == 32)
			xMOVAPS(xRegisterSSE(xmmreg), ptr[VU0.ACC.F]);
		else
			xMOVAPS(xRegisterSSE(xmmreg), ptr[VU0.VF[xmmregs[xmmreg].reg].F]);
	}

	return xmmreg;
}

void _flushCOP2regs()
{
	for (uint i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_VFREG)
		{
			RALOG("Flushing cop2 fpr %u with vf%u\n", i, xmmregs[i].reg);
			_freeXMMreg(i);
		}
	}
}

void _flushXMMreg(int xmmreg)
{
	if (xmmregs[xmmreg].inuse && xmmregs[xmmreg].mode & MODE_WRITE)
	{
		RALOG("Flushing xmm reg %u in _flushXMMregs()\n", i);
		_writebackXMMreg(xmmreg);
		xmmregs[xmmreg].mode = (xmmregs[xmmreg].mode & ~MODE_WRITE) | MODE_READ;
	}
}

// Flush in memory all inuse registers but registers are still valid
void _flushXMMregs()
{
	for (u32 i = 0; i < iREGCNT_XMM; ++i)
		_flushXMMreg(i);
}

int _allocIfUsedGPRtoX86(int gprreg, int mode)
{
	const int x86reg = _checkX86reg(X86TYPE_GPR, gprreg, mode);
	if (x86reg >= 0)
		return x86reg;

	return EEINST_USEDTEST(gprreg) ? _allocX86reg(X86TYPE_GPR, gprreg, mode) : -1;
}

int _allocIfUsedVItoX86(int vireg, int mode)
{
	const int x86reg = _checkX86reg(X86TYPE_VIREG, vireg, mode);
	if (x86reg >= 0)
		return x86reg;

	// Prefer not to stop on COP2 reserved registers here.
	return EEINST_VIUSEDTEST(vireg) ? _allocX86reg(X86TYPE_VIREG, vireg, mode | MODE_COP2) : -1;
}

int _allocIfUsedGPRtoXMM(int gprreg, int mode)
{
	const int mmreg = _checkXMMreg(XMMTYPE_GPRREG, gprreg, mode);
	if (mmreg >= 0)
		return mmreg;

	return EEINST_XMMUSEDTEST(gprreg) ? _allocGPRtoXMMreg(gprreg, mode) : -1;
}

int _allocIfUsedFPUtoXMM(int fpureg, int mode)
{
	const int mmreg = _checkXMMreg(XMMTYPE_FPREG, fpureg, mode);
	if (mmreg >= 0)
		return mmreg;

	return FPUINST_USEDTEST(fpureg) ? _allocFPtoXMMreg(fpureg, mode) : -1;
}

void _recClearInst(EEINST* pinst)
{
	// we set everything as being live to begin with, since it needs to be written at the end of the block
	std::memset(pinst, 0, sizeof(EEINST));
	std::memset(pinst->regs, EEINST_LIVE, sizeof(pinst->regs));
	std::memset(pinst->fpuregs, EEINST_LIVE, sizeof(pinst->fpuregs));
	std::memset(pinst->vfregs, EEINST_LIVE, sizeof(pinst->vfregs));
	std::memset(pinst->viregs, EEINST_LIVE, sizeof(pinst->viregs));
}

// returns nonzero value if reg has been written between [startpc, endpc-4]
u32 _recIsRegReadOrWritten(EEINST* pinst, int size, u8 xmmtype, u8 reg)
{
	u32 inst = 1;

	while (size-- > 0)
	{
		for (u32 i = 0; i < std::size(pinst->writeType); ++i)
		{
			if ((pinst->writeType[i] == xmmtype) && (pinst->writeReg[i] == reg))
				return inst;
		}

		for (u32 i = 0; i < std::size(pinst->readType); ++i)
		{
			if ((pinst->readType[i] == xmmtype) && (pinst->readReg[i] == reg))
				return inst;
		}

		++inst;
		pinst++;
	}

	return 0;
}

void _recFillRegister(EEINST& pinst, int type, int reg, int write)
{
	if (write)
	{
		for (size_t i = 0; i < std::size(pinst.writeType); ++i)
		{
			if (pinst.writeType[i] == XMMTYPE_TEMP)
			{
				pinst.writeType[i] = type;
				pinst.writeReg[i] = reg;
				return;
			}
		}
		pxAssume(false);
	}
	else
	{
		for (size_t i = 0; i < std::size(pinst.readType); ++i)
		{
			if (pinst.readType[i] == XMMTYPE_TEMP)
			{
				pinst.readType[i] = type;
				pinst.readReg[i] = reg;
				return;
			}
		}
		pxAssume(false);
	}
}
