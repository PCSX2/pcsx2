// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// Implementations found here: Increment and Decrement Instructions!
// (They're soooo lonely... but I dunno where else to stick this class!)

namespace x86Emitter
{

	// --------------------------------------------------------------------------------------
	//  xImpl_IncDec
	// --------------------------------------------------------------------------------------
	struct xImpl_IncDec
	{
		bool isDec;

		void operator()(const xRegisterInt& to) const;
		void operator()(const xIndirect64orLess& to) const;
	};

} // End namespace x86Emitter
