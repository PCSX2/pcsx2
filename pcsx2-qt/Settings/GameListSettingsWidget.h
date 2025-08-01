// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_GameListSettingsWidget.h"

#include "SettingsWidget.h"

#include <string>

class GameListSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	GameListSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~GameListSettingsWidget();

	bool addExcludedPath(const std::string& path);
	void refreshExclusionList();

Q_SIGNALS:
	void preferEnglishGameListChanged();

public Q_SLOTS:
	void addSearchDirectory(QWidget* parent_widget);

private Q_SLOTS:
	void onDirectoryListContextMenuRequested(const QPoint& point);
	void onDirectoryListSelectionChanged();
	void onAddSearchDirectoryButtonClicked();
	void onRemoveSearchDirectoryButtonClicked();
	void onAddExcludedFileButtonClicked();
	void onAddExcludedPathButtonClicked();
	void onRemoveExcludedPathButtonClicked();
	void onExcludedPathsSelectionChanged();
	void onScanForNewGamesClicked();
	void onRescanAllGamesClicked();

protected:
	bool event(QEvent* event);

private:
	void addPathToTable(const std::string& path, bool recursive);
	void refreshDirectoryList();
	void addSearchDirectory(const QString& path, bool recursive);
	void removeSearchDirectory(const QString& path);

	Ui::GameListSettingsWidget m_ui;
};
