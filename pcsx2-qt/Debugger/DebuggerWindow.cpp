// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWindow.h"

#include "Debugger/Docking/DockManager.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/SymbolImporter.h"
#include "VMManager.h"
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

	m_dock_manager->loadLayouts();

	connect(m_ui.actionRun, &QAction::triggered, this, &DebuggerWindow::onRunPause);
	connect(m_ui.actionStepInto, &QAction::triggered, this, &DebuggerWindow::onStepInto);
	connect(m_ui.actionStepOver, &QAction::triggered, this, &DebuggerWindow::onStepOver);
	connect(m_ui.actionStepOut, &QAction::triggered, this, &DebuggerWindow::onStepOut);
	connect(m_ui.actionAnalyse, &QAction::triggered, this, &DebuggerWindow::onAnalyse);
	connect(m_ui.actionOnTop, &QAction::triggered, [this] { this->setWindowFlags(this->windowFlags() ^ Qt::WindowStaysOnTopHint); this->show(); });

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

	connect(g_emu_thread, &EmuThread::onVMPaused, this, &DebuggerWindow::onVMStateChanged);
	connect(g_emu_thread, &EmuThread::onVMResumed, this, &DebuggerWindow::onVMStateChanged);

	onVMStateChanged(); // If we missed a state change while we weren't loaded

	m_dock_manager->switchToLayout(0);

	QMenuBar* menu_bar = menuBar();

	setMenuWidget(m_dock_manager->createLayoutSwitcher(menu_bar));

	Host::RunOnCPUThread([]() {
		R5900SymbolImporter.OnDebuggerOpened();
	});
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

// There is no straightforward way to set the tab text to bold in Qt
// Sorry colour blind people, but this is the best we can do for now
void DebuggerWindow::setTabActiveStyle(BreakPointCpu enabledCpu)
{
	//m_ui.cpuTabs->tabBar()->setTabTextColor(m_ui.cpuTabs->indexOf(m_cpuWidget_r5900), (enabledCpu == BREAKPOINT_EE) ? Qt::red : this->palette().text().color());
	//m_ui.cpuTabs->tabBar()->setTabTextColor(m_ui.cpuTabs->indexOf(m_cpuWidget_r3000), (enabledCpu == BREAKPOINT_IOP) ? Qt::red : this->palette().text().color());
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
		setTabActiveStyle(BREAKPOINT_IOP_AND_EE);
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
			setTabActiveStyle(triggeredCpu);
			switch (triggeredCpu)
			{
				case BREAKPOINT_EE:
					//m_ui.cpuTabs->setCurrentWidget(m_cpuWidget_r5900);
					break;
				case BREAKPOINT_IOP:
					//m_ui.cpuTabs->setCurrentWidget(m_cpuWidget_r3000);
					break;
				default:
					break;
			}
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
	//CpuWidget* currentCpu = static_cast<CpuWidget*>(m_ui.cpuTabs->currentWidget());
	//currentCpu->onStepInto();
}

void DebuggerWindow::onStepOver()
{
	//CpuWidget* currentCpu = static_cast<CpuWidget*>(m_ui.cpuTabs->currentWidget());
	//currentCpu->onStepOver();
}

void DebuggerWindow::onStepOut()
{
	//CpuWidget* currentCpu = static_cast<CpuWidget*>(m_ui.cpuTabs->currentWidget());
	//currentCpu->onStepOut();
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
