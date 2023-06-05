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

#include "PrecompiledHeader.h"

#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "Settings/GameCheatSettingsWidget.h"
#include "Settings/SettingsDialog.h"

#include "pcsx2/GameList.h"
#include "pcsx2/Patch.h"

#include "common/HeterogeneousContainers.h"

GameCheatSettingsWidget::GameCheatSettingsWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent)
	: m_dialog(dialog)
	, m_serial(entry->serial)
	, m_crc(entry->crc)
{
	m_ui.setupUi(this);
	QtUtils::ResizeColumnsForTreeView(m_ui.cheatList, {300, 100, -1});

	reloadList();

	SettingsInterface* sif = m_dialog->getSettingsInterface();
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableCheats, "EmuCore", "EnableCheats", false);
	updateListEnabled();

	connect(m_ui.enableCheats, &QCheckBox::stateChanged, this, &GameCheatSettingsWidget::updateListEnabled);
	connect(m_ui.cheatList, &QTreeWidget::itemDoubleClicked, this, &GameCheatSettingsWidget::onCheatListItemDoubleClicked);
	connect(m_ui.cheatList, &QTreeWidget::itemChanged, this, &GameCheatSettingsWidget::onCheatListItemChanged);
	connect(m_ui.reloadCheats, &QPushButton::clicked, this, &GameCheatSettingsWidget::reloadList);
	connect(m_ui.enableAll, &QPushButton::clicked, this, [this]() { setStateForAll(true); });
	connect(m_ui.disableAll, &QPushButton::clicked, this, [this]() { setStateForAll(false); });
}

GameCheatSettingsWidget::~GameCheatSettingsWidget() = default;

void GameCheatSettingsWidget::onCheatListItemDoubleClicked(QTreeWidgetItem* item, int column)
{
	QVariant data = item->data(0, Qt::UserRole);
	if (!data.isValid())
		return;

	std::string cheat_name = data.toString().toStdString();
	const bool new_state = !(item->checkState(0) == Qt::Checked);
	item->setCheckState(0, new_state ? Qt::Checked : Qt::Unchecked);
	setCheatEnabled(std::move(cheat_name), new_state, true);
}

void GameCheatSettingsWidget::onCheatListItemChanged(QTreeWidgetItem* item, int column)
{
	QVariant data = item->data(0, Qt::UserRole);
	if (!data.isValid())
		return;

	std::string cheat_name = data.toString().toStdString();
	const bool current_enabled =
		(std::find(m_enabled_patches.begin(), m_enabled_patches.end(), cheat_name) != m_enabled_patches.end());
	const bool current_checked = (item->checkState(0) == Qt::Checked);
	if (current_enabled == current_checked)
		return;

	setCheatEnabled(std::move(cheat_name), current_checked, true);
}

void GameCheatSettingsWidget::onReloadClicked()
{
	reloadList();

	// reload it on the emu thread too, so it picks up any changes
	g_emu_thread->reloadPatches();
}

void GameCheatSettingsWidget::updateListEnabled()
{
	const bool cheats_enabled = m_dialog->getEffectiveBoolValue("EmuCore", "EnableCheats", false);
	m_ui.cheatList->setEnabled(cheats_enabled);
	m_ui.enableAll->setEnabled(cheats_enabled);
	m_ui.disableAll->setEnabled(cheats_enabled);
	m_ui.reloadCheats->setEnabled(cheats_enabled);
}

void GameCheatSettingsWidget::setCheatEnabled(std::string name, bool enabled, bool save_and_reload_settings)
{
	SettingsInterface* si = m_dialog->getSettingsInterface();
	auto it = std::find(m_enabled_patches.begin(), m_enabled_patches.end(), name);

	if (enabled)
	{
		si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, name.c_str());
		if (it == m_enabled_patches.end())
			m_enabled_patches.push_back(std::move(name));
	}
	else
	{
		si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, name.c_str());
		if (it != m_enabled_patches.end())
			m_enabled_patches.erase(it);
	}

	if (save_and_reload_settings)
	{
		si->Save();
		g_emu_thread->reloadGameSettings();
	}
}

void GameCheatSettingsWidget::setStateForAll(bool enabled)
{
	QSignalBlocker sb(m_ui.cheatList);
	setStateRecursively(nullptr, enabled);
	m_dialog->getSettingsInterface()->Save();
	g_emu_thread->reloadGameSettings();
}

void GameCheatSettingsWidget::setStateRecursively(QTreeWidgetItem* parent, bool enabled)
{
	const int count = parent ? parent->childCount() : m_ui.cheatList->topLevelItemCount();
	for (int i = 0; i < count; i++)
	{
		QTreeWidgetItem* item = parent ? parent->child(i) : m_ui.cheatList->topLevelItem(i);
		QVariant data = item->data(0, Qt::UserRole);
		if (data.isValid())
		{
			if ((item->checkState(0) == Qt::Checked) != enabled)
			{
				item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
				setCheatEnabled(data.toString().toStdString(), enabled, false);
			}
		}
		else
		{
			setStateRecursively(item, enabled);
		}
	}
}

void GameCheatSettingsWidget::reloadList()
{
	u32 num_unlabelled_codes = 0;
	m_patches = Patch::GetPatchInfo(m_serial, m_crc, true, &num_unlabelled_codes);
	m_enabled_patches =
		m_dialog->getSettingsInterface()->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);

	m_parent_map.clear();
	while (m_ui.cheatList->topLevelItemCount() > 0)
		delete m_ui.cheatList->takeTopLevelItem(0);

	for (const Patch::PatchInfo& pi : m_patches)
	{
		const bool enabled =
			(std::find(m_enabled_patches.begin(), m_enabled_patches.end(), pi.name) != m_enabled_patches.end());

		const std::string_view parent_part = pi.GetNameParentPart();

		QTreeWidgetItem* parent = getTreeWidgetParent(parent_part);
		QTreeWidgetItem* item = new QTreeWidgetItem();
		populateTreeWidgetItem(item, pi, enabled);
		if (parent)
			parent->addChild(item);
		else
			m_ui.cheatList->addTopLevelItem(item);
	}

	// Hide root indicator when there's no groups, frees up some whitespace.
	m_ui.cheatList->setRootIsDecorated(!m_parent_map.empty());

	if (num_unlabelled_codes > 0)
	{
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, tr("%1 unlabelled patch codes will automatically activate.").arg(num_unlabelled_codes));
		m_ui.cheatList->addTopLevelItem(item);
	}
}

QTreeWidgetItem* GameCheatSettingsWidget::getTreeWidgetParent(const std::string_view& parent)
{
	if (parent.empty())
		return nullptr;

	auto it = UnorderedStringMapFind(m_parent_map, parent);
	if (it != m_parent_map.end())
		return it->second;

	std::string_view this_part = parent;
	QTreeWidgetItem* parent_to_this = nullptr;
	const std::string_view::size_type pos = parent.rfind('\\');
	if (pos != std::string::npos && pos != (parent.size() - 1))
	{
		// go up the chain until we find the real parent, then back down
		parent_to_this = getTreeWidgetParent(parent.substr(0, pos));
		this_part = parent.substr(pos + 1);
	}

	QTreeWidgetItem* item = new QTreeWidgetItem();
	item->setText(0, QString::fromUtf8(this_part.data(), this_part.length()));

	if (parent_to_this)
		parent_to_this->addChild(item);
	else
		m_ui.cheatList->addTopLevelItem(item);

	// Must be called after adding.
	item->setExpanded(true);
	m_parent_map.emplace(parent, item);
	return item;
}

void GameCheatSettingsWidget::populateTreeWidgetItem(QTreeWidgetItem* item, const Patch::PatchInfo& pi, bool enabled)
{
	const std::string_view name_part = pi.GetNamePart();
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemNeverHasChildren);
	item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
	item->setData(0, Qt::UserRole, QString::fromStdString(pi.name));
	if (!name_part.empty())
		item->setText(0, QString::fromUtf8(name_part.data(), name_part.length()));
	item->setText(1, QString::fromStdString(pi.author));
	item->setText(2, QString::fromStdString(pi.description));
}
