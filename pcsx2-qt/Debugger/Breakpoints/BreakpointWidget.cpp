// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BreakpointWidget.h"

#include "QtUtils.h"
#include "Debugger/DebuggerSettingsManager.h"
#include "BreakpointDialog.h"
#include "BreakpointModel.h"

#include <QClipboard>

BreakpointWidget::BreakpointWidget(DebugInterface& cpu, QWidget* parent)
	: DebuggerWidget(&cpu, parent)
	, m_model(cpu)
{
	m_ui.setupUi(this);

	connect(m_ui.breakpointList, &QTableView::customContextMenuRequested, this, &BreakpointWidget::onContextMenu);
	connect(m_ui.breakpointList, &QTableView::doubleClicked, this, &BreakpointWidget::onDoubleClicked);

	m_ui.breakpointList->setModel(&m_model);
	for (std::size_t i = 0; auto mode : BreakpointModel::HeaderResizeModes)
	{
		m_ui.breakpointList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	connect(&m_model, &BreakpointModel::dataChanged, &m_model, &BreakpointModel::refreshData);
}

void BreakpointWidget::onDoubleClicked(const QModelIndex& index)
{
	if (index.isValid() && index.column() == BreakpointModel::OFFSET)
	{
		not_yet_implemented();
		//m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_model.data(index, BreakpointModel::DataRole).toUInt());
	}
}

void BreakpointWidget::onContextMenu(QPoint pos)
{
	QMenu* contextMenu = new QMenu(tr("Breakpoint List Context Menu"), m_ui.breakpointList);
	if (cpu().isAlive())
	{

		QAction* newAction = new QAction(tr("New"), m_ui.breakpointList);
		connect(newAction, &QAction::triggered, this, &BreakpointWidget::contextNew);
		contextMenu->addAction(newAction);

		const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

		if (selModel->hasSelection())
		{
			QAction* editAction = new QAction(tr("Edit"), m_ui.breakpointList);
			connect(editAction, &QAction::triggered, this, &BreakpointWidget::contextEdit);
			contextMenu->addAction(editAction);

			if (selModel->selectedIndexes().count() == 1)
			{
				QAction* copyAction = new QAction(tr("Copy"), m_ui.breakpointList);
				connect(copyAction, &QAction::triggered, this, &BreakpointWidget::contextCopy);
				contextMenu->addAction(copyAction);
			}

			QAction* deleteAction = new QAction(tr("Delete"), m_ui.breakpointList);
			connect(deleteAction, &QAction::triggered, this, &BreakpointWidget::contextDelete);
			contextMenu->addAction(deleteAction);
		}
	}

	contextMenu->addSeparator();
	if (m_model.rowCount() > 0)
	{
		QAction* actionExport = new QAction(tr("Copy all as CSV"), m_ui.breakpointList);
		connect(actionExport, &QAction::triggered, [this]() {
			// It's important to use the Export Role here to allow pasting to be translation agnostic
			QGuiApplication::clipboard()->setText(
				QtUtils::AbstractItemModelToCSV(m_ui.breakpointList->model(),
					BreakpointModel::ExportRole, true));
		});
		contextMenu->addAction(actionExport);
	}

	if (cpu().isAlive())
	{
		QAction* actionImport = new QAction(tr("Paste from CSV"), m_ui.breakpointList);
		connect(actionImport, &QAction::triggered, this, &BreakpointWidget::contextPasteCSV);
		contextMenu->addAction(actionImport);

		QAction* actionLoad = new QAction(tr("Load from Settings"), m_ui.breakpointList);
		connect(actionLoad, &QAction::triggered, [this]() {
			m_model.clear();
			DebuggerSettingsManager::loadGameSettings(&m_model);
		});
		contextMenu->addAction(actionLoad);

		QAction* actionSave = new QAction(tr("Save to Settings"), m_ui.breakpointList);
		connect(actionSave, &QAction::triggered, this, &BreakpointWidget::saveBreakpointsToDebuggerSettings);
		contextMenu->addAction(actionSave);
	}

	contextMenu->popup(m_ui.breakpointList->viewport()->mapToGlobal(pos));
}

void BreakpointWidget::contextCopy()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	QGuiApplication::clipboard()->setText(m_model.data(selModel->currentIndex()).toString());
}

void BreakpointWidget::contextDelete()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	QModelIndexList rows = selModel->selectedIndexes();

	std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) {
		return a.row() > b.row();
	});

	for (const QModelIndex& index : rows)
	{
		m_model.removeRows(index.row(), 1);
	}
}

void BreakpointWidget::contextNew()
{
	BreakpointDialog* bpDialog = new BreakpointDialog(this, &cpu(), m_model);
	bpDialog->show();
}

void BreakpointWidget::contextEdit()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	const int selectedRow = selModel->selectedIndexes().first().row();

	auto bpObject = m_model.at(selectedRow);

	BreakpointDialog* bpDialog = new BreakpointDialog(this, &cpu(), m_model, bpObject, selectedRow);
	bpDialog->show();
}

void BreakpointWidget::contextPasteCSV()
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
		m_model.loadBreakpointFromFieldList(fields);
	}
}

void BreakpointWidget::saveBreakpointsToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(&m_model);
}
