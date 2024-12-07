// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DebuggerWindow.h"

#include "DockManager.h"

#include <kddockwidgets/MainWindow.h>

class DebuggerWindow : public KDDockWidgets::QtWidgets::MainWindow
{
	Q_OBJECT

public:
	DebuggerWindow(QWidget* parent);
	~DebuggerWindow();

public slots:
	void onVMStateChanged();
	void onRunPause();
	void onStepInto();
	void onStepOver();
	void onStepOut();
	void onAnalyse();

protected:
	void showEvent(QShowEvent* event);
	void hideEvent(QHideEvent* event);

private:
	Ui::DebuggerWindow m_ui;
	QAction* m_actionRunPause;
	QAction* m_actionStepInto;
	QAction* m_actionStepOver;
	QAction* m_actionStepOut;

	DockManager m_dock_manager;

	void setTabActiveStyle(BreakPointCpu toggledCPU);
};
