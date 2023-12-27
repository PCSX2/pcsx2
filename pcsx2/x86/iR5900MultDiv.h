// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recMULT();
	void recMULTU();
	void recDIV();
	void recDIVU();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
