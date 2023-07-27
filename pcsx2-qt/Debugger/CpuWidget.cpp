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
#include "Models/BreakpointModel.h"
#include "Models/ThreadModel.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/BiosDebugData.h"
#include "DebugTools/MipsStackWalk.h"

#include "QtUtils.h"
#include <QtGui/QClipboard>
#include <QtWidgets/QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QFutureWatcher>

#include "demangler/demangler.h"

using namespace QtUtils;
using namespace MipsStackWalk;

CpuWidget::CpuWidget(QWidget* parent, DebugInterface& cpu)
	: m_cpu(cpu)
	, m_bpModel(cpu)
	, m_threadModel(cpu)
	, m_stackModel(cpu)
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

	connect(m_ui.breakpointList, &QTableView::customContextMenuRequested, this, &CpuWidget::onBPListContextMenu);
	connect(m_ui.breakpointList, &QTableView::doubleClicked, this, &CpuWidget::onBPListDoubleClicked);

	m_ui.breakpointList->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_ui.breakpointList->setModel(&m_bpModel);

	connect(m_ui.threadList, &QTableView::customContextMenuRequested, this, &CpuWidget::onThreadListContextMenu);
	connect(m_ui.threadList, &QTableView::doubleClicked, this, &CpuWidget::onThreadListDoubleClick);

	m_ui.threadList->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_ui.threadList->setModel(&m_threadModel);

	connect(m_ui.stackList, &QTableView::customContextMenuRequested, this, &CpuWidget::onStackListContextMenu);
	connect(m_ui.stackList, &QTableView::doubleClicked, this, &CpuWidget::onStackListDoubleClick);

	m_ui.stackList->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_ui.stackList->setModel(&m_stackModel);

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

	this->repaint();
}

CpuWidget::~CpuWidget() = default;

void CpuWidget::paintEvent(QPaintEvent* event)
{
	m_ui.registerWidget->update();
	m_ui.disassemblyWidget->update();
	m_ui.memoryviewWidget->update();
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

	if (m_stackModel.rowCount() < 2)
		return;

	Host::RunOnCPUThread([&] {
		CBreakPoints::AddBreakPoint(m_cpu.getCpuType(), m_stackModel.data(m_stackModel.index(1, StackModel::PC), Qt::UserRole).toUInt(), true);
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
	m_bpModel.refreshData();
}

void CpuWidget::onBPListDoubleClicked(const QModelIndex& index)
{
	if (index.isValid())
	{
		if (index.column() == BreakpointModel::OFFSET)
		{
			m_ui.disassemblyWidget->gotoAddress(m_bpModel.data(index, Qt::UserRole).toUInt());
		}
	}
}

void CpuWidget::onBPListContextMenu(QPoint pos)
{
	if (!m_cpu.isAlive())
		return;

	QMenu* contextMenu = new QMenu(tr("Breakpoint List Context Menu"), m_ui.breakpointList);

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

	contextMenu->popup(m_ui.breakpointList->mapToGlobal(pos));
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

	contextMenu->popup(m_ui.threadList->mapToGlobal(pos));
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
			m_ui.disassemblyWidget->gotoAddress(m_ui.threadList->model()->data(m_ui.threadList->model()->index(index.row(), ThreadModel::ThreadColumns::PC), Qt::UserRole).toUInt());
			break;
	}
}

void CpuWidget::onFuncListContextMenu(QPoint pos)
{
	if (!m_funclistContextMenu)
		m_funclistContextMenu = new QMenu(m_ui.listFunctions);
	else
		m_funclistContextMenu->clear();

	//: "Demangling" is the opposite of "Name mangling", which is a process where a compiler takes function names and combines them with other characteristics of the function (e.g. what types of data it accepts) to ensure they stay unique even when multiple functions exist with the same name (but different inputs / const-ness). See here: https://en.wikipedia.org/wiki/Name_mangling#C++
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

	contextMenu->popup(m_ui.stackList->mapToGlobal(pos));
}

void CpuWidget::onStackListDoubleClick(const QModelIndex& index)
{
	switch (index.column())
	{
		case StackModel::StackModel::ENTRY:
		case StackModel::StackModel::ENTRY_LABEL:
			m_ui.disassemblyWidget->gotoAddress(m_ui.stackList->model()->data(m_ui.stackList->model()->index(index.row(), StackModel::StackColumns::ENTRY), Qt::UserRole).toUInt());
			break;
		case StackModel::StackModel::SP:
			m_ui.memoryviewWidget->gotoAddress(m_ui.stackList->model()->data(index, Qt::UserRole).toUInt());
			m_ui.tabWidget->setCurrentWidget(m_ui.tab_memory);
			break;
		default: // Default to PC
			m_ui.disassemblyWidget->gotoAddress(m_ui.stackList->model()->data(m_ui.stackList->model()->index(index.row(), StackModel::StackColumns::PC), Qt::UserRole).toUInt());
			break;
	}
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
					const float memValue = std::bit_cast<float, u32>(cpu->read32(addr));
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
					const double memValue = std::bit_cast<double, u64>(cpu->read64(addr));
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

static std::vector<u32> searchWorkerByteArray(DebugInterface* cpu, u32 start, u32 end, QByteArray value)
{
	std::vector<u32> hitAddresses;
	for (u32 addr = start; addr < end; addr += 1)
	{
		bool hit = true;
		for (qsizetype i = 0; i < value.length(); i++)
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
			return searchWorkerByteArray(cpu, start, end, value.toUtf8());
		case 7:
			return searchWorkerByteArray(cpu, start, end, QByteArray::fromHex(value.toUtf8()));
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			break;
	};
	return {};
}

void CpuWidget::onSearchButtonClicked()
{
	if (!m_cpu.isAlive())
		return;

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

	unsigned long long value;

	switch (searchType)
	{
		case 0:
		case 1:
		case 2:
		case 3:
			value = searchValue.toULongLong(&ok, searchHex ? 16 : 10);
			break;
		case 4:
		case 5:
			searchValue.toDouble(&ok);
			break;
		case 6:
			ok = !searchValue.isEmpty();
			break;
		case 7:
			ok = !searchValue.trimmed().isEmpty();
			break;
	}

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid search value"));
		return;
	}

	switch (searchType)
	{
		case 7:
		case 6:
		case 5:
		case 4:
			break;
		case 3:
			if (value <= std::numeric_limits<unsigned long long>::max())
				break;
		case 2:
			if (value <= std::numeric_limits<unsigned long>::max())
				break;
		case 1:
			if (value <= std::numeric_limits<unsigned short>::max())
				break;
		case 0:
			if (value <= std::numeric_limits<unsigned char>::max())
				break;
		default:
			QMessageBox::critical(this, tr("Debugger"), tr("Value is larger than type"));
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
