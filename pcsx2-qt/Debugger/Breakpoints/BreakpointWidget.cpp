// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BreakpointWidget.h"

#include "QtUtils.h"
#include "Debugger/DebuggerSettingsManager.h"
#include "BreakpointDialog.h"
#include "BreakpointModel.h"

#include <QtGui/QClipboard>

BreakpointWidget::BreakpointWidget(const DebuggerWidgetParameters& parameters)
	: DebuggerWidget(parameters, DISALLOW_MULTIPLE_INSTANCES)
	, m_model(BreakpointModel::getInstance(cpu()))
{
	m_ui.setupUi(this);

	m_ui.breakpointList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.breakpointList, &QTableView::customContextMenuRequested, this, &BreakpointWidget::openContextMenu);
	connect(m_ui.breakpointList, &QTableView::doubleClicked, this, &BreakpointWidget::onDoubleClicked);

	m_ui.breakpointList->setModel(m_model);
	for (std::size_t i = 0; auto mode : BreakpointModel::HeaderResizeModes)
	{
		m_ui.breakpointList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}
}

void BreakpointWidget::onDoubleClicked(const QModelIndex& index)
{
	if (index.isValid() && index.column() == BreakpointModel::OFFSET)
		goToInDisassembler(m_model->data(index, BreakpointModel::DataRole).toUInt(), true);
}

void BreakpointWidget::openContextMenu(QPoint pos)
{
	QMenu* menu = new QMenu(m_ui.breakpointList);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	if (cpu().isAlive())
	{
		QAction* newAction = menu->addAction(tr("New"));
		connect(newAction, &QAction::triggered, this, &BreakpointWidget::contextNew);

		const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

		if (selModel->hasSelection())
		{
			QAction* editAction = menu->addAction(tr("Edit"));
			connect(editAction, &QAction::triggered, this, &BreakpointWidget::contextEdit);

			if (selModel->selectedIndexes().count() == 1)
			{
				QAction* copyAction = menu->addAction(tr("Copy"));
				connect(copyAction, &QAction::triggered, this, &BreakpointWidget::contextCopy);
			}

			QAction* deleteAction = menu->addAction(tr("Delete"));
			connect(deleteAction, &QAction::triggered, this, &BreakpointWidget::contextDelete);
		}
	}

	menu->addSeparator();
	if (m_model->rowCount() > 0)
	{
		QAction* actionExport = menu->addAction(tr("Copy all as CSV"));
		connect(actionExport, &QAction::triggered, [this]() {
			// It's important to use the Export Role here to allow pasting to be translation agnostic
			QGuiApplication::clipboard()->setText(
				QtUtils::AbstractItemModelToCSV(m_model, BreakpointModel::ExportRole, true));
		});
	}

	if (cpu().isAlive())
	{
		QAction* actionImport = menu->addAction(tr("Paste from CSV"));
		connect(actionImport, &QAction::triggered, this, &BreakpointWidget::contextPasteCSV);

		if (cpu().getCpuType() == BREAKPOINT_EE)
		{
			QAction* actionLoad = menu->addAction(tr("Load from Settings"));
			connect(actionLoad, &QAction::triggered, [this]() {
				m_model->clear();
				DebuggerSettingsManager::loadGameSettings(m_model);
			});

			QAction* actionSave = menu->addAction(tr("Save to Settings"));
			connect(actionSave, &QAction::triggered, this, &BreakpointWidget::saveBreakpointsToDebuggerSettings);
		}
	}

	menu->popup(m_ui.breakpointList->viewport()->mapToGlobal(pos));
}

void BreakpointWidget::contextCopy()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	QGuiApplication::clipboard()->setText(m_model->data(selModel->currentIndex()).toString());
}

void BreakpointWidget::contextDelete()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	QModelIndexList indices = selModel->selectedIndexes();

	std::set<int> rows;
	for (QModelIndex index : indices)
		rows.emplace(index.row());

	for (auto row = rows.rbegin(); row != rows.rend(); row++)
		m_model->removeRows(*row, 1);
}

void BreakpointWidget::contextNew()
{
	BreakpointDialog* bpDialog = new BreakpointDialog(this, &cpu(), *m_model);
	bpDialog->setAttribute(Qt::WA_DeleteOnClose);
	bpDialog->show();
}

void BreakpointWidget::contextEdit()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	const int selectedRow = selModel->selectedIndexes().first().row();

	auto bpObject = m_model->at(selectedRow);

	BreakpointDialog* bpDialog = new BreakpointDialog(this, &cpu(), *m_model, bpObject, selectedRow);
	bpDialog->setAttribute(Qt::WA_DeleteOnClose);
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
		m_model->loadBreakpointFromFieldList(fields);
	}
}

void BreakpointWidget::saveBreakpointsToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(m_model);
}
