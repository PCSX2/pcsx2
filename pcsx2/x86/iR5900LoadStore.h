/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __IR5900LOADSTORE_H__
#define __IR5900LOADSTORE_H__
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

#endif
