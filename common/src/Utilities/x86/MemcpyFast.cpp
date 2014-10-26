/******************************************************************************

 Copyright (c) 2001 Advanced Micro Devices, Inc.

 LIMITATION OF LIABILITY:  THE MATERIALS ARE PROVIDED *AS IS* WITHOUT ANY
 EXPRESS OR IMPLIED WARRANTY OF ANY KIND INCLUDING WARRANTIES OF MERCHANTABILITY,
 NONINFRINGEMENT OF THIRD-PARTY INTELLECTUAL PROPERTY, OR FITNESS FOR ANY
 PARTICULAR PURPOSE.  IN NO EVENT SHALL AMD OR ITS SUPPLIERS BE LIABLE FOR ANY
 DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS,
 BUSINESS INTERRUPTION, LOSS OF INFORMATION) ARISING OUT OF THE USE OF OR
 INABILITY TO USE THE MATERIALS, EVEN IF AMD HAS BEEN ADVISED OF THE POSSIBILITY
 OF SUCH DAMAGES.  BECAUSE SOME JURISDICTIONS PROHIBIT THE EXCLUSION OR LIMITATION
 OF LIABILITY FOR CONSEQUENTIAL OR INCIDENTAL DAMAGES, THE ABOVE LIMITATION MAY
 NOT APPLY TO YOU.

 AMD does not assume any responsibility for any errors which may appear in the
 Materials nor any responsibility to support or update the Materials.  AMD retains
 the right to make changes to its test specifications at any time, without notice.

 NO SUPPORT OBLIGATION: AMD is not obligated to furnish, support, or make any
 further information, software, technical information, know-how, or show-how
 available to you.

 So that all may benefit from your experience, please report  any  problems
 or  suggestions about this software to 3dsdk.support@amd.com

 AMD Developer Technologies, M/S 585
 Advanced Micro Devices, Inc.
 5900 E. Ben White Blvd.
 Austin, TX 78741
 3dsdk.support@amd.com
******************************************************************************/

// GH: AMD memcpy was removed. The remaining part (memcmp_mmx) is likely from Zerofrog.
// Hopefully memcmp_mmx will be dropped in the future.

#include "PrecompiledHeader.h"

#ifdef _MSC_VER
#pragma warning(disable:4414)
#endif

// Inline assembly syntax for use with Visual C++

#if defined(_MSC_VER)

// mmx mem-compare implementation, size has to be a multiple of 8
// returns 0 is equal, nonzero value if not equal
// ~10 times faster than standard memcmp
// (zerofrog)
u8 memcmp_mmx(const void* src1, const void* src2, int cmpsize)
{
	pxAssert( (cmpsize&7) == 0 );

	__asm {
		mov ecx, cmpsize
		mov edx, src1
		mov esi, src2

		cmp ecx, 32
		jl Done4

		// custom test first 8 to make sure things are ok
		movq mm0, [esi]
		movq mm1, [esi+8]
		pcmpeqd mm0, [edx]
		pcmpeqd mm1, [edx+8]
		pand mm0, mm1
		movq mm2, [esi+16]
		pmovmskb eax, mm0
		movq mm3, [esi+24]

		// check if eq
		cmp eax, 0xff
		je NextComp
		mov eax, 1
		jmp End

NextComp:
		pcmpeqd mm2, [edx+16]
		pcmpeqd mm3, [edx+24]
		pand mm2, mm3
		pmovmskb eax, mm2

		sub ecx, 32
		add esi, 32
		add edx, 32

		// check if eq
		cmp eax, 0xff
		je ContinueTest
		mov eax, 1
		jmp End

		cmp ecx, 64
		jl Done8

Cmp8:
		movq mm0, [esi]
		movq mm1, [esi+8]
		movq mm2, [esi+16]
		movq mm3, [esi+24]
		movq mm4, [esi+32]
		movq mm5, [esi+40]
		movq mm6, [esi+48]
		movq mm7, [esi+56]
		pcmpeqd mm0, [edx]
		pcmpeqd mm1, [edx+8]
		pcmpeqd mm2, [edx+16]
		pcmpeqd mm3, [edx+24]
		pand mm0, mm1
		pcmpeqd mm4, [edx+32]
		pand mm0, mm2
		pcmpeqd mm5, [edx+40]
		pand mm0, mm3
		pcmpeqd mm6, [edx+48]
		pand mm0, mm4
		pcmpeqd mm7, [edx+56]
		pand mm0, mm5
		pand mm0, mm6
		pand mm0, mm7
		pmovmskb eax, mm0

		// check if eq
		cmp eax, 0xff
		je Continue
		mov eax, 1
		jmp End

Continue:
		sub ecx, 64
		add esi, 64
		add edx, 64
ContinueTest:
		cmp ecx, 64
		jge Cmp8

Done8:
		test ecx, 0x20
		jz Done4
		movq mm0, [esi]
		movq mm1, [esi+8]
		movq mm2, [esi+16]
		movq mm3, [esi+24]
		pcmpeqd mm0, [edx]
		pcmpeqd mm1, [edx+8]
		pcmpeqd mm2, [edx+16]
		pcmpeqd mm3, [edx+24]
		pand mm0, mm1
		pand mm0, mm2
		pand mm0, mm3
		pmovmskb eax, mm0
		sub ecx, 32
		add esi, 32
		add edx, 32

		// check if eq
		cmp eax, 0xff
		je Done4
		mov eax, 1
		jmp End

Done4:
		cmp ecx, 24
		jne Done2
		movq mm0, [esi]
		movq mm1, [esi+8]
		movq mm2, [esi+16]
		pcmpeqd mm0, [edx]
		pcmpeqd mm1, [edx+8]
		pcmpeqd mm2, [edx+16]
		pand mm0, mm1
		pand mm0, mm2
		pmovmskb eax, mm0

		// check if eq
		cmp eax, 0xff
		setne al
		jmp End

Done2:
		cmp ecx, 16
		jne Done1

		movq mm0, [esi]
		movq mm1, [esi+8]
		pcmpeqd mm0, [edx]
		pcmpeqd mm1, [edx+8]
		pand mm0, mm1
		pmovmskb eax, mm0

		// check if eq
		cmp eax, 0xff
		setne al
		jmp End

Done1:
		cmp ecx, 8
		jne Done

		mov eax, [esi]
		mov esi, [esi+4]
		cmp eax, [edx]
		je Next
		mov eax, 1
		jmp End

Next:
		cmp esi, [edx+4]
		setne al
		jmp End

Done:
		xor eax, eax

End:
		emms
	}
}

#endif
