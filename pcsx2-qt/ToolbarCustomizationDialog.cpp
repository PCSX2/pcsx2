// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ToolbarCustomizationDialog.h"
#include "MainWindow.h"
#include "QtHost.h"

ToolbarCustomizationDialog::ToolbarCustomizationDialog(QWidget* parent, std::vector<ToolbarActionInfo> action_registry)
	: QDialog(parent)
	, m_action_registry(std::move(action_registry))
{
	m_ui.setupUi(this);
	setWindowIcon(QtHost::GetAppIcon());

	m_ui.currentList->setDragDropMode(QAbstractItemView::InternalMove);
	m_ui.currentList->setDefaultDropAction(Qt::MoveAction);

	std::string layout_str = Host::GetBaseStringSettingValue("UI", "ToolbarItems", MainWindow::DEFAULT_TOOLBAR_LAYOUT);
	if (layout_str.empty())
		layout_str = MainWindow::DEFAULT_TOOLBAR_LAYOUT;
	populateFromLayout(QString::fromStdString(layout_str));

	connect(m_ui.addButton, &QPushButton::clicked, this, &ToolbarCustomizationDialog::onAddClicked);
	connect(m_ui.removeButton, &QPushButton::clicked, this, &ToolbarCustomizationDialog::onRemoveClicked);
	connect(m_ui.moveUpButton, &QPushButton::clicked, this, &ToolbarCustomizationDialog::onMoveUpClicked);
	connect(m_ui.moveDownButton, &QPushButton::clicked, this, &ToolbarCustomizationDialog::onMoveDownClicked);
	connect(m_ui.addSeparatorButton, &QPushButton::clicked, this, &ToolbarCustomizationDialog::onAddSeparatorClicked);
	connect(m_ui.buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
		&ToolbarCustomizationDialog::onRestoreDefaultsClicked);

	connect(m_ui.availableList, &QListWidget::itemDoubleClicked, this, &ToolbarCustomizationDialog::onAddClicked);
	connect(m_ui.currentList, &QListWidget::itemDoubleClicked, this, &ToolbarCustomizationDialog::onRemoveClicked);

	connect(m_ui.availableList, &QListWidget::itemSelectionChanged, this, &ToolbarCustomizationDialog::updateButtonStates);
	connect(m_ui.currentList, &QListWidget::itemSelectionChanged, this, &ToolbarCustomizationDialog::updateButtonStates);

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &ToolbarCustomizationDialog::onAccepted);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	updateButtonStates();
}

ToolbarCustomizationDialog::~ToolbarCustomizationDialog() = default;

void ToolbarCustomizationDialog::populateFromLayout(const QString& layout_str)
{
	m_ui.availableList->clear();
	m_ui.currentList->clear();

	const QStringList items = layout_str.split(QLatin1Char(','), Qt::KeepEmptyParts);
	QSet<QString> added_ids;

	QHash<QString, const ToolbarActionInfo*> registry_map;
	for (const ToolbarActionInfo& info : m_action_registry)
		registry_map.insert(QString::fromUtf8(info.id), &info);

	for (const QString& item_id : items)
	{
		const QString trimmed = item_id.trimmed();
		if (trimmed.isEmpty())
		{
			QListWidgetItem* item = new QListWidgetItem(tr("--- Separator ---"), m_ui.currentList);
			item->setData(Qt::UserRole, QString());
		}
		else
		{
			auto it = registry_map.find(trimmed);
			if (it != registry_map.end())
			{
				const ToolbarActionInfo* info = it.value();
				const QIcon icon = info->action ? info->action->icon() : QIcon();
				const QString text = info->action ? info->action->iconText() : QString();
				QListWidgetItem* item = new QListWidgetItem(icon, text, m_ui.currentList);
				item->setData(Qt::UserRole, QString::fromUtf8(info->id));
				added_ids.insert(QString::fromUtf8(info->id));
			}
		}
	}

	for (const ToolbarActionInfo& info : m_action_registry)
	{
		if (!added_ids.contains(QString::fromUtf8(info.id)))
		{
			const QIcon icon = info.action ? info.action->icon() : QIcon();
			const QString text = info.action ? info.action->iconText() : QString();
			QListWidgetItem* item = new QListWidgetItem(icon, text, m_ui.availableList);
			item->setData(Qt::UserRole, QString::fromUtf8(info.id));
		}
	}

	updateButtonStates();
}

void ToolbarCustomizationDialog::onRestoreDefaultsClicked()
{
	populateFromLayout(QString::fromUtf8(MainWindow::DEFAULT_TOOLBAR_LAYOUT));
}

void ToolbarCustomizationDialog::onAddClicked()
{
	QListWidgetItem* taken = m_ui.availableList->takeItem(m_ui.availableList->currentRow());
	if (!taken)
		return;

	int target_row = m_ui.currentList->currentRow();
	if (target_row < 0)
		target_row = m_ui.currentList->count();
	else
		target_row++;

	m_ui.currentList->insertItem(target_row, taken);
	m_ui.currentList->setCurrentRow(target_row);
	updateButtonStates();
}

void ToolbarCustomizationDialog::onRemoveClicked()
{
	QListWidgetItem* current = m_ui.currentList->currentItem();
	if (!current)
		return;

	const int row = m_ui.currentList->row(current);
	const QString id = current->data(Qt::UserRole).toString();

	if (id.isEmpty())
	{
		delete m_ui.currentList->takeItem(row);
	}
	else
	{
		QListWidgetItem* taken = m_ui.currentList->takeItem(row);
		m_ui.availableList->addItem(taken);
		m_ui.availableList->setCurrentItem(taken);
	}

	updateButtonStates();
}

void ToolbarCustomizationDialog::onMoveUpClicked()
{
	const int row = m_ui.currentList->currentRow();
	if (row <= 0)
		return;

	QListWidgetItem* item = m_ui.currentList->takeItem(row);
	m_ui.currentList->insertItem(row - 1, item);
	m_ui.currentList->setCurrentRow(row - 1);
	updateButtonStates();
}

void ToolbarCustomizationDialog::onMoveDownClicked()
{
	const int row = m_ui.currentList->currentRow();
	if (row < 0 || row >= m_ui.currentList->count() - 1)
		return;

	QListWidgetItem* item = m_ui.currentList->takeItem(row);
	m_ui.currentList->insertItem(row + 1, item);
	m_ui.currentList->setCurrentRow(row + 1);
	updateButtonStates();
}

void ToolbarCustomizationDialog::onAddSeparatorClicked()
{
	int target_row = m_ui.currentList->currentRow();
	if (target_row < 0)
		target_row = m_ui.currentList->count();
	else
		target_row++;

	QListWidgetItem* item = new QListWidgetItem(tr("--- Separator ---"));
	item->setData(Qt::UserRole, QString());
	m_ui.currentList->insertItem(target_row, item);
	m_ui.currentList->setCurrentRow(target_row);
	updateButtonStates();
}

void ToolbarCustomizationDialog::onAccepted()
{
	QStringList items;
	for (int i = 0; i < m_ui.currentList->count(); ++i)
	{
		const QListWidgetItem* item = m_ui.currentList->item(i);
		items.append(item->data(Qt::UserRole).toString());
	}

	const QString layout_str = items.join(QLatin1Char(','));
	if (layout_str == QLatin1StringView(MainWindow::DEFAULT_TOOLBAR_LAYOUT))
		Host::RemoveBaseSettingValue("UI", "ToolbarItems");
	else
		Host::SetBaseStringSettingValue("UI", "ToolbarItems", layout_str.toUtf8().constData());

	Host::CommitBaseSettingChanges();
	accept();
}

void ToolbarCustomizationDialog::updateButtonStates()
{
	const bool has_available_selection = (m_ui.availableList->currentItem() != nullptr);
	const bool has_current_selection = (m_ui.currentList->currentItem() != nullptr);
	const int current_row = m_ui.currentList->currentRow();
	const int current_count = m_ui.currentList->count();

	m_ui.addButton->setEnabled(has_available_selection);
	m_ui.removeButton->setEnabled(has_current_selection);
	m_ui.moveUpButton->setEnabled(has_current_selection && current_row > 0);
	m_ui.moveDownButton->setEnabled(has_current_selection && current_row >= 0 && current_row < current_count - 1);
}

#include "moc_ToolbarCustomizationDialog.cpp"
