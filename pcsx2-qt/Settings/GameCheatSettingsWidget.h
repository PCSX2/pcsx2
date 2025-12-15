// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtGui/QStandardItemModel>
#include <QtCore/QSortFilterProxyModel>

#include "ui_GameCheatSettingsWidget.h"

#include "SettingsWidget.h"

#include "pcsx2/Patch.h"

#include "common/HeterogeneousContainers.h"

#include <string>
#include <string_view>
#include <vector>

namespace GameList
{
	struct Entry;
}

class GameCheatSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	GameCheatSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~GameCheatSettingsWidget();

	void disableAllCheats();
	bool eventFilter(QObject* watched, QEvent* event) override;

protected:
	void resizeEvent(QResizeEvent* event) override;

private Q_SLOTS:
	void onCheatListItemDoubleClicked(const QModelIndex& index);
	void onCheatListItemChanged(QStandardItem* item);
	void onCheatListItemHovered(const QModelIndex& index);
	void onReloadClicked();
	void updateListEnabled();
	void reloadList();

private:
	QStandardItem* getTreeViewParent(const std::string_view parent);
	QList<QStandardItem*> populateTreeViewRow(const Patch::PatchInfo& pi, bool enabled);
	void setCheatEnabled(std::string name, bool enabled, bool save_and_reload_settings);
	void setStateForAll(bool enabled);
	void setStateRecursively(QStandardItem* parent, bool enabled);

	enum Roles
	{
		NAME_ROLE = Qt::UserRole,
		PLACE_ROLE = Qt::UserRole + 1
	};

	Ui::GameCheatSettingsWidget m_ui;
	QStandardItemModel* m_model = nullptr;
	QSortFilterProxyModel* m_model_proxy = nullptr;

	UnorderedStringMap<QStandardItem*> m_parent_map;
	std::vector<Patch::PatchInfo> m_patches;
	std::vector<std::string> m_enabled_patches;
};
