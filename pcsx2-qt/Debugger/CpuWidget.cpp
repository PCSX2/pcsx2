/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "CpuWidget.h"

#include "DisassemblyWidget.h"
#include "BreakpointDialog.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/BiosDebugData.h"
#include "DebugTools/MipsStackWalk.h"
#include "common/BitCast.h"

#include "QtUtils.h"
#include <QtWidgets/QHeaderView>
#include <QtGui/QClipboard>
#include <QtWidgets/QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QFutureWatcher>

#include "demangler/demangler.h"

using namespace QtUtils;
using namespace MipsStackWalk;

CpuWidget::CpuWidget(QWidget* parent, DebugInterface& cpu)
	: m_cpu(cpu)
{
	m_ui.setupUi(this);

	connect(g_emu_thread, &EmuThread::onVMPaused, this, &CpuWidget::onVMPaused);

	connect(m_ui.registerWidget, &RegisterWidget::gotoInDisasm, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddress);
	connect(m_ui.memoryviewWidget, &MemoryViewWidget::gotoInDisasm, m_ui.disassemblyWidget, &DisassemblyWidget::gotoAddress);

	connect(m_ui.registerWidget, &RegisterWidget::gotoInMemory, m_ui.memoryviewWidget, &MemoryViewWidget::gotoAddress);
	connect(m_ui.disassemblyWidget, &DisassemblyWidget::gotoInMemory, m_ui.memoryviewWidget, &MemoryViewWidget::gotoAddress);

	connect(m_ui.memoryviewWidget, &MemoryViewWidget::VMUpdate, this, &CpuWidget::reloadCPUWidgets);
	connect(m_ui.registerWidget, &RegisterWidget::VMUpdate, this, &CpuWidget::reloadCPUWidgets);
	connect(m_ui.disassemblyWidget, &DisassemblyWidget::VMUpdate, this, &CpuWidget::reloadCPUWidgets);

	connect(m_ui.tabWidget, &QTabWidget::currentChanged, [this] { fixBPListColumnSize(); });
	connect(m_ui.breakpointList, &QTableWidget::customContextMenuRequested, this, &CpuWidget::onBPListContextMenu);
	connect(m_ui.breakpointList, &QTableWidget::itemChanged, this, &CpuWidget::onBPListItemChange);

	connect(m_ui.threadList, &QTableWidget::customContextMenuRequested, this, &CpuWidget::onThreadListContextMenu);
	connect(m_ui.threadList, &QTableWidget::cellDoubleClicked, this, &CpuWidget::onThreadListDoubleClick);

	connect(m_ui.threadList, &QTableWidget::customContextMenuRequested, this, &CpuWidget::onThreadListContextMenu);
	connect(m_ui.threadList, &QTableWidget::cellDoubleClicked, this, &CpuWidget::onThreadListDoubleClick);

	connect(m_ui.stackframeList, &QTableWidget::customContextMenuRequested, this, &CpuWidget::onStackListContextMenu);
	connect(m_ui.stackframeList, &QTableWidget::cellDoubleClicked, this, &CpuWidget::onStackListDoubleClick);

	connect(m_ui.tabWidgetRegFunc, &QTabWidget::currentChanged, [this](int i) {if(i == 1){updateFunctionList(true);} });
	connect(m_ui.listFunctions, &QListWidget::customContextMenuRequested, this, &CpuWidget::onFuncListContextMenu);
	connect(m_ui.listFunctions, &QListWidget::itemDoubleClicked, this, &CpuWidget::onFuncListDoubleClick);
	connect(m_ui.btnRefreshFunctions, &QPushButton::clicked, [this] { updateFunctionList(); });
	connect(m_ui.txtFuncSearch, &QLineEdit::textChanged, [this] { updateFunctionList(); });

	connect(m_ui.btnSearch, &QPushButton::clicked, this, &CpuWidget::onSearchButtonClicked);
	connect(m_ui.listSearchResults, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item) { m_ui.memoryviewWidget->gotoAddress(item->data(256).toUInt()); });
	connect(m_ui.cmbSearchType, &QComboBox::currentIndexChanged, [this](int i) {
		if (i < 4)
			m_ui.chkSearchHex->setEnabled(true);
		else
			m_ui.chkSearchHex->setEnabled(false);
	});
	m_ui.disassemblyWidget->SetCpu(&cpu);
	m_ui.registerWidget->SetCpu(&cpu);
	m_ui.memoryviewWidget->SetCpu(&cpu);

	if (m_cpu.getCpuType() == BREAKPOINT_EE)
		CBreakPoints::SetUpdateHandler(std::bind(&CpuWidget::reloadCPUWidgets, this));

	this->repaint();
}

CpuWidget::~CpuWidget() = default;

void CpuWidget::paintEvent(QPaintEvent* event)
{
	m_ui.registerWidget->update();
	m_ui.disassemblyWidget->update();
	m_ui.memoryviewWidget->update();
}

void CpuWidget::resizeEvent(QResizeEvent* event)
{
	fixBPListColumnSize();
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

	Host::RunOnCPUThread([&] {
		CBreakPoints::AddBreakPoint(m_cpu.getCpuType(), bpAddr, true);
		m_cpu.resumeCpu();
	});

	this->repaint();
}

void CpuWidget::onStepOut()
{
	if (!m_cpu.isAlive() || !m_cpu.isCpuPaused())
		return;

	// Allow the cpu to skip this pc if it is a breakpoint
	CBreakPoints::SetSkipFirst(m_cpu.getCpuType(), m_cpu.getPC());

	if (m_stacklistObjects.size() < 2)
		return;

	Host::RunOnCPUThread([&] {
		CBreakPoints::AddBreakPoint(m_cpu.getCpuType(), m_stacklistObjects.at(1).pc, true);
		m_cpu.resumeCpu();
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

	Host::RunOnCPUThread([&] {
		CBreakPoints::AddBreakPoint(m_cpu.getCpuType(), bpAddr, true);
		m_cpu.resumeCpu();
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
		m_ui.disassemblyWidget->gotoAddress(m_cpu.getPC());
	}

	reloadCPUWidgets();
	this->repaint();
}

void CpuWidget::updateBreakpoints()
{
	m_ui.breakpointList->setRowCount(0);
	m_bplistObjects.clear();

	int iter = 0;
	for (const auto& breakpoint : CBreakPoints::GetBreakpoints())
	{
		if (breakpoint.cpu != m_cpu.getCpuType())
			continue;

		if (breakpoint.temporary)
			continue;

		m_ui.breakpointList->insertRow(iter);
		BreakpointObject obj;
		obj.bp = std::make_shared<BreakPoint>(breakpoint);
		m_bplistObjects.push_back(obj);

		// Type (R/O)
		QTableWidgetItem* typeItem = new QTableWidgetItem();
		typeItem->setText(tr("Execute"));
		typeItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 0, typeItem);

		// Offset (R/O), possibly allow changing offset???
		QTableWidgetItem* offsetItem = new QTableWidgetItem();
		offsetItem->setText(FilledQStringFromValue(breakpoint.addr, 16));
		offsetItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 1, offsetItem);

		// Size & Label (R/O)
		QTableWidgetItem* sizeLabelItem = new QTableWidgetItem();
		sizeLabelItem->setText(m_cpu.GetSymbolMap().GetLabelString(breakpoint.addr).c_str());
		sizeLabelItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 2, sizeLabelItem);

		// Opcode (R/O)
		QTableWidgetItem* opcodeItem = new QTableWidgetItem();
		opcodeItem->setText(m_ui.disassemblyWidget->GetLineDisasm(breakpoint.addr));
		opcodeItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 3, opcodeItem);

		// Condition (R/W)
		QTableWidgetItem* conditionItem = new QTableWidgetItem();
		conditionItem->setText(breakpoint.hasCond ? QString::fromLocal8Bit(breakpoint.cond.expressionString) : "");
		conditionItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEditable);
		m_ui.breakpointList->setItem(iter, 4, conditionItem);

		// Hits (R/O) (Disabled for execute bp)
		QTableWidgetItem* hitsItem = new QTableWidgetItem();
		hitsItem->setText("N/A");
		hitsItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 5, hitsItem);

		// Enabled (R/W)
		QTableWidgetItem* enabledItem = new QTableWidgetItem();
		enabledItem->setCheckState(breakpoint.enabled ? Qt::Checked : Qt::Unchecked);
		enabledItem->setFlags(Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.breakpointList->setItem(iter, 6, enabledItem);

		iter++;
	}

	for (const auto& memcheck : CBreakPoints::GetMemChecks())
	{
		if (memcheck.cpu != m_cpu.getCpuType())
			continue;

		m_ui.breakpointList->insertRow(iter);
		BreakpointObject obj;
		obj.mc = std::make_shared<MemCheck>(memcheck);
		m_bplistObjects.push_back(obj);

		// Type (R/O)
		QTableWidgetItem* typeItem = new QTableWidgetItem();
		QString type("");
		type += memcheck.cond & MEMCHECK_READ ? tr("Read") : "";
		type += ((memcheck.cond & MEMCHECK_BOTH) == MEMCHECK_BOTH) ? ", " : " ";
		type += memcheck.cond & MEMCHECK_WRITE ? memcheck.cond & MEMCHECK_WRITE_ONCHANGE ? tr("Write(C)") : tr("Write") : "";
		typeItem->setText(type);
		typeItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 0, typeItem);

		// Offset (R/O), possibly allow changing offset?
		QTableWidgetItem* offsetItem = new QTableWidgetItem();
		offsetItem->setText(FilledQStringFromValue(memcheck.start, 16));
		offsetItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 1, offsetItem);

		// Size & Label (R/O)
		QTableWidgetItem* sizeLabelItem = new QTableWidgetItem();
		sizeLabelItem->setText(QString::number(memcheck.end - memcheck.start, 16));
		sizeLabelItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEditable);
		m_ui.breakpointList->setItem(iter, 2, sizeLabelItem);

		// Opcode (R/O)
		QTableWidgetItem* opcodeItem = new QTableWidgetItem();
		opcodeItem->setText(m_ui.disassemblyWidget->GetLineDisasm(memcheck.start));
		opcodeItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 3, opcodeItem);

		// Condition (R/W) (Disabled for memchecks)
		QTableWidgetItem* conditionItem = new QTableWidgetItem();
		conditionItem->setText("N/A");
		conditionItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 4, conditionItem);


		// Hits (R/O)
		QTableWidgetItem* hitsItem = new QTableWidgetItem();
		hitsItem->setText(QString::number(memcheck.numHits));
		hitsItem->setFlags(Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable);
		m_ui.breakpointList->setItem(iter, 5, hitsItem);

		// Enabled (R/W)
		QTableWidgetItem* enabledItem = new QTableWidgetItem();
		enabledItem->setCheckState((memcheck.result & MEMCHECK_BREAK) ? Qt::Checked : Qt::Unchecked);
		enabledItem->setFlags(Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.breakpointList->setItem(iter, 6, enabledItem);

		iter++;
	}
}

void CpuWidget::fixBPListColumnSize()
{
	m_ui.breakpointList->horizontalHeader()->resizeSection(0, 90);
	m_ui.breakpointList->horizontalHeader()->resizeSection(1, 65);
	m_ui.breakpointList->horizontalHeader()->resizeSection(5, 40);
	m_ui.breakpointList->horizontalHeader()->resizeSection(6, 60);

	constexpr int currentWidthTotal = 90 + 65 + 40 + 60;
	const int sectionWidth = (m_ui.breakpointList->width() - currentWidthTotal) / 3.0f;
	m_ui.breakpointList->horizontalHeader()->resizeSection(2, sectionWidth);
	m_ui.breakpointList->horizontalHeader()->resizeSection(3, sectionWidth);
	m_ui.breakpointList->horizontalHeader()->resizeSection(4, sectionWidth);
}

void CpuWidget::onBPListContextMenu(QPoint pos)
{
	if (m_bplistContextMenu)
		delete m_bplistContextMenu;

	m_bplistContextMenu = new QMenu(m_ui.breakpointList);

	QAction* newAction = new QAction(tr("New"), m_ui.breakpointList);
	connect(newAction, &QAction::triggered, this, &CpuWidget::contextBPListNew);
	m_bplistContextMenu->addAction(newAction);

	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (selModel->hasSelection())
	{
		QAction* editAction = new QAction(tr("Edit"), m_ui.breakpointList);
		connect(editAction, &QAction::triggered, this, &CpuWidget::contextBPListEdit);
		m_bplistContextMenu->addAction(editAction);

		// Only copy when one column is selected
		// Shouldn't be trivial to support cross column copy
		if (selModel->selectedIndexes().count() == 1)
		{
			QAction* copyAction = new QAction(tr("Copy"), m_ui.breakpointList);
			connect(copyAction, &QAction::triggered, this, &CpuWidget::contextBPListCopy);
			m_bplistContextMenu->addAction(copyAction);
		}

		QAction* deleteAction = new QAction(tr("Delete"), m_ui.breakpointList);
		connect(deleteAction, &QAction::triggered, this, &CpuWidget::contextBPListDelete);
		m_bplistContextMenu->addAction(deleteAction);
	}

	m_bplistContextMenu->popup(m_ui.breakpointList->mapToGlobal(pos));
}

void CpuWidget::onBPListItemChange(QTableWidgetItem* item)
{
	if (item->column() == 2 && m_bplistObjects.at(item->row()).mc) // Size / Label column. Size is editable for memchecks
	{
		const auto& mc = m_bplistObjects.at(item->row()).mc;

		bool ok;
		u32 val = item->text().toUInt(&ok, 16);
		if (!ok)
		{
			QMessageBox::warning(this, tr("Error"), tr("Invalid size \"%1\"").arg(item->text()));
			item->setText(QString::number((mc->end - mc->start), 16));
			return;
		}

		if (val == (mc->end - mc->start))
		{
			return;
		}
		Host::RunOnCPUThread([this, val, mc] {
			CBreakPoints::RemoveMemCheck(m_cpu.getCpuType(), mc->start, mc->end);

			CBreakPoints::AddMemCheck(m_cpu.getCpuType(), mc->start, mc->start + val, mc->cond, mc->result);
		});
		updateBreakpoints();
	}
	else if (item->column() == 4 && m_bplistObjects.at(item->row()).bp) // Condition column. Only editable for breakpoints
	{
		const auto& bp = m_bplistObjects.at(item->row()).bp;

		if (item->text().isEmpty() && bp->hasCond)
		{
			Host::RunOnCPUThread([this, bp] {
				CBreakPoints::ChangeBreakPointRemoveCond(m_cpu.getCpuType(), bp->addr);
			});

			updateBreakpoints();
		}
		else if (item->text() != QString::fromLocal8Bit(&bp->cond.expressionString[0]))
		{
			PostfixExpression expression;

			if (!m_cpu.initExpression(item->text().toLocal8Bit().constData(), expression))
			{
				QMessageBox::warning(this, tr("Error"), tr("Invalid condition \"%1\"").arg(item->text()));
				item->setText(QString::fromLocal8Bit(&bp->cond.expressionString[0]));
				return;
			}
			BreakPointCond cond;
			cond.debug = &m_cpu;
			cond.expression = expression;
			strcpy(&cond.expressionString[0], item->text().toLocal8Bit().constData());
			Host::RunOnCPUThread([this, bp, cond] {
				CBreakPoints::ChangeBreakPointAddCond(m_cpu.getCpuType(), bp->addr, cond);
			});
			updateBreakpoints();
		}
	}
}

void CpuWidget::contextBPListCopy()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	QGuiApplication::clipboard()->setText(m_ui.breakpointList->selectedItems().first()->text());
}

void CpuWidget::contextBPListDelete()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	int last_row = -1;
	for (auto& index : selModel->selectedIndexes())
	{
		if (index.row() == last_row) // If the next index is in the same row, don't delete that breakpoint twice!
			continue;
		auto& bpObject = m_bplistObjects.at(index.row());

		Host::RunOnCPUThread([&] {
			if (bpObject.bp)
			{
				CBreakPoints::RemoveBreakPoint(m_cpu.getCpuType(), bpObject.bp->addr);
			}
			else
			{
				CBreakPoints::RemoveMemCheck(m_cpu.getCpuType(), bpObject.mc->start, bpObject.mc->end);
			}
		});

		last_row = index.row();
	}
	updateBreakpoints();
}

void CpuWidget::contextBPListNew()
{
	BreakpointDialog* bpDialog = new BreakpointDialog(this, &m_cpu);
	connect(bpDialog, &BreakpointDialog::accepted, this, &CpuWidget::updateBreakpoints);

	bpDialog->show();
}

void CpuWidget::contextBPListEdit()
{
	const QItemSelectionModel* selModel = m_ui.breakpointList->selectionModel();

	if (!selModel->hasSelection())
		return;

	auto& bpObject = m_bplistObjects.at(selModel->selectedIndexes().first().row());

	BreakpointDialog* bpDialog;

	if (bpObject.bp)
	{
		bpDialog = new BreakpointDialog(this, &m_cpu, bpObject.bp.get());
	}
	else
	{
		bpDialog = new BreakpointDialog(this, &m_cpu, bpObject.mc.get());
	}

	connect(bpDialog, &BreakpointDialog::accepted, this, &CpuWidget::updateBreakpoints);
	bpDialog->show();
}

void CpuWidget::updateFunctionList(bool whenEmpty)
{
	if (!m_cpu.isAlive())
		return;

	if (whenEmpty && m_ui.listFunctions->count())
		return;

	m_ui.listFunctions->clear();

	const auto demangler = demangler::CDemangler::createGcc();
	const QString filter = m_ui.txtFuncSearch->text().toLower();
	for (const auto& symbol : m_cpu.GetSymbolMap().GetAllSymbols(SymbolType::ST_FUNCTION))
	{
		QString symbolName = symbol.name.c_str();
		if (m_demangleFunctions)
		{
			symbolName = QString(demangler->demangleToString(symbol.name).c_str());

			// If the name isn't mangled, or it doesn't understand, it'll return an empty string
			// Fall back to the original name if this is the case
			if (symbolName.isEmpty())
				symbolName = symbol.name.c_str();
		}

		if (filter.size() && !symbolName.toLower().contains(filter))
			continue;

		QListWidgetItem* item = new QListWidgetItem();

		item->setText(QString("%0 %1").arg(FilledQStringFromValue(symbol.address, 16)).arg(symbolName));

		item->setData(256, symbol.address);

		m_ui.listFunctions->addItem(item);
	}
}

void CpuWidget::updateThreads()
{
	m_ui.threadList->setRowCount(0);

	if (m_cpu.getCpuType() == BREAKPOINT_EE)
		m_threadlistObjects = getEEThreads();

	for (size_t i = 0; i < m_threadlistObjects.size(); i++)
	{
		m_ui.threadList->insertRow(i);

		const auto& thread = m_threadlistObjects[i];

		if (thread.data.status == THS_RUN)
			m_activeThread = thread;

		QTableWidgetItem* idItem = new QTableWidgetItem();
		idItem->setText(QString::number(thread.tid));
		idItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.threadList->setItem(i, 0, idItem);

		QTableWidgetItem* pcItem = new QTableWidgetItem();
		if (thread.data.status == THS_RUN)
			pcItem->setText(FilledQStringFromValue(m_cpu.getPC(), 16));
		else
			pcItem->setText(FilledQStringFromValue(thread.data.entry, 16));
		pcItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.threadList->setItem(i, 1, pcItem);

		QTableWidgetItem* entryItem = new QTableWidgetItem();
		entryItem->setText(FilledQStringFromValue(thread.data.entry_init, 16));
		entryItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.threadList->setItem(i, 2, entryItem);

		QTableWidgetItem* priorityItem = new QTableWidgetItem();
		priorityItem->setText(QString::number(thread.data.currentPriority));
		priorityItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.threadList->setItem(i, 3, priorityItem);

		QString statusString;
		switch (thread.data.status)
		{
			case THS_BAD:
				statusString = tr("Bad");
				break;
			case THS_RUN:
				statusString = tr("Running");
				break;
			case THS_READY:
				statusString = tr("Ready");
				break;
			case THS_WAIT:
				statusString = tr("Waiting");
				break;
			case THS_SUSPEND:
				statusString = tr("Suspended");
				break;
			case THS_WAIT_SUSPEND:
				statusString = tr("Waiting/Suspended");
				break;
			case THS_DORMANT:
				statusString = tr("Dormant");
				break;
		}

		QTableWidgetItem* statusItem = new QTableWidgetItem();
		statusItem->setText(statusString);
		statusItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.threadList->setItem(i, 4, statusItem);

		QString waitTypeString;
		switch (thread.data.waitType)
		{
			case WAIT_NONE:
				waitTypeString = tr("None");
				break;
			case WAIT_WAKEUP_REQ:
				waitTypeString = tr("Wakeup request");
				break;
			case WAIT_SEMA:
				waitTypeString = tr("Semaphore");
				break;
		}

		QTableWidgetItem* waitItem = new QTableWidgetItem();
		waitItem->setText(waitTypeString);
		waitItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.threadList->setItem(i, 5, waitItem);
	}
}

void CpuWidget::onThreadListContextMenu(QPoint pos)
{
	if (!m_threadlistContextMenu)
	{
		m_threadlistContextMenu = new QMenu(m_ui.threadList);

		QAction* copyAction = new QAction(tr("Copy"), m_ui.threadList);
		connect(copyAction, &QAction::triggered, [this] {
			const auto& items = m_ui.threadList->selectedItems();
			if (!items.size())
				return;
			QApplication::clipboard()->setText(items.first()->text());
		});
		m_threadlistContextMenu->addAction(copyAction);
	}

	m_threadlistContextMenu->exec(m_ui.threadList->mapToGlobal(pos));
}

void CpuWidget::onThreadListDoubleClick(int row, int column)
{
	const auto& entry = m_threadlistObjects.at(row);
	if (column == 1) // PC
	{
		if (entry.data.status == THS_RUN)
			m_ui.disassemblyWidget->gotoAddress(m_cpu.getPC());
		else
			m_ui.disassemblyWidget->gotoAddress(entry.data.entry);
	}
	else if (column == 2) // Entry Point
	{
		m_ui.disassemblyWidget->gotoAddress(entry.data.entry_init);
	}
}

void CpuWidget::onFuncListContextMenu(QPoint pos)
{
	if (!m_funclistContextMenu)
		m_funclistContextMenu = new QMenu(m_ui.listFunctions);
	else
		m_funclistContextMenu->clear();

	QAction* demangleAction = new QAction(tr("Demangle Symbols"), m_ui.listFunctions);
	demangleAction->setCheckable(true);
	demangleAction->setChecked(m_demangleFunctions);

	connect(demangleAction, &QAction::triggered, [this] {
		m_demangleFunctions = !m_demangleFunctions;
		updateFunctionList();
	});

	m_funclistContextMenu->addAction(demangleAction);

	QAction* copyName = new QAction(tr("Copy Function Name"), m_ui.listFunctions);
	connect(copyName, &QAction::triggered, [this] {
		// We only store the address in the widget item
		// Resolve the function name by fetching the symbolmap and filtering the address

		const QListWidgetItem* selectedItem = m_ui.listFunctions->selectedItems().first();
		const QString functionName = QString(m_cpu.GetSymbolMap().GetLabelString(selectedItem->data(256).toUInt()).c_str());
		QApplication::clipboard()->setText(functionName);
	});
	m_funclistContextMenu->addAction(copyName);

	QAction* copyAddress = new QAction(tr("Copy Function Address"), m_ui.listFunctions);
	connect(copyAddress, &QAction::triggered, [this] {
		const QString addressString = FilledQStringFromValue(m_ui.listFunctions->selectedItems().first()->data(256).toUInt(), 16);
		QApplication::clipboard()->setText(addressString);
	});

	m_funclistContextMenu->addAction(copyAddress);

	m_funclistContextMenu->addSeparator();

	QAction* gotoDisasm = new QAction(tr("Go to in Disassembly"), m_ui.listFunctions);
	connect(gotoDisasm, &QAction::triggered, [this] {
		m_ui.disassemblyWidget->gotoAddress(m_ui.listFunctions->selectedItems().first()->data(256).toUInt());
	});

	m_funclistContextMenu->addAction(gotoDisasm);

	QAction* gotoMemory = new QAction(tr("Go to in Memory View"), m_ui.listFunctions);
	connect(gotoMemory, &QAction::triggered, [this] {
		m_ui.memoryviewWidget->gotoAddress(m_ui.listFunctions->selectedItems().first()->data(256).toUInt());
	});

	m_funclistContextMenu->addAction(gotoMemory);


	m_funclistContextMenu->popup(m_ui.listFunctions->mapToGlobal(pos));
}

void CpuWidget::onFuncListDoubleClick(QListWidgetItem* item)
{
	m_ui.disassemblyWidget->gotoAddress(item->data(256).toUInt());
}

void CpuWidget::updateStackFrames()
{
	m_ui.stackframeList->setRowCount(0);

	m_stacklistObjects = MipsStackWalk::Walk(&m_cpu, m_cpu.getPC(), m_cpu.getRegister(0, 31), m_cpu.getRegister(0, 29),
		m_activeThread.data.entry_init, m_activeThread.data.stack);

	for (size_t i = 0; i < m_stacklistObjects.size(); i++)
	{
		m_ui.stackframeList->insertRow(i);

		const auto& stackFrame = m_stacklistObjects.at(i);

		QTableWidgetItem* entryItem = new QTableWidgetItem();
		entryItem->setText(FilledQStringFromValue(stackFrame.entry, 16));
		entryItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.stackframeList->setItem(i, 0, entryItem);

		QTableWidgetItem* entryName = new QTableWidgetItem();
		entryName->setText(m_cpu.GetSymbolMap().GetLabelString(stackFrame.entry).c_str());
		entryName->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.stackframeList->setItem(i, 1, entryName);

		QTableWidgetItem* entryPC = new QTableWidgetItem();
		entryPC->setText(FilledQStringFromValue(stackFrame.pc, 16));
		entryPC->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.stackframeList->setItem(i, 2, entryPC);

		QTableWidgetItem* entryOpcode = new QTableWidgetItem();
		entryOpcode->setText(m_ui.disassemblyWidget->GetLineDisasm(stackFrame.pc));
		entryOpcode->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.stackframeList->setItem(i, 3, entryOpcode);

		QTableWidgetItem* entrySP = new QTableWidgetItem();
		entrySP->setText(FilledQStringFromValue(stackFrame.sp, 16));
		entrySP->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.stackframeList->setItem(i, 4, entrySP);

		QTableWidgetItem* entryStackSize = new QTableWidgetItem();
		entryStackSize->setText(QString::number(stackFrame.stackSize));
		entryStackSize->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.stackframeList->setItem(i, 5, entryStackSize);
	}
}

void CpuWidget::onStackListContextMenu(QPoint pos)
{
	if (!m_stacklistContextMenu)
	{
		m_stacklistContextMenu = new QMenu(m_ui.stackframeList);

		QAction* copyAction = new QAction(tr("Copy"), m_ui.stackframeList);
		connect(copyAction, &QAction::triggered, [this] {
			const auto& items = m_ui.stackframeList->selectedItems();
			if (!items.size())
				return;
			QApplication::clipboard()->setText(items.first()->text());
		});
		m_stacklistContextMenu->addAction(copyAction);
	}

	m_stacklistContextMenu->exec(m_ui.stackframeList->mapToGlobal(pos));
}

void CpuWidget::onStackListDoubleClick(int row, int column)
{
	const auto& entry = m_stacklistObjects.at(row);
	m_ui.disassemblyWidget->gotoAddress(entry.pc);
}

template <typename T>
static std::vector<u32> searchWorker(DebugInterface* cpu, u32 start, u32 end, T value)
{
	std::vector<u32> hitAddresses;
	for (u32 addr = start; addr < end; addr += sizeof(T))
	{
		T val = 0;
		switch (sizeof(T))
		{
			case sizeof(u8):
				val = cpu->read8(addr);
				break;
			case sizeof(u16):
				val = cpu->read16(addr);
				break;
			case sizeof(u32):
			{
				if (std::is_same_v<T, float>)
				{
					const float fTop = value + 0.00001f;
					const float fBottom = value - 0.00001f;
					const float memValue = bit_cast<float, u32>(cpu->read32(addr));
					if (fBottom < memValue && memValue < fTop)
					{
						hitAddresses.emplace_back(addr);
					}
					continue;
				}

				val = cpu->read32(addr);
				break;
			}
			case sizeof(u64):
			{
				if (std::is_same_v<T, double>)
				{
					const double dTop = value + 0.00001f;
					const double dBottom = value - 0.00001f;
					const double memValue = bit_cast<double, u64>(cpu->read64(addr));
					if (dBottom < memValue && memValue < dTop)
					{
						hitAddresses.emplace_back(addr);
					}
					continue;
				}

				val = cpu->read64(addr);
				break;
			}

			default:
				Console.Error("Debugger: Unknown type when doing memory search!");
				return hitAddresses;
				break;
		}

		if (val == value)
		{
			hitAddresses.push_back(addr);
		}
	}
	return hitAddresses;
}

static std::vector<u32> searchWorkerString(DebugInterface* cpu, u32 start, u32 end, std::string value)
{
	std::vector<u32> hitAddresses;
	for (u32 addr = start; addr < end; addr += 1)
	{
		bool hit = true;
		for (size_t i = 0; i < value.length(); i++)
		{
			if (static_cast<char>(cpu->read8(addr + i)) != value[i])
			{
				hit = false;
				break;
			}
		}
		if (hit)
		{
			hitAddresses.emplace_back(addr);
			addr += value.length() - 1;
		}
	}
	return hitAddresses;
}

std::vector<u32> startWorker(DebugInterface* cpu, int type, u32 start, u32 end, QString value, int base)
{

	const bool isSigned = value.startsWith("-");
	switch (type)
	{
		case 0:
			return isSigned ? searchWorker<s8>(cpu, start, end, value.toShort(nullptr, base)) : searchWorker<u8>(cpu, start, end, value.toUShort(nullptr, base));
		case 1:
			return isSigned ? searchWorker<s16>(cpu, start, end, value.toShort(nullptr, base)) : searchWorker<u16>(cpu, start, end, value.toUShort(nullptr, base));
		case 2:
			return isSigned ? searchWorker<s32>(cpu, start, end, value.toInt(nullptr, base)) : searchWorker<u32>(cpu, start, end, value.toUInt(nullptr, base));
		case 3:
			return isSigned ? searchWorker<s64>(cpu, start, end, value.toLong(nullptr, base)) : searchWorker<s64>(cpu, start, end, value.toULongLong(nullptr, base));
		case 4:
			return searchWorker<float>(cpu, start, end, value.toFloat());
		case 5:
			return searchWorker<double>(cpu, start, end, value.toDouble());
		case 6:
			return searchWorkerString(cpu, start, end, value.toStdString());
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			break;
	};
	return {};
}

void CpuWidget::onSearchButtonClicked()
{
	const int searchType = m_ui.cmbSearchType->currentIndex();
	const bool searchHex = m_ui.chkSearchHex->isChecked();

	bool ok;
	const u32 searchStart = m_ui.txtSearchStart->text().toUInt(&ok, 16);

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid start address"));
		return;
	}

	const u32 searchEnd = m_ui.txtSearchEnd->text().toUInt(&ok, 16);

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid end address"));
		return;
	}

	if (searchStart >= searchEnd)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Start address can't be equal to or greater than the end address"));
		return;
	}

	const QString searchValue = m_ui.txtSearchValue->text();

	if (searchType < 4)
	{
		searchValue.toLong(&ok, searchHex ? 16 : 10);
	}
	else if (searchType != 6)
	{
		searchValue.toDouble(&ok);
	}

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid search value"));
		return;
	}

	QFutureWatcher<std::vector<u32>>* workerWatcher = new QFutureWatcher<std::vector<u32>>;

	connect(workerWatcher, &QFutureWatcher<std::vector<u32>>::finished, [this, workerWatcher] {
		m_ui.btnSearch->setDisabled(false);

		m_ui.listSearchResults->clear();
		const auto& results = workerWatcher->future().result();

		for (const auto& address : results)
		{
			QListWidgetItem* item = new QListWidgetItem(QtUtils::FilledQStringFromValue(address, 16));
			item->setData(256, address);
			m_ui.listSearchResults->addItem(item);
		}
	});

	m_ui.btnSearch->setDisabled(true);
	QFuture<std::vector<u32>> workerFuture =
		QtConcurrent::run(startWorker, &m_cpu, searchType, searchStart, searchEnd, searchValue, searchHex ? 16 : 10);
	workerWatcher->setFuture(workerFuture);
}
