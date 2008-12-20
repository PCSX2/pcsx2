#ifndef _VTLB_H_
#define _VTLB_H_

#include <xmmintrin.h>

#ifndef PCSX2_VIRTUAL_MEM

#define mem8_t u8
#define mem16_t u16
#define mem32_t u32
#define mem64_t u64
#define mem128_t u64

// unsafe version needed to avoid template hell on gcc. :/
typedef  int __fastcall vltbMemRFP(u32 addr,void* data);

typedef  int __fastcall vltbMemR8FP(u32 addr,mem8_t* data);
typedef  int __fastcall vltbMemR16FP(u32 addr,mem16_t* data);
typedef  int __fastcall vltbMemR32FP(u32 addr,mem32_t* data);
typedef  int __fastcall vltbMemR64FP(u32 addr,mem64_t* data);
typedef  int __fastcall vltbMemR128FP(u32 addr,mem128_t* data);

typedef  void __fastcall vltbMemW8FP(u32 addr,mem8_t data);
typedef  void __fastcall vltbMemW16FP(u32 addr,mem16_t data);
typedef  void __fastcall vltbMemW32FP(u32 addr,mem32_t data);
typedef  void __fastcall vltbMemW64FP(u32 addr,const mem64_t* data);
typedef  void __fastcall vltbMemW128FP(u32 addr,const mem128_t* data);

typedef u32 vtlbHandler;

bool vtlb_Init();
void vtlb_Term();

//physical stuff
vtlbHandler vtlb_RegisterHandler(	vltbMemR8FP* r8,vltbMemR16FP* r16,vltbMemR32FP* r32,vltbMemR64FP* r64,vltbMemR128FP* r128,
									vltbMemW8FP* w8,vltbMemW16FP* w16,vltbMemW32FP* w32,vltbMemW64FP* w64,vltbMemW128FP* w128);

void vtlb_MapHandler(vtlbHandler handler,u32 start,u32 size);
void vtlb_MapBlock(void* base,u32 start,u32 size,u32 blocksize=0);
void* vtlb_GetPhyPtr(u32 paddr);
//void vtlb_Mirror(u32 new_region,u32 start,u32 size); // -> not working yet :(

//virtual mappings
void vtlb_VMap(u32 vaddr,u32 paddr,u32 sz);
void vtlb_VMapBuffer(u32 vaddr,void* buffer,u32 sz);
void vtlb_VMapUnmap(u32 vaddr,u32 sz);

//Memory functions

int __fastcall vtlb_memRead8(u32 mem, u8  *out);
int __fastcall vtlb_memRead16(u32 mem, u16 *out);
int __fastcall vtlb_memRead32(u32 mem, u32 *out);
int __fastcall vtlb_memRead64(u32 mem, u64 *out);
int __fastcall vtlb_memRead128(u32 mem, u64 *out);
void __fastcall vtlb_memWrite8 (u32 mem, u8  value);
void __fastcall vtlb_memWrite16(u32 mem, u16 value);
void __fastcall vtlb_memWrite32(u32 mem, u32 value);
void __fastcall vtlb_memWrite64(u32 mem, const u64* value);
void __fastcall vtlb_memWrite128(u32 mem, const u64* value);

namespace EE { namespace Dynarec {

void vtlb_DynGenWrite(u32 sz,int freereg);
void vtlb_DynGenRead(u32 sz,int freereg);

} }

#endif

#endif
