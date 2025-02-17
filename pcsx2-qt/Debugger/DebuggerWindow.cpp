// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWindow.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/Docking/DockManager.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MIPSAnalyst.h"
#include "DebugTools/MipsStackWalk.h"
#include "DebugTools/SymbolImporter.h"
#include "QtHost.h"
#include "MainWindow.h"
#include "AnalysisOptionsDialog.h"

#include <QtWidgets/QMessageBox>

DebuggerWindow* g_debugger_window = nullptr;

DebuggerWindow::DebuggerWindow(QWidget* parent)
	: KDDockWidgets::QtWidgets::MainWindow(QStringLiteral("DebuggerWindow"), {}, parent)
	, m_dock_manager(new DockManager(this))
{
	m_ui.setupUi(this);

	g_debugger_window = this;

	setupDefaultToolBarState();

	m_dock_manager->loadLayouts();

	connect(m_ui.actionRun, &QAction::triggered, this, &DebuggerWindow::onRunPause);
	connect(m_ui.actionStepInto, &QAction::triggered, this, &DebuggerWindow::onStepInto);
	connect(m_ui.actionStepOver, &QAction::triggered, this, &DebuggerWindow::onStepOver);
	connect(m_ui.actionStepOut, &QAction::triggered, this, &DebuggerWindow::onStepOut);
	connect(m_ui.actionAnalyse, &QAction::triggered, this, &DebuggerWindow::onAnalyse);
	connect(m_ui.actionOnTop, &QAction::triggered, [this] { this->setWindowFlags(this->windowFlags() ^ Qt::WindowStaysOnTopHint); this->show(); });

	connect(m_ui.menuTools, &QMenu::aboutToShow, this, [this]() {
		m_dock_manager->createToolsMenu(m_ui.menuTools);
	});

	connect(m_ui.menuWindows, &QMenu::aboutToShow, this, [this]() {
		m_dock_manager->createWindowsMenu(m_ui.menuWindows);
	});

	connect(m_ui.actionResetAllLayouts, &QAction::triggered, [this]() {
		QMessageBox::StandardButton result = QMessageBox::question(
			g_debugger_window, tr("Confirmation"), tr("Are you sure you want to reset all layouts?"));

		if (result == QMessageBox::Yes)
			m_dock_manager->resetAllLayouts();
	});

	connect(m_ui.actionResetDefaultLayouts, &QAction::triggered, [this]() {
		QMessageBox::StandardButton result = QMessageBox::question(
			g_debugger_window, tr("Confirmation"), tr("Are you sure you want to reset all default layouts?"));

		if (result == QMessageBox::Yes)
			m_dock_manager->resetDefaultLayouts();
	});

	connect(g_emu_thread, &EmuThread::onVMPaused, this, []() {
		DebuggerWidget::broadcastEvent(DebuggerEvents::VMUpdate());
	});

	connect(g_emu_thread, &EmuThread::onVMPaused, this, &DebuggerWindow::onVMStateChanged);
	connect(g_emu_thread, &EmuThread::onVMResumed, this, &DebuggerWindow::onVMStateChanged);

	onVMStateChanged(); // If we missed a state change while we weren't loaded

	m_dock_manager->switchToLayout(0);

	QMenuBar* menu_bar = menuBar();

	setMenuWidget(m_dock_manager->createLayoutSwitcher(menu_bar));

	Host::RunOnCPUThread([]() {
		R5900SymbolImporter.OnDebuggerOpened();
	});

	QTimer* refresh_timer = new QTimer(this);
	connect(refresh_timer, &QTimer::timeout, this, []() {
		DebuggerWidget::broadcastEvent(DebuggerEvents::Refresh());
	});
	refresh_timer->start(1000);
}

DebuggerWindow::~DebuggerWindow() = default;

DebuggerWindow* DebuggerWindow::getInstance()
{
	if (!g_debugger_window)
		createInstance();

	return g_debugger_window;
}

DebuggerWindow* DebuggerWindow::createInstance()
{
	// Setup KDDockWidgets.
	DockManager::configureDockingSystem();

	if (g_debugger_window)
		destroyInstance();

	return new DebuggerWindow(nullptr);
}

void DebuggerWindow::destroyInstance()
{
	if (g_debugger_window)
		g_debugger_window->close();
}

DockManager& DebuggerWindow::dockManager()
{
	return *m_dock_manager;
}

void DebuggerWindow::clearToolBarState()
{
	restoreState(m_default_toolbar_state);
}

void DebuggerWindow::onVMStateChanged()
{
	if (!QtHost::IsVMPaused())
	{
		m_ui.actionRun->setText(tr("Pause"));
		m_ui.actionRun->setIcon(QIcon::fromTheme(QStringLiteral("pause-line")));
		m_ui.actionStepInto->setEnabled(false);
		m_ui.actionStepOver->setEnabled(false);
		m_ui.actionStepOut->setEnabled(false);
	}
	else
	{
		m_ui.actionRun->setText(tr("Run"));
		m_ui.actionRun->setIcon(QIcon::fromTheme(QStringLiteral("play-line")));
		m_ui.actionStepInto->setEnabled(true);
		m_ui.actionStepOver->setEnabled(true);
		m_ui.actionStepOut->setEnabled(true);
		// Switch to the CPU tab that triggered the breakpoint
		// Also bold the tab text to indicate that a breakpoint was triggered
		if (CBreakPoints::GetBreakpointTriggered())
		{
			const BreakPointCpu triggeredCpu = CBreakPoints::GetBreakpointTriggeredCpu();
			m_dock_manager->switchToLayoutWithCPU(triggeredCpu);
			Host::RunOnCPUThread([] {
				CBreakPoints::ClearTemporaryBreakPoints();
				CBreakPoints::SetBreakpointTriggered(false, BREAKPOINT_IOP_AND_EE);
				// Our current PC is on a breakpoint.
				// When we run the core again, we want to skip this breakpoint and run
				CBreakPoints::SetSkipFirst(BREAKPOINT_EE, r5900Debug.getPC());
				CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, r3000Debug.getPC());
			});
		}
	}
	return;
}

void DebuggerWindow::onRunPause()
{
	g_emu_thread->setVMPaused(!QtHost::IsVMPaused());
}

void DebuggerWindow::onStepInto()
{
	DebugInterface* cpu = currentCPU();
	if (!cpu)
		return;

	if (!cpu->isAlive() || !cpu->isCpuPaused())
		return;

	// Allow the cpu to skip this pc if it is a breakpoint
	CBreakPoints::SetSkipFirst(cpu->getCpuType(), cpu->getPC());

	const u32 pc = cpu->getPC();
	const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(cpu, pc);

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

	Host::RunOnCPUThread([cpu, bpAddr] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), bpAddr, true);
		cpu->resumeCpu();
	});

	repaint();
}

void DebuggerWindow::onStepOver()
{
	DebugInterface* cpu = currentCPU();
	if (!cpu)
		return;

	if (!cpu->isAlive() || !cpu->isCpuPaused())
		return;

	const u32 pc = cpu->getPC();
	const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(cpu, pc);

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

	Host::RunOnCPUThread([cpu, bpAddr] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), bpAddr, true);
		cpu->resumeCpu();
	});

	this->repaint();
}

void DebuggerWindow::onStepOut()
{
	DebugInterface* cpu = currentCPU();
	if (!cpu)
		return;

	if (!cpu->isAlive() || !cpu->isCpuPaused())
		return;

	// Allow the cpu to skip this pc if it is a breakpoint
	CBreakPoints::SetSkipFirst(cpu->getCpuType(), cpu->getPC());

	std::vector<MipsStackWalk::StackFrame> stack_frames;
	for (const auto& thread : cpu->GetThreadList())
	{
		if (thread->Status() == ThreadStatus::THS_RUN)
		{
			stack_frames = MipsStackWalk::Walk(
				cpu,
				cpu->getPC(),
				cpu->getRegister(0, 31),
				cpu->getRegister(0, 29),
				thread->EntryPoint(),
				thread->StackTop());
			break;
		}
	}

	if (stack_frames.size() < 2)
		return;

	u32 breakpoint_pc = stack_frames.at(1).pc;

	Host::RunOnCPUThread([cpu, breakpoint_pc] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), breakpoint_pc, true);
		cpu->resumeCpu();
	});

	this->repaint();
}

void DebuggerWindow::onAnalyse()
{
	AnalysisOptionsDialog* dialog = new AnalysisOptionsDialog(this);
	dialog->show();
}

void DebuggerWindow::closeEvent(QCloseEvent* event)
{
	dockManager().saveCurrentLayout();

	Host::RunOnCPUThread([]() {
		R5900SymbolImporter.OnDebuggerClosed();
	});

	KDDockWidgets::QtWidgets::MainWindow::closeEvent(event);

	g_debugger_window = nullptr;
	deleteLater();
}

DebugInterface* DebuggerWindow::currentCPU()
{
	std::optional<BreakPointCpu> maybe_cpu = m_dock_manager->cpu();
	if (!maybe_cpu.has_value())
		return nullptr;

	return &DebugInterface::get(*maybe_cpu);
}

void DebuggerWindow::setupDefaultToolBarState()
{
	// Hiding all the toolbars lets us save the default state of the window with
	// all the toolbars hidden. The DockManager will show the appropriate ones
	// later anyway.
	for (QToolBar* toolbar : findChildren<QToolBar*>())
		toolbar->hide();

	m_default_toolbar_state = saveState();

	for (QToolBar* toolbar : findChildren<QToolBar*>())
		connect(toolbar, &QToolBar::topLevelChanged, m_dock_manager, &DockManager::updateToolBarLockState);
}
