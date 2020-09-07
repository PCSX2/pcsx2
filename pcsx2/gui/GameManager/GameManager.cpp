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

#include "GameManager.h"

#include "App.h"
#include "MainFrame.h"
#include "MSWstuff.h"
#include "Saveslots.h"
#include "SavestateTab.h"

GameManagerFrame::GameManagerFrame(wxWindow* parent, AppConfig::GameManagerOptions& options)
	: wxFrame(parent, wxID_ANY, wxEmptyString)
	, options(options)
{
	// Sizers and Tabs
	rootSizer = new wxBoxSizer(wxHORIZONTAL);
	tabContainer = new wxNotebook(this, wxID_ANY);
	savestateTab = new SavestateTab(tabContainer, options);

	// Menu Creation
	menu = new wxMenuBar();
	wxMenu* tempMenu;
	tempMenu = new wxMenu();
	tempMenu->Append(MenuId_AutoDock, _("Autodock"), wxEmptyString, wxITEM_CHECK);
	tempMenu->AppendSeparator();
	tempMenu->Append(MenuId_Close, _("Close"), wxEmptyString);
	menu->Append(tempMenu, _("Options"));
	tempMenu = new wxMenu();
	// TODO - it may be a good idea to mirror the "Backup before Save" menu option into here
	tempMenu->Append(MenuId_OnlyShowSelectedBackup, _("Only show current slot's backup"), wxEmptyString, wxITEM_CHECK);
	/*
	-- Future Work - Might be nice to be able to create an archive of all savestates
	
	tempMenu->AppendSeparator();
	tempMenu->Append(wxID_ANY, _("Archive all savestates and backups"), wxEmptyString);
	*/
	menu->Append(tempMenu, _("Savestates"));
	/*
	-- Future Work - ISO Tab

	tempMenu = new wxMenu();
	tempMenu->Append(wxID_ANY, _("Clear ISO list"), wxEmptyString);
	tempMenu->Append(wxID_ANY, _("Clear non-existant ISOs"), wxEmptyString);
	tempMenu->AppendSeparator();
	tempMenu->Append(wxID_ANY, _("Update compatibility"), wxEmptyString);
	menu->Append(tempMenu, _("ISOs"));
	*/

	// Init Menubar from Saved Options
	menu->Check(MenuId_AutoDock, options.AutoDock);
	menu->Check(MenuId_OnlyShowSelectedBackup, options.DisplaySingleBackup);

	// Init Frame
	SetMenuBar(menu);
	tabContainer->AddPage(savestateTab, _("Savestates"));
	rootSizer->Add(tabContainer, 1, wxEXPAND, 0);
	SetSizer(rootSizer);
	SetTitle(_("PCSX2 Game Manager"));

	if (options.DisplaySize.GetWidth() <= 0)
		options.DisplaySize.SetWidth(parent->GetSize().GetWidth());
	SetSize(options.DisplaySize);
	InitPosition(parent);
	SetPosition(options.DisplayPosition);
	SetIcons(wxGetApp().GetIconBundle());
	Layout();
	Show(options.Visible);

	// Bind Menu Events
	Bind(wxEVT_MENU, &GameManagerFrame::OnAutoDock, this, MenuId_AutoDock);
	Bind(wxEVT_MENU, &GameManagerFrame::Close, this, MenuId_Close);
	Bind(wxEVT_MENU, &GameManagerFrame::ShowOnlySelectedBackup, this, MenuId_OnlyShowSelectedBackup);
	// Bind Window Events
	Bind(wxEVT_CLOSE_WINDOW, &GameManagerFrame::OnCloseWindow, this);
	Bind(wxEVT_MOVE, &GameManagerFrame::OnMoveAround, this);
	Bind(wxEVT_SIZE, &GameManagerFrame::OnResize, this);
	Bind(wxEVT_ACTIVATE, &GameManagerFrame::OnActivate, this);
}

void GameManagerFrame::InitPosition(wxWindow* parent)
{
	options.DisplaySize.Set(
		std::min(options.DisplaySize.GetWidth(), wxGetDisplayArea().GetWidth()),
		std::min(options.DisplaySize.GetHeight(), wxGetDisplayArea().GetHeight()));

	if (options.AutoDock)
	{
		int newX = parent->GetRect().GetBottomRight().x + 1 - options.DisplaySize.x;
		// Add some extra space to the Y coordinate to more closely match the gap between the console logger
		g_Conf->GameManager.DisplayPosition = wxPoint(newX, parent->GetRect().GetBottomRight().y + 16);
	}
	else if (options.DisplayPosition != wxDefaultPosition)
	{
		if (!wxGetDisplayArea().Contains(wxRect(options.DisplayPosition, options.DisplaySize)))
			options.DisplayPosition = wxDefaultPosition;
	}
}

SavestateTab* GameManagerFrame::getSavestateTab()
{
	return savestateTab;
}

void GameManagerFrame::OnAutoDock(wxCommandEvent& evt)
{
	if (auto menuBar = GetMenuBar())
		options.AutoDock = menuBar->IsChecked(MenuId_AutoDock);
	evt.Skip();
}

void GameManagerFrame::Close(wxCommandEvent& evt)
{
	if (auto menuBar = GetMenuBar())
		options.Visible = menuBar->IsChecked(MenuId_Close);
	Show(options.Visible);
	GetMainFramePtr()->OnGameManagerHidden();
	evt.Skip();
}

void GameManagerFrame::ShowOnlySelectedBackup(wxCommandEvent& evt)
{
	if (auto menuBar = GetMenuBar())
	{
		options.DisplaySingleBackup = menuBar->IsChecked(MenuId_OnlyShowSelectedBackup);
		savestateTab->updateBackupSlotVisibility();
		savestateTab->refreshSlots();
	}
	evt.Skip();
}


void GameManagerFrame::OnMoveAround(wxMoveEvent& evt)
{
	if (IsBeingDeleted() || !IsVisible() || IsIconized())
		return;

	if (!IsMaximized())
		options.DisplayPosition = GetPosition();
	evt.Skip();
}

void GameManagerFrame::OnResize(wxSizeEvent& evt)
{
	if (!IsMaximized())
		options.DisplaySize = GetSize();
	if (savestateTab)
		savestateTab->resizeSlotWidth(GetSize().GetWidth());
	evt.Skip();
}

void GameManagerFrame::OnActivate(wxActivateEvent& evt)
{
	if (MainEmuFrame* mainframe = GetMainFramePtr())
		MSW_SetWindowAfter(mainframe->GetHandle(), GetHandle());
	evt.Skip();
}

void GameManagerFrame::OnCloseWindow(wxCloseEvent& evt)
{
	// instead of closing just hide the window to be able to Show() it later
	if (evt.CanVeto())
	{
		Show(false);
		GetMainFramePtr()->OnGameManagerHidden();
	}
	else
	{
		evt.Skip();
	}
}
