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

#include "PrecompiledHeader.h"
#include "App.h"
#include "AppGameDatabase.h"
#include <wx/stdpaths.h>

class DBLoaderHelper
{
	DeclareNoncopyableObject( DBLoaderHelper );

protected:
	IGameDatabase&	m_gamedb;
	wxInputStream&	m_reader;

	// temp areas used as buffers for accelerated loading of database content.  These strings are
	// allocated and grown only once, and then reused for the duration of the database loading
	// process; saving thousands of heapp allocation operations.

	wxString		m_dest;
	std::string		m_intermediate;

	key_pair		m_keyPair;

public:	
	DBLoaderHelper( wxInputStream& reader, IGameDatabase& db )
		: m_gamedb(db)
		, m_reader(reader)
	{
	}

	void ReadGames();

protected:
	void doError(const wxString& msg);
	bool extractMultiLine();
	void extract();
};

void DBLoaderHelper::doError(const wxString& msg) {
	Console.Error(msg);
	m_keyPair.Clear();
}

// Multiline Sections are in the form of:
//
// [section=value]
//   content
//   content
// [/section]
//
// ... where the =value part is OPTIONAL.
bool DBLoaderHelper::extractMultiLine() {

	if (m_dest[0] != L'[') return false;		// All multiline sections begin with a '['!

	if (!m_dest.EndsWith(L"]")) {
		doError("GameDatabase: Malformed section start tag: " + m_dest);
		return false;
	}

	m_keyPair.key = m_dest;

	// Use Mid() to strip off the left and right side brackets.
	wxString midLine(m_dest.Mid(1, m_dest.Length()-2));
	wxString lvalue(midLine.BeforeFirst(L'=').Trim(true).Trim(false));
	wxString rvalue(midLine.AfterFirst(L'=').Trim(true).Trim(false));

	wxString key = '[' + lvalue + (rvalue.empty() ? "" : " = ") + rvalue + ']';
	if (key != m_keyPair.key)
		Console.Warning("GameDB: Badly formatted section start tag.\nActual: " + m_keyPair.key + "\nExpected: " + key);

	wxString endString;
	endString.Printf( L"[/%s]", lvalue.c_str() );

	while(!m_reader.Eof()) {
		pxReadLine( m_reader, m_dest, m_intermediate );
		// Abort if the closing tag is missing/incorrect so subsequent database entries aren't affected.
		if (m_dest == "---------------------------------------------")
			break;
		if (m_dest.CmpNoCase(endString) == 0)
			return true;
		m_keyPair.value += m_dest + L"\n";
	}
	doError("GameDatabase: Missing or incorrect section end tag:\n" + m_keyPair.key + "\n" + m_keyPair.value);
	return true;
}

void DBLoaderHelper::extract() {

	if( !pxParseAssignmentString( m_dest, m_keyPair.key, m_keyPair.value ) ) return;
	if( m_keyPair.value.IsEmpty() ) doError("GameDatabase: Bad file data: " + m_dest);
}

void DBLoaderHelper::ReadGames()
{
	Game_Data* game = NULL;

	while(!m_reader.Eof()) { // Fill game data, find new game, repeat...
		pthread_testcancel();
		pxReadLine(m_reader, m_dest, m_intermediate);
		m_dest.Trim(true).Trim(false);
		if( m_dest.IsEmpty() ) continue;

		m_keyPair.Clear();
		if (!extractMultiLine()) extract();

		if (!m_keyPair.IsOk()) continue;
		if (m_keyPair.CompareKey(m_gamedb.getBaseKey())) {
			game = m_gamedb.createNewGame(m_keyPair.value);
			continue;
		}

		game->writeString( m_keyPair.key, m_keyPair.value );
	}
}

// --------------------------------------------------------------------------------------
//  AppGameDatabase  (implementations)
// --------------------------------------------------------------------------------------

AppGameDatabase& AppGameDatabase::LoadFromFile(const wxString& _file, const wxString& key )
{
	wxString file(_file);
	if( wxFileName(file).IsRelative() )
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
		file = ( dir + file ).GetFullPath();
	}
	
	
	if (!wxFileExists(file))
	{
		Console.Error(L"(GameDB) Database Not Found! [%s]", WX_STR(file));
		return *this;
	}

	wxFFileInputStream reader( file );

	if (!reader.IsOk())
	{
		//throw Exception::FileNotFound( file );
		Console.Error(L"(GameDB) Could not access file (permission denied?) [%s]", WX_STR(file));
	}

	DBLoaderHelper loader( reader, *this );

	u64 qpc_Start = GetCPUTicks();
	loader.ReadGames();
	u64 qpc_end = GetCPUTicks();

	Console.WriteLn( "(GameDB) %d games on record (loaded in %ums)",
		gHash.size(), (u32)(((qpc_end-qpc_Start)*1000) / GetTickFrequency()) );

	return *this;
}

AppGameDatabase* Pcsx2App::GetGameDatabase()
{
	pxAppResources& res( GetResourceCache() );

	ScopedLock lock( m_mtx_LoadingGameDB );
	if( !res.GameDB )
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
