// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWindow.h"

#include "Debugger/DebuggerView.h"
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
	setupFonts();
	restoreWindowGeometry();

	m_dock_manager->loadLayouts();

	connect(m_ui.actionAnalyse, &QAction::triggered, this, &DebuggerWindow::onAnalyse);
	connect(m_ui.actionSettings, &QAction::triggered, this, &DebuggerWindow::onSettings);
	connect(m_ui.actionGameSettings, &QAction::triggered, this, &DebuggerWindow::onGameSettings);
	connect(m_ui.actionClose, &QAction::triggered, this, &DebuggerWindow::close);

	connect(m_ui.actionOnTop, &QAction::triggered, this, [this](bool checked) {
		if (checked)
			setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
		else
			setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
		show();
	});

	connect(m_ui.actionRun, &QAction::triggered, this, &DebuggerWindow::onRunPause);
	connect(m_ui.actionStepInto, &QAction::triggered, this, &DebuggerWindow::onStepInto);
	connect(m_ui.actionStepOver, &QAction::triggered, this, &DebuggerWindow::onStepOver);
	connect(m_ui.actionStepOut, &QAction::triggered, this, &DebuggerWindow::onStepOut);

	connect(m_ui.actionShutDown, &QAction::triggered, [this]() {
		if (currentCPU() && currentCPU()->isAlive())
			g_emu_thread->shutdownVM(false);
	});

	connect(m_ui.actionReset, &QAction::triggered, [this]() {
		if (currentCPU() && currentCPU()->isAlive())
			g_emu_thread->resetVM();
	});

	connect(m_ui.menuTools, &QMenu::aboutToShow, this, [this]() {
		m_dock_manager->createToolsMenu(m_ui.menuTools);
	});

	connect(m_ui.menuWindows, &QMenu::aboutToShow, this, [this]() {
		m_dock_manager->createWindowsMenu(m_ui.menuWindows);
	});

	connect(m_ui.actionResetAllLayouts, &QAction::triggered, this, [this]() {
		QString text = tr("Are you sure you want to reset all layouts?");
		if (QMessageBox::question(g_debugger_window, tr("Confirmation"), text) != QMessageBox::Yes)
			return;

		m_dock_manager->resetAllLayouts();
	});

	connect(m_ui.actionResetDefaultLayouts, &QAction::triggered, this, [this]() {
		QString text = tr("Are you sure you want to reset the default layouts?");
		if (QMessageBox::question(g_debugger_window, tr("Confirmation"), text) != QMessageBox::Yes)
			return;

		m_dock_manager->resetDefaultLayouts();
	});

	connect(g_emu_thread, &EmuThread::onVMPaused, this, []() {
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});

	connect(g_emu_thread, &EmuThread::onVMStarting, this, &DebuggerWindow::onVMStarting);
	connect(g_emu_thread, &EmuThread::onVMPaused, this, &DebuggerWindow::onVMPaused);
	connect(g_emu_thread, &EmuThread::onVMResumed, this, &DebuggerWindow::onVMResumed);
	connect(g_emu_thread, &EmuThread::onVMStopped, this, &DebuggerWindow::onVMStopped);

	if (QtHost::IsVMValid())
	{
		onVMStarting();

		if (QtHost::IsVMPaused())
			onVMPaused();
		else
			onVMResumed();
	}
	else
	{
		onVMStopped();
	}

	m_dock_manager->switchToLayout(0);

	QMenuBar* menu_bar = menuBar();

	setMenuWidget(m_dock_manager->createMenuBar(menu_bar));

	updateTheme();

	Host::RunOnCPUThread([]() {
		R5900SymbolImporter.OnDebuggerOpened();
	});

	updateFromSettings();
}

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

bool DebuggerWindow::shouldShowOnStartup()
{
	return Host::GetBaseBoolSettingValue("Debugger/UserInterface", "ShowOnStartup", false);
}

DockManager& DebuggerWindow::dockManager()
{
	return *m_dock_manager;
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

void DebuggerWindow::clearToolBarState()
{
	restoreState(m_default_toolbar_state);
}

void DebuggerWindow::setupFonts()
{
	m_font_size = Host::GetBaseIntSettingValue("Debugger/UserInterface", "FontSize", DEFAULT_FONT_SIZE);
	if (m_font_size < MINIMUM_FONT_SIZE || m_font_size > MAXIMUM_FONT_SIZE)
		m_font_size = DEFAULT_FONT_SIZE;

	m_ui.actionIncreaseFontSize->setShortcuts(QKeySequence::ZoomIn);
	connect(m_ui.actionIncreaseFontSize, &QAction::triggered, this, [this]() {
		if (m_font_size >= MAXIMUM_FONT_SIZE)
			return;

		m_font_size++;

		updateFontActions();
		updateTheme();
		saveFontSize();
	});

	m_ui.actionDecreaseFontSize->setShortcut(QKeySequence::ZoomOut);
	connect(m_ui.actionDecreaseFontSize, &QAction::triggered, this, [this]() {
		if (m_font_size <= MINIMUM_FONT_SIZE)
			return;

		m_font_size--;

		updateFontActions();
		updateTheme();
		saveFontSize();
	});

	connect(m_ui.actionResetFontSize, &QAction::triggered, this, [this]() {
		m_font_size = DEFAULT_FONT_SIZE;

		updateFontActions();
		updateTheme();
		saveFontSize();
	});

	updateFontActions();
}

void DebuggerWindow::updateFontActions()
{
	m_ui.actionIncreaseFontSize->setEnabled(m_font_size < MAXIMUM_FONT_SIZE);
	m_ui.actionDecreaseFontSize->setEnabled(m_font_size > MINIMUM_FONT_SIZE);
	m_ui.actionResetFontSize->setEnabled(m_font_size != DEFAULT_FONT_SIZE);
}

void DebuggerWindow::saveFontSize()
{
	Host::SetBaseIntSettingValue("Debugger/UserInterface", "FontSize", m_font_size);
	Host::CommitBaseSettingChanges();
}

int DebuggerWindow::fontSize()
{
	return m_font_size;
}

void DebuggerWindow::updateTheme()
{
	// TODO: Migrate away from stylesheets to improve performance.
	if (m_font_size != DEFAULT_FONT_SIZE)
	{
		int size = m_font_size + QApplication::font().pointSize() - DEFAULT_FONT_SIZE;
		setStyleSheet(QString("* { font-size: %1pt; } QTabBar { font-size: %2pt; }").arg(size).arg(size + 1));
	}
	else
	{
		setStyleSheet(QString());
	}

	dockManager().updateTheme();
}

void DebuggerWindow::saveWindowGeometry()
{
	std::string old_geometry = Host::GetBaseStringSettingValue("Debugger/UserInterface", "WindowGeometry");

	std::string geometry;
	if (shouldSaveWindowGeometry())
		geometry = saveGeometry().toBase64().toStdString();

	if (geometry != old_geometry)
	{
		Host::SetBaseStringSettingValue("Debugger/UserInterface", "WindowGeometry", geometry.c_str());
		Host::CommitBaseSettingChanges();
	}
}

void DebuggerWindow::restoreWindowGeometry()
{
	if (!shouldSaveWindowGeometry())
		return;

	std::string geometry = Host::GetBaseStringSettingValue("Debugger/UserInterface", "WindowGeometry");
	restoreGeometry(QByteArray::fromBase64(QByteArray::fromStdString(geometry)));
}

bool DebuggerWindow::shouldSaveWindowGeometry()
{
	return Host::GetBaseBoolSettingValue("Debugger/UserInterface", "SaveWindowGeometry", true);
}

void DebuggerWindow::updateFromSettings()
{
	const int refresh_interval = Host::GetBaseIntSettingValue("Debugger/UserInterface", "RefreshInterval", 1000);
	const int effective_refresh_interval = std::clamp(refresh_interval, 10, 100000);

	if (!m_refresh_timer)
	{
		m_refresh_timer = new QTimer(this);
		connect(m_refresh_timer, &QTimer::timeout, this, []() {
			DebuggerView::broadcastEvent(DebuggerEvents::Refresh());
		});
		m_refresh_timer->start(effective_refresh_interval);
	}
	else
	{
		m_refresh_timer->setInterval(effective_refresh_interval);
	}
}

void DebuggerWindow::onVMStarting()
{
	m_ui.actionRun->setEnabled(true);
	m_ui.actionStepInto->setEnabled(true);
	m_ui.actionStepOver->setEnabled(true);
	m_ui.actionStepOut->setEnabled(true);

	m_ui.actionAnalyse->setEnabled(true);
	m_ui.actionGameSettings->setEnabled(true);

	m_ui.actionShutDown->setEnabled(true);
	m_ui.actionReset->setEnabled(true);
}

void DebuggerWindow::onVMPaused()
{
	m_ui.actionRun->setText(tr("Run"));
	m_ui.actionRun->setIcon(QIcon::fromTheme(QStringLiteral("play-line")));
	m_ui.actionStepInto->setEnabled(true);
	m_ui.actionStepOver->setEnabled(true);
	m_ui.actionStepOut->setEnabled(true);

	if (CBreakPoints::GetBreakpointTriggered())
	{
		// Select a layout tab corresponding to the CPU that triggered the
		// breakpoint and make it start blinking unless said breakpoint was
		// generated as a result of stepping.
		const BreakPointCpu cpu_type = CBreakPoints::GetBreakpointTriggeredCpu();
		if (cpu_type == BREAKPOINT_EE || cpu_type == BREAKPOINT_IOP)
		{
			DebugInterface& cpu = DebugInterface::get(cpu_type);
			bool blink_tab = !CBreakPoints::IsSteppingBreakPoint(cpu_type, cpu.getPC());
			m_dock_manager->switchToLayoutWithCPU(cpu_type, blink_tab);
		}

		Host::RunOnCPUThread([] {
			CBreakPoints::ClearTemporaryBreakPoints();
			CBreakPoints::SetBreakpointTriggered(false, BREAKPOINT_IOP_AND_EE);

			// Our current PC is on a breakpoint.
			// When we run the core again, we want to skip this breakpoint and run.
			CBreakPoints::SetSkipFirst(BREAKPOINT_EE, r5900Debug.getPC());
			CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, r3000Debug.getPC());
		});
	}

	// Stops us from telling the disassembly view to jump somwhere because
	// breakpoint code paused the core.
	if (!CBreakPoints::GetCorePaused())
		emit onVMActuallyPaused();
	else
		CBreakPoints::SetCorePaused(false);
}

void DebuggerWindow::onVMResumed()
{
	m_ui.actionRun->setText(tr("Pause"));
	m_ui.actionRun->setIcon(QIcon::fromTheme(QStringLiteral("pause-line")));
	m_ui.actionStepInto->setEnabled(false);
	m_ui.actionStepOver->setEnabled(false);
	m_ui.actionStepOut->setEnabled(false);
}

void DebuggerWindow::onVMStopped()
{
	m_ui.actionRun->setEnabled(false);
	m_ui.actionStepInto->setEnabled(false);
	m_ui.actionStepOver->setEnabled(false);
	m_ui.actionStepOut->setEnabled(false);

	m_ui.actionAnalyse->setEnabled(false);
	m_ui.actionGameSettings->setEnabled(false);

	m_ui.actionShutDown->setEnabled(false);
	m_ui.actionReset->setEnabled(false);
}

void DebuggerWindow::onAnalyse()
{
	AnalysisOptionsDialog* dialog = new AnalysisOptionsDialog(this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
}

void DebuggerWindow::onSettings()
{
	g_main_window->doSettings("Debug");
}

void DebuggerWindow::onGameSettings()
{
	g_main_window->doGameSettings("Debug");
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
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), bpAddr, true, true, true);
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
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), bpAddr, true, true, true);
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
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), breakpoint_pc, true, true, true);
		cpu->resumeCpu();
	});

	this->repaint();
}

void DebuggerWindow::closeEvent(QCloseEvent* event)
{
	dockManager().saveCurrentLayout();
	saveWindowGeometry();

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
