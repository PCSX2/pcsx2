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

#pragma once

#include "AppCoreThread.h"

class DriveListManager : public wxEvtHandler
{
protected:
	struct DriveListItem
	{
		wxString driveLetter;
		wxMenuItem* itemPtr;

		DriveListItem() { itemPtr = NULL; }

		DriveListItem(const wxString& src)
			: driveLetter(src)
		{
			itemPtr = NULL;
		}
	};

	std::vector<std::unique_ptr<DriveListItem>> m_Items;
	wxMenu* m_Menu;

public:
	DriveListManager(wxMenu* menu);
	virtual ~DriveListManager() = default;

	void RefreshList();

protected:
	void ClearList();
	void OnChangedSelection(wxCommandEvent& evt);
	void OnRefreshClicked(wxCommandEvent& evt);
};

struct DriveList
{
	wxMenu* Menu;
	std::unique_ptr<DriveListManager> Manager;

	DriveList();
};

extern wxWindowID SwapOrReset_Disc(wxWindow* owner, IScopedCoreThread& core, const wxString driveLetter);
