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

#pragma once

//#include "Common.h"
#include "AppConfig.h"

// _Target_ is defined by R300A.h and R5900.h and the definition leaks to here.
// The problem, at least with Visual Studio 2019 on Windows,
// is that unordered_map includes xhash which uses _Target_ as a template
// parameter. Unless we undef it here, the build breaks with a cryptic error message.
#undef _Target_
#include <unordered_map>
#include <wx/wfstream.h>

struct	key_pair;
struct	Game_Data;

struct StringHash
{
	std::size_t operator()( const wxString& src ) const
	{
#ifdef _WIN32
		return std::hash<std::wstring>{}(src.ToStdWstring());
#else
		return std::hash<std::string>{}({src.utf8_str()});
#endif
	}
};


typedef std::vector<key_pair>	KeyPairArray;

struct key_pair {
	wxString key;
	wxString value;
	
	key_pair() {}
	key_pair(const wxString& _key, const wxString& _value)
		: key(_key) , value(_value) {}

	void Clear() {
		key.clear();
		value.clear();
	}

	// Performs case-insensitive compare against the key value.
	bool CompareKey( const wxString& cmpto ) const {
		return key.CmpNoCase(cmpto) == 0;
	}
	
	bool IsOk() const {
		return !key.IsEmpty();
	}
};

// --------------------------------------------------------------------------------------
//  Game_Data
// --------------------------------------------------------------------------------------
struct Game_Data
{
	wxString		id;				// Serial Identification Code
	KeyPairArray	kList;			// List of all (key, value) pairs for game data

	Game_Data(const wxString& _id = wxEmptyString)
		: id(_id) {}
	
	// Performs a case-insensitive compare of two IDs, returns TRUE if the IDs match
	// or FALSE if the ids differ in a case-insensitive way.
	bool CompareId( const wxString& _id ) const {
		return id.CmpNoCase(_id) == 0;
	}
	
	void clear() {
		id.clear();
		kList.clear();
	}

	bool keyExists(const wxString& key) const;
	wxString getString(const wxString& key) const;
	void writeString(const wxString& key, const wxString& value);

	bool IsOk() const {
		return !id.IsEmpty();
	}

	bool sectionExists(const wxString& key, const wxString& value) const {
		return keyExists("[" + key + (value.empty() ? "" : " = ") + value + "]");
	}

	wxString getSection(const wxString& key, const wxString& value) const {
		return getString("[" + key + (value.empty() ? "" : " = ") + value + "]");
	}

	// Gets an integer representation of the 'value' for the given key
	int getInt(const wxString& key) const {
		unsigned long val;
		getString(key).ToULong(&val);
		return val;
	}

	// Gets a u8 representation of the 'value' for the given key
	u8 getU8(const wxString& key) const {
		return (u8)wxAtoi(getString(key));
	}

	// Gets a bool representation of the 'value' for the given key
	bool getBool(const wxString& key) const {
		return !!wxAtoi(getString(key));
	}
};

// --------------------------------------------------------------------------------------
//  IGameDatabase
// --------------------------------------------------------------------------------------
class IGameDatabase
{
public:
	virtual ~IGameDatabase() = default;

	virtual wxString getBaseKey() const=0;
	virtual bool findGame(Game_Data& dest, const wxString& id)=0;
	virtual Game_Data* createNewGame( const wxString& id )=0;
};

using GameDataHash = std::unordered_map<wxString, Game_Data, StringHash>;

// --------------------------------------------------------------------------------------
//  BaseGameDatabaseImpl 
// --------------------------------------------------------------------------------------
class BaseGameDatabaseImpl : public IGameDatabase
{
protected:
	GameDataHash	gHash;			// hash table of game serials matched to their gList indexes!
	wxString		m_baseKey;

public:
	BaseGameDatabaseImpl();
	virtual ~BaseGameDatabaseImpl() = default;

	wxString getBaseKey() const { return m_baseKey; }
	void setBaseKey( const wxString& key ) { m_baseKey = key; }

	bool findGame(Game_Data& dest, const wxString& id);
	Game_Data* createNewGame( const wxString& id );
};

extern IGameDatabase* AppHost_GetGameDatabase();
