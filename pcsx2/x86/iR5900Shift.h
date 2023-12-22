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

	void recSLL();
	void recSRL();
	void recSRA();
	void recDSLL();
	void recDSRL();
	void recDSRA();
	void recDSLL32();
	void recDSRL32();
	void recDSRA32();

	void recSLLV();
	void recSRLV();
	void recSRAV();
	void recDSLLV();
	void recDSRLV();
	void recDSRAV();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
