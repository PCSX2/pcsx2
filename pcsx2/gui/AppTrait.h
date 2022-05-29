/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include <wx/apptrait.h>

// --------------------------------------------------------------------------------------
//  Pcsx2AppTraits
// --------------------------------------------------------------------------------------
// Overrides and customizes some default wxWidgets behaviors.  This class is instanized by
// calls to Pcsx2App::CreateTraits(), which is called from wxWidgets as-needed.  wxWidgets
// does cache an instance of the traits, so the object construction need not be trivial
// (translation: it can be complicated-ish -- it won't affect performance).
//
class Pcsx2AppTraits : public wxGUIAppTraits
{
	typedef wxGUIAppTraits _parent;

public:
	virtual ~Pcsx2AppTraits() {}
	wxMessageOutput* CreateMessageOutput();

#ifdef wxUSE_STDPATHS
	wxStandardPaths& GetStandardPaths();
#endif
};
