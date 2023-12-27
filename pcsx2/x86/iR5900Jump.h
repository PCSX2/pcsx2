// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
