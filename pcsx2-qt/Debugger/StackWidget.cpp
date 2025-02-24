// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "StackWidget.h"

#include "QtUtils.h"

#include <QClipboard>
#include <QMenu>

StackWidget::StackWidget(const DebuggerWidgetParameters& parameters)
	: DebuggerWidget(parameters, NO_DEBUGGER_FLAGS)
	, m_model(new StackModel(cpu()))
{
	m_ui.setupUi(this);

	connect(m_ui.stackList, &QTableView::customContextMenuRequested, this, &StackWidget::openContextMenu);
	connect(m_ui.stackList, &QTableView::doubleClicked, this, &StackWidget::onDoubleClick);

	m_ui.stackList->setModel(m_model);
	for (std::size_t i = 0; auto mode : StackModel::HeaderResizeModes)
	{
		m_ui.stackList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	receiveEvent<DebuggerEvents::VMUpdate>([this](const DebuggerEvents::VMUpdate& event) -> bool {
		m_model->refreshData();
		return true;
	});
}

void StackWidget::openContextMenu(QPoint pos)
{
	if (!m_ui.stackList->selectionModel()->hasSelection())
		return;

	QMenu* menu = new QMenu(tr("Stack List Context Menu"), m_ui.stackList);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy = new QAction(tr("Copy"), m_ui.stackList);
	connect(copy, &QAction::triggered, [this]() {
		const auto* selection_model = m_ui.stackList->selectionModel();
		if (!selection_model->hasSelection())
			return;

		QGuiApplication::clipboard()->setText(m_model->data(selection_model->currentIndex()).toString());
	});
	menu->addAction(copy);

	menu->addSeparator();

	QAction* copy_all_as_csv = new QAction(tr("Copy all as CSV"), m_ui.stackList);
	connect(copy_all_as_csv, &QAction::triggered, [this]() {
		QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.stackList->model()));
	});
	menu->addAction(copy_all_as_csv);

	menu->popup(m_ui.stackList->viewport()->mapToGlobal(pos));
}

void StackWidget::onDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case StackModel::StackModel::ENTRY:
		case StackModel::StackModel::ENTRY_LABEL:
		{
			QModelIndex entry_index = m_model->index(index.row(), StackModel::StackColumns::ENTRY);
			goToInDisassembler(m_model->data(entry_index, Qt::UserRole).toUInt());
			break;
		}
		case StackModel::StackModel::SP:
		{
			goToInMemoryView(m_model->data(index, Qt::UserRole).toUInt(), DebuggerEvents::SWITCH_TO_RECEIVER);
			break;
		}
		default: // Default to PC
		{
			QModelIndex pc_index = m_model->index(index.row(), StackModel::StackColumns::PC);
			goToInDisassembler(m_model->data(pc_index, Qt::UserRole).toUInt());
			break;
		}
	}
}
