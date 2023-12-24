// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recADD();
	void recADDU();
	void recDADD();
	void recDADDU();
	void recSUB();
	void recSUBU();
	void recDSUB();
	void recDSUBU();
	void recAND();
	void recOR();
	void recXOR();
	void recNOR();
	void recSLT();
	void recSLTU();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
