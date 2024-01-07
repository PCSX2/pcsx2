// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

class SettingsWindow;

class GameCheatSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GameCheatSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~GameCheatSettingsWidget();

	void disableAllCheats();

protected:
	void resizeEvent(QResizeEvent* event) override;

private Q_SLOTS:
	void onCheatListItemDoubleClicked(QTreeWidgetItem* item, int column);
	void onCheatListItemChanged(QTreeWidgetItem* item, int column);
	void onReloadClicked();
	void updateListEnabled();
	void reloadList();

private:
	QTreeWidgetItem* getTreeWidgetParent(const std::string_view& parent);
	void populateTreeWidgetItem(QTreeWidgetItem* item, const Patch::PatchInfo& pi, bool enabled);
	void setCheatEnabled(std::string name, bool enabled, bool save_and_reload_settings);
	void setStateForAll(bool enabled);
	void setStateRecursively(QTreeWidgetItem* parent, bool enabled);

	Ui::GameCheatSettingsWidget m_ui;
	SettingsWindow* m_dialog;

	UnorderedStringMap<QTreeWidgetItem*> m_parent_map;
	std::vector<Patch::PatchInfo> m_patches;
	std::vector<std::string> m_enabled_patches;
};
