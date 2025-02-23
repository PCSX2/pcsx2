// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SavedAddressesWidget.h"

#include "QtUtils.h"
#include "Debugger/DebuggerSettingsManager.h"

#include <QClipboard>
#include <QMenu>

SavedAddressesWidget::SavedAddressesWidget(const DebuggerWidgetParameters& parameters)
	: DebuggerWidget(parameters, DISALLOW_MULTIPLE_INSTANCES)
	, m_model(new SavedAddressesModel(cpu(), this))
{
	m_ui.setupUi(this);

	m_ui.savedAddressesList->setModel(m_model);
	m_ui.savedAddressesList->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(g_emu_thread, &EmuThread::onGameChanged, this, [this](const QString& title) {
		if (title.isEmpty())
			return;

		if (m_model->rowCount() == 0)
			DebuggerSettingsManager::loadGameSettings(m_model);
	});

	DebuggerSettingsManager::loadGameSettings(m_model);

	connect(
		m_ui.savedAddressesList,
		&QTableView::customContextMenuRequested,
		this,
		&SavedAddressesWidget::openContextMenu);

	for (std::size_t i = 0; auto mode : SavedAddressesModel::HeaderResizeModes)
	{
		m_ui.savedAddressesList->horizontalHeader()->setSectionResizeMode(i++, mode);
	}
	QTableView* savedAddressesTableView = m_ui.savedAddressesList;
	connect(m_model, &QAbstractItemModel::dataChanged, [savedAddressesTableView](const QModelIndex& topLeft) {
		savedAddressesTableView->resizeColumnToContents(topLeft.column());
	});

	receiveEvent<DebuggerEvents::AddToSavedAddresses>([this](const DebuggerEvents::AddToSavedAddresses& event) {
		addAddress(event.address);
		return true;
	});
}

void SavedAddressesWidget::openContextMenu(QPoint pos)
{
	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* new_action = new QAction(tr("New"), menu);
	connect(new_action, &QAction::triggered, this, &SavedAddressesWidget::contextNew);
	menu->addAction(new_action);

	const QModelIndex index_at_pos = m_ui.savedAddressesList->indexAt(pos);
	const bool is_index_valid = index_at_pos.isValid();
	bool is_cpu_alive = cpu().isAlive();

	std::vector<QAction*> go_to_actions = createEventActions<DebuggerEvents::GoToAddress>(
		menu, [this, index_at_pos]() -> std::optional<DebuggerEvents::GoToAddress> {
			const QModelIndex rowAddressIndex = m_model->index(index_at_pos.row(), 0, QModelIndex());

			DebuggerEvents::GoToAddress event;
			event.address = m_model->data(rowAddressIndex, Qt::UserRole).toUInt();
			return event;
		});

	for (QAction* go_to_action : go_to_actions)
		go_to_action->setEnabled(is_index_valid);

	QAction* copy_action = new QAction(index_at_pos.column() == 0 ? tr("Copy Address") : tr("Copy Text"), menu);
	copy_action->setEnabled(is_index_valid);
	connect(copy_action, &QAction::triggered, [this, index_at_pos]() {
		QGuiApplication::clipboard()->setText(
			m_model->data(index_at_pos, Qt::DisplayRole).toString());
	});
	menu->addAction(copy_action);

	if (m_model->rowCount() > 0)
	{
		QAction* copy_all_as_csv_action = new QAction(tr("Copy all as CSV"), menu);
		connect(copy_all_as_csv_action, &QAction::triggered, [this]() {
			QGuiApplication::clipboard()->setText(
				QtUtils::AbstractItemModelToCSV(m_ui.savedAddressesList->model(), Qt::DisplayRole, true));
		});
		menu->addAction(copy_all_as_csv_action);
	}

	QAction* paste_from_csv_action = new QAction(tr("Paste from CSV"), menu);
	connect(paste_from_csv_action, &QAction::triggered, this, &SavedAddressesWidget::contextPasteCSV);
	menu->addAction(paste_from_csv_action);

	QAction* load_action = new QAction(tr("Load from Settings"), menu);
	load_action->setEnabled(is_cpu_alive);
	connect(load_action, &QAction::triggered, [this]() {
		m_model->clear();
		DebuggerSettingsManager::loadGameSettings(m_model);
	});
	menu->addAction(load_action);

	QAction* save_action = new QAction(tr("Save to Settings"), menu);
	save_action->setEnabled(is_cpu_alive);
	connect(save_action, &QAction::triggered, this, &SavedAddressesWidget::saveToDebuggerSettings);
	menu->addAction(save_action);

	QAction* delete_action = new QAction(tr("Delete"), menu);
	connect(delete_action, &QAction::triggered, this, [this, index_at_pos]() {
		m_model->removeRows(index_at_pos.row(), 1);
	});
	delete_action->setEnabled(is_index_valid);
	menu->addAction(delete_action);

	menu->popup(m_ui.savedAddressesList->viewport()->mapToGlobal(pos));
}

void SavedAddressesWidget::contextPasteCSV()
{
	QString csv = QGuiApplication::clipboard()->text();
	// Skip header
	csv = csv.mid(csv.indexOf('\n') + 1);

	for (const QString& line : csv.split('\n'))
	{
		QStringList fields;
		// In order to handle text with commas in them we must wrap values in quotes to mark
		// where a value starts and end so that text commas aren't identified as delimiters.
		// So matches each quote pair, parse it out, and removes the quotes to get the value.
		QRegularExpression each_quote_pair(R"("([^"]|\\.)*")");
		QRegularExpressionMatchIterator it = each_quote_pair.globalMatch(line);
		while (it.hasNext())
		{
			QRegularExpressionMatch match = it.next();
			QString matched_value = match.captured(0);
			fields << matched_value.mid(1, matched_value.length() - 2);
		}

		m_model->loadSavedAddressFromFieldList(fields);
	}
}

void SavedAddressesWidget::contextNew()
{
	qobject_cast<SavedAddressesModel*>(m_ui.savedAddressesList->model())->addRow();
	const u32 row_count = m_model->rowCount();
	m_ui.savedAddressesList->edit(m_model->index(row_count - 1, 0));
}

void SavedAddressesWidget::addAddress(u32 address)
{
	qobject_cast<SavedAddressesModel*>(m_ui.savedAddressesList->model())->addRow();
	const u32 row_count = m_model->rowCount();
	const QModelIndex address_index = m_model->index(row_count - 1, 0);
	m_model->setData(address_index, address, Qt::UserRole);
	m_ui.savedAddressesList->edit(m_model->index(row_count - 1, 1));
}

void SavedAddressesWidget::saveToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(m_model);
}
