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

	static DebuggerWindow* getInstance();
	static DebuggerWindow* createInstance();
	static void destroyInstance();
	static bool shouldShowOnStartup();

	DockManager& dockManager();

	void setupDefaultToolBarState();
	void clearToolBarState();
	void setupFonts();
	void updateFontActions();
	void saveFontSize();
	int fontSize();
	void updateStyleSheets();

	void saveWindowGeometry();
	void restoreWindowGeometry();
	bool shouldSaveWindowGeometry();

public slots:
	void onVMStarting();
	void onVMPaused();
	void onVMResumed();
	void onVMStopped();

	void onAnalyse();
	void onSettings();
	void onGameSettings();
	void onRunPause();
	void onStepInto();
	void onStepOver();
	void onStepOut();

Q_SIGNALS:
	// Only emitted if the pause wasn't a temporary one triggered by the
	// breakpoint code.
	void onVMActuallyPaused();

protected:
	void closeEvent(QCloseEvent* event);

private:
	DebugInterface* currentCPU();

	Ui::DebuggerWindow m_ui;

	DockManager* m_dock_manager;

	QByteArray m_default_toolbar_state;

	int m_font_size;
	static const constexpr int DEFAULT_FONT_SIZE = 10;
	static const constexpr int MINIMUM_FONT_SIZE = 5;
	static const constexpr int MAXIMUM_FONT_SIZE = 30;
};

extern DebuggerWindow* g_debugger_window;
