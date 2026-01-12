// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_GameSummaryWidget.h"

#include "SettingsWidget.h"

namespace GameList
{
	struct Entry;
}

class GameSummaryWidget : public SettingsWidget
{
	Q_OBJECT

public:
	GameSummaryWidget(const GameList::Entry* entry, SettingsWindow* settings_dialog, QWidget* parent);
	~GameSummaryWidget();

private Q_SLOTS:
	void onInputProfileChanged(int index);
	void onDiscPathChanged(const QString& value);
	void onDiscPathBrowseClicked();
	void onVerifyClicked();
	void onSearchHashClicked();
	void onCheckWikiClicked(const std::string& serial);

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
	std::string m_entry_path;
	std::string m_redump_search_keyword;
};
