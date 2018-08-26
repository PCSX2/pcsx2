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
#include "GameDatabase.h"

BaseGameDatabaseImpl::BaseGameDatabaseImpl()
	: gHash( 9900 )
	, m_baseKey( L"Serial" )
{
}

// Sets the current game to the one matching the serial id given
// Returns true if game found, false if not found...
bool BaseGameDatabaseImpl::findGame(Game_Data& dest, const wxString& id) {

	GameDataHash::const_iterator iter( gHash.find(id) );
	if( iter == gHash.end() ) {
		dest.clear();
		return false;
	}
	dest = iter->second;
	return true;
}

Game_Data* BaseGameDatabaseImpl::createNewGame( const wxString& id )
{
	return &gHash.emplace(id, Game_Data{id}).first->second;
}

// Searches the current game's data to see if the given key exists
bool Game_Data::keyExists(const wxString& key) const {
	for (auto it = kList.begin(); it != kList.end(); ++it) {
		if (it->CompareKey(key)) {
			return true;
		}
	}
	return false;
}

// Gets a string representation of the 'value' for the given key
wxString Game_Data::getString(const wxString& key) const {
	for (auto it = kList.begin(); it != kList.end(); ++it) {
		if (it->CompareKey(key)) {
			return it->value;
		}
	}
	return wxString();
}

void Game_Data::writeString(const wxString& key, const wxString& value) {
	for (auto it = kList.begin(); it != kList.end(); ++it) {
		if (it->CompareKey(key)) {
			if( value.IsEmpty() )
				kList.erase(it);
			else
				it->value = value;
			return;
		}
	}
	if( !value.IsEmpty() ) {
		kList.push_back(key_pair(key, value));
	}
}
