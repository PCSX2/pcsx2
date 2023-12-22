// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recADDI();
	void recADDIU();
	void recDADDI();
	void recDADDIU();
	void recANDI();
	void recORI();
	void recXORI();

	void recSLTI();
	void recSLTIU();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
