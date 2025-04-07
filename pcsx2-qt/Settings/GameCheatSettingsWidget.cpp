// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "Settings/GameCheatSettingsWidget.h"
#include "Settings/SettingsWindow.h"

#include "pcsx2/GameList.h"
#include "pcsx2/Patch.h"

#include "common/HeterogeneousContainers.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>

GameCheatSettingsWidget::GameCheatSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: m_dialog(dialog)
{
	m_ui.setupUi(this);

	m_model = new QStandardItemModel(this);

	QStringList headers;
	headers.push_back(tr("Name"));
	headers.push_back(tr("Author"));
	headers.push_back(tr("Description"));
	m_model->setHorizontalHeaderLabels(headers);

	m_model_proxy = new QSortFilterProxyModel(this);
	m_model_proxy->setSourceModel(m_model);
	m_model_proxy->setRecursiveFilteringEnabled(true);
	m_model_proxy->setAutoAcceptChildRows(true);
	m_model_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

	m_ui.cheatList->setModel(m_model_proxy);
	reloadList();

	m_ui.cheatList->expandAll();

	SettingsInterface* sif = m_dialog->getSettingsInterface();
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableCheats, "EmuCore", "EnableCheats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.allCRCsCheckbox, "EmuCore", "ShowCheatsForAllCRCs", false);
	updateListEnabled();

	connect(m_ui.enableCheats, &QCheckBox::checkStateChanged, this, &GameCheatSettingsWidget::updateListEnabled);
	connect(m_ui.cheatList, &QTreeView::doubleClicked, this, &GameCheatSettingsWidget::onCheatListItemDoubleClicked);
	connect(m_model, &QStandardItemModel::itemChanged, this, &GameCheatSettingsWidget::onCheatListItemChanged);
	connect(m_ui.reloadCheats, &QPushButton::clicked, this, &GameCheatSettingsWidget::onReloadClicked);
	connect(m_ui.enableAll, &QPushButton::clicked, this, [this]() { setStateForAll(true); });
	connect(m_ui.disableAll, &QPushButton::clicked, this, [this]() { setStateForAll(false); });
	connect(m_ui.allCRCsCheckbox, &QCheckBox::checkStateChanged, this, &GameCheatSettingsWidget::onReloadClicked);
	connect(m_ui.searchText, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_model_proxy->setFilterFixedString(text);
		m_ui.cheatList->expandAll();
	});
	connect(m_dialog, &SettingsWindow::discSerialChanged, this, &GameCheatSettingsWidget::reloadList);

	dialog->registerWidgetHelp(m_ui.allCRCsCheckbox, tr("Show Cheats For All CRCs"), tr("Checked"),
		tr("Toggles scanning patch files for all CRCs of the game. With this enabled available patches for the game serial with different CRCs will also be loaded."));
}

GameCheatSettingsWidget::~GameCheatSettingsWidget() = default;

void GameCheatSettingsWidget::onCheatListItemDoubleClicked(const QModelIndex& index)
{
	const QModelIndex source_index = m_model_proxy->mapToSource(index);
	const QModelIndex sibling_index = source_index.sibling(source_index.row(), 0);
	QStandardItem* item = m_model->itemFromIndex(sibling_index);
	if (!item)
		return;

	if (item->hasChildren() && index.column() != 0)
	{
		const QModelIndex view_sibling_index = index.sibling(index.row(), 0);
		const bool isExpanded = m_ui.cheatList->isExpanded(view_sibling_index);
		if (isExpanded)
			m_ui.cheatList->collapse(view_sibling_index);
		else
			m_ui.cheatList->expand(view_sibling_index);
		return;
	}

	QVariant data = item->data(Qt::UserRole);
	if (!data.isValid())
		return;

	std::string cheat_name = data.toString().toStdString();
	const bool new_state = !(item->checkState() == Qt::Checked);
	item->setCheckState(new_state ? Qt::Checked : Qt::Unchecked);
	setCheatEnabled(std::move(cheat_name), new_state, true);
}

void GameCheatSettingsWidget::onCheatListItemChanged(QStandardItem* item)
{
	QVariant data = item->data(Qt::UserRole);
	if (!data.isValid())
		return;

	std::string cheat_name = data.toString().toStdString();
	const bool current_enabled =
		(std::find(m_enabled_patches.begin(), m_enabled_patches.end(), cheat_name) != m_enabled_patches.end());
	const bool current_checked = (item->checkState() == Qt::Checked);
	if (current_enabled == current_checked)
		return;

	setCheatEnabled(std::move(cheat_name), current_checked, true);
}

void GameCheatSettingsWidget::onReloadClicked()
{
	reloadList();
	m_ui.cheatList->expandAll();

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
	m_ui.allCRCsCheckbox->setEnabled(cheats_enabled && !m_dialog->getSerial().empty());
	m_ui.searchText->setEnabled(cheats_enabled);
}

void GameCheatSettingsWidget::disableAllCheats()
{
	SettingsInterface* si = m_dialog->getSettingsInterface();
	si->ClearSection(Patch::CHEATS_CONFIG_SECTION);
	si->Save();
}

void GameCheatSettingsWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	QtUtils::ResizeColumnsForTreeView(m_ui.cheatList, {320, 100, -1});
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
	// Temporarily disconnect from itemChanged to prevent redundant saves
	disconnect(m_model, &QStandardItemModel::itemChanged, this, &GameCheatSettingsWidget::onCheatListItemChanged);

	setStateRecursively(nullptr, enabled);
	m_dialog->getSettingsInterface()->Save();
	g_emu_thread->reloadGameSettings();

	connect(m_model, &QStandardItemModel::itemChanged, this, &GameCheatSettingsWidget::onCheatListItemChanged);
}

void GameCheatSettingsWidget::setStateRecursively(QStandardItem* parent, bool enabled)
{
	const int count = parent ? parent->rowCount() : m_model->rowCount();
	for (int i = 0; i < count; i++)
	{
		QStandardItem* item = parent ? parent->child(i, 0) : m_model->item(i, 0);
		QVariant data = item->data(Qt::UserRole);
		if (data.isValid())
		{
			if ((item->checkState() == Qt::Checked) != enabled)
			{
				item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
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
	bool showAllCRCS = m_ui.allCRCsCheckbox->isChecked();
	m_patches = Patch::GetPatchInfo(m_dialog->getSerial(), m_dialog->getDiscCRC(), true, showAllCRCS, & num_unlabelled_codes);
	m_enabled_patches =
		m_dialog->getSettingsInterface()->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);

	m_parent_map.clear();
	m_model->removeRows(0, m_model->rowCount());
	m_ui.allCRCsCheckbox->setEnabled(!m_dialog->getSerial().empty() && m_ui.cheatList->isEnabled());

	for (const Patch::PatchInfo& pi : m_patches)
	{
		const bool enabled =
			(std::find(m_enabled_patches.begin(), m_enabled_patches.end(), pi.name) != m_enabled_patches.end());

		const std::string_view parent_part = pi.GetNameParentPart();
		QStandardItem* parent = getTreeViewParent(parent_part);
		QList<QStandardItem*> items = populateTreeViewRow(pi, enabled);
		if (parent)
			parent->appendRow(items);
		else
			m_model->appendRow(items);
	}

	// Hide root indicator when there's no groups, frees up some whitespace.
	m_ui.cheatList->setRootIsDecorated(!m_parent_map.empty());

	if (num_unlabelled_codes > 0)
	{
		QStandardItem* item = new QStandardItem();
		item->setText(tr("%1 unlabelled patch codes will automatically activate.").arg(num_unlabelled_codes));
		m_model->appendRow(item);
	}
}

QStandardItem* GameCheatSettingsWidget::getTreeViewParent(const std::string_view parent)
{
	if (parent.empty())
		return nullptr;

	auto it = m_parent_map.find(parent);
	if (it != m_parent_map.end())
		return it->second;

	std::string_view this_part = parent;
	QStandardItem* parent_to_this = nullptr;
	const std::string_view::size_type pos = parent.rfind('\\');
	if (pos != std::string::npos && pos != (parent.size() - 1))
	{
		// go up the chain until we find the real parent, then back down
		parent_to_this = getTreeViewParent(parent.substr(0, pos));
		this_part = parent.substr(pos + 1);
	}

	QStandardItem* item = new QStandardItem();
	item->setText(QString::fromUtf8(this_part.data(), this_part.length()));

	if (parent_to_this)
		parent_to_this->appendRow(item);
	else
		m_model->appendRow(item);

	m_parent_map.emplace(parent, item);
	return item;
}

QList<QStandardItem*> GameCheatSettingsWidget::populateTreeViewRow(const Patch::PatchInfo& pi, bool enabled)
{
	QList<QStandardItem*> items;

	QStandardItem* nameItem = new QStandardItem();
	const std::string_view name_part = pi.GetNamePart();
	nameItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);
	nameItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
	nameItem->setData(QString::fromStdString(pi.name), Qt::UserRole);
	if (!name_part.empty())
		nameItem->setText(QString::fromUtf8(name_part.data(), name_part.length()));

	QStandardItem* authorItem = new QStandardItem(QString::fromStdString(pi.author));
	QStandardItem* descriptionItem = new QStandardItem(QString::fromStdString(pi.description));
	descriptionItem->setToolTip(QString::fromStdString(pi.description));

	items.push_back(nameItem);
	items.push_back(authorItem);
	items.push_back(descriptionItem);
	return items;
}
