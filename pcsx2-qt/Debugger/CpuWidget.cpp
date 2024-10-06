// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "CpuWidget.h"

#include "DisassemblyWidget.h"
#include "BreakpointDialog.h"
#include "Models/BreakpointModel.h"
#include "Models/ThreadModel.h"
#include "Models/SavedAddressesModel.h"
#include "Debugger/DebuggerSettingsManager.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MipsStackWalk.h"

#include "QtUtils.h"

#include "common/Console.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QFutureWatcher>
#include <QtCore/QRegularExpression>
#include <QtCore/QRegularExpressionMatchIterator>
#include <QtCore/QStringList>
#include <QtWidgets/QScrollBar>

using namespace QtUtils;
using namespace MipsStackWalk;

CpuWidget::CpuWidget(QWidget* parent, DebugInterface& cpu)
	: m_cpu(cpu)
	, m_bpModel(cpu)
	, m_threadModel(cpu)
	, m_stackModel(cpu)
	, m_savedAddressesModel(cpu)
{
	m_ui.setupUi(this);

	connect(g_emu_thread, &EmuThread::onVMPaused, this, &CpuWidget::onVMPaused);
	connect(g_emu_thread, &EmuThread::onGameChanged, this, [this](const QString& title) {
		if (title.isEmpty())
			return;
		// Don't overwrite users BPs/Saved Addresses unless they have a clean state.
		if (m_bpModel.rowCount() == 0)
			DebuggerSettingsManager::loadGameSettings(&m_bpModel);
		if (m_savedAddressesModel.rowCount() == 0)
			DebuggerSettingsManager::loadGameSettings(&m_savedAddressesModel);
	});

	connect(m_ui.registerWidget, &RegisterWidget::gotoInDisasm, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddress);
	connect(m_ui.memoryviewWidget, &MemoryViewWidget::gotoInDisasm, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddress);
	connect(m_ui.memoryviewWidget, &MemoryViewWidget::addToSavedAddresses, this, &CpuWidget::addAddressToSavedAddressesList);

	connect(m_ui.registerWidget, &RegisterWidget::gotoInMemory, this, &CpuWidget::onGotoInMemory);
	connect(m_ui.disassemblyWidget, &DisassemblyWidget::gotoInMemory, this, &CpuWidget::onGotoInMemory);

	connect(m_ui.memoryviewWidget, &MemoryViewWidget::VMUpdate, this, &CpuWidget::reloadCPUWidgets);
	connect(m_ui.registerWidget, &RegisterWidget::VMUpdate, this, &CpuWidget::reloadCPUWidgets);
	connect(m_ui.disassemblyWidget, &DisassemblyWidget::VMUpdate, this, &CpuWidget::reloadCPUWidgets);

	connect(m_ui.disassemblyWidget, &DisassemblyWidget::breakpointsChanged, this, &CpuWidget::updateBreakpoints);

	connect(m_ui.breakpointList, &QTableView::customContextMenuRequested, this, &CpuWidget::onBPListContextMenu);
	connect(m_ui.breakpointList, &QTableView::doubleClicked, this, &CpuWidget::onBPListDoubleClicked);

	m_ui.breakpointList->setModel(&m_bpModel);
	for (std::size_t i = 0; auto mode : BreakpointModel::HeaderResizeModes)
	{
		m_ui.breakpointList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	connect(&m_bpModel, &BreakpointModel::dataChanged, this, &CpuWidget::updateBreakpoints);

	connect(m_ui.threadList, &QTableView::customContextMenuRequested, this, &CpuWidget::onThreadListContextMenu);
	connect(m_ui.threadList, &QTableView::doubleClicked, this, &CpuWidget::onThreadListDoubleClick);

	m_threadProxyModel.setSourceModel(&m_threadModel);
	m_threadProxyModel.setSortRole(Qt::UserRole);
	m_ui.threadList->setModel(&m_threadProxyModel);
	m_ui.threadList->setSortingEnabled(true);
	m_ui.threadList->sortByColumn(ThreadModel::ThreadColumns::ID, Qt::SortOrder::AscendingOrder);
	for (std::size_t i = 0; auto mode : ThreadModel::HeaderResizeModes)
	{
		m_ui.threadList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	connect(m_ui.stackList, &QTableView::customContextMenuRequested, this, &CpuWidget::onStackListContextMenu);
	connect(m_ui.stackList, &QTableView::doubleClicked, this, &CpuWidget::onStackListDoubleClick);

	m_ui.stackList->setModel(&m_stackModel);
	for (std::size_t i = 0; auto mode : StackModel::HeaderResizeModes)
	{
		m_ui.stackList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	m_ui.disassemblyWidget->SetCpu(&cpu);
	m_ui.registerWidget->SetCpu(&cpu);
	m_ui.memoryviewWidget->SetCpu(&cpu);

	this->repaint();

	m_ui.savedAddressesList->setModel(&m_savedAddressesModel);
	m_ui.savedAddressesList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.savedAddressesList, &QTableView::customContextMenuRequested, this, &CpuWidget::onSavedAddressesListContextMenu);
	for (std::size_t i = 0; auto mode : SavedAddressesModel::HeaderResizeModes)
	{
		m_ui.savedAddressesList->horizontalHeader()->setSectionResizeMode(i++, mode);
	}
	QTableView* savedAddressesTableView = m_ui.savedAddressesList;
	connect(m_ui.savedAddressesList->model(), &QAbstractItemModel::dataChanged, [savedAddressesTableView](const QModelIndex& topLeft) {
		savedAddressesTableView->resizeColumnToContents(topLeft.column());
	});

	setupSymbolTrees();

	DebuggerSettingsManager::loadGameSettings(&m_bpModel);
	DebuggerSettingsManager::loadGameSettings(&m_savedAddressesModel);

	connect(m_ui.memorySearchWidget, &MemorySearchWidget::addAddressToSavedAddressesList, this, &CpuWidget::addAddressToSavedAddressesList);
	connect(m_ui.memorySearchWidget, &MemorySearchWidget::goToAddressInDisassemblyView,
		[this](u32 address) { m_ui.disassemblyWidget->gotoAddress(address, true); });
	connect(m_ui.memorySearchWidget, &MemorySearchWidget::goToAddressInMemoryView, m_ui.memoryviewWidget, &MemoryViewWidget::gotoAddress);
	connect(m_ui.memorySearchWidget, &MemorySearchWidget::switchToMemoryViewTab,
		[this]() { m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory); });
	m_ui.memorySearchWidget->setCpu(&m_cpu);

	m_refreshDebuggerTimer.setInterval(1000);
	connect(&m_refreshDebuggerTimer, &QTimer::timeout, this, &CpuWidget::refreshDebugger);
	m_refreshDebuggerTimer.start();
}

CpuWidget::~CpuWidget() = default;

void CpuWidget::setupSymbolTrees()
{
	m_ui.tabFunctions->setLayout(new QVBoxLayout());
	m_ui.tabGlobalVariables->setLayout(new QVBoxLayout());
	m_ui.tabLocalVariables->setLayout(new QVBoxLayout());
	m_ui.tabParameterVariables->setLayout(new QVBoxLayout());

	m_ui.tabFunctions->layout()->setContentsMargins(0, 0, 0, 0);
	m_ui.tabGlobalVariables->layout()->setContentsMargins(0, 0, 0, 0);
	m_ui.tabLocalVariables->layout()->setContentsMargins(0, 0, 0, 0);
	m_ui.tabParameterVariables->layout()->setContentsMargins(0, 0, 0, 0);

	m_function_tree = new FunctionTreeWidget(m_cpu);
	m_global_variable_tree = new GlobalVariableTreeWidget(m_cpu);
	m_local_variable_tree = new LocalVariableTreeWidget(m_cpu);
	m_parameter_variable_tree = new ParameterVariableTreeWidget(m_cpu);

	m_ui.tabFunctions->layout()->addWidget(m_function_tree);
	m_ui.tabGlobalVariables->layout()->addWidget(m_global_variable_tree);
	m_ui.tabLocalVariables->layout()->addWidget(m_local_variable_tree);
	m_ui.tabParameterVariables->layout()->addWidget(m_parameter_variable_tree);

	connect(m_ui.tabWidgetRegFunc, &QTabWidget::currentChanged, m_function_tree, &SymbolTreeWidget::updateModel);
	connect(m_ui.tabWidget, &QTabWidget::currentChanged, m_global_variable_tree, &SymbolTreeWidget::updateModel);
	connect(m_ui.tabWidget, &QTabWidget::currentChanged, m_local_variable_tree, &SymbolTreeWidget::updateModel);

	connect(m_function_tree, &SymbolTreeWidget::goToInDisassembly, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddressAndSetFocus);
	connect(m_global_variable_tree, &SymbolTreeWidget::goToInDisassembly, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddressAndSetFocus);
	connect(m_local_variable_tree, &SymbolTreeWidget::goToInDisassembly, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddressAndSetFocus);
	connect(m_parameter_variable_tree, &SymbolTreeWidget::goToInDisassembly, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddressAndSetFocus);

	connect(m_function_tree, &SymbolTreeWidget::goToInMemoryView, this, &CpuWidget::onGotoInMemory);
	connect(m_global_variable_tree, &SymbolTreeWidget::goToInMemoryView, this, &CpuWidget::onGotoInMemory);
	connect(m_local_variable_tree, &SymbolTreeWidget::goToInMemoryView, this, &CpuWidget::onGotoInMemory);
	connect(m_parameter_variable_tree, &SymbolTreeWidget::goToInMemoryView, this, &CpuWidget::onGotoInMemory);

	connect(m_function_tree, &SymbolTreeWidget::nameColumnClicked, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddressAndSetFocus);
	connect(m_function_tree, &SymbolTreeWidget::locationColumnClicked, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddressAndSetFocus);
}

void CpuWidget::refreshDebugger()
{
	if (!m_cpu.isAlive())
		return;

	m_ui.registerWidget->update();
	m_ui.disassemblyWidget->update();
	m_ui.memoryviewWidget->update();
	m_ui.memorySearchWidget->update();

	m_function_tree->updateModel();
	m_global_variable_tree->updateModel();
	m_local_variable_tree->updateModel();
	m_parameter_variable_tree->updateModel();
}

void CpuWidget::reloadCPUWidgets()
{
	updateThreads();
	updateStackFrames();

	m_ui.registerWidget->update();
	m_ui.disassemblyWidget->update();
	m_ui.memoryviewWidget->update();

	m_local_variable_tree->reset();
	m_parameter_variable_tree->reset();
}

void CpuWidget::paintEvent(QPaintEvent* event)
{
	m_ui.registerWidget->update();
	m_ui.disassemblyWidget->update();
	m_ui.memoryviewWidget->update();
	m_ui.memorySearchWidget->update();
}

// The cpu shouldn't be alive when these are called
// But make sure it isn't just in case
void CpuWidget::onStepInto()
{
	if (!m_cpu.isAlive() || !m_cpu.isCpuPaused())
		return;

	// Allow the cpu to skip this pc if it is a breakpoint
	CBreakPoints::SetSkipFirst(m_cpu.getCpuType(), m_cpu.getPC());

	const u32 pc = m_cpu.getPC();
	const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&m_cpu, pc);

	u32 bpAddr = pc + 0x4; // Default to the next instruction

	if (info.isBranch)
	{
		if (!info.isConditional)
		{
			bpAddr = info.branchTarget;
		}
		else
		{
			if (info.conditionMet)
			{
				bpAddr = info.branchTarget;
			}
			else
			{
				bpAddr = pc + (2 * 4); // Skip branch delay slot
			}
		}
	}

	if (info.isSyscall)
		bpAddr = info.branchTarget; // Syscalls are always taken

	Host::RunOnCPUThread([cpu = &m_cpu, bpAddr] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), bpAddr, true);
		cpu->resumeCpu();
	});

	this->repaint();
}

void CpuWidget::onStepOut()
{
	if (!m_cpu.isAlive() || !m_cpu.isCpuPaused())
		return;

	// Allow the cpu to skip this pc if it is a breakpoint
	CBreakPoints::SetSkipFirst(m_cpu.getCpuType(), m_cpu.getPC());

	if (m_stackModel.rowCount() < 2)
		return;

	Host::RunOnCPUThread([cpu = &m_cpu, stackModel = &m_stackModel] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), stackModel->data(stackModel->index(1, StackModel::PC), Qt::UserRole).toUInt(), true);
		cpu->resumeCpu();
	});

	this->repaint();
}

void CpuWidget::onStepOver()
{
	if (!m_cpu.isAlive() || !m_cpu.isCpuPaused())
		return;

	const u32 pc = m_cpu.getPC();
	const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&m_cpu, pc);

	u32 bpAddr = pc + 0x4; // Default to the next instruction

	if (info.isBranch)
	{
		if (!info.isConditional)
		{
			if (info.isLinkedBranch) // jal, jalr
			{
				// it's a function call with a delay slot - skip that too
				bpAddr += 4;
			}
			else // j, ...
			{
				// in case of absolute branches, set the breakpoint at the branch target
				bpAddr = info.branchTarget;
			}
		}
		else // beq, ...
		{
			if (info.conditionMet)
			{
				bpAddr = info.branchTarget;
			}
			else
			{
				bpAddr = pc + (2 * 4); // Skip branch delay slot
			}
		}
	}

	Host::RunOnCPUThread([cpu = &m_cpu, bpAddr] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), bpAddr, true);
		cpu->resumeCpu();
	});

	this->repaint();
}

void CpuWidget::onVMPaused()
{
	// Stops us from telling the disassembly dialog to jump somwhere because breakpoint code paused the core.
	if (CBreakPoints::GetCorePaused())
	{
		CBreakPoints::SetCorePaused(false);
	}
	else
	{
		m_ui.disassemblyWidget->gotoAddress(m_cpu.getPC(), false);
	}

	reloadCPUWidgets();
	this->repaint();
}

void CpuWidget::updateBreakpoints()
{
	m_bpModel.refreshData();
}

void CpuWidget::onBPListDoubleClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		if (index.column() == BreakpointModel::OFFSET)
		{
			m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_bpModel.data(index, BreakpointModel::DataRole).toUInt());
		}
	}
}

void CpuWidget::onBPListContextMenu(QPoint pos)
{
	QMenu* contextMenu = new QMenu(tr("Breakpoint List Context Menu"), m_ui.breakpointList);
	if (m_cpu.isAlive())
	{

		QAction* newAction = new QAction(tr("New"), m_ui.breakpointList);
		connect(newAction, &QAction::triggered, this, &CpuWidget::contextBPListNew);
		contextMenu->addAction(newAction);

		const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

		if (selModel->hasSelection())
		{
			QAction* editAction = new QAction(tr("Edit"), m_ui.breakpointList);
			connect(editAction, &QAction::triggered, this, &CpuWidget::contextBPListEdit);
			contextMenu->addAction(editAction);

			if (selModel->selectedIndexes().count() == 1)
			{
				QAction* copyAction = new QAction(tr("Copy"), m_ui.breakpointList);
				connect(copyAction, &QAction::triggered, this, &CpuWidget::contextBPListCopy);
				contextMenu->addAction(copyAction);
			}

			QAction* deleteAction = new QAction(tr("Delete"), m_ui.breakpointList);
			connect(deleteAction, &QAction::triggered, this, &CpuWidget::contextBPListDelete);
			contextMenu->addAction(deleteAction);
		}
	}

	contextMenu->addSeparator();
	if (m_bpModel.rowCount() > 0)
	{
		QAction* actionExport = new QAction(tr("Copy all as CSV"), m_ui.breakpointList);
		connect(actionExport, &QAction::triggered, [this]() {
			// It's important to use the Export Role here to allow pasting to be translation agnostic
			QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.breakpointList->model(), BreakpointModel::ExportRole, true));
		});
		contextMenu->addAction(actionExport);
	}

	if (m_cpu.isAlive())
	{
		QAction* actionImport = new QAction(tr("Paste from CSV"), m_ui.breakpointList);
		connect(actionImport, &QAction::triggered, this, &CpuWidget::contextBPListPasteCSV);
		contextMenu->addAction(actionImport);

		QAction* actionLoad = new QAction(tr("Load from Settings"), m_ui.breakpointList);
		connect(actionLoad, &QAction::triggered, [this]() {
			m_bpModel.clear();
			DebuggerSettingsManager::loadGameSettings(&m_bpModel);
		});
		contextMenu->addAction(actionLoad);

		QAction* actionSave = new QAction(tr("Save to Settings"), m_ui.breakpointList);
		connect(actionSave, &QAction::triggered, this, &CpuWidget::saveBreakpointsToDebuggerSettings);
		contextMenu->addAction(actionSave);
	}

	contextMenu->popup(m_ui.breakpointList->viewport()->mapToGlobal(pos));
}

void CpuWidget::onGotoInMemory(u32 address)
{
	m_ui.memoryviewWidget->gotoAddress(address);
	m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
}

void CpuWidget::contextBPListCopy()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	QGuiApplication::clipboard()->setText(m_bpModel.data(selModel->currentIndex()).toString());
}

void CpuWidget::contextBPListDelete()
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
		m_bpModel.removeRows(index.row(), 1);
	}
}

void CpuWidget::contextBPListNew()
{
	BreakpointDialog* bpDialog = new BreakpointDialog(this, &m_cpu, m_bpModel);
	bpDialog->show();
}

void CpuWidget::contextBPListEdit()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	const int selectedRow = selModel->selectedIndexes().first().row();

	auto bpObject = m_bpModel.at(selectedRow);

	BreakpointDialog* bpDialog = new BreakpointDialog(this, &m_cpu, m_bpModel, bpObject, selectedRow);
	bpDialog->show();
}

void CpuWidget::contextBPListPasteCSV()
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
		m_bpModel.loadBreakpointFromFieldList(fields);
	}
}

void CpuWidget::onSavedAddressesListContextMenu(QPoint pos)
{
	QMenu* contextMenu = new QMenu("Saved Addresses List Context Menu", m_ui.savedAddressesList);

	QAction* newAction = new QAction(tr("New"), m_ui.savedAddressesList);
	connect(newAction, &QAction::triggered, this, &CpuWidget::contextSavedAddressesListNew);
	contextMenu->addAction(newAction);

	const QModelIndex indexAtPos = m_ui.savedAddressesList->indexAt(pos);
	const bool isIndexValid = indexAtPos.isValid();

	if (isIndexValid)
	{
		if (m_cpu.isAlive())
		{
			QAction* goToAddressMemViewAction = new QAction(tr("Go to in Memory View"), m_ui.savedAddressesList);
			connect(goToAddressMemViewAction, &QAction::triggered, this, [this, indexAtPos]() {
				const QModelIndex rowAddressIndex = m_ui.savedAddressesList->model()->index(indexAtPos.row(), 0, QModelIndex());
				m_ui.memoryviewWidget->gotoAddress(m_ui.savedAddressesList->model()->data(rowAddressIndex, Qt::UserRole).toUInt());
				m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
			});
			contextMenu->addAction(goToAddressMemViewAction);

			QAction* goToAddressDisassemblyAction = new QAction(tr("Go to in Disassembly"), m_ui.savedAddressesList);
			connect(goToAddressDisassemblyAction, &QAction::triggered, this, [this, indexAtPos]() {
				const QModelIndex rowAddressIndex = m_ui.savedAddressesList->model()->index(indexAtPos.row(), 0, QModelIndex());
				m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_ui.savedAddressesList->model()->data(rowAddressIndex, Qt::UserRole).toUInt());
			});
			contextMenu->addAction(goToAddressDisassemblyAction);
		}

		QAction* copyAction = new QAction(indexAtPos.column() == 0 ? tr("Copy Address") : tr("Copy Text"), m_ui.savedAddressesList);
		connect(copyAction, &QAction::triggered, [this, indexAtPos]() {
			QGuiApplication::clipboard()->setText(m_ui.savedAddressesList->model()->data(indexAtPos, Qt::DisplayRole).toString());
		});
		contextMenu->addAction(copyAction);
	}

	if (m_ui.savedAddressesList->model()->rowCount() > 0)
	{
		QAction* actionExportCSV = new QAction(tr("Copy all as CSV"), m_ui.savedAddressesList);
		connect(actionExportCSV, &QAction::triggered, [this]() {
			QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.savedAddressesList->model(), Qt::DisplayRole, true));
		});
		contextMenu->addAction(actionExportCSV);
	}

	QAction* actionImportCSV = new QAction(tr("Paste from CSV"), m_ui.savedAddressesList);
	connect(actionImportCSV, &QAction::triggered, this, &CpuWidget::contextSavedAddressesListPasteCSV);
	contextMenu->addAction(actionImportCSV);

	if (m_cpu.isAlive())
	{
		QAction* actionLoad = new QAction(tr("Load from Settings"), m_ui.savedAddressesList);
		connect(actionLoad, &QAction::triggered, [this]() {
			m_savedAddressesModel.clear();
			DebuggerSettingsManager::loadGameSettings(&m_savedAddressesModel);
		});
		contextMenu->addAction(actionLoad);

		QAction* actionSave = new QAction(tr("Save to Settings"), m_ui.savedAddressesList);
		connect(actionSave, &QAction::triggered, this, &CpuWidget::saveSavedAddressesToDebuggerSettings);
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

void CpuWidget::contextSavedAddressesListPasteCSV()
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

		m_savedAddressesModel.loadSavedAddressFromFieldList(fields);
	}
}

void CpuWidget::contextSavedAddressesListNew()
{
	qobject_cast<SavedAddressesModel*>(m_ui.savedAddressesList->model())->addRow();
	const u32 rowCount = m_ui.savedAddressesList->model()->rowCount();
	m_ui.savedAddressesList->edit(m_ui.savedAddressesList->model()->index(rowCount - 1, 0));
}

void CpuWidget::addAddressToSavedAddressesList(u32 address)
{
	qobject_cast<SavedAddressesModel*>(m_ui.savedAddressesList->model())->addRow();
	const u32 rowCount = m_ui.savedAddressesList->model()->rowCount();
	const QModelIndex addressIndex = m_ui.savedAddressesList->model()->index(rowCount - 1, 0);
	m_ui.tabWidget->setCurrentWidget(m_ui.tab_savedaddresses);
	m_ui.savedAddressesList->model()->setData(addressIndex, address, Qt::UserRole);
	m_ui.savedAddressesList->edit(m_ui.savedAddressesList->model()->index(rowCount - 1, 1));
}

void CpuWidget::updateThreads()
{
	m_threadModel.refreshData();
}

void CpuWidget::onThreadListContextMenu(QPoint pos)
{
	if (!m_ui.threadList->selectionModel()->hasSelection())
		return;

	QMenu* contextMenu = new QMenu(tr("Thread List Context Menu"), m_ui.threadList);

	QAction* actionCopy = new QAction(tr("Copy"), m_ui.threadList);
	connect(actionCopy, &QAction::triggered, [this]() {
		const auto* selModel = m_ui.threadList->selectionModel();

		if (!selModel->hasSelection())
			return;

		QGuiApplication::clipboard()->setText(m_ui.threadList->model()->data(selModel->currentIndex()).toString());
	});
	contextMenu->addAction(actionCopy);

	contextMenu->addSeparator();

	QAction* actionExport = new QAction(tr("Copy all as CSV"), m_ui.threadList);
	connect(actionExport, &QAction::triggered, [this]() {
		QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.threadList->model()));
	});
	contextMenu->addAction(actionExport);

	contextMenu->popup(m_ui.threadList->viewport()->mapToGlobal(pos));
}

void CpuWidget::onThreadListDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case ThreadModel::ThreadColumns::ENTRY:
			m_ui.memoryviewWidget->gotoAddress(m_ui.threadList->model()->data(index, Qt::UserRole).toUInt());
			m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
			break;
		default: // Default to PC
			m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_ui.threadList->model()->data(m_ui.threadList->model()->index(index.row(), ThreadModel::ThreadColumns::PC), Qt::UserRole).toUInt());
			break;
	}
}

void CpuWidget::updateStackFrames()
{
	m_stackModel.refreshData();
}

void CpuWidget::onStackListContextMenu(QPoint pos)
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

void CpuWidget::onStackListDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case StackModel::StackModel::ENTRY:
		case StackModel::StackModel::ENTRY_LABEL:
			m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_ui.stackList->model()->data(m_ui.stackList->model()->index(index.row(), StackModel::StackColumns::ENTRY), Qt::UserRole).toUInt());
			break;
		case StackModel::StackModel::SP:
			m_ui.memoryviewWidget->gotoAddress(m_ui.stackList->model()->data(index, Qt::UserRole).toUInt());
			m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
			break;
		default: // Default to PC
			m_ui.disassemblyWidget->gotoAddressAndSetFocus(m_ui.stackList->model()->data(m_ui.stackList->model()->index(index.row(), StackModel::StackColumns::PC), Qt::UserRole).toUInt());
			break;
	}
}

void CpuWidget::saveBreakpointsToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(&m_bpModel);
}

void CpuWidget::saveSavedAddressesToDebuggerSettings()
{
	DebuggerSettingsManager::saveGameSettings(&m_savedAddressesModel);
}
