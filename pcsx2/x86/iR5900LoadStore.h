// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recLB();
	void recLBU();
	void recLH();
	void recLHU();
	void recLW();
	void recLWU();
	void recLWL();
	void recLWR();
	void recLD();
	void recLDR();
	void recLDL();
	void recLQ();
	void recSB();
	void recSH();
	void recSW();
	void recSWL();
	void recSWR();
	void recSD();
	void recSDL();
	void recSDR();
	void recSQ();
	void recLWC1();
	void recSWC1();
	void recLQC2();
	void recSQC2();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
