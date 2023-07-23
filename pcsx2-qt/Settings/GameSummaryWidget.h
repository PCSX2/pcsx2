/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include <QtWidgets/QWidget>

#include "ui_GameSummaryWidget.h"

namespace GameList
{
	struct Entry;
}

class SettingsDialog;

class GameSummaryWidget : public QWidget
{
	Q_OBJECT

public:
	GameSummaryWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent);
	~GameSummaryWidget();

private Q_SLOTS:
	void onInputProfileChanged(int index);
	void onDiscPathChanged(const QString& value);
	void onDiscPathBrowseClicked();
	void onVerifyClicked();
	void onSearchHashClicked();

private:
	void populateInputProfiles();
	void populateDetails(const GameList::Entry* entry);
	void populateDiscPath(const GameList::Entry* entry);
	void populateTrackList(const GameList::Entry* entry);
	void setVerifyResult(QString error);
	void repopulateCurrentDetails();

	void setCustomTitle(const std::string& text);
	void setCustomRegion(int region);

	Ui::GameSummaryWidget m_ui;
	SettingsDialog* m_dialog;
	std::string m_entry_path;
	std::string m_redump_search_keyword;
};
