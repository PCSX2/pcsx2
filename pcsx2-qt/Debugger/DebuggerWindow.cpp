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

#include "DebuggerWindow.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "VMManager.h"
#include "QtHost.h"
#include "MainWindow.h"

DebuggerWindow::DebuggerWindow(QWidget* parent)
	: QMainWindow(parent)
{
	m_ui.setupUi(this);

// Easiest way to handle cross platform monospace fonts
// There are issues related to TabWidget -> Children font inheritance otherwise
#ifdef WIN32
	this->setStyleSheet("font: 8pt 'Lucida Console'");
#else
	this->setStyleSheet("font: 8pt 'Monospace'");
#endif

	m_actionRunPause = new QAction(tr("Run"), this);
	m_actionStepInto = new QAction(tr("Step Into"), this);
	m_actionStepOver = new QAction(tr("Step Over"), this);
	m_actionStepOut = new QAction(tr("Step Out"), this);

	m_ui.menubar->addAction(m_actionRunPause);
	m_ui.menubar->addAction(m_actionStepInto);
	m_ui.menubar->addAction(m_actionStepOver);
	m_ui.menubar->addAction(m_actionStepOut);

	connect(m_actionRunPause, &QAction::triggered, this, &DebuggerWindow::onRunPause);
	connect(m_actionStepInto, &QAction::triggered, this, &DebuggerWindow::onStepInto);
	connect(m_actionStepOver, &QAction::triggered, this, &DebuggerWindow::onStepOver);
	connect(m_actionStepOut, &QAction::triggered, this, &DebuggerWindow::onStepOut);

	connect(g_emu_thread, &EmuThread::onVMPaused, this, &DebuggerWindow::onVMStateChanged);
	connect(g_emu_thread, &EmuThread::onVMResumed, this, &DebuggerWindow::onVMStateChanged);

	onVMStateChanged(); // If we missed a state change while we weren't loaded

	m_cpuWidget_r5900 = new CpuWidget(this, r5900Debug);
	m_cpuWidget_r3000 = new CpuWidget(this, r3000Debug);

	m_ui.cpuTabs->addTab(m_cpuWidget_r5900, "R5900");
	m_ui.cpuTabs->addTab(m_cpuWidget_r3000, "R3000");
	return;
}

DebuggerWindow::~DebuggerWindow() = default;

// TODO: not this
bool nextStatePaused = true;
void DebuggerWindow::onVMStateChanged()
{
	if (!QtHost::IsVMPaused())
	{
		nextStatePaused = true;
		m_actionRunPause->setText(tr("Pause"));
		m_actionStepInto->setEnabled(false);
		m_actionStepOver->setEnabled(false);
		m_actionStepOut->setEnabled(false);
	}
	else
	{
		nextStatePaused = false;
		m_actionRunPause->setText(tr("Run"));
		m_actionStepInto->setEnabled(true);
		m_actionStepOver->setEnabled(true);
		m_actionStepOut->setEnabled(true);
		CBreakPoints::ClearTemporaryBreakPoints();

		if (CBreakPoints::GetBreakpointTriggered())
		{
			CBreakPoints::SetBreakpointTriggered(false);
			// Our current PC is on a breakpoint.
			// When we run the core again, we want to skip this breakpoint and run
			CBreakPoints::SetSkipFirst(BREAKPOINT_EE, r5900Debug.getPC());
			CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, r3000Debug.getPC());
		}
	}
	return;
}

void DebuggerWindow::onRunPause()
{
	g_emu_thread->setVMPaused(nextStatePaused);
}

void DebuggerWindow::onStepInto()
{
	CpuWidget* currentCpu = static_cast<CpuWidget*>(m_ui.cpuTabs->currentWidget());
	currentCpu->onStepInto();
}

void DebuggerWindow::onStepOver()
{
	CpuWidget* currentCpu = static_cast<CpuWidget*>(m_ui.cpuTabs->currentWidget());
	currentCpu->onStepOver();
}

void DebuggerWindow::onStepOut()
{
	CpuWidget* currentCpu = static_cast<CpuWidget*>(m_ui.cpuTabs->currentWidget());
	currentCpu->onStepOut();
}
