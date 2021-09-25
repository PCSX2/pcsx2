/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include <wx/ffile.h>

#include "common/Console.h"
#include "common/Path.h"

#include "AppConfig.h"
#include "Host.h"

std::optional<std::vector<u8>> Host::ReadResourceFile(const char* filename)
{
	const wxString full_path(Path::Combine(EmuFolders::Resources, wxString::FromUTF8(filename)));
	wxFFile file(full_path, L"rb");

	if (!file.IsOpened())
	{
		Console.Error("Failed to open resource file '%s'", filename);
		return std::nullopt;
	}

	const size_t size = file.Length();
	std::vector<u8> ret(size);
	if (!file.Read(ret.data(), size))
	{
		Console.Error("Failed to read resource file '%s'", filename);
		return std::nullopt;
	}

	return ret;
}

std::optional<std::string> Host::ReadResourceFileToString(const char* filename)
{
	const wxString full_path(Path::Combine(EmuFolders::Resources, wxString::FromUTF8(filename)));
	wxFFile file(full_path, L"rb");

	if (!file.IsOpened())
	{
		Console.Error("Failed to open resource file '%s'", filename);
		return std::nullopt;
	}

	const size_t size = file.Length();
	std::string ret;
	ret.resize(size);
	if (!file.Read(ret.data(), size))
	{
		Console.Error("Failed to read resource file '%s'", filename);
		return std::nullopt;
	}

	return ret;
}
