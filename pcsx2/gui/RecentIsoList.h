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

// --------------------------------------------------------------------------------------
//  RecentIsoManager
// --------------------------------------------------------------------------------------
class RecentIsoManager : public wxEvtHandler,
	public EventListener_AppStatus
{
protected:
	struct RecentItem
	{
		wxString	Filename;
		wxMenuItem*	ItemPtr;

		RecentItem() { ItemPtr = NULL; }

		RecentItem( const wxString& src )
			: Filename( src )
		{
			ItemPtr = NULL;
		}
	};

public:
	using VectorType = std::vector<RecentItem>;

protected:
	VectorType m_Items;

	wxMenu*		m_Menu;
	uint		m_MaxLength;
	int			m_cursel;

	int m_firstIdForMenuItems_or_wxID_ANY;

	wxMenuItem* m_Separator;
	wxMenuItem* m_ClearSeparator;
	wxMenuItem* m_Clear;
	wxMenuItem* m_ClearMissing;

public:
	RecentIsoManager( wxMenu* menu , int firstIdForMenuItems_or_wxID_ANY );
	virtual ~RecentIsoManager();

	VectorType GetMissingFiles() const;

	void RemoveAllFromMenu();
	void EnableItems(bool display);
	void Repopulate();
	void Clear();
	void ClearMissing();
	void Add( const wxString& src );

protected:
	void InsertIntoMenu( int id );
	void OnChangedSelection( wxCommandEvent& evt );
	void LoadListFrom( IniInterface& ini );

	void AppStatusEvent_OnUiSettingsLoadSave( const AppSettingsEventInfo& ini );
	void AppStatusEvent_OnSettingsApplied();
};


// --------------------------------------------------------------------------------------
//  RecentIsoList
// --------------------------------------------------------------------------------------
struct RecentIsoList
{
	std::unique_ptr<RecentIsoManager>		Manager;
	std::unique_ptr<wxMenu>				Menu;

	RecentIsoList(int firstIdForMenuItems_or_wxID_ANY);
	virtual ~RecentIsoList() = default;
};

