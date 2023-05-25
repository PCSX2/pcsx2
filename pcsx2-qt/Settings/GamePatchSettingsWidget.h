/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2023 PCSX2 Dev Team
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

#include "ui_GamePatchDetailsWidget.h"
#include "ui_GamePatchSettingsWidget.h"

#include "pcsx2/Patch.h"

namespace GameList
{
	struct Entry;
}

class SettingsDialog;

class GamePatchDetailsWidget : public QWidget
{
	Q_OBJECT

public:
	GamePatchDetailsWidget(std::string name, const std::string& author, const std::string& description, bool enabled,
		SettingsDialog* dialog, QWidget* parent);
	~GamePatchDetailsWidget();

private Q_SLOTS:
	void onEnabledStateChanged(int state);

private:
	Ui::GamePatchDetailsWidget m_ui;
	SettingsDialog* m_dialog;
	std::string m_name;
};

class GamePatchSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GamePatchSettingsWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent);
	~GamePatchSettingsWidget();

private Q_SLOTS:
	void onReloadClicked();

private:
	void reloadList();

	Ui::GamePatchSettingsWidget m_ui;
	SettingsDialog* m_dialog;

	std::string m_serial;
	u32 m_crc;
};
