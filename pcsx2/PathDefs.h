/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2009  PCSX2 Dev Team
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

#include "common/PathUtils.h"

enum FoldersEnum_t
{
	// FIXME : Settings folder is no longer part of the user-local
	// ini storage.  They have been moved to an app-local ini storage class (which is currently
	// handled lazily via global variables)
	FolderId_Settings = 0,

	FolderId_Bios,
	FolderId_Snapshots,
	FolderId_Savestates,
	FolderId_MemoryCards,
	FolderId_Logs,
	FolderId_Langs,

	FolderId_Documents,

	FolderId_Cheats,
	FolderId_CheatsWS,

	FolderId_COUNT
};

// --------------------------------------------------------------------------------------
//  PathDefs  (namespace)
// --------------------------------------------------------------------------------------
// Contains default values for various pcsx2 path names and locations.
//
// Note: The members of this namespace are intended for default value initialization only.
// Everything else should use the path folder assignments in g_Conf instead, since those
// are user-configurable.
//
namespace PathDefs
{
	// complete pathnames are returned by these functions (may or may not be absolute)

	extern fs::path GetSnapshots();
	extern fs::path GetBios();
	extern fs::path GetSavestates();
	extern fs::path GetMemoryCards();
	extern fs::path GetSettings();
	extern fs::path GetLogs();
	extern fs::path GetLangs();
	extern fs::path GetCheats();
	extern fs::path GetCheatsWS();
	extern fs::path GetDocs();
	extern fs::path AppRoot();

	extern fs::path Get( FoldersEnum_t folderidx );

	// Base folder names used to extend out the documents/approot folder base into a complete
	// path.  These are typically for internal AppConfig use only, barring a few special cases.
	namespace Base
	{
		extern const std::string& Snapshots();
		extern const std::string& Savestates();
		extern const std::string& MemoryCards();
		extern const std::string& Settings();
		extern const std::string& Logs();
		extern const std::string& Dumps();
		extern const std::string& Langs();
		extern const std::string& Cheats();
		extern const std::string& CheatsWS();
		extern const std::string& Docs();
	}
}

namespace FilenameDefs
{
	extern wxFileName GetUiConfig();
	extern wxFileName GetUiKeysConfig();
	extern wxFileName GetVmConfig();
	extern wxFileName GetUsermodeConfig();
	extern const wxFileName& Memcard( uint port, uint slot );
};

