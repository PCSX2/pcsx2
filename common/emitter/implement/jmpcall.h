// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

// Implementations found here: CALL and JMP!  (unconditional only)

namespace x86Emitter
{

	extern void xJccKnownTarget(JccComparisonType comparison, const void* target, bool slideForward);

	// ------------------------------------------------------------------------
	struct xImpl_JmpCall
	{
		bool isJmp;

		void operator()(const xAddressReg& absreg) const;
		void operator()(const xIndirectNative& src) const;

		// Special form for calling functions.  This form automatically resolves the
		// correct displacement based on the size of the instruction being generated.
		void operator()(const void* func) const
		{
			if (isJmp)
				xJccKnownTarget(Jcc_Unconditional, (const void*)(uptr)func, false); // double cast to/from (uptr) needed to appease GCC
			else
			{
				// calls are relative to the instruction after this one, and length is
				// always 5 bytes (16 bit calls are bad mojo, so no bother to do special logic).

				sptr dest = (sptr)func - ((sptr)xGetPtr() + 5);
				pxAssertMsg(dest == (s32)dest, "Indirect jump is too far, must use a register!");
				xWrite8(0xe8);
				xWrite32(dest);
			}
		}
	};

	// yes it is awful. Due to template code is in a header with a nice circular dep.
	extern const xImpl_Mov xMOV;
	extern const xImpl_JmpCall xCALL;

	struct xImpl_FastCall
	{
		// FIXME: current 64 bits is mostly a copy/past potentially it would require to push/pop
		// some registers. But I think it is enough to handle the first call.

		void operator()(const void* f, const xRegister32& a1 = xEmptyReg, const xRegister32& a2 = xEmptyReg) const;

		void operator()(const void* f, u32 a1, const xRegister32& a2) const;
		void operator()(const void* f, const xIndirect32& a1) const;
		void operator()(const void* f, u32 a1, u32 a2) const;
		void operator()(const void* f, void* a1) const;

		void operator()(const void* f, const xRegisterLong& a1, const xRegisterLong& a2 = xEmptyReg) const;
		void operator()(const void* f, u32 a1, const xRegisterLong& a2) const;

		template <typename T>
		__fi void operator()(T* func, u32 a1, const xRegisterLong& a2 = xEmptyReg) const
		{
			(*this)((const void*)func, a1, a2);
		}

		template <typename T>
		__fi void operator()(T* func, const xIndirect32& a1) const
		{
			(*this)((const void*)func, a1);
		}

		template <typename T>
		__fi void operator()(T* func, u32 a1, u32 a2) const
		{
			(*this)((const void*)func, a1, a2);
		}

		void operator()(const xIndirectNative& f, const xRegisterLong& a1 = xEmptyReg, const xRegisterLong& a2 = xEmptyReg) const;
	};

} // End namespace x86Emitter
