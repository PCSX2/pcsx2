// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SavedAddressesWidget.h"

#include "QtUtils.h"
#include "Debugger/DebuggerSettingsManager.h"

#include <QClipboard>
#include <QMenu>

SavedAddressesWidget::SavedAddressesWidget(DebugInterface& cpu, QWidget* parent)
	: DebuggerWidget(&cpu, parent)
	, m_model(cpu)
{
	//m_ui.savedAddressesList->setModel(&m_model);
	//m_ui.savedAddressesList->setContextMenuPolicy(Qt::CustomContextMenu);
	//connect(m_ui.savedAddressesList, &QTableView::customContextMenuRequested, this, &CpuWidget::onSavedAddressesListContextMenu);
	//for (std::size_t i = 0; auto mode : SavedAddressesModel::HeaderResizeModes)
	//{
	//	m_ui.savedAddressesList->horizontalHeader()->setSectionResizeMode(i++, mode);
	//}
	//QTableView* savedAddressesTableView = m_ui.savedAddressesList;
	//connect(m_ui.savedAddressesList->model(), &QAbstractItemModel::dataChanged, [savedAddressesTableView](const QModelIndex& topLeft) {
	//	savedAddressesTableView->resizeColumnToContents(topLeft.column());
	//});
}

void SavedAddressesWidget::onContextMenu(QPoint pos)
{
	QMenu* contextMenu = new QMenu("Saved Addresses List Context Menu", m_ui.savedAddressesList);

	QAction* newAction = new QAction(tr("New"), m_ui.savedAddressesList);
	connect(newAction, &QAction::triggered, this, &SavedAddressesWidget::contextNew);
	contextMenu->addAction(newAction);

	const QModelIndex indexAtPos = m_ui.savedAddressesList->indexAt(pos);
	const bool isIndexValid = indexAtPos.isValid();

	if (isIndexValid)
	{
		if (cpu().isAlive())
		{
			QAction* goToAddressMemViewAction = new QAction(tr("Go to in Memory View"), m_ui.savedAddressesList);
			connect(goToAddressMemViewAction, &QAction::triggered, this, [this, indexAtPos]() {
				const QModelIndex rowAddressIndex = m_ui.savedAddressesList->model()->index(indexAtPos.row(), 0, QModelIndex());
				//m_ui.memoryviewWidget->gotoAddress(m_ui.savedAddressesList->model()->data(rowAddressIndex, Qt::UserRole).toUInt());
				//m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
				not_yet_implemented();
			});
			contextMenu->addAction(goToAddressMemViewAction);

			QAction* goToAddressDisassemblyAction = new QAction(tr("Go to in Disassembly"), m_ui.savedAddressesList);
			connect(goToAddressDisassemblyAction, &QAction::triggered, this, [this, indexAtPos]() {
				const QModelIndex rowAddressIndex =
					m_ui.savedAddressesList->model()->index(indexAtPos.row(), 0, QModelIndex());
				//m_ui.disassemblyWidget->gotoAddressAndSetFocus(
				//	m_ui.savedAddressesList->model()->data(rowAddressIndex, Qt::UserRole).toUInt());
				not_yet_implemented();
			});
			contextMenu->addAction(goToAddressDisassemblyAction);
		}

		QAction* copyAction = new QAction(indexAtPos.column() == 0 ? tr("Copy Address") : tr("Copy Text"), m_ui.savedAddressesList);
		connect(copyAction, &QAction::triggered, [this, indexAtPos]() {
			QGuiApplication::clipboard()->setText(
				m_ui.savedAddressesList->model()->data(indexAtPos, Qt::DisplayRole).toString());
		});
		contextMenu->addAction(copyAction);
	}

	if (m_ui.savedAddressesList->model()->rowCount() > 0)
	{
		QAction* actionExportCSV = new QAction(tr("Copy all as CSV"), m_ui.savedAddressesList);
		connect(actionExportCSV, &QAction::triggered, [this]() {
			QGuiApplication::clipboard()->setText(
				QtUtils::AbstractItemModelToCSV(m_ui.savedAddressesList->model(), Qt::DisplayRole, true));
		});
		contextMenu->addAction(actionExportCSV);
	}

	QAction* actionImportCSV = new QAction(tr("Paste from CSV"), m_ui.savedAddressesList);
	connect(actionImportCSV, &QAction::triggered, this, &SavedAddressesWidget::contextPasteCSV);
	contextMenu->addAction(actionImportCSV);

	if (cpu().isAlive())
	{
		QAction* actionLoad = new QAction(tr("Load from Settings"), m_ui.savedAddressesList);
		connect(actionLoad, &QAction::triggered, [this]() {
			m_model.clear();
			DebuggerSettingsManager::loadGameSettings(&m_model);
		});
		contextMenu->addAction(actionLoad);

		QAction* actionSave = new QAction(tr("Save to Settings"), m_ui.savedAddressesList);
		connect(actionSave, &QAction::triggered, this, &SavedAddressesWidget::saveToDebuggerSettings);
		contextMenu->addAction(actionSave);
	}

	if (isIndexValid)
	{
		QAction* deleteAction = new QAction(tr("Delete"), m_ui.savedAddressesList);
		connect(deleteAction, &QAction::triggered, this, [this, indexAtPos]() {
			m_ui.savedAddressesList->model()->removeRows(indexAtPos.row(), 1);
		});
		contextMenu->addAction(deleteAction);
	}

	contextMenu->popup(m_ui.savedAddressesList->viewport()->mapToGlobal(pos));
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
		QRegularExpression eachQuotePair(R"("([^"]|\\.)*")");
		QRegularExpressionMatchIterator it = eachQuotePair.globalMatch(line);
		while (it.hasNext())
		{
			QRegularExpressionMatch match = it.next();
			QString matchedValue = match.captured(0);
			fields << matchedValue.mid(1, matchedValue.length() - 2);
		}

		m_model.loadSavedAddressFromFieldList(fields);
	}
}

void SavedAddressesWidget::contextNew()
{
	qobject_cast<SavedAddressesModel*>(m_ui.savedAddressesList->model())->addRow();
	const u32 rowCount = m_ui.savedAddressesList->model()->rowCount();
	m_ui.savedAddressesList->edit(m_ui.savedAddressesList->model()->index(rowCount - 1, 0));
}

void SavedAddressesWidget::addAddress(u32 address)
{
	qobject_cast<SavedAddressesModel*>(m_ui.savedAddressesList->model())->addRow();
	const u32 rowCount = m_ui.savedAddressesList->model()->rowCount();
	const QModelIndex addressIndex = m_ui.savedAddressesList->model()->index(rowCount - 1, 0);
	//m_ui.tabWidget->setCurrentWidget(m_ui.tab_savedaddresses);
	not_yet_implemented();
	m_ui.savedAddressesList->model()->setData(addressIndex, address, Qt::UserRole);
	m_ui.savedAddressesList->edit(m_ui.savedAddressesList->model()->index(rowCount - 1, 1));
}

void SavedAddressesWidget::saveToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(&m_model);
}
