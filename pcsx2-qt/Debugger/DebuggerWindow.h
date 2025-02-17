// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DebuggerWindow.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/MainWindow.h>

class DockManager;

class DebuggerWindow : public KDDockWidgets::QtWidgets::MainWindow
{
	Q_OBJECT

public:
	DebuggerWindow(QWidget* parent);
	~DebuggerWindow();

	static DebuggerWindow* getInstance();
	static DebuggerWindow* createInstance();
	static void destroyInstance();

	DockManager& dockManager();

	void clearToolBarState();

public slots:
	void onVMStateChanged();
	void onRunPause();
	void onStepInto();
	void onStepOver();
	void onStepOut();
	void onAnalyse();

protected:
	void closeEvent(QCloseEvent* event);

private:
	void setupDefaultToolBarState();

	Ui::DebuggerWindow m_ui;
	QAction* m_actionRunPause;
	QAction* m_actionStepInto;
	QAction* m_actionStepOver;
	QAction* m_actionStepOut;

	DockManager* m_dock_manager;

	QByteArray m_default_toolbar_state;

	void setTabActiveStyle(BreakPointCpu toggledCPU);
};

extern DebuggerWindow* g_debugger_window;
