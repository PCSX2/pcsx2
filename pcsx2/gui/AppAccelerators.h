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

#include "AppCommon.h"

#include <string>
#include <unordered_map>

// --------------------------------------------------------------------------------------
//  KeyAcceleratorCode
//  A custom keyboard accelerator that I like better than wx's wxAcceleratorEntry.
// --------------------------------------------------------------------------------------
struct KeyAcceleratorCode
{
	union
	{
		struct
		{
			u16 keycode;
			u16 win : 1, // win32 only.
				cmd : 1, // ctrl in win32, Command in Mac
				alt : 1,
				shift : 1;
		};
		u32 val32;
	};

	KeyAcceleratorCode()
		: val32(0)
	{
	}
	KeyAcceleratorCode(const wxKeyEvent& evt);

	//grab event attributes only
	KeyAcceleratorCode(const wxAcceleratorEntry& right)
	{
		val32 = 0;
		keycode = right.GetKeyCode();
		if (right.GetFlags() & wxACCEL_ALT)
			Alt();
		if (right.GetFlags() & wxACCEL_CMD)
			Cmd();
		if (right.GetFlags() & wxACCEL_SHIFT)
			Shift();
	}

	KeyAcceleratorCode(wxKeyCode code)
	{
		val32 = 0;
		keycode = code;
	}

	KeyAcceleratorCode(u32 value)
	{
		val32 = value;
	}

	KeyAcceleratorCode& Shift(bool enabled = true)
	{
		shift = enabled;
		return *this;
	}

	KeyAcceleratorCode& Alt(bool enabled = true)
	{
		alt = enabled;
		return *this;
	}

	KeyAcceleratorCode& Win(bool enabled = true)
	{
		win = enabled;
		return *this;
	}

	KeyAcceleratorCode& Cmd(bool enabled = true)
	{
		cmd = enabled;
		return *this;
	}

	wxString ToString() const;

	// Capitalizes the key portion of an accelerator code for displaying to the UI
	// ie. Shift-a becomes Shift-A / Ctrl+Shift+a becomes Ctrl+Shift+A
	wxString toTitleizedString() const
	{
		if (val32 == 0)
			return wxEmptyString;

		std::vector<wxString> tokens;
		wxStringTokenizer tokenizer(ToString(), "+");
		while (tokenizer.HasMoreTokens())
			tokens.push_back(tokenizer.GetNextToken());

		if (tokens.size() == 1)
			return tokens.at(0);
		else if (tokens.size() < 1)
			return wxEmptyString;

		wxString lastToken = tokens.at(tokens.size() - 1);
		// If the final token is a key that is multiple characters. For example 'Tab' or 'Esc'.  There is no need to modify it
		// Otherwise, it could be a normal letter key, so we capitalize it for stylistic reasons.
		if (lastToken.Length() == 1)
		{
			tokens.at(tokens.size() - 1) = lastToken[0] = wxToupper(lastToken[0]);
		}
		wxString modifiedKeyCode;
		for (int i = 0; i < (int)tokens.size(); i++)
		{
			if (i == (int)(tokens.size() - 1))
				modifiedKeyCode.append(tokens.at(i));
			else
				modifiedKeyCode.append(wxString::Format("%s+", tokens.at(i)));
		}
		return modifiedKeyCode;
	}
};


// --------------------------------------------------------------------------------------
//  GlobalCommandDescriptor
// --------------------------------------------------------------------------------------
//  Describes a global command which can be invoked from the main GUI.

struct GlobalCommandDescriptor
{
	const char* Id;   // Identifier string
	void (*Invoke)(); // Do it!!  Do it NOW!!!

	const wxChar* Fullname; // Name displayed in pulldown menus
	const wxChar* Tooltip;  // text displayed in toolbar tooltips and menu status bars.

	bool AlsoApplyToGui; // Indicates that the GUI should be updated if possible.

	wxString keycodeString;
};

// --------------------------------------------------------------------------------------
//  CommandDictionary
// --------------------------------------------------------------------------------------
class CommandDictionary : public std::unordered_map<std::string, const GlobalCommandDescriptor*>
{
	typedef std::unordered_map<std::string, const GlobalCommandDescriptor*> _parent;

protected:
public:
	using _parent::operator[];
	virtual ~CommandDictionary() = default;
};

// --------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------
class AcceleratorDictionary : public std::unordered_map<int, const GlobalCommandDescriptor*>
{
	typedef std::unordered_map<int, const GlobalCommandDescriptor*> _parent;

protected:
public:
	using _parent::operator[];

	virtual ~AcceleratorDictionary() = default;
	void Map(const KeyAcceleratorCode& acode, const char* searchfor);
	// Searches the dictionary _by the value (command ID string)_ and returns
	// the associated KeyAcceleratorCode.  Do not expect constant time lookup
	// Returns a blank KeyAcceleratorCode if nothing is found
	KeyAcceleratorCode findKeycodeWithCommandId(const char* commandId);
};
