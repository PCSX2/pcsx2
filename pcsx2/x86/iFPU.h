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

#ifndef __IFPU_H__
#define __IFPU_H__

alignas(16) extern const u32 g_minvals[4];
alignas(16) extern const u32 g_maxvals[4];

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {

	void recMFC1();
	void recCFC1();
	void recMTC1();
	void recCTC1();
	void recCOP1_BC1();
	void recCOP1_S();
	void recCOP1_W();
	void recC_EQ();
	void recC_F();
	void recC_LT();
	void recC_LE();
	void recADD_S();
	void recSUB_S();
	void recMUL_S();
	void recDIV_S();
	void recSQRT_S();
	void recABS_S();
	void recMOV_S();
	void recNEG_S();
	void recRSQRT_S();
	void recADDA_S();
	void recSUBA_S();
	void recMULA_S();
	void recMADD_S();
	void recMSUB_S();
	void recMADDA_S();
	void recMSUBA_S();
	void recCVT_S();
	void recCVT_W();
	void recMAX_S();
	void recMIN_S();
	void recBC1F();
	void recBC1T();
	void recBC1FL();
	void recBC1TL();

} // namespace COP1
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900

#endif
