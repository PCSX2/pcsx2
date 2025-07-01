// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ModuleView.h"

#include "QtUtils.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

ModuleView::ModuleView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
	, m_model(new ModuleModel(cpu()))
{
	m_ui.setupUi(this);
	m_ui.moduleList->setModel(m_model);
	m_ui.moduleList->horizontalHeader()->setSectionsMovable(true);

	m_ui.moduleList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.moduleList, &QTableView::customContextMenuRequested, this, &ModuleView::openContextMenu);
	connect(m_ui.moduleList, &QTableView::doubleClicked, this, &ModuleView::onDoubleClick);

	m_ui.moduleList->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
	for (std::size_t i = 0; auto mode : ModuleModel::HeaderResizeModes)
	{
		m_ui.moduleList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		if (!QtHost::IsVMPaused())
			m_model->refreshData();
		return true;
	});

	receiveEvent<DebuggerEvents::VMUpdate>([this](const DebuggerEvents::VMUpdate& event) -> bool {
		m_model->refreshData();
		return true;
	});
}

void ModuleView::openContextMenu(QPoint pos)
{
	if (!m_ui.moduleList->selectionModel()->hasSelection())
		return;

	QMenu* menu = new QMenu(m_ui.moduleList);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy = menu->addAction(tr("Copy"));
	connect(copy, &QAction::triggered, [this]() {
		const QItemSelectionModel* selection_model = m_ui.moduleList->selectionModel();
		if (!selection_model->hasSelection())
			return;

		QGuiApplication::clipboard()->setText(m_model->data(selection_model->currentIndex()).toString());
	});

	menu->addSeparator();

	QAction* copy_all_as_csv = menu->addAction(tr("Copy all as CSV"));
	connect(copy_all_as_csv, &QAction::triggered, [this]() {
		QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.moduleList->model()));
	});

	menu->popup(m_ui.moduleList->viewport()->mapToGlobal(pos));
}

void ModuleView::onDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case ModuleModel::ModuleColumns::ENTRY:
		{
			goToInDisassembler(m_model->data(index, Qt::UserRole).toUInt(), true);
			break;
		}
		case ModuleModel::ModuleColumns::GP:
		{
			goToInMemoryView(m_model->data(index, Qt::UserRole).toUInt(), true);
			break;
		}
		case ModuleModel::ModuleColumns::TEXT_SECTION:
		{
			goToInDisassembler(m_model->data(index, Qt::UserRole).toUInt(), true);
			break;
		}
		case ModuleModel::ModuleColumns::DATA_SECTION:
		{
			goToInMemoryView(m_model->data(index, Qt::UserRole).toUInt(), true);
			break;
		}
		case ModuleModel::ModuleColumns::BSS_SECTION:
		{
			auto data = m_model->data(index, Qt::UserRole).toUInt();
			if (data)
			{
				goToInMemoryView(data, true);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}
