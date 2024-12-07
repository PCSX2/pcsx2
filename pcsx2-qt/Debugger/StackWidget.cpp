// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "StackWidget.h"

#include "QtUtils.h"

#include <QClipboard>
#include <QMenu>

StackWidget::StackWidget(DebugInterface& cpu, QWidget* parent)
	: DebuggerWidget(&cpu, parent)
	, m_model(cpu)
{
	m_ui.setupUi(this);

	connect(m_ui.stackList, &QTableView::customContextMenuRequested, this, &StackWidget::onContextMenu);
	connect(m_ui.stackList, &QTableView::doubleClicked, this, &StackWidget::onDoubleClick);

	m_ui.stackList->setModel(&m_model);
	for (std::size_t i = 0; auto mode : StackModel::HeaderResizeModes)
	{
		m_ui.stackList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}
}

void StackWidget::onContextMenu(QPoint pos)
{
	if (!m_ui.stackList->selectionModel()->hasSelection())
		return;

	QMenu* contextMenu = new QMenu(tr("Stack List Context Menu"), m_ui.stackList);

	QAction* actionCopy = new QAction(tr("Copy"), m_ui.stackList);
	connect(actionCopy, &QAction::triggered, [this]() {
		const auto* selModel = m_ui.stackList->selectionModel();

		if (!selModel->hasSelection())
			return;

		QGuiApplication::clipboard()->setText(m_ui.stackList->model()->data(selModel->currentIndex()).toString());
	});
	contextMenu->addAction(actionCopy);

	contextMenu->addSeparator();

	QAction* actionExport = new QAction(tr("Copy all as CSV"), m_ui.stackList);
	connect(actionExport, &QAction::triggered, [this]() {
		QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.stackList->model()));
	});
	contextMenu->addAction(actionExport);

	contextMenu->popup(m_ui.stackList->viewport()->mapToGlobal(pos));
}

void StackWidget::onDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case StackModel::StackModel::ENTRY:
		case StackModel::StackModel::ENTRY_LABEL:
			//m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_ui.stackList->model()->data(m_ui.stackList->model()->index(index.row(), StackModel::StackColumns::ENTRY), Qt::UserRole).toUInt());
			not_yet_implemented();
			break;
		case StackModel::StackModel::SP:
			//m_ui.memoryviewWidget->gotoAddress(m_ui.stackList->model()->data(index, Qt::UserRole).toUInt());
			//m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
			not_yet_implemented();
			break;
		default: // Default to PC
			//m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_ui.stackList->model()->data(m_ui.stackList->model()->index(index.row(), StackModel::StackColumns::PC), Qt::UserRole).toUInt());
			not_yet_implemented();
			break;
	}
}
