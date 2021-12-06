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
#include "AppResources.h"
#include "AppConfig.h"
#include <wx/ffile.h>

template <typename T>
static std::optional<T> LoadResourceInternal(const char* name)
{
	wxString path = Path::Combine(PathDefs::GetProgramDataDir(), wxFileName(fromUTF8(name)));
	wxFFile file(path, L"rb");

	if (!file.IsOpened())
	{
		Console.Error("Failed to open resource file %s", name);
		return std::nullopt;
	}

	const size_t size = file.Length();
	T out(size, 0);

	if (file.Read(out.data(), size) != size)
	{
		Console.Error("Failed to read resource file %s", name);
		return std::nullopt;
	}

	return out;
}

std::optional<std::vector<u8>> AppResources::LoadResource(const char* name)
{
	return LoadResourceInternal<std::vector<u8>>(name);
}

std::optional<std::string> AppResources::LoadTextResource(const char* name)
{
	return LoadResourceInternal<std::string>(name);
}
