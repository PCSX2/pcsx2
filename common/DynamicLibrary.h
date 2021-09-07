/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2021 PCSX2 Dev Team
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
#include <string_view>
#include <memory>

#include "common/Pcsx2Types.h"

namespace Common
{
	// This is a small uility class for loading dynamic libraries (DLL on windows, dynlib on mac, so on linux)
	// ex:
	// auto lib = Common::Dynalib("xinput1_4"); // alternatively lib.Open("xinput1_4");
	//
	// //  make sure it's open
	// if(!lib.IsOpen())
	//     // error
	//
	// auto sym = lib.GetSymbol<_XInputEnable>("XInputEnable");
	// if(!sym)
	//     // error
	//
	// lib.Close(); // will be done automatically when lib falls out of scope
	class DynamicLibrary
	{
	public:
		DynamicLibrary();
		// same as Open
		DynamicLibrary(const char* path);
		~DynamicLibrary();

		// open a dynalib using it's name minus ext
		bool Open(const char* path);
		// close the dynalib
		void Close();

		// check if the dynalib was successfully loaded
		bool IsOpen() const;

		// get a symbol automatically casted to the correct type
		template<typename T>
		T GetSymbol(const char* name)
		{
			return reinterpret_cast<T>(GetSymbolByName(name));
		}

		// get a symbol by it's ordinal value
		// this is needed for some xinput features
		// note: win32 only, everything else returns nullptr
		template <typename T>
		T GetSymbolOrdinal(const uint id)
		{
			return reinterpret_cast<T>(GetSymbolByOrdinal(id));
		}
	private:
		void* GetSymbolByName(const char* name) const;
		void* GetSymbolByOrdinal(const uint ordinal) const;

		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};
}