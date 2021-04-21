/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "wx/string.h"

namespace StringUtils
{
	namespace UTF8
	{
		// There should be no reason for a unix environment to have to convert string->wstring
		// See - http://utf8everywhere.org/#how.cvt
#ifdef _WIN32
		/**
		 * @brief Converts a UTF-16 string to a UTF-8 string
		 * @param str UTF-16 string
		 * @return UTF-8 string
		*/
		std::string narrow(const std::wstring_view& str);
		/**
		 * @brief Converts a UTF-8 string to a UTF-16 string
		 * @param str UTF-8 string
		 * @return UTF-16 string
		*/
		std::wstring widen(const std::string_view& str);
#endif

		// --- wxWidgets Conversions

		/**
		 * @brief Converts a wxString to a UTF-8 std::string
		 * @param str wxString
		 * @return UTF-8 std::string
		*/
		std::string fromWxString(const wxString& str);

	} // namespace UTF8
} // namespace StringUtils