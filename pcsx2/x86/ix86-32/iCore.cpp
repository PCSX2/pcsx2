// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "R3000A.h"
#include "VU.h"
#include "Vif.h"
#include "x86/iR3000A.h"
#include "x86/iR5900.h"

#include "common/Console.h"
#include "common/emitter/x86emitter.h"

using namespace x86Emitter;

// yay sloppy crap needed until we can remove dependency on this hippopotamic
// landmass of shared code. (air)
extern u32 g_psxConstRegs[32];

// X86 caching
static uint g_x86checknext;

// use special x86 register allocation for ia32

void _initX86regs()
{
	std::memset(x86regs, 0, sizeof(x86regs));
	g_x86AllocCounter = 0;
	g_x86checknext = 0;
}

int _getFreeX86reg(int mode)
{
	int tempi = -1;
	u32 bestcount = 0x10000;

	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		const int reg = (g_x86checknext + i) % iREGCNT_GPR;
		if (x86regs[reg].inuse || !_isAllocatableX86reg(reg))
			continue;

		if ((mode & MODE_CALLEESAVED) && xRegister32::IsCallerSaved(reg))
			continue;

		if ((mode & MODE_COP2) && mVUIsReservedCOP2(reg))
			continue;

		if (x86regs[reg].inuse == 0)
		{
			g_x86checknext = (reg + 1) % iREGCNT_GPR;
			return reg;
		}
	}

	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (!_isAllocatableX86reg(i))
			continue;

		if ((mode & MODE_CALLEESAVED) && xRegister32::IsCallerSaved(i))
			continue;

		if ((mode & MODE_COP2) && mVUIsReservedCOP2(i))
			continue;

		// should have checked inuse in the previous loop.
		pxAssert(x86regs[i].inuse);

		if (x86regs[i].needed)
			continue;

		if (x86regs[i].type != X86TYPE_TEMP)
		{

			if (x86regs[i].counter < bestcount)
			{
				tempi = static_cast<int>(i);
				bestcount = x86regs[i].counter;
			}
			continue;
		}

		_freeX86reg(i);
		return i;
	}

	if (tempi != -1)
	{
		_freeX86reg(tempi);
		return tempi;
	}

	pxFailRel("x86 register allocation error");
	return -1;
}

void _flushConstReg(int reg)
{
	if (GPR_IS_CONST1(reg) && !(g_cpuFlushedConstReg & (1 << reg)))
	{
		xWriteImm64ToMem(&cpuRegs.GPR.r[reg].UD[0], rax, g_cpuConstRegs[reg].SD[0]);
		g_cpuFlushedConstReg |= (1 << reg);
		if (reg == 0)
			DevCon.Warning("Flushing r0!");
	}
}

void _flushConstRegs()
{
	int zero_reg_count = 0;
	int minusone_reg_count = 0;
	for (u32 i = 0; i < 32; i++)
	{
		if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1u << i))
			continue;

		if (g_cpuConstRegs[i].SD[0] == 0)
			zero_reg_count++;
		else if (g_cpuConstRegs[i].SD[0] == -1)
			minusone_reg_count++;
	}

	// if we have more than one of zero/minus-one, precompute
	bool rax_is_zero = false;
	if (zero_reg_count > 1)
	{
		xXOR(eax, eax);
		for (u32 i = 0; i < 32; i++)
		{
			if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1u << i))
				continue;

			if (g_cpuConstRegs[i].SD[0] == 0)
			{
				xMOV(ptr64[&cpuRegs.GPR.r[i].UD[0]], rax);
				g_cpuFlushedConstReg |= 1u << i;
			}
		}
		rax_is_zero = true;
	}
	if (minusone_reg_count > 1)
	{
		if (!rax_is_zero)
			xMOV(rax, -1);
		else
			xNOT(rax);

		for (u32 i = 0; i < 32; i++)
		{
			if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1u << i))
				continue;

			if (g_cpuConstRegs[i].SD[0] == -1)
			{
				xMOV(ptr64[&cpuRegs.GPR.r[i].UD[0]], rax);
				g_cpuFlushedConstReg |= 1u << i;
			}
		}
	}

	// and whatever's left over..
	for (u32 i = 0; i < 32; i++)
	{
		if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1u << i))
			continue;

		xWriteImm64ToMem(&cpuRegs.GPR.r[i].UD[0], rax, g_cpuConstRegs[i].UD[0]);
		g_cpuFlushedConstReg |= 1u << i;
	}
}

void _validateRegs()
{
#ifdef PCSX2_DEVBUILD
#define MODE_STRING(x) ((((x) & MODE_READ)) ? (((x)&MODE_WRITE) ? "readwrite" : "read") : "write")
	// check that no two registers are in write mode in both fprs and gprs
	for (s8 guestreg = 0; guestreg < 32; guestreg++)
	{
		u32 gprreg = 0, gprmode = 0;
		u32 fprreg = 0, fprmode = 0;
		for (u32 hostreg = 0; hostreg < iREGCNT_GPR; hostreg++)
		{
			if (x86regs[hostreg].inuse && x86regs[hostreg].type == X86TYPE_GPR && x86regs[hostreg].reg == guestreg)
			{
				pxAssertMsg(gprreg == 0 && gprmode == 0, "register is not already allocated in a GPR");
				gprreg = hostreg;
				gprmode = x86regs[hostreg].mode;
			}
		}
		for (u32 hostreg = 0; hostreg < iREGCNT_XMM; hostreg++)
		{
			if (xmmregs[hostreg].inuse && xmmregs[hostreg].type == XMMTYPE_GPRREG && xmmregs[hostreg].reg == guestreg)
			{
				pxAssertMsg(fprreg == 0 && fprmode == 0, "register is not already allocated in a XMM");
				fprreg = hostreg;
				fprmode = xmmregs[hostreg].mode;
			}
		}

		if ((gprmode | fprmode) & MODE_WRITE)
			pxAssertMsg((gprmode & MODE_WRITE) != (fprmode & MODE_WRITE), "only one of gpr or fps is in write state");

		if (gprmode & MODE_WRITE)
			pxAssertMsg(fprmode == 0, "when writing to the gpr, fpr is invalid");
		if (fprmode & MODE_WRITE)
			pxAssertMsg(gprmode == 0, "when writing to the fpr, gpr is invalid");
	}
#undef MODE_STRING
#endif
}

int _allocX86reg(int type, int reg, int mode)
{
	if (type == X86TYPE_GPR || type == X86TYPE_PSX)
	{
		pxAssertMsg(reg >= 0 && reg < 34, "Register index out of bounds.");
	}

	int hostXMMreg = (type == X86TYPE_GPR) ? _checkXMMreg(XMMTYPE_GPRREG, reg, 0) : -1;
	if (type != X86TYPE_TEMP)
	{
		for (int i = 0; i < static_cast<int>(iREGCNT_GPR); i++)
		{
			if (!x86regs[i].inuse || x86regs[i].type != type || x86regs[i].reg != reg)
				continue;

			pxAssert(type != X86TYPE_GPR || !GPR_IS_CONST1(reg) || (GPR_IS_CONST1(reg) && g_cpuFlushedConstReg & (1u << reg)));

			// can't go from write to read
			pxAssert(!((x86regs[i].mode & (MODE_READ | MODE_WRITE)) == MODE_WRITE && (mode & (MODE_READ | MODE_WRITE)) == MODE_READ));
			// if (type != X86TYPE_TEMP && !(x86regs[i].mode & MODE_READ) && (mode & MODE_READ))

			if (type == X86TYPE_GPR)
			{
				RALOG("Changing host reg %d for guest reg %d from %s to %s mode\n", i, reg, GetModeString(x86regs[i].mode), GetModeString(x86regs[i].mode | mode));

				if (mode & MODE_WRITE)
				{
					if (GPR_IS_CONST1(reg))
					{
						RALOG("Clearing constant value for guest reg %d on change to write mode\n", reg);
						GPR_DEL_CONST(reg);
					}

					if (hostXMMreg >= 0)
					{
						// ensure upper bits get written
						RALOG("Invalidating host XMM reg %d for guest reg %d due to GPR write transition\n", hostXMMreg, reg);
						pxAssert(!(xmmregs[hostXMMreg].mode & MODE_WRITE));
						_freeXMMreg(hostXMMreg);
					}
				}
			}
			else if (type == X86TYPE_PSX)
			{
				RALOG("Changing host reg %d for guest PSX reg %d from %s to %s mode\n", i, reg, GetModeString(x86regs[i].mode), GetModeString(x86regs[i].mode | mode));

				if (mode & MODE_WRITE)
				{
					if (PSX_IS_CONST1(reg))
					{
						RALOG("Clearing constant value for guest PSX reg %d on change to write mode\n", reg);
						PSX_DEL_CONST(reg);
					}
				}
			}
			else if (type == X86TYPE_VIREG)
			{
				// keep VI temporaries separate
				if (reg < 0)
					continue;
			}

			x86regs[i].counter = g_x86AllocCounter++;
			x86regs[i].mode |= mode & ~MODE_CALLEESAVED;
			x86regs[i].needed = true;
			return i;
		}
	}

	const int regnum = _getFreeX86reg(mode);
	xRegister64 new_reg(regnum);
	x86regs[regnum].type = type;
	x86regs[regnum].reg = reg;
	x86regs[regnum].mode = mode & ~MODE_CALLEESAVED;
	x86regs[regnum].counter = g_x86AllocCounter++;
	x86regs[regnum].needed = true;
	x86regs[regnum].inuse = true;

	if (type == X86TYPE_GPR)
	{
		RALOG("Allocating host reg %d to guest reg %d in %s mode\n", regnum, reg, GetModeString(mode));
	}

	if (mode & MODE_READ)
	{
		switch (type)
		{
			case X86TYPE_GPR:
			{
				if (reg == 0)
				{
					xXOR(xRegister32(new_reg), xRegister32(new_reg)); // 32-bit is smaller and zexts anyway
				}
				else
				{
					if (hostXMMreg >= 0)
					{
						// is in a XMM. we don't need to free the XMM since we're not writing, and it's still valid
						RALOG("Copying %d from XMM %d to GPR %d on read\n", reg, hostXMMreg, regnum);
						xMOVD(new_reg, xRegisterSSE(hostXMMreg)); // actually MOVQ

						// if the XMM was dirty, just get rid of it, we don't want to try to sync the values up...
						if (xmmregs[hostXMMreg].mode & MODE_WRITE)
						{
							RALOG("Freeing dirty XMM %d for GPR %d\n", hostXMMreg, reg);
							_freeXMMreg(hostXMMreg);
						}
					}
					else if (GPR_IS_CONST1(reg))
					{
						xMOV64(new_reg, g_cpuConstRegs[reg].SD[0]);
						g_cpuFlushedConstReg |= (1u << reg);
						x86regs[regnum].mode |= MODE_WRITE; // reg is dirty

						RALOG("Writing constant value %lld from guest reg %d to host reg %d\n", g_cpuConstRegs[reg].SD[0], reg, regnum);
					}
					else
					{
						// not loaded
						RALOG("Loading guest reg %d to GPR %d\n", reg, regnum);
						xMOV(new_reg, ptr64[&cpuRegs.GPR.r[reg].UD[0]]);
					}
				}
			}
			break;

			case X86TYPE_FPRC:
				RALOG("Loading guest reg FPCR %d to GPR %d\n", reg, regnum);
				xMOV(xRegister32(regnum), ptr32[&fpuRegs.fprc[reg]]);
				break;

			case X86TYPE_PSX:
			{
				const xRegister32 new_reg32(regnum);
				if (reg == 0)
				{
					xXOR(new_reg32, new_reg32);
				}
				else
				{
					if (PSX_IS_CONST1(reg))
					{
						xMOV(new_reg32, g_psxConstRegs[reg]);
						g_psxFlushedConstReg |= (1u << reg);
						x86regs[regnum].mode |= MODE_WRITE; // reg is dirty

						RALOG("Writing constant value %d from guest PSX reg %d to host reg %d\n", g_psxConstRegs[reg], reg, regnum);
					}
					else
					{
						RALOG("Loading guest PSX reg %d to GPR %d\n", reg, regnum);
						xMOV(new_reg32, ptr32[&psxRegs.GPR.r[reg]]);
					}
				}
			}
			break;

			case X86TYPE_VIREG:
			{
				RALOG("Loading guest VI reg %d to GPR %d", reg, regnum);
				xMOVZX(xRegister32(regnum), ptr16[&VU0.VI[reg].US[0]]);
			}
			break;

			default:
				abort();
				break;
		}
	}

	if (type == X86TYPE_GPR && (mode & MODE_WRITE))
	{
		if (reg < 32 && GPR_IS_CONST1(reg))
		{
			RALOG("Clearing constant value for guest reg %d on write allocation\n", reg);
			GPR_DEL_CONST(reg);
		}
		if (hostXMMreg >= 0)
		{
			// writing, so kill the xmm allocation. gotta ensure the upper bits gets stored first.
			RALOG("Invalidating %d from XMM %d because of GPR %d write\n", reg, hostXMMreg, regnum);
			_freeXMMreg(hostXMMreg);
		}
	}
	else if (type == X86TYPE_PSX && (mode & MODE_WRITE))
	{
		if (reg < 32 && PSX_IS_CONST1(reg))
		{
			RALOG("Clearing constant value for guest PSX reg %d on write allocation\n", reg);
			PSX_DEL_CONST(reg);
		}
	}

	// Console.WriteLn("Allocating reg %d", regnum);
	return regnum;
}

void _writebackX86Reg(int x86reg)
{
	switch (x86regs[x86reg].type)
	{
		case X86TYPE_GPR:
			RALOG("Writing back GPR reg %d for guest reg %d P2\n", x86reg, x86regs[x86reg].reg);
			xMOV(ptr64[&cpuRegs.GPR.r[x86regs[x86reg].reg].UD[0]], xRegister64(x86reg));
			break;

		case X86TYPE_FPRC:
			RALOG("Writing back GPR reg %d for guest reg FPCR %d P2\n", x86reg, x86regs[x86reg].reg);
			xMOV(ptr32[&fpuRegs.fprc[x86regs[x86reg].reg]], xRegister32(x86reg));
			break;

		case X86TYPE_VIREG:
			RALOG("Writing back VI reg %d for guest reg %d P2\n", x86reg, x86regs[x86reg].reg);
			xMOV(ptr16[&VU0.VI[x86regs[x86reg].reg].UL], xRegister16(x86reg));
			break;

		case X86TYPE_PCWRITEBACK:
			RALOG("Writing back PC writeback in host reg %d\n", x86reg);
			xMOV(ptr32[&cpuRegs.pcWriteback], xRegister32(x86reg));
			break;

		case X86TYPE_PSX:
			RALOG("Writing back PSX GPR reg %d for guest reg %d P2\n", x86reg, x86regs[x86reg].reg);
			xMOV(ptr32[&psxRegs.GPR.r[x86regs[x86reg].reg]], xRegister32(x86reg));
			break;

		case X86TYPE_PSX_PCWRITEBACK:
			RALOG("Writing back PSX PC writeback in host reg %d\n", x86reg);
			xMOV(ptr32[&psxRegs.pcWriteback], xRegister32(x86reg));
			break;

		default:
			abort();
			break;
	}
}

int _checkX86reg(int type, int reg, int mode)
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type)
		{
			// shouldn't have dirty constants...
			pxAssert((type != X86TYPE_GPR || !GPR_IS_DIRTY_CONST(reg)) &&
					 (type != X86TYPE_PSX || !PSX_IS_DIRTY_CONST(reg)));

			if ((type == X86TYPE_GPR || type == X86TYPE_PSX) && !(x86regs[i].mode & MODE_READ) && (mode & MODE_READ))
				pxFailRel("Somehow ended up with an allocated x86 without mode");

			// ensure constants get deleted once we alloc as write
			if (mode & MODE_WRITE)
			{
				if (type == X86TYPE_GPR)
				{
					// go through the alloc path instead, because we might need to invalidate an xmm.
					return _allocX86reg(X86TYPE_GPR, reg, mode);
				}
				else if (type == X86TYPE_PSX)
				{
					pxAssert(!PSX_IS_DIRTY_CONST(reg));
					PSX_DEL_CONST(reg);
				}
			}

			x86regs[i].mode |= mode;
			x86regs[i].counter = g_x86AllocCounter++;
			x86regs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

void _addNeededX86reg(int type, int reg)
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (!x86regs[i].inuse || x86regs[i].reg != reg || x86regs[i].type != type)
			continue;

		x86regs[i].counter = g_x86AllocCounter++;
		x86regs[i].needed = 1;
	}
}

void _clearNeededX86regs()
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].needed)
		{
			if (x86regs[i].inuse && (x86regs[i].mode & MODE_WRITE))
				x86regs[i].mode |= MODE_READ;
		}
		x86regs[i].needed = 0;
	}
}

void _freeX86reg(const x86Emitter::xRegister32& x86reg)
{
	_freeX86reg(x86reg.GetId());
}

void _freeX86reg(int x86reg)
{
	pxAssert(x86reg >= 0 && x86reg < (int)iREGCNT_GPR);

	if (x86regs[x86reg].inuse && (x86regs[x86reg].mode & MODE_WRITE))
	{
		_writebackX86Reg(x86reg);
		x86regs[x86reg].mode &= ~MODE_WRITE;
	}

	_freeX86regWithoutWriteback(x86reg);
}

void _freeX86regWithoutWriteback(int x86reg)
{
	pxAssert(x86reg >= 0 && x86reg < (int)iREGCNT_GPR);

	x86regs[x86reg].inuse = 0;

	if (x86regs[x86reg].type == X86TYPE_VIREG)
	{
		RALOG("Freeing VI reg %d in host GPR %d\n", x86regs[x86reg].reg, x86reg);
		mVUFreeCOP2GPR(x86reg);
	}
	else if (x86regs[x86reg].inuse && x86regs[x86reg].type == X86TYPE_GPR)
	{
		RALOG("Freeing X86 register %d (was guest %d)...\n", x86reg, x86regs[x86reg].reg);
	}
	else if (x86regs[x86reg].inuse)
	{
		RALOG("Freeing X86 register %d...\n", x86reg);
	}
}

void _freeX86regs()
{
	for (uint i = 0; i < iREGCNT_GPR; i++)
		_freeX86reg(i);
}

void _flushX86regs()
{
	for (u32 i = 0; i < iREGCNT_GPR; ++i)
	{
		if (x86regs[i].inuse && x86regs[i].mode & MODE_WRITE)
		{
			// shouldn't be const, because if we got to write mode, we should've flushed then
			pxAssert(x86regs[i].type != X86TYPE_GPR || !GPR_IS_DIRTY_CONST(x86regs[i].reg));

			RALOG("Flushing x86 reg %u in _eeFlushAllDirty()\n", i);
			_writebackX86Reg(i);
			x86regs[i].mode = (x86regs[i].mode & ~MODE_WRITE) | MODE_READ;
		}
	}
}
