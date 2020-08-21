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

#include "SavestateSlotPanel.h"

#include "AppConfig.h"
#include "SavestateTab.h"

#include "wx/wx.h"
#include "wx/notebook.h"

class GameManagerFrame : public wxFrame
{
public:
	GameManagerFrame(wxWindow* parent, AppConfig::GameManagerOptions& options);
	virtual ~GameManagerFrame() = default;

	SavestateTab* getSavestateTab();

private:
	enum MenuIds
	{
		MenuId_AutoDock,
		MenuId_Close,
		MenuId_OnlyShowSelectedBackup
	};

	AppConfig::GameManagerOptions& options;

	wxMenuBar* menu;
	wxBoxSizer* rootSizer;
	wxNotebook* tabContainer;
	SavestateTab* savestateTab;

	void InitPosition(wxWindow* parent);

	void OnAutoDock(wxCommandEvent& evt);
	void Close(wxCommandEvent& evt);
	void ShowOnlySelectedBackup(wxCommandEvent& evt);

	void OnMoveAround(wxMoveEvent& evt);
	void OnResize(wxSizeEvent& evt);
	// OnFocus / OnActivate : Special implementation to "connect" the console log window
	// with the main frame window.  When one is clicked, the other is assured to be brought
	// to the foreground with it.  (Currently only MSW only, as wxWidgets appears to have no
	// equivalent to this). We don't bother with OnFocus here because it doesn't propagate
	// up the window hierarchy anyway, so it always gets swallowed by the text control.
	// But no matter: the console doesn't have the same problem as the Main Window of missing
	// the initial activation event.
	void OnActivate(wxActivateEvent& evt);
	virtual void OnCloseWindow(wxCloseEvent& evt);
};
