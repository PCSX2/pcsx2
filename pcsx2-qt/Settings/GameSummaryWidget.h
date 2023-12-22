// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_GameSummaryWidget.h"

namespace GameList
{
	struct Entry;
}

class SettingsWindow;

class GameSummaryWidget : public QWidget
{
	Q_OBJECT

public:
	GameSummaryWidget(const GameList::Entry* entry, SettingsWindow* dialog, QWidget* parent);
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
	SettingsWindow* m_dialog;
	std::string m_entry_path;
	std::string m_redump_search_keyword;
};
