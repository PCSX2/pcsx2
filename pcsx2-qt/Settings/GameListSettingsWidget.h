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
#include <string>
#include <QtWidgets/QWidget>

#include "ui_GameListSettingsWidget.h"

class SettingsDialog;

class GameListSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GameListSettingsWidget(SettingsDialog* dialog, QWidget* parent);
	~GameListSettingsWidget();

	bool addExcludedPath(const std::string& path);
	void refreshExclusionList();

public Q_SLOTS:
	void addSearchDirectory(QWidget* parent_widget);

private Q_SLOTS:
	void onDirectoryListContextMenuRequested(const QPoint& point);
	void onAddSearchDirectoryButtonClicked();
	void onRemoveSearchDirectoryButtonClicked();
	void onAddExcludedFileButtonClicked();
	void onAddExcludedPathButtonClicked();
	void onRemoveExcludedPathButtonClicked();
	void onScanForNewGamesClicked();
	void onRescanAllGamesClicked();

protected:
	void resizeEvent(QResizeEvent* event);

private:
	void addPathToTable(const std::string& path, bool recursive);
	void refreshDirectoryList();
	void addSearchDirectory(const QString& path, bool recursive);
	void removeSearchDirectory(const QString& path);

	Ui::GameListSettingsWidget m_ui;
};
