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

// used to make sure regs don't get changed while in recompiler
// use FreezeXMMRegs
u32 g_recWriteback = 0;

_xmmregs xmmregs[iREGCNT_XMM], s_saveXMMregs[iREGCNT_XMM];

// X86 caching
_x86regs x86regs[iREGCNT_GPR], s_saveX86regs[iREGCNT_GPR];

// XMM Caching
#define VU_VFx_ADDR(x) (uptr)&VU->VF[x].UL[0]
#define VU_ACCx_ADDR   (uptr)&VU->ACC.UL[0]


alignas(16) u32 xmmBackup[iREGCNT_XMM][4];

alignas(16) u64 gprBackup[iREGCNT_GPR];

static int s_xmmchecknext = 0;

void _backupNeededXMM()
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse)
		{
			xMOVAPS(ptr128[&xmmBackup[i][0]], xRegisterSSE(i));
		}
	}
}

void _restoreNeededXMM()
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse)
		{
			xMOVAPS(xRegisterSSE(i), ptr128[&xmmBackup[i][0]]);
		}
	}
}

void _backupNeededx86()
{
	for (size_t i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse)
		{
			xMOV(ptr64[&gprBackup[i]], xRegister64(i));
		}
	}
}

void _restoreNeededx86()
{
	for (size_t i = 0; i < iREGCNT_GPR; i++)
	{
		if (x86regs[i].inuse)
		{
			xMOV(xRegister64(i), ptr64[&gprBackup[i]]);
		}
	}
}

void _cop2BackupRegs()
{
	_backupNeededx86();
	_backupNeededXMM();
}

void _cop2RestoreRegs()
{
	_restoreNeededx86();
	_restoreNeededXMM();
}
// Clear current register mapping structure
// Clear allocation counter
void _initXMMregs()
{
	memzero(xmmregs);
	g_xmmAllocCounter = 0;
	s_xmmchecknext = 0;
}

// Get a pointer to the physical register (GPR / FPU / VU etc..)
__fi void* _XMMGetAddr(int type, int reg, VURegs* VU)
{
	switch (type)
	{
		case XMMTYPE_VFREG:
			return (void*)VU_VFx_ADDR(reg);

		case XMMTYPE_ACC:
			return (void*)VU_ACCx_ADDR;

		case XMMTYPE_GPRREG:
			if (reg < 32)
				pxAssert(!(g_cpuHasConstReg & (1 << reg)) || (g_cpuFlushedConstReg & (1 << reg)));
			return &cpuRegs.GPR.r[reg].UL[0];

		case XMMTYPE_FPREG:
			return &fpuRegs.fpr[reg];

		case XMMTYPE_FPACC:
			return &fpuRegs.ACC.f;

		jNO_DEFAULT
	}

	return NULL;
}

// Get the index of a free register
// Step1: check any available register (inuse == 0)
// Step2: check registers that are not live (both EEINST_LIVE* are cleared)
// Step3: check registers that won't use SSE in the future (likely broken as EEINST_XMM isn't set properly)
// Step4: take a randome register
//
// Note: I don't understand why we don't check register that aren't useful anymore
// (i.e EEINST_USED is cleared)
int _getFreeXMMreg()
{
	int i, tempi;
	u32 bestcount = 0x10000;

	for (i = 0; (uint)i < iREGCNT_XMM; i++)
	{
		if (xmmregs[(i + s_xmmchecknext) % iREGCNT_XMM].inuse == 0)
		{
			int ret = (s_xmmchecknext + i) % iREGCNT_XMM;
			s_xmmchecknext = (s_xmmchecknext + i + 1) % iREGCNT_XMM;
			return ret;
		}
	}

	// check for dead regs
	for (i = 0; (uint)i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].needed)
			continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG)
		{
			if (!(EEINST_ISLIVEXMM(xmmregs[i].reg)))
			{
				_freeXMMreg(i);
				return i;
			}
		}
	}

	// check for future xmm usage
	for (i = 0; (uint)i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].needed)
			continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG)
		{
			if (!(g_pCurInstInfo->regs[xmmregs[i].reg] & EEINST_XMM))
			{
				_freeXMMreg(i);
				return i;
			}
		}
	}

	tempi = -1;
	bestcount = 0xffff;
	for (i = 0; (uint)i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].needed)
			continue;
		if (xmmregs[i].type != XMMTYPE_TEMP)
		{

			if (xmmregs[i].counter < bestcount)
			{
				tempi = i;
				bestcount = xmmregs[i].counter;
			}
			continue;
		}

		_freeXMMreg(i);
		return i;
	}

	if (tempi != -1)
	{
		_freeXMMreg(tempi);
		return tempi;
	}

	pxFailDev("*PCSX2*: XMM Reg Allocation Error in _getFreeXMMreg()!");
	throw Exception::FailedToAllocateRegister();
}

// Reserve a XMM register for temporary operation.
int _allocTempXMMreg(XMMSSEType type, int xmmreg)
{
	if (xmmreg == -1)
		xmmreg = _getFreeXMMreg();
	else
		_freeXMMreg(xmmreg);

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

			if (!(xmmregs[i].mode & MODE_READ))
			{
				if (mode & MODE_READ)
				{
					xMOVDQA(xRegisterSSE(i), ptr[_XMMGetAddr(xmmregs[i].type, xmmregs[i].reg, xmmregs[i].VU ? &VU1 : &VU0)]);
				}
				else if (mode & MODE_READHALF)
				{
					if (g_xmmtypes[i] == XMMT_INT)
						xMOVQZX(xRegisterSSE(i), ptr[(void*)(uptr)_XMMGetAddr(xmmregs[i].type, xmmregs[i].reg, xmmregs[i].VU ? &VU1 : &VU0)]);
					else
						xMOVL.PS(xRegisterSSE(i), ptr[(void*)(uptr)_XMMGetAddr(xmmregs[i].type, xmmregs[i].reg, xmmregs[i].VU ? &VU1 : &VU0)]);
				}
			}

			xmmregs[i].mode |= mode & ~MODE_READHALF;
			xmmregs[i].counter = g_xmmAllocCounter++; // update counter
			xmmregs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

// Fully allocate a FPU register
// first trial:
//     search an already reserved reg then populate it if we read it
// Second trial:
//     reserve a new reg, then populate it if we read it
//
// Note: FPU are always in XMM register
int _allocFPtoXMMreg(int xmmreg, int fpreg, int mode)
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

	if (xmmreg == -1)
		xmmreg = _getFreeXMMreg();

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

// In short try to allocate a GPR register. Code is an uterly mess
// due to XMM/MMX/X86 crazyness !
int _allocGPRtoXMMreg(int xmmreg, int gprreg, int mode)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;
		if (xmmregs[i].type != XMMTYPE_GPRREG)
			continue;
		if (xmmregs[i].reg != gprreg)
			continue;

		g_xmmtypes[i] = XMMT_INT;

		if (!(xmmregs[i].mode & MODE_READ) && (mode & MODE_READ))
		{
			if (gprreg == 0)
			{
				xPXOR(xRegisterSSE(i), xRegisterSSE(i));
			}
			else
			{
				//pxAssert( !(g_cpuHasConstReg & (1<<gprreg)) || (g_cpuFlushedConstReg & (1<<gprreg)) );
				_flushConstReg(gprreg);
				xMOVDQA(xRegisterSSE(i), ptr[&cpuRegs.GPR.r[gprreg].UL[0]]);
			}
			xmmregs[i].mode |= MODE_READ;
		}

		if ((mode & MODE_WRITE) && (gprreg < 32))
		{
			g_cpuHasConstReg &= ~(1 << gprreg);
			//pxAssert( !(g_cpuHasConstReg & (1<<gprreg)) );
		}

		xmmregs[i].counter = g_xmmAllocCounter++; // update counter
		xmmregs[i].needed = 1;
		xmmregs[i].mode |= mode;
		return i;
	}

	// currently only gpr regs are const
	// fixme - do we really need to execute this both here and in the loop?
	if ((mode & MODE_WRITE) && gprreg < 32)
	{
		//pxAssert( !(g_cpuHasConstReg & (1<<gprreg)) );
		g_cpuHasConstReg &= ~(1 << gprreg);
	}

	if (xmmreg == -1)
		xmmreg = _getFreeXMMreg();

	g_xmmtypes[xmmreg] = XMMT_INT;
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
			// DOX86
			if (mode & MODE_READ)
				_flushConstReg(gprreg);

			xMOVDQA(xRegisterSSE(xmmreg), ptr[&cpuRegs.GPR.r[gprreg].UL[0]]);
		}
	}

	return xmmreg;
}

// Same code as _allocFPtoXMMreg but for the FPU ACC register
// (seriously boy you could have factorized it)
int _allocFPACCtoXMMreg(int xmmreg, int mode)
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

	if (xmmreg == -1)
		xmmreg = _getFreeXMMreg();

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

// Mark reserved GPR reg as needed. It won't be evicted anymore.
// You must use _clearNeededXMMregs to clear the flag
void _addNeededGPRtoXMMreg(int gprreg)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
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
	for (size_t i = 0; i < iREGCNT_XMM; i++)
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
	for (size_t i = 0; i < iREGCNT_XMM; i++)
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
	for (size_t i = 0; i < iREGCNT_XMM; i++)
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
void _deleteGPRtoXMMreg(int reg, int flush)
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{

		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_GPRREG && xmmregs[i].reg == reg)
		{

			switch (flush)
			{
				case 0:
					_freeXMMreg(i);
					break;
				case 1:
				case 2:
					if (xmmregs[i].mode & MODE_WRITE)
					{
						pxAssert(reg != 0);

						//pxAssert( g_xmmtypes[i] == XMMT_INT );
						xMOVDQA(ptr[&cpuRegs.GPR.r[reg].UL[0]], xRegisterSSE(i));

						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}

					if (flush == 2)
						xmmregs[i].inuse = 0;
					break;

				case 3:
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
				case 0:
					_freeXMMreg(i);
					return;

				case 1:
					if (xmmregs[i].mode & MODE_WRITE)
					{
						xMOVSS(ptr[&fpuRegs.fpr[reg].UL], xRegisterSSE(i));
						// get rid of MODE_WRITE since don't want to flush again
						xmmregs[i].mode &= ~MODE_WRITE;
						xmmregs[i].mode |= MODE_READ;
					}
					return;

				case 2:
					xmmregs[i].inuse = 0;
					return;
			}
		}
	}
}

// Free cached register
// Step 1: flush content in memory if MODE_WRITE
// Step 2: clear 'inuse' field
void _freeXMMreg(u32 xmmreg)
{
	pxAssert(xmmreg < iREGCNT_XMM);

	if (!xmmregs[xmmreg].inuse)
		return;

	if (xmmregs[xmmreg].mode & MODE_WRITE)
	{
		switch (xmmregs[xmmreg].type)
		{
			case XMMTYPE_VFREG:
			{
				const VURegs* VU = xmmregs[xmmreg].VU ? &VU1 : &VU0;
				if (xmmregs[xmmreg].mode & MODE_VUXYZ)
				{
					if (xmmregs[xmmreg].mode & MODE_VUZ)
					{
						// don't destroy w
						uint t0reg;
						for (t0reg = 0; t0reg < iREGCNT_XMM; ++t0reg)
						{
							if (!xmmregs[t0reg].inuse)
								break;
						}

						if (t0reg < iREGCNT_XMM)
						{
							xMOVHL.PS(xRegisterSSE(t0reg), xRegisterSSE(xmmreg));
							xMOVL.PS(ptr[(void*)(VU_VFx_ADDR(xmmregs[xmmreg].reg))], xRegisterSSE(xmmreg));
							xMOVSS(ptr[(void*)(VU_VFx_ADDR(xmmregs[xmmreg].reg) + 8)], xRegisterSSE(t0reg));
						}
						else
						{
							// no free reg
							xMOVL.PS(ptr[(void*)(VU_VFx_ADDR(xmmregs[xmmreg].reg))], xRegisterSSE(xmmreg));
							xSHUF.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg), 0xc6);
							//xMOVHL.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg));
							xMOVSS(ptr[(void*)(VU_VFx_ADDR(xmmregs[xmmreg].reg) + 8)], xRegisterSSE(xmmreg));
							xSHUF.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg), 0xc6);
						}
					}
					else
					{
						xMOVL.PS(ptr[(void*)(VU_VFx_ADDR(xmmregs[xmmreg].reg))], xRegisterSSE(xmmreg));
					}
				}
				else
				{
					xMOVAPS(ptr[(void*)(VU_VFx_ADDR(xmmregs[xmmreg].reg))], xRegisterSSE(xmmreg));
				}
			}
			break;

			case XMMTYPE_ACC:
			{
				const VURegs* VU = xmmregs[xmmreg].VU ? &VU1 : &VU0;
				if (xmmregs[xmmreg].mode & MODE_VUXYZ)
				{
					if (xmmregs[xmmreg].mode & MODE_VUZ)
					{
						// don't destroy w
						uint t0reg;

						for (t0reg = 0; t0reg < iREGCNT_XMM; ++t0reg)
						{
							if (!xmmregs[t0reg].inuse)
								break;
						}

						if (t0reg < iREGCNT_XMM)
						{
							xMOVHL.PS(xRegisterSSE(t0reg), xRegisterSSE(xmmreg));
							xMOVL.PS(ptr[(void*)(VU_ACCx_ADDR)], xRegisterSSE(xmmreg));
							xMOVSS(ptr[(void*)(VU_ACCx_ADDR + 8)], xRegisterSSE(t0reg));
						}
						else
						{
							// no free reg
							xMOVL.PS(ptr[(void*)(VU_ACCx_ADDR)], xRegisterSSE(xmmreg));
							xSHUF.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg), 0xc6);
							//xMOVHL.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg));
							xMOVSS(ptr[(void*)(VU_ACCx_ADDR + 8)], xRegisterSSE(xmmreg));
							xSHUF.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg), 0xc6);
						}
					}
					else
					{
						xMOVL.PS(ptr[(void*)(VU_ACCx_ADDR)], xRegisterSSE(xmmreg));
					}
				}
				else
				{
					xMOVAPS(ptr[(void*)(VU_ACCx_ADDR)], xRegisterSSE(xmmreg));
				}
			}
			break;

			case XMMTYPE_GPRREG:
				pxAssert(xmmregs[xmmreg].reg != 0);
				//pxAssert( g_xmmtypes[xmmreg] == XMMT_INT );
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
	xmmregs[xmmreg].mode &= ~(MODE_WRITE | MODE_VUXYZ);
	xmmregs[xmmreg].inuse = 0;
}

void _clearNeededCOP2Regs()
{
	for (size_t i = 0; i < iREGCNT_XMM - 1; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_VFREG)
		{
			xmmregs[i].inuse = false;
			xmmregs[i].type = XMMTYPE_VFREG;
			xmmregs[i].counter = 0;
		}
	}
}

u16 _freeXMMregsCOP2()
{
	// First check what's already free, it might be enough
	for (size_t i = 0; i < iREGCNT_XMM - 1; i++)
	{
		if (!xmmregs[i].inuse)
		{
			xmmregs[i].inuse = true;
			xmmregs[i].type = XMMTYPE_VFREG;
			xmmregs[i].counter = 9999;
			return i;
		}
	}

	// If we still don't have enough, find regs in use but not needed
	for (size_t i = 0; i < iREGCNT_XMM - 1; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].counter == 0)
		{
			_freeXMMreg(i);
			xmmregs[i].inuse = true;
			xmmregs[i].type = XMMTYPE_VFREG;
			xmmregs[i].counter = 9999;
			return i;
		}
	}

	int regtoclear = -1;
	int maxcount = 9999;

	for (size_t i = 0; i < iREGCNT_XMM - 1; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].counter < maxcount)
		{
			regtoclear = i;
			maxcount = xmmregs[i].counter;
		}
	}
	if (regtoclear != -1)
	{
		_freeXMMreg(regtoclear);
		xmmregs[regtoclear].inuse = true;
		xmmregs[regtoclear].type = XMMTYPE_VFREG;
		xmmregs[regtoclear].counter = 9999;
		return regtoclear;
	}

	pxAssert(0);

	return -1;
}

// Return the number of inuse XMM register that have the MODE_WRITE flag
int _getNumXMMwrite()
{
	int num = 0;
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && (xmmregs[i].mode & MODE_WRITE))
			++num;
	}

	return num;
}

// Step1: check any available register (inuse == 0)
// Step2: check registers that are not live (both EEINST_LIVE* are cleared)
// Step3: check registers that are not useful anymore (EEINST_USED cleared)
u8 _hasFreeXMMreg()
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (!xmmregs[i].inuse)
			return 1;
	}

	// check for dead regs
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].needed)
			continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG)
		{
			if (!EEINST_ISLIVEXMM(xmmregs[i].reg))
			{
				return 1;
			}
		}
	}

	// check for dead regs
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].needed)
			continue;
		if (xmmregs[i].type == XMMTYPE_GPRREG)
		{
			if (!(g_pCurInstInfo->regs[xmmregs[i].reg] & EEINST_USED))
			{
				return 1;
			}
		}
	}
	return 0;
}

// Flush in memory all inuse registers but registers are still valid
void _flushXMMregs()
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;

		pxAssert(xmmregs[i].type != XMMTYPE_TEMP);
		pxAssert(xmmregs[i].mode & (MODE_READ | MODE_WRITE));

		_freeXMMreg(i);
		xmmregs[i].inuse = 1;
		xmmregs[i].mode &= ~MODE_WRITE;
		xmmregs[i].mode |= MODE_READ;
	}
}

// Flush in memory all inuse registers. All registers are invalid
void _freeXMMregs()
{
	for (size_t i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse == 0)
			continue;

		pxAssert(xmmregs[i].type != XMMTYPE_TEMP);
		//pxAssert( xmmregs[i].mode & (MODE_READ|MODE_WRITE) );

		_freeXMMreg(i);
	}
}

int _signExtendXMMtoM(uptr to, x86SSERegType from, int candestroy)
{
	g_xmmtypes[from] = XMMT_INT;
	if (candestroy)
	{
		if (g_xmmtypes[from] == XMMT_FPS)
			xMOVSS(ptr[(void*)(to)], xRegisterSSE(from));
		else
			xMOVD(ptr[(void*)(to)], xRegisterSSE(from));

		xPSRA.D(xRegisterSSE(from), 31);
		xMOVD(ptr[(void*)(to + 4)], xRegisterSSE(from));
		return 1;
	}
	else
	{
		// can't destroy and type is int
		pxAssert(g_xmmtypes[from] == XMMT_INT);


		if (_hasFreeXMMreg())
		{
			xmmregs[from].needed = 1;
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(from));
			xPSRA.D(xRegisterSSE(from), 31);
			xMOVD(ptr[(void*)(to)], xRegisterSSE(t0reg));
			xMOVD(ptr[(void*)(to + 4)], xRegisterSSE(from));

			// swap xmm regs.. don't ask
			xmmregs[t0reg] = xmmregs[from];
			xmmregs[from].inuse = 0;
		}
		else
		{
			xMOVD(ptr[(void*)(to + 4)], xRegisterSSE(from));
			xMOVD(ptr[(void*)(to)], xRegisterSSE(from));
			xSAR(ptr32[(u32*)(to + 4)], 31);
		}

		return 0;
	}

	pxAssume(false);
}

// Seem related to the mix between XMM/x86 in order to avoid a couple of move
// But it is quite obscure !!!
int _allocCheckGPRtoXMM(EEINST* pinst, int gprreg, int mode)
{
	if (pinst->regs[gprreg] & EEINST_XMM)
		return _allocGPRtoXMMreg(-1, gprreg, mode);

	return _checkXMMreg(XMMTYPE_GPRREG, gprreg, mode);
}

// Seem related to the mix between XMM/x86 in order to avoid a couple of move
// But it is quite obscure !!!
int _allocCheckFPUtoXMM(EEINST* pinst, int fpureg, int mode)
{
	if (pinst->fpuregs[fpureg] & EEINST_XMM)
		return _allocFPtoXMMreg(-1, fpureg, mode);

	return _checkXMMreg(XMMTYPE_FPREG, fpureg, mode);
}

int _allocCheckGPRtoX86(EEINST* pinst, int gprreg, int mode)
{
	if (pinst->regs[gprreg] & EEINST_USED)
		return _allocX86reg(xEmptyReg, X86TYPE_GPR, gprreg, mode);

	return _checkX86reg(X86TYPE_GPR, gprreg, mode);
}

void _recClearInst(EEINST* pinst)
{
	memzero(*pinst);
	memset8<EEINST_LIVE0 | EEINST_LIVE2>(pinst->regs);
	memset8<EEINST_LIVE0>(pinst->fpuregs);
}

// returns nonzero value if reg has been written between [startpc, endpc-4]
u32 _recIsRegWritten(EEINST* pinst, int size, u8 xmmtype, u8 reg)
{
	u32 inst = 1;

	while (size-- > 0)
	{
        for (size_t i = 0; i < std::size(pinst->writeType); ++i)
		{
			if ((pinst->writeType[i] == xmmtype) && (pinst->writeReg[i] == reg))
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
