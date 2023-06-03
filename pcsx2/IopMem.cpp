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
#include "common/AlignedMalloc.h"
#include "R3000A.h"
#include "Common.h"
#include "ps2/pgif.h" // for PSX kernel TTY in iopMemWrite32
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "IopHw.h"

uptr *psxMemWLUT = NULL;
const uptr *psxMemRLUT = NULL;

IopVM_MemoryAllocMess* iopMem = NULL;

alignas(__pagesize) u8 iopHw[Ps2MemSize::IopHardware];

// --------------------------------------------------------------------------------------
//  iopMemoryReserve
// --------------------------------------------------------------------------------------
iopMemoryReserve::iopMemoryReserve()
	: _parent("IOP Main Memory (2mb)")
{
}

iopMemoryReserve::~iopMemoryReserve()
{
	Release();
}

void iopMemoryReserve::Assign(VirtualMemoryManagerPtr allocator)
{
	psxMemWLUT = (uptr*)_aligned_malloc(0x2000 * sizeof(uptr) * 2, 16);
	if (!psxMemWLUT)
		pxFailRel("Failed to allocate IOP memory lookup table");

	psxMemRLUT = psxMemWLUT + 0x2000; //(uptr*)_aligned_malloc(0x10000 * sizeof(uptr),16);

	VtlbMemoryReserve::Assign(std::move(allocator), HostMemoryMap::IOPmemOffset, sizeof(*iopMem));
	iopMem = reinterpret_cast<IopVM_MemoryAllocMess*>(GetPtr());
}

void iopMemoryReserve::Release()
{
	_parent::Release();

	safe_aligned_free(psxMemWLUT);
	psxMemRLUT = nullptr;
	iopMem = nullptr;
}

// Note!  Resetting the IOP's memory state is dependent on having *all* psx memory allocated,
// which is performed by MemInit and PsxMemInit()
void iopMemoryReserve::Reset()
{
	_parent::Reset();

	pxAssert( iopMem );

	DbgCon.WriteLn("IOP resetting main memory...");

	memset(psxMemWLUT, 0, 0x2000 * sizeof(uptr) * 2);	// clears both allocations, RLUT and WLUT

	// Trick!  We're accessing RLUT here through WLUT, since it's the non-const pointer.
	// So the ones with a 0x2000 prefixed are RLUT tables.

	// Map IOP main memory, which is Read/Write, and mirrored three times
	// at 0x0, 0x8000, and 0xa000:
	for (int i=0; i<0x0080; i++)
	{
		psxMemWLUT[i + 0x0000] = (uptr)&iopMem->Main[(i & 0x1f) << 16];

		// RLUTs, accessed through WLUT.
		psxMemWLUT[i + 0x2000] = (uptr)&iopMem->Main[(i & 0x1f) << 16];
	}

	// A few single-page allocations for things we store in special locations.
	psxMemWLUT[0x2000 + 0x1f00] = (uptr)iopMem->P;
	psxMemWLUT[0x2000 + 0x1f80] = (uptr)iopHw;
	//psxMemWLUT[0x1bf80] = (uptr)iopHw;

	psxMemWLUT[0x1f00] = (uptr)iopMem->P;
	psxMemWLUT[0x1f80] = (uptr)iopHw;
	//psxMemWLUT[0xbf80] = (uptr)iopHw;

	// Read-only memory areas, so don't map WLUT for these...
	for (int i = 0; i < 0x0040; i++)
	{
		psxMemWLUT[i + 0x2000 + 0x1fc0] = (uptr)&eeMem->ROM[i << 16];
	}

	for (int i = 0; i < 0x0040; i++)
	{
		psxMemWLUT[i + 0x2000 + 0x1e00] = (uptr)&eeMem->ROM1[i << 16];
	}

	for (int i = 0; i < 0x0008; i++)
	{
		psxMemWLUT[i + 0x2000 + 0x1e40] = (uptr)&eeMem->ROM2[i << 16];
	}

	// sif!! (which is read only? (air))
	psxMemWLUT[0x2000 + 0x1d00] = (uptr)iopMem->Sif;
	//psxMemWLUT[0x1bd00] = (uptr)iopMem->Sif;

	// this one looks like an old hack for some special write-only memory area,
	// but leaving it in for reference (air)
	//for (i=0; i<0x0008; i++) psxMemWLUT[i + 0xbfc0] = (uptr)&psR[i << 16];
}

u8 iopMemRead8(u32 mem)
{
	mem &= 0x1fffffff;
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		switch( mem & 0xf000 )
		{
			case 0x1000: return IopMemory::iopHwRead8_Page1(mem);
			case 0x3000: return IopMemory::iopHwRead8_Page3(mem);
			case 0x8000: return IopMemory::iopHwRead8_Page8(mem);

			default:
				return psxHu8(mem);
		}
	}
	else if (t == 0x1f40)
	{
		return psxHw4Read8(mem);
	}
	else
	{
		const u8* p = (const u8*)(psxMemRLUT[mem >> 16]);
		if (p != NULL)
		{
			return *(const u8 *)(p + (mem & 0xffff));
		}
		else
		{
			if (t == 0x1000)
				return DEV9read8(mem);
			PSXMEM_LOG("err lb %8.8lx", mem);
			return 0;
		}
	}
}

u16 iopMemRead16(u32 mem)
{
	mem &= 0x1fffffff;
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		switch( mem & 0xf000 )
		{
			case 0x1000: return IopMemory::iopHwRead16_Page1(mem);
			case 0x3000: return IopMemory::iopHwRead16_Page3(mem);
			case 0x8000: return IopMemory::iopHwRead16_Page8(mem);

			default:
				return psxHu16(mem);
		}
	}
	else
	{
		const u8* p = (const u8*)(psxMemRLUT[mem >> 16]);
		if (p != NULL)
		{
			if (t == 0x1d00)
			{
				u16 ret;
				switch(mem & 0xF0)
				{
				case 0x00:
					ret= psHu16(SBUS_F200);
					break;
				case 0x10:
					ret= psHu16(SBUS_F210);
					break;
				case 0x40:
					ret= psHu16(SBUS_F240) | 0x0002;
					break;
				case 0x60:
					ret = 0;
					break;
				default:
					ret = psxHu16(mem);
					break;
				}
				//SIF_LOG("Sif reg read %x value %x", mem, ret);
				return ret;
			}
			return *(const u16 *)(p + (mem & 0xffff));
		}
		else
		{
			if (t == 0x1F90)
				return SPU2read(mem);
			if (t == 0x1000)
				return DEV9read16(mem);
			PSXMEM_LOG("err lh %8.8lx", mem);
			return 0;
		}
	}
}

u32 iopMemRead32(u32 mem)
{
	mem &= 0x1fffffff;
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		switch( mem & 0xf000 )
		{
			case 0x1000: return IopMemory::iopHwRead32_Page1(mem);
			case 0x3000: return IopMemory::iopHwRead32_Page3(mem);
			case 0x8000: return IopMemory::iopHwRead32_Page8(mem);

			default:
				return psxHu32(mem);
		}
	} else
	{
		//see also Hw.c
		const u8* p = (const u8*)(psxMemRLUT[mem >> 16]);
		if (p != NULL)
		{
			if (t == 0x1d00)
			{
				u32 ret;
				switch(mem & 0x8F0)
				{
				case 0x00:
					ret= psHu32(SBUS_F200);
					break;
				case 0x10:
					ret= psHu32(SBUS_F210);
					break;
				case 0x20:
					ret= psHu32(SBUS_F220);
					break;
				case 0x30:	// EE Side
					ret= psHu32(SBUS_F230);
					break;
				case 0x40:
					ret= psHu32(SBUS_F240) | 0xF0000002;
					break;
				case 0x60:
					ret = 0;
					break;

				default:
					ret = psxHu32(mem);
					break;
				}
				//SIF_LOG("Sif reg read %x value %x", mem, ret);
				return ret;
			}
			return *(const u32 *)(p + (mem & 0xffff));
		}
		else
		{
			if (t == 0x1000)
				return DEV9read32(mem);
			return 0;
		}
	}
}

void iopMemWrite8(u32 mem, u8 value)
{
	mem &= 0x1fffffff;
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		switch( mem & 0xf000 )
		{
			case 0x1000: IopMemory::iopHwWrite8_Page1(mem,value); break;
			case 0x3000: IopMemory::iopHwWrite8_Page3(mem,value); break;
			case 0x8000: IopMemory::iopHwWrite8_Page8(mem,value); break;

			default:
				psxHu8(mem) = value;
			break;
		}
	}
	else if (t == 0x1f40)
	{
		psxHw4Write8(mem, value);
	}
	else
	{
		u8* p = (u8 *)(psxMemWLUT[mem >> 16]);
		if (p != NULL && !(psxRegs.CP0.n.Status & 0x10000) )
		{
			*(u8  *)(p + (mem & 0xffff)) = value;
			psxCpu->Clear(mem&~3, 1);
		}
		else
		{
			if (t == 0x1d00)
			{
				Console.WriteLn("sw8 [0x%08X]=0x%08X", mem, value);
				psxSu8(mem) = value;
				return;
			}
			if (t == 0x1000)
			{
				DEV9write8(mem, value); return;
			}
			PSXMEM_LOG("err sb %8.8lx = %x", mem, value);
		}
	}
}

void iopMemWrite16(u32 mem, u16 value)
{
	mem &= 0x1fffffff;
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		switch( mem & 0xf000 )
		{
			case 0x1000: IopMemory::iopHwWrite16_Page1(mem,value); break;
			case 0x3000: IopMemory::iopHwWrite16_Page3(mem,value); break;
			case 0x8000: IopMemory::iopHwWrite16_Page8(mem,value); break;

			default:
				psxHu16(mem) = value;
			break;
		}
	} else
	{
		u8* p = (u8 *)(psxMemWLUT[mem >> 16]);
		if (p != NULL && !(psxRegs.CP0.n.Status & 0x10000) )
		{
			if( t==0x1D00 ) Console.WriteLn("sw16 [0x%08X]=0x%08X", mem, value);
			*(u16 *)(p + (mem & 0xffff)) = value;
			psxCpu->Clear(mem&~3, 1);
		}
		else
		{
			if (t == 0x1d00)
			{
				switch (mem & 0x8f0)
				{
					case 0x10:
						// write to ps2 mem
						psHu16(SBUS_F210) = value;
						return;
					case 0x40:
					{
						u32 temp = value & 0xF0;
						// write to ps2 mem
						if(value & 0x20 || value & 0x80)
						{
							psHu16(SBUS_F240) &= ~0xF000;
							psHu16(SBUS_F240) |= 0x2000;
						}


						if(psHu16(SBUS_F240) & temp)
							psHu16(SBUS_F240) &= ~temp;
						else
							psHu16(SBUS_F240) |= temp;
						return;
					}
					case 0x60:
						psHu32(SBUS_F260) = 0;
						return;
				}
#if PSX_EXTRALOGS
				DevCon.Warning("IOP 16 Write to %x value %x", mem, value);
#endif
				psxSu16(mem) = value; return;
			}
			if (t == 0x1F90) {
				SPU2write(mem, value); return;
			}
			if (t == 0x1000) {
				DEV9write16(mem, value); return;
			}
			PSXMEM_LOG("err sh %8.8lx = %x", mem, value);
		}
	}
}

void iopMemWrite32(u32 mem, u32 value)
{
	mem &= 0x1fffffff;
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		switch( mem & 0xf000 )
		{
			case 0x1000: IopMemory::iopHwWrite32_Page1(mem,value); break;
			case 0x3000: IopMemory::iopHwWrite32_Page3(mem,value); break;
			case 0x8000: IopMemory::iopHwWrite32_Page8(mem,value); break;

			default:
				psxHu32(mem) = value;
			break;
		}
	} else
	{
		//see also Hw.c
		u8* p = (u8 *)(psxMemWLUT[mem >> 16]);
		if( p != NULL && !(psxRegs.CP0.n.Status & 0x10000) )
		{
			*(u32 *)(p + (mem & 0xffff)) = value;
			psxCpu->Clear(mem&~3, 1);
		}
		else
		{
			if (t == 0x1d00)
			{
				MEM_LOG("iop Sif reg write %x value %x", mem, value);
				switch (mem & 0x8f0)
				{
					case 0x00:		// EE write path (EE/IOP readable)
						return;		// this is the IOP, so read-only (do nothing)

					case 0x10:		// IOP write path (EE/IOP readable)
						psHu32(SBUS_F210) = value;
						return;

					case 0x20:		// Bits cleared when written from IOP.
						psHu32(SBUS_F220) &= ~value;
						return;

					case 0x30:		// bits set when written from IOP
						psHu32(SBUS_F230) |= value;
						return;

					case 0x40:		// Control Register
					{
						u32 temp = value & 0xF0;
						if (value & 0x20 || value & 0x80)
						{
							psHu32(SBUS_F240) &= ~0xF000;
							psHu32(SBUS_F240) |= 0x2000;
						}


						if (psHu32(SBUS_F240) & temp)
							psHu32(SBUS_F240) &= ~temp;
						else
							psHu32(SBUS_F240) |= temp;
						return;
					}

					case 0x60:
						psHu32(SBUS_F260) = 0;
					return;

				}
#if PSX_EXTRALOGS
				DevCon.Warning("IOP 32 Write to %x value %x", mem, value);
#endif
				psxSu32(mem) = value;

				// wtf?  why were we writing to the EE's sif space?  Commenting this out doesn't
				// break any of my games, and should be more correct, but I guess we'll see.  --air
				//*(u32*)(eeHw+0xf200+(mem&0xf0)) = value;
				return;
			}
			else if (t == 0x1000)
			{
				DEV9write32(mem, value); return;
			}
		}
	}
}

int iopMemSafeCmpBytes(u32 mem, const void* src, u32 size)
{
	// can memcpy so long as pages aren't crossed
	const u8* sptr = static_cast<const u8*>(src);
	const u8* const sptr_end = sptr + size;
	while (sptr != sptr_end)
	{
		u8* dst = iopVirtMemW<u8>(mem);
		if (!dst)
			return -1;

		const u32 remaining_in_page = std::min(0x1000 - (mem & 0xfff), static_cast<u32>(sptr_end - sptr));
		const int res = std::memcmp(sptr, dst, remaining_in_page);
		if (res != 0)
			return res;

		sptr += remaining_in_page;
		mem += remaining_in_page;
	}

	return 0;
}

bool iopMemSafeReadBytes(u32 mem, void* dst, u32 size)
{
	// can memcpy so long as pages aren't crossed
	u8* dptr = static_cast<u8*>(dst);
	u8* const dptr_end = dptr + size;
	while (dptr != dptr_end)
	{
		const u8* src = iopVirtMemR<u8>(mem);
		if (!src)
			return false;

		const u32 remaining_in_page = std::min(0x1000 - (mem & 0xfff), static_cast<u32>(dptr_end - dptr));
		std::memcpy(dptr, src, remaining_in_page);
		dptr += remaining_in_page;
		mem += remaining_in_page;
	}

	return true;
}

bool iopMemSafeWriteBytes(u32 mem, const void* src, u32 size)
{
	// can memcpy so long as pages aren't crossed
	const u8* sptr = static_cast<const u8*>(src);
	const u8* const sptr_end = sptr + size;
	while (sptr != sptr_end)
	{
		u8* dst = iopVirtMemW<u8>(mem);
		if (!dst)
			return false;

		const u32 remaining_in_page = std::min(0x1000 - (mem & 0xfff), static_cast<u32>(sptr_end - sptr));
		std::memcpy(dst, sptr, remaining_in_page);
		sptr += remaining_in_page;
		mem += remaining_in_page;
	}

	return true;
}

std::string iopMemReadString(u32 mem, int maxlen)
{
    std::string ret;
    char c;

    while ((c = iopMemRead8(mem++)) && maxlen--)
        ret.push_back(c);

    return ret;
}
