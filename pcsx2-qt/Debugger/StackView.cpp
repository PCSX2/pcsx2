// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "StackView.h"

#include "QtUtils.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

StackView::StackView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, NO_DEBUGGER_FLAGS)
	, m_model(new StackModel(cpu()))
{
	m_ui.setupUi(this);

	m_ui.stackList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.stackList, &QTableView::customContextMenuRequested, this, &StackView::openContextMenu);
	connect(m_ui.stackList, &QTableView::doubleClicked, this, &StackView::onDoubleClick);

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

void StackView::openContextMenu(QPoint pos)
{
	if (!m_ui.stackList->selectionModel()->hasSelection())
		return;

	QMenu* menu = new QMenu(m_ui.stackList);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy_action = menu->addAction(tr("Copy"));
	connect(copy_action, &QAction::triggered, [this]() {
		const auto* selection_model = m_ui.stackList->selectionModel();
		if (!selection_model->hasSelection())
			return;

		QGuiApplication::clipboard()->setText(m_model->data(selection_model->currentIndex()).toString());
	});

	menu->addSeparator();

	QAction* copy_all_as_csv_action = menu->addAction(tr("Copy all as CSV"));
	connect(copy_all_as_csv_action, &QAction::triggered, [this]() {
		QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.stackList->model()));
	});

	menu->popup(m_ui.stackList->viewport()->mapToGlobal(pos));
}

void StackView::onDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case StackModel::StackModel::ENTRY:
		case StackModel::StackModel::ENTRY_LABEL:
		{
			QModelIndex entry_index = m_model->index(index.row(), StackModel::StackColumns::ENTRY);
			goToInDisassembler(m_model->data(entry_index, Qt::UserRole).toUInt(), true);
			break;
		}
		case StackModel::StackModel::SP:
		{
			goToInMemoryView(m_model->data(index, Qt::UserRole).toUInt(), true);
			break;
		}
		default: // Default to PC
		{
			QModelIndex pc_index = m_model->index(index.row(), StackModel::StackColumns::PC);
			goToInDisassembler(m_model->data(pc_index, Qt::UserRole).toUInt(), true);
			break;
		}
	}
}
