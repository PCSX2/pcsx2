// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recBEQ();
	void recBEQL();
	void recBNE();
	void recBNEL();
	void recBLTZ();
	void recBLTZL();
	void recBLTZAL();
	void recBLTZALL();
	void recBGTZ();
	void recBGTZL();
	void recBLEZ();
	void recBLEZL();
	void recBGEZ();
	void recBGEZL();
	void recBGEZAL();
	void recBGEZALL();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
