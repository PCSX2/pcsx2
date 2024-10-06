// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recJ();
	void recJAL();
	void recJR();
	void recJALR();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
