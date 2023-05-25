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

#include "ui_GameCheatSettingsWidget.h"

#include "pcsx2/Patch.h"

#include "common/HeterogeneousContainers.h"

#include <string>
#include <string_view>
#include <vector>

namespace GameList
{
	struct Entry;
}

class SettingsDialog;

class GameCheatSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GameCheatSettingsWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent);
	~GameCheatSettingsWidget();

private Q_SLOTS:
	void onCheatListItemDoubleClicked(QTreeWidgetItem* item, int column);
	void onCheatListItemChanged(QTreeWidgetItem* item, int column);
	void onReloadClicked();
	void updateListEnabled();

private:
	QTreeWidgetItem* getTreeWidgetParent(const std::string_view& parent);
	void populateTreeWidgetItem(QTreeWidgetItem* item, const Patch::PatchInfo& pi, bool enabled);
	void setCheatEnabled(std::string name, bool enabled, bool save_and_reload_settings);
	void setStateForAll(bool enabled);
	void setStateRecursively(QTreeWidgetItem* parent, bool enabled);
	void reloadList();

	Ui::GameCheatSettingsWidget m_ui;
	SettingsDialog* m_dialog;

	std::string m_serial;
	u32 m_crc;

	UnorderedStringMap<QTreeWidgetItem*> m_parent_map;
	std::vector<Patch::PatchInfo> m_patches;
	std::vector<std::string> m_enabled_patches;
};
