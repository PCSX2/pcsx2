// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SavedAddressesView.h"

#include "QtUtils.h"
#include "Debugger/DebuggerSettingsManager.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

SavedAddressesView::SavedAddressesView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, DISALLOW_MULTIPLE_INSTANCES)
	, m_model(SavedAddressesModel::getInstance(cpu()))
{
	m_ui.setupUi(this);

	m_ui.savedAddressesList->setModel(m_model);

	m_ui.savedAddressesList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.savedAddressesList, &QTableView::customContextMenuRequested,
		this, &SavedAddressesView::openContextMenu);

	connect(g_emu_thread, &EmuThread::onGameChanged, this, [this](const QString& title) {
		if (title.isEmpty())
			return;

		if (m_model->rowCount() == 0)
			DebuggerSettingsManager::loadGameSettings(m_model);
	});

	DebuggerSettingsManager::loadGameSettings(m_model);

	for (std::size_t i = 0; auto mode : SavedAddressesModel::HeaderResizeModes)
	{
		m_ui.savedAddressesList->horizontalHeader()->setSectionResizeMode(i++, mode);
	}

	QTableView* savedAddressesTableView = m_ui.savedAddressesList;
	connect(m_model, &QAbstractItemModel::dataChanged, this, [savedAddressesTableView](const QModelIndex& topLeft) {
		savedAddressesTableView->resizeColumnToContents(topLeft.column());
	});

	receiveEvent<DebuggerEvents::AddToSavedAddresses>([this](const DebuggerEvents::AddToSavedAddresses& event) {
		addAddress(event.address);

		if (event.switch_to_tab)
			switchToThisTab();

		return true;
	});
}

void SavedAddressesView::openContextMenu(QPoint pos)
{
	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* new_action = menu->addAction(tr("New"));
	connect(new_action, &QAction::triggered, this, &SavedAddressesView::contextNew);

	const QModelIndex index_at_pos = m_ui.savedAddressesList->indexAt(pos);
	const bool is_index_valid = index_at_pos.isValid();
	bool is_cpu_alive = cpu().isAlive();

	std::vector<QAction*> go_to_actions = createEventActions<DebuggerEvents::GoToAddress>(
		menu, [this, index_at_pos]() {
			const QModelIndex rowAddressIndex = m_model->index(index_at_pos.row(), 0, QModelIndex());

			DebuggerEvents::GoToAddress event;
			event.address = m_model->data(rowAddressIndex, Qt::UserRole).toUInt();
			return std::optional(event);
		});

	for (QAction* go_to_action : go_to_actions)
		go_to_action->setEnabled(is_index_valid);

	QAction* copy_action = menu->addAction(index_at_pos.column() == 0 ? tr("Copy Address") : tr("Copy Text"));
	copy_action->setEnabled(is_index_valid);
	connect(copy_action, &QAction::triggered, [this, index_at_pos]() {
		QGuiApplication::clipboard()->setText(
			m_model->data(index_at_pos, Qt::DisplayRole).toString());
	});

	if (m_model->rowCount() > 0)
	{
		QAction* copy_all_as_csv_action = menu->addAction(tr("Copy all as CSV"));
		connect(copy_all_as_csv_action, &QAction::triggered, [this]() {
			QGuiApplication::clipboard()->setText(
				QtUtils::AbstractItemModelToCSV(m_ui.savedAddressesList->model(), Qt::DisplayRole, true));
		});
	}

	QAction* paste_from_csv_action = menu->addAction(tr("Paste from CSV"));
	connect(paste_from_csv_action, &QAction::triggered, this, &SavedAddressesView::contextPasteCSV);

	QAction* load_action = menu->addAction(tr("Load from Settings"));
	load_action->setEnabled(is_cpu_alive);
	connect(load_action, &QAction::triggered, [this]() {
		m_model->clear();
		DebuggerSettingsManager::loadGameSettings(m_model);
	});

	QAction* save_action = menu->addAction(tr("Save to Settings"));
	save_action->setEnabled(is_cpu_alive);
	connect(save_action, &QAction::triggered, this, &SavedAddressesView::saveToDebuggerSettings);

	QAction* delete_action = menu->addAction(tr("Delete"));
	connect(delete_action, &QAction::triggered, this, [this, index_at_pos]() {
		m_model->removeRows(index_at_pos.row(), 1);
	});
	delete_action->setEnabled(is_index_valid);

	menu->popup(m_ui.savedAddressesList->viewport()->mapToGlobal(pos));
}

void SavedAddressesView::contextPasteCSV()
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

void SavedAddressesView::contextNew()
{
	m_model->addRow();
	const u32 row_count = m_model->rowCount();
	m_ui.savedAddressesList->edit(m_model->index(row_count - 1, 0));
}

void SavedAddressesView::addAddress(u32 address)
{
	m_model->addRow();

	u32 row_count = m_model->rowCount();

	QModelIndex address_index = m_model->index(row_count - 1, SavedAddressesModel::ADDRESS);
	m_model->setData(address_index, address, Qt::UserRole);

	QModelIndex label_index = m_model->index(row_count - 1, SavedAddressesModel::LABEL);
	if (label_index.isValid())
		m_ui.savedAddressesList->edit(label_index);
}

void SavedAddressesView::saveToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(m_model);
}
