// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "COP0.h"

/*********************************************************
*   COP0 opcodes                                         *
*                                                        *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP0 {

	void recMFC0();
	void recMTC0();
	void recBC0F();
	void recBC0T();
	void recBC0FL();
	void recBC0TL();
	void recTLBR();
	void recTLBWI();
	void recTLBWR();
	void recTLBP();
	void recERET();
	void recDI();
	void recEI();

} // namespace COP0
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
