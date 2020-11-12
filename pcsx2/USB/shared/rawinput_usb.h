/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#pragma once
#include <windows.h>
#include <setupapi.h>
#include "hidapi.h"

namespace shared
{
	namespace rawinput
	{

		class ParseRawInputCB
		{
		public:
			virtual void ParseRawInput(PRAWINPUT pRawInput) = 0;
		};

		int Initialize(void* hWnd);
		void Uninitialize();

		void RegisterCallback(ParseRawInputCB* cb);
		void UnregisterCallback(ParseRawInputCB* cb);
	} // namespace rawinput
} // namespace shared
