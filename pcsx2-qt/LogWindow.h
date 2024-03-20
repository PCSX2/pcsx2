// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Console.h"

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>

class LogWindow : public QMainWindow
{
	Q_OBJECT

public:
	LogWindow(bool attach_to_main);
	~LogWindow();

	static void updateSettings();
	static void destroy();

	__fi bool isAttachedToMainWindow() const { return m_attached_to_main_window; }
	void reattachToMainWindow();

	void updateWindowTitle();

private:
	void createUi();

	static void logCallback(LOGLEVEL level, ConsoleColors color, std::string_view message);

protected:
	void closeEvent(QCloseEvent* event);

private Q_SLOTS:
	void onClearTriggered();
	void onSaveTriggered();
	void appendMessage(quint32 level, quint32 color, const QString& message);

private:
	static constexpr int DEFAULT_WIDTH = 750;
	static constexpr int DEFAULT_HEIGHT = 400;

	void saveSize();
	void restoreSize();

	QPlainTextEdit* m_text;
	QMenu* m_level_menu;

	bool m_attached_to_main_window = true;
	bool m_destroying = false;
};

extern LogWindow* g_log_window;
