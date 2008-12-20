/*
	EE physical map :
	[0000 0000,1000 0000) -> Ram (mirrored ?)
	[1000 0000,1400 0000) -> Registers
	[1400 0000,1fc0 0000) -> Reserved (ingored writes, 'random' reads)
	[1fc0 0000,2000 0000) -> Boot ROM

	[2000 0000,4000 0000) -> Unmapped (BUS ERROR)
	[4000 0000,8000 0000) -> "Extended memory", probably unmapped (BUS ERROR) on retail ps2's :)
	[8000 0000,FFFF FFFF] -> Unmapped (BUS ERROR)

	vtlb/phy only supports the [0000 0000,2000 0000) region, with 4k pages.
	vtlb/vmap supports mapping to either of these locations, or some other (externaly) specified address.
*/

#ifndef PCSX2_VIRTUAL_MEM

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/stat.h>

#include "Common.h"
#include "vtlb.h"
#include "x86/ix86/ix86.h"

#define verify(x) {if (!(x)) { (*(u8*)0)=3; }}

static const uint VTLB_PAGE_BITS =12;
static const uint VTLB_PAGE_MASK=(4095);
static const uint VTLB_PAGE_SIZE=(4096);

static const uint VTLB_PMAP_ITEMS=(0x20000000/VTLB_PAGE_SIZE);
static const uint VTLB_PMAP_SZ=0x20000000;
static const uint VTLB_VMAP_ITEMS=(0x100000000ULL/VTLB_PAGE_SIZE);
static s32 pmap[VTLB_PMAP_ITEMS];	//512KB
static s32 vmap[VTLB_VMAP_ITEMS];   //4MB

//5 -> one for each size
//2 -> read/write
//
void* RWFT[5][2][128];
vtlbHandler vtlbHandlerCount=0;

vtlbHandler DefaultPhyHandler;
vtlbHandler UnmappedVirtHandler0;
vtlbHandler UnmappedVirtHandler1;
vtlbHandler UnmappedPhyHandler0;
vtlbHandler UnmappedPhyHandler1;


	/*
	__asm
	{
		mov eax,ecx;
		shr ecx,12;
		mov ecx,[ecx*4+vmap];	//translate
		add ecx,eax;			//transform
		
		js callfunction;		//if <0 its invalid ptr :)

		mov eax,[ecx];
		mov [edx],eax;
		xor eax,eax;
		ret;

callfunction:
		xchg eax,ecx;
		shr eax,12; //get the 'ppn'

		//ecx = original addr
		//eax = function entry + 0x800000
		//edx = data ptr
		jmp [readfunctions8-0x800000+eax];
	}*/


template<int DataSize,typename DataType>
__forceinline int __fastcall MemOp_r(u32 addr, DataType* data)
{
	u32 vmv=vmap[addr>>VTLB_PAGE_BITS];
	s32 ppf=addr+vmv;

	if (!(ppf<0))
	{
		data[0]=*reinterpret_cast<DataType*>(ppf);
		if (DataSize==128)
			data[1]=*reinterpret_cast<DataType*>(ppf+8);
		return 0;
	}
	else
	{	
		//has to: translate, find function, call function
		u32 hand=(u8)vmv;
		u32 paddr=ppf-hand+0x80000000;
		//SysPrintf("Translted 0x%08X to 0x%08X\n",addr,paddr);
		//return reinterpret_cast<TemplateHelper<DataSize,false>::HandlerType*>(RWFT[TemplateHelper<DataSize,false>::sidx][0][hand])(paddr,data);

		switch( DataSize )
		{
			case 8: return ((vltbMemRFP*)RWFT[0][0][hand])(paddr, data);
			case 16: return ((vltbMemRFP*)RWFT[1][0][hand])(paddr, data);
			case 32: return ((vltbMemRFP*)RWFT[2][0][hand])(paddr, data);
			case 64: return ((vltbMemRFP*)RWFT[3][0][hand])(paddr, data);
			case 128: return ((vltbMemRFP*)RWFT[4][0][hand])(paddr, data);

			jNO_DEFAULT;
		}
	}
}


template<int DataSize,typename DataType>
__forceinline void __fastcall MemOp_w0(u32 addr, DataType data)
{
	u32 vmv=vmap[addr>>VTLB_PAGE_BITS];
	s32 ppf=addr+vmv;
	if (!(ppf<0))
	{
		*reinterpret_cast<DataType*>(ppf)=data;
	}
	else
	{	
		//has to: translate, find function, call function
		u32 hand=(u8)vmv;
		u32 paddr=ppf-hand+0x80000000;
		//SysPrintf("Translted 0x%08X to 0x%08X\n",addr,paddr);

		switch( DataSize )
		{
			case 8: return ((vltbMemW8FP*)RWFT[0][1][hand])(paddr, (u8)data);
			case 16: return ((vltbMemW16FP*)RWFT[1][1][hand])(paddr, (u16)data);
			case 32: return ((vltbMemW32FP*)RWFT[2][1][hand])(paddr, (u32)data);

			jNO_DEFAULT;
		}
	}
}
template<int DataSize,typename DataType>
__forceinline void __fastcall MemOp_w1(u32 addr,const DataType* data)
{
	verify(DataSize==128 || DataSize==64);
	u32 vmv=vmap[addr>>VTLB_PAGE_BITS];
	s32 ppf=addr+vmv;
	if (!(ppf<0))
	{
		*reinterpret_cast<DataType*>(ppf)=*data;
		if (DataSize==128)
			*reinterpret_cast<DataType*>(ppf+8)=data[1];
	}
	else
	{	
		//has to: translate, find function, call function
		u32 hand=(u8)vmv;
		u32 paddr=ppf-hand+0x80000000;
		//SysPrintf("Translted 0x%08X to 0x%08X\n",addr,paddr);
		switch( DataSize )
		{
			case 64: return ((vltbMemW64FP*)RWFT[3][1][hand])(paddr, data);
			case 128: return ((vltbMemW128FP*)RWFT[4][1][hand])(paddr, data);

			jNO_DEFAULT;
		}
	}
}
int __fastcall vtlb_memRead8(u32 mem, u8  *out)
{
	return MemOp_r<8,u8>(mem,out);
}
int __fastcall vtlb_memRead16(u32 mem, u16 *out)
{
	return MemOp_r<16,u16>(mem,out);
}
int __fastcall vtlb_memRead32(u32 mem, u32 *out)
{
	return MemOp_r<32,u32>(mem,out);
}
int __fastcall vtlb_memRead64(u32 mem, u64 *out)
{
	return MemOp_r<64,u64>(mem,out);
}
int __fastcall vtlb_memRead128(u32 mem, u64 *out)
{
	return MemOp_r<128,u64>(mem,out);
}
void __fastcall vtlb_memWrite8 (u32 mem, u8  value)
{
	MemOp_w0<8,u8>(mem,value);
}
void __fastcall vtlb_memWrite16(u32 mem, u16 value)
{
	MemOp_w0<16,u16>(mem,value);
}
void __fastcall vtlb_memWrite32(u32 mem, u32 value)
{
	MemOp_w0<32,u32>(mem,value);
}
void __fastcall vtlb_memWrite64(u32 mem, const u64* value)
{
	MemOp_w1<64,u64>(mem,value);
}
void __fastcall vtlb_memWrite128(u32 mem, const u64 *value)
{
	MemOp_w1<128,u64>(mem,value);
}

static __forceinline int vtlb_Miss(u32 addr,u32 mode)
{
	SysPrintf("vtlb miss : addr 0x%X, mode %d\n",addr,mode);
	verify(false);
	if (mode==0)
		cpuTlbMissR(addr, cpuRegs.branch);
	else
		cpuTlbMissW(addr, cpuRegs.branch);
	
	return -1;
}
static __forceinline int vtlb_BusError(u32 addr,u32 mode)
{
	SysPrintf("vtlb bus error : addr 0x%X, mode %d\n",addr,mode);
	verify(false);
	return -1;
}
/////
template<u32 saddr>
int __fastcall vtlbUnmappedVRead8(u32 addr,mem8_t* data) { return vtlb_Miss(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedVRead16(u32 addr,mem16_t* data)  { return vtlb_Miss(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedVRead32(u32 addr,mem32_t* data) { return vtlb_Miss(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedVRead64(u32 addr,mem64_t* data) { return vtlb_Miss(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedVRead128(u32 addr,mem128_t* data) { return vtlb_Miss(addr|saddr,0); }
template<u32 saddr>
void __fastcall vtlbUnmappedVWrite8(u32 addr,mem8_t data) { vtlb_Miss(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedVWrite16(u32 addr,mem16_t data) { vtlb_Miss(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedVWrite32(u32 addr,mem32_t data) { vtlb_Miss(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedVWrite64(u32 addr,const mem64_t* data) { vtlb_Miss(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedVWrite128(u32 addr,const mem128_t* data) { vtlb_Miss(addr|saddr,1); }
/////
template<u32 saddr>
int __fastcall vtlbUnmappedPRead8(u32 addr,mem8_t* data) { return vtlb_BusError(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedPRead16(u32 addr,mem16_t* data)  { return vtlb_BusError(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedPRead32(u32 addr,mem32_t* data) { return vtlb_BusError(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedPRead64(u32 addr,mem64_t* data) { return vtlb_BusError(addr|saddr,0); }
template<u32 saddr>
int __fastcall vtlbUnmappedPRead128(u32 addr,mem128_t* data) { return vtlb_BusError(addr|saddr,0); }
template<u32 saddr>
void __fastcall vtlbUnmappedPWrite8(u32 addr,mem8_t data) { vtlb_BusError(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedPWrite16(u32 addr,mem16_t data) { vtlb_BusError(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedPWrite32(u32 addr,mem32_t data) { vtlb_BusError(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedPWrite64(u32 addr,const mem64_t* data) { vtlb_BusError(addr|saddr,1); }
template<u32 saddr>
void __fastcall vtlbUnmappedPWrite128(u32 addr,const mem128_t* data) { vtlb_BusError(addr|saddr,1); }
/////
int __fastcall vtlbDefaultPhyRead8(u32 addr,mem8_t* data) { SysPrintf("vtlbDefaultPhyRead8: 0x%X\n",addr); verify(false); return -1; }
int __fastcall vtlbDefaultPhyRead16(u32 addr,mem16_t* data)  { SysPrintf("vtlbDefaultPhyRead16: 0x%X\n",addr); verify(false); return -1; }
int __fastcall vtlbDefaultPhyRead32(u32 addr,mem32_t* data) { SysPrintf("vtlbDefaultPhyRead32: 0x%X\n",addr); verify(false); return -1; }
int __fastcall vtlbDefaultPhyRead64(u32 addr,mem64_t* data) { SysPrintf("vtlbDefaultPhyRead64: 0x%X\n",addr); verify(false); return -1; }
int __fastcall vtlbDefaultPhyRead128(u32 addr,mem128_t* data) { SysPrintf("vtlbDefaultPhyRead128: 0x%X\n",addr); verify(false); return -1; }

void __fastcall vtlbDefaultPhyWrite8(u32 addr,mem8_t data) { SysPrintf("vtlbDefaultPhyWrite8: 0x%X\n",addr); verify(false); }
void __fastcall vtlbDefaultPhyWrite16(u32 addr,mem16_t data) { SysPrintf("vtlbDefaultPhyWrite16: 0x%X\n",addr); verify(false); }
void __fastcall vtlbDefaultPhyWrite32(u32 addr,mem32_t data) { SysPrintf("vtlbDefaultPhyWrite32: 0x%X\n",addr); verify(false); }
void __fastcall vtlbDefaultPhyWrite64(u32 addr,const mem64_t* data) { SysPrintf("vtlbDefaultPhyWrite64: 0x%X\n",addr); verify(false); }
void __fastcall vtlbDefaultPhyWrite128(u32 addr,const mem128_t* data) { SysPrintf("vtlbDefaultPhyWrite128: 0x%X\n",addr); verify(false); }
/////
vtlbHandler vtlb_RegisterHandler(	vltbMemR8FP* r8,vltbMemR16FP* r16,vltbMemR32FP* r32,vltbMemR64FP* r64,vltbMemR128FP* r128,
									vltbMemW8FP* w8,vltbMemW16FP* w16,vltbMemW32FP* w32,vltbMemW64FP* w64,vltbMemW128FP* w128)
{
	//write the code :p
	vtlbHandler rv=vtlbHandlerCount++;
	
	RWFT[0][0][rv]=r8!=0?r8:vtlbDefaultPhyRead8;
	RWFT[1][0][rv]=r16!=0?r16:vtlbDefaultPhyRead16;
	RWFT[2][0][rv]=r32!=0?r32:vtlbDefaultPhyRead32;
	RWFT[3][0][rv]=r64!=0?r64:vtlbDefaultPhyRead64;
	RWFT[4][0][rv]=r128!=0?r128:vtlbDefaultPhyRead128;

	RWFT[0][1][rv]=w8!=0?w8:vtlbDefaultPhyWrite8;
	RWFT[1][1][rv]=w16!=0?w16:vtlbDefaultPhyWrite16;
	RWFT[2][1][rv]=w32!=0?w32:vtlbDefaultPhyWrite32;
	RWFT[3][1][rv]=w64!=0?w64:vtlbDefaultPhyWrite64;
	RWFT[4][1][rv]=w128!=0?w128:vtlbDefaultPhyWrite128;

	return rv;
}

void vtlb_MapHandler(vtlbHandler handler,u32 start,u32 size)
{
	verify(0==(start&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);
	s32 value=handler|0x80000000;

	while(size>0)
	{
		pmap[start>>VTLB_PAGE_BITS]=value;

		start+=VTLB_PAGE_SIZE;
		size-=VTLB_PAGE_SIZE;
	}	
}
void vtlb_MapBlock(void* base,u32 start,u32 size,u32 blocksize)
{
	s32 baseint=(s32)base;

	verify(0==(start&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);
	if (blocksize==0) 
		blocksize=size;
	verify(0==(blocksize&VTLB_PAGE_MASK) && blocksize>0);
	verify(0==(size%blocksize));

	while(size>0)
	{
		u32 blocksz=blocksize;
		s32 ptr=baseint;

		while(blocksz>0)
		{
			pmap[start>>VTLB_PAGE_BITS]=ptr;

			start+=VTLB_PAGE_SIZE;
			ptr+=VTLB_PAGE_SIZE;
			blocksz-=VTLB_PAGE_SIZE;
			size-=VTLB_PAGE_SIZE;
		}
	}
}
void vtlb_Mirror(u32 new_region,u32 start,u32 size)
{
	verify(0==(new_region&VTLB_PAGE_MASK));
	verify(0==(start&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);

	while(size>0)
	{
		pmap[start>>VTLB_PAGE_BITS]=pmap[new_region>>VTLB_PAGE_BITS];

		start+=VTLB_PAGE_SIZE;
		new_region+=VTLB_PAGE_SIZE;
		size-=VTLB_PAGE_SIZE;
	}	
}

__forceinline void* vtlb_GetPhyPtr(u32 paddr)
{
	if (paddr>=VTLB_PMAP_SZ || pmap[paddr>>VTLB_PAGE_BITS]<0)
		return 0;
	else
		return reinterpret_cast<void*>(pmap[paddr>>VTLB_PAGE_BITS]+(paddr&VTLB_PAGE_MASK));

}
//virtual mappings
//TODO: Add invalid paddr checks
void vtlb_VMap(u32 vaddr,u32 paddr,u32 sz)
{
	verify(0==(vaddr&VTLB_PAGE_MASK));
	verify(0==(paddr&VTLB_PAGE_MASK));
	verify(0==(sz&VTLB_PAGE_MASK) && sz>0);

	while(sz>0)
	{
		s32 pme;
		if (paddr>=VTLB_PMAP_SZ)
		{
			pme=UnmappedPhyHandler0;
			if (paddr&0x80000000)
				pme=UnmappedPhyHandler1;
			pme|=0x80000000;
			pme|=paddr;// top bit is set anyway ...
		}
		else
		{
			pme=pmap[paddr>>VTLB_PAGE_BITS];
			if (pme<0)
				pme|=paddr;// top bit is set anyway ...
		}
		vmap[vaddr>>VTLB_PAGE_BITS]=pme-vaddr;
		vaddr+=VTLB_PAGE_SIZE;
		paddr+=VTLB_PAGE_SIZE;
		sz-=VTLB_PAGE_SIZE;
	}
}

void vtlb_VMapBuffer(u32 vaddr,void* buffer,u32 sz)
{
	verify(0==(vaddr&VTLB_PAGE_MASK));
	verify(0==(sz&VTLB_PAGE_MASK) && sz>0);
	u32 bu8=(u32)buffer;
	while(sz>0)
	{
		vmap[vaddr>>VTLB_PAGE_BITS]=bu8-vaddr;
		vaddr+=VTLB_PAGE_SIZE;
		bu8+=VTLB_PAGE_SIZE;
		sz-=VTLB_PAGE_SIZE;
	}
}
void vtlb_VMapUnmap(u32 vaddr,u32 sz)
{
	verify(0==(vaddr&VTLB_PAGE_MASK));
	verify(0==(sz&VTLB_PAGE_MASK) && sz>0);
	
	while(sz>0)
	{
		u32 handl=UnmappedVirtHandler0;
		if (vaddr&0x80000000)
		{
			handl=UnmappedVirtHandler1;
		}
		handl|=vaddr; // top bit is set anyway ...
		handl|=0x80000000;
		vmap[vaddr>>VTLB_PAGE_BITS]=handl-vaddr;
		vaddr+=VTLB_PAGE_SIZE;
		sz-=VTLB_PAGE_SIZE;
	}
}
bool vtlb_Init()
{
	//Reset all vars to default values
	vtlbHandlerCount=0;
	memset(RWFT,0,sizeof(RWFT));

	//Register default handlers
	//Unmapped Virt handlers _MUST_ be registed frist.
	//On address translation the top bit cannot be preserved.This is not normaly a problem since
	//the physical address space can be 'compressed' to just 29 bits.However, to properly handle exceptions
	//there must be a way to get the full address back.Thats why i use these 2 functions and encode the hi bit directly into em :)

	UnmappedVirtHandler0=vtlb_RegisterHandler(vtlbUnmappedVRead8<0>,vtlbUnmappedVRead16<0>,vtlbUnmappedVRead32<0>,vtlbUnmappedVRead64<0>,vtlbUnmappedVRead128<0>,
											  vtlbUnmappedVWrite8<0>,vtlbUnmappedVWrite16<0>,vtlbUnmappedVWrite32<0>,vtlbUnmappedVWrite64<0>,vtlbUnmappedVWrite128<0>);

	UnmappedVirtHandler1=vtlb_RegisterHandler(vtlbUnmappedVRead8<0x80000000>,vtlbUnmappedVRead16<0x80000000>,vtlbUnmappedVRead32<0x80000000>,
												vtlbUnmappedVRead64<0x80000000>,vtlbUnmappedVRead128<0x80000000>,
											  vtlbUnmappedVWrite8<0x80000000>,vtlbUnmappedVWrite16<0x80000000>,vtlbUnmappedVWrite32<0x80000000>,
												vtlbUnmappedVWrite64<0x80000000>,vtlbUnmappedVWrite128<0x80000000>);

	UnmappedPhyHandler0=vtlb_RegisterHandler(vtlbUnmappedPRead8<0>,vtlbUnmappedPRead16<0>,vtlbUnmappedPRead32<0>,vtlbUnmappedPRead64<0>,vtlbUnmappedPRead128<0>,
											  vtlbUnmappedPWrite8<0>,vtlbUnmappedPWrite16<0>,vtlbUnmappedPWrite32<0>,vtlbUnmappedPWrite64<0>,vtlbUnmappedPWrite128<0>);

	UnmappedPhyHandler1=vtlb_RegisterHandler(vtlbUnmappedPRead8<0x80000000>,vtlbUnmappedPRead16<0x80000000>,vtlbUnmappedPRead32<0x80000000>,
												vtlbUnmappedPRead64<0x80000000>,vtlbUnmappedPRead128<0x80000000>,
											  vtlbUnmappedPWrite8<0x80000000>,vtlbUnmappedPWrite16<0x80000000>,vtlbUnmappedPWrite32<0x80000000>,
												vtlbUnmappedPWrite64<0x80000000>,vtlbUnmappedPWrite128<0x80000000>);
	DefaultPhyHandler=vtlb_RegisterHandler(0,0,0,0,0,0,0,0,0,0);

	//Setup the initial mappings
	vtlb_MapHandler(DefaultPhyHandler,0,VTLB_PMAP_SZ);	
	
	//Set the V space as unmapped
	vtlb_VMapUnmap(0,(VTLB_VMAP_ITEMS-1)*VTLB_PAGE_SIZE);
	//yeah i know, its stupid .. but this code has to be here for now ;p
	vtlb_VMapUnmap((VTLB_VMAP_ITEMS-1)*VTLB_PAGE_SIZE,VTLB_PAGE_SIZE);
	//done !
	return true;
}
void vtlb_Term()
{
	//nothing to do for now
}


namespace EE { namespace Dynarec
{

//ecx = addr
//edx = ptr
void vtlb_DynGenRead(u32 sz,int freereg)
{
	freereg=-1;
	/*
		u32 vmv=vmap[addr>>VTLB_PAGE_BITS];
	s32 ppf=addr+vmv;
	if (!(ppf<0))
	{
		data[0]=*reinterpret_cast<DataType*>(ppf);
		if (DataSize==128)
			data[1]=*reinterpret_cast<DataType*>(ppf+8);
		return 0;
	}
	else
	{	
		//has to: translate, find function, call function
		u32 hand=(u8)vmv;
		u32 paddr=ppf-hand+0x80000000;
		//SysPrintf("Translted 0x%08X to 0x%08X\n",addr,paddr);
		return reinterpret_cast<TemplateHelper<DataSize,false>::HandlerType*>(RWFT[TemplateHelper<DataSize,false>::sidx][0][hand])(paddr,data);
	}

		mov eax,ecx;
		shr eax,VTLB_PAGE_BITS;
		mov eax,[eax*4+vmap];
		add ecx,eax;
		js _fullread;

		//these are wrong order, just an example ...
		mov [eax],ecx;
		mov ecx,[edx];
		mov [eax+4],ecx;
		mov ecx,[edx+4];
		mov [eax+4+4],ecx;
		mov ecx,[edx+4+4];
		mov [eax+4+4+4+4],ecx;
		mov ecx,[edx+4+4+4+4];
		///....

		jmp cont;
		_fullread:
		movzx eax,al;
		sub   ecx,eax;
		sub   ecx,0x80000000;
		call [eax+stuff];
		cont:
		........

	*/
	MOV32RtoR(EAX,ECX);
	SHR32ItoR(EAX,VTLB_PAGE_BITS);
	MOV32RmSOffsettoR(EAX,EAX,(int)vmap,2);
	ADD32RtoR(ECX,EAX);
	u8* _fullread=JS8(0);
	switch(sz)
	{
	case 8:
		MOVZX32Rm8toR(EAX,ECX);
		MOV8RtoRm(EDX,EAX);
		break;
	case 16:
		MOVZX32Rm16toR(EAX,ECX);
		MOV16RtoRm(EDX,EAX);
		break;
	case 32:
		MOV32RmtoR(EAX,ECX);
		MOV32RtoRm(EDX,EAX);
		break;

	case 64:
		if (freereg>0)
		{
			MOVQRmtoROffset(freereg,ECX,0);
			MOVQRtoRmOffset(EDX,freereg,0);
		}
		else
		{
			MOV32RmtoR(EAX,ECX);
			MOV32RtoRm(EDX,EAX);

			MOV32RmtoROffset(EAX,ECX,4);
			MOV32RtoRmOffset(EDX,EAX,4);
		}
		break;
	case 128:
		if (freereg>0)
		{
			SSE_MOVAPSRmtoROffset(freereg,ECX,0);
			SSE_MOVAPSRtoRmOffset(EDX,freereg,0);
		}
		else
		{
			MOV32RmtoR(EAX,ECX);
			MOV32RtoRm(EDX,EAX);

			MOV32RmtoROffset(EAX,ECX,4);
			MOV32RtoRmOffset(EDX,EAX,4);

			MOV32RmtoROffset(EAX,ECX,8);
			MOV32RtoRmOffset(EDX,EAX,8);

			MOV32RmtoROffset(EAX,ECX,12);
			MOV32RtoRmOffset(EDX,EAX,12);
		}
		break;
	}
	u8* cont=JMP8(0);
	x86SetJ8(_fullread);
	int szidx=0;

	switch(sz)
	{
	case 8:  szidx=0;	break;
	case 16:   szidx=1;	break;
	case 32:   szidx=2;	break;
	case 64:   szidx=3;	break;
	case 128:   szidx=4; break;
	}
	MOVZX32R8toR(EAX,EAX);
	SUB32RtoR(ECX,EAX);
	//eax=[funct+eax]
	MOV32RmSOffsettoR(EAX,EAX,(int)&RWFT[szidx][0][0],2);
	SUB32ItoR(ECX,0x80000000);
	CALL32R(EAX);

	x86SetJ8(cont);
}

void vtlb_DynGenWrite(u32 sz,int freereg)
{
	MOV32RtoR(EAX,ECX);
	SHR32ItoR(EAX,VTLB_PAGE_BITS);
	MOV32RmSOffsettoR(EAX,EAX,(int)vmap,2);
	ADD32RtoR(ECX,EAX);
	u8* _full=JS8(0);
	switch(sz)
	{
		//8 , 16, 32 : data on EDX
	case 8:
		MOV8RtoRm(ECX,EDX);
		break;
	case 16:
		MOV16RtoRm(ECX,EDX);
		break;
	case 32:
		MOV32RtoRm(ECX,EDX);
		break;
/*
	case 64:
		//write8(0xCC);
		POP32R(EAX);
		MOV32RtoRm(ECX,EAX);

		POP32R(EAX);
		MOV32RtoRmOffset(ECX,EAX,4);
		break;*/
	case 64:
	case 128:
		if (freereg>0)
		{
			if (sz==64)
			{
				MOVQRmtoROffset(freereg,EDX,0);
				MOVQRtoRmOffset(ECX,freereg,0);
			}
			else
			{
				SSE_MOVAPSRmtoROffset(freereg,EDX,0);
				SSE_MOVAPSRtoRmOffset(ECX,freereg,0);
			}
		}
		else
		{
			MOV32RmtoR(EAX,EDX);
			MOV32RtoRm(ECX,EAX);

			MOV32RmtoROffset(EAX,EDX,4);
			MOV32RtoRmOffset(ECX,EAX,4);
			if (sz==128)
			{
				MOV32RmtoROffset(EAX,EDX,8);
				MOV32RtoRmOffset(ECX,EAX,8);
				MOV32RmtoROffset(EAX,EDX,12);
				MOV32RtoRmOffset(ECX,EAX,12);
			}
		}
		break;
	}
	u8* cont=JMP8(0);
	x86SetJ8(_full);
	int szidx=0;

	switch(sz)
	{
	case 8:  szidx=0;	break;
	case 16:   szidx=1;	break;
	case 32:   szidx=2;	break;
	case 64:   szidx=3;	break;
	case 128:   szidx=4; break;
	}
	MOVZX32R8toR(EAX,EAX);
	SUB32RtoR(ECX,EAX);
	//eax=[funct+eax]
	MOV32RmSOffsettoR(EAX,EAX,(int)&RWFT[szidx][1][0],2);
	SUB32ItoR(ECX,0x80000000);
	CALL32R(EAX);

	x86SetJ8(cont);
}

} }

#endif		// PCSX2_VIRTUAL_MEM
