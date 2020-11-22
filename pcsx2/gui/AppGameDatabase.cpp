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

#include "PrecompiledHeader.h"

#include "App.h"
#include "AppGameDatabase.h"

#include <wx/stdpaths.h>
#include "fmt/core.h"
#include <fstream>

std::ifstream AppGameDatabase::getFileAsStream(const wxString& file)
{
// TODO - config - refactor with std::filesystem/ghc::filesystem
#ifdef _WIN32
	return std::ifstream(file.wc_str());
#else
	return std::ifstream(file.c_str());
#endif
}

AppGameDatabase& AppGameDatabase::LoadFromFile(const wxString& _file)
{
	// TODO - config - refactor with std::filesystem/ghc::filesystem

	wxString file(_file);
	if (wxFileName(file).IsRelative())
	{
		// InstallFolder is the preferred base directory for the DB file, but the registry can point to previous
		// installs if uninstall wasn't done properly.
		// Since the games DB file is considered part of pcsx2.exe itself, look for it at the exe folder
		//   regardless of any other settings.

		// Note 1: Portable setup didn't suffer from this as install folder pointed already to the exe folder in portable.
		// Note 2: Other folders are either configurable (plugins, memcards, etc) or create their content automatically (inis)
		//           So the games DB was really the only one that suffers from residues of prior installs.

		//wxDirName dir = InstallFolder;
		wxDirName dir = (wxDirName)wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
		file = (dir + file).GetFullPath();
	}


	if (!wxFileExists(file))
	{
		Console.Error(L"[GameDB] Database Not Found! [%s]", WX_STR(file));
		return *this;
	}

	const u64 qpc_Start = GetCPUTicks();

	std::ifstream fileStream = getFileAsStream(file);
	if (!this->initDatabase(fileStream))
	{
		Console.Error(L"[GameDB] Database could not be loaded successfully");
		return *this;
	}

	const u64 qpc_end = GetCPUTicks();

	Console.WriteLn(fmt::format("[GameDB] {} games on record (loaded in {}ms)", this->numGames(),
								(u32)(((qpc_end - qpc_Start) * 1000) / GetTickFrequency())));

	return *this;
}

AppGameDatabase* Pcsx2App::GetGameDatabase()
{
	pxAppResources& res(GetResourceCache());

	ScopedLock lock(m_mtx_LoadingGameDB);
	if (!res.GameDB)
	{
		res.GameDB = std::make_unique<AppGameDatabase>();
		res.GameDB->LoadFromFile();
	}
	return res.GameDB.get();
}

IGameDatabase* AppHost_GetGameDatabase()
{
	return wxGetApp().GetGameDatabase();
}
