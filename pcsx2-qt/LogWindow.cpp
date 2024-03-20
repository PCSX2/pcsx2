// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "LogWindow.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "SettingWidgetBinder.h"

#include <QtCore/QLatin1StringView>
#include <QtCore/QUtf8StringView>
#include <QtGui/QIcon>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QScrollBar>

#include <mutex>

// Need a lock so the other threads don't try to write to a deleting window.
LogWindow* g_log_window;
static std::mutex s_log_mutex;

static LOGLEVEL GetWindowLogLevel()
{
#ifdef _DEBUG
	return LOGLEVEL_DEBUG;
#else
	return (IsDevBuild || Host::GetBaseBoolSettingValue("Logging", "EnableVerbose", false)) ? LOGLEVEL_DEV : LOGLEVEL_INFO;
#endif
}

LogWindow::LogWindow(bool attach_to_main)
	: QMainWindow()
	, m_attached_to_main_window(attach_to_main)
{
	restoreSize();
	createUi();

	Log::SetHostOutputLevel(GetWindowLogLevel(), &LogWindow::logCallback);
}

LogWindow::~LogWindow()
{
	Log::SetHostOutputLevel(LOGLEVEL_NONE, nullptr);
}

void LogWindow::updateSettings()
{
	std::unique_lock lock(s_log_mutex);

	const bool new_enabled = Host::GetBaseBoolSettingValue("Logging", "EnableLogWindow", false) && !QtHost::InNoGUIMode();
	const bool attach_to_main = Host::GetBaseBoolSettingValue("Logging", "AttachLogWindowToMainWindow", true);
	const bool curr_enabled = Log::IsHostOutputEnabled();

	if (new_enabled == curr_enabled)
	{
		if (g_log_window && g_log_window->m_attached_to_main_window != attach_to_main)
		{
			g_log_window->m_attached_to_main_window = attach_to_main;
			if (attach_to_main)
				g_log_window->reattachToMainWindow();
		}

		// Update level.
		if (new_enabled)
			Log::SetHostOutputLevel(GetWindowLogLevel(), &LogWindow::logCallback);

		return;
	}

	if (new_enabled)
	{
		g_log_window = new LogWindow(attach_to_main);
		if (attach_to_main && g_main_window && g_main_window->isVisible())
			g_log_window->reattachToMainWindow();

		g_log_window->show();
	}
	else if (g_log_window)
	{
		g_log_window->m_destroying = true;
		g_log_window->close();
		g_log_window->deleteLater();
		g_log_window = nullptr;
	}
}

void LogWindow::destroy()
{
	std::unique_lock lock(s_log_mutex);
	if (!g_log_window)
		return;

	g_log_window->m_destroying = true;
	g_log_window->close();
	g_log_window->deleteLater();
	g_log_window = nullptr;
}

void LogWindow::reattachToMainWindow()
{
	// Skip when maximized.
	if (g_main_window->windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen))
		return;

	const QPoint new_pos = g_main_window->pos() + QPoint(g_main_window->width() + 10, 0);
	if (pos() != new_pos)
		move(new_pos);
}

void LogWindow::updateWindowTitle()
{
	QString title;

	const QString& serial = QtHost::GetCurrentGameSerial();

	if (QtHost::IsVMValid() && !serial.isEmpty())
	{
		const QFileInfo fi(QtHost::GetCurrentGamePath());
		title = tr("Log Window - %1 [%2]").arg(serial).arg(fi.fileName());
	}
	else
	{
		title = tr("Log Window");
	}

	setWindowTitle(title);
}

void LogWindow::createUi()
{
	setWindowIcon(QtHost::GetAppIcon());
	setWindowFlag(Qt::WindowCloseButtonHint, false);
	updateWindowTitle();

	QAction* action;

	QMenuBar* menu = new QMenuBar(this);
	setMenuBar(menu);

	QMenu* log_menu = menu->addMenu("&Log");
	action = log_menu->addAction(tr("&Clear"));
	connect(action, &QAction::triggered, this, &LogWindow::onClearTriggered);
	action = log_menu->addAction(tr("&Save..."));
	connect(action, &QAction::triggered, this, &LogWindow::onSaveTriggered);

	log_menu->addSeparator();

	action = log_menu->addAction(tr("Cl&ose"));
	connect(action, &QAction::triggered, this, &LogWindow::close);

	QMenu* settings_menu = menu->addMenu(tr("&Settings"));

#if 0
	// TODO: These are duplicated with the main window...
	action = settings_menu->addAction(tr("Log To &System Console"));
	action->setCheckable(true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, action, "Logging", "EnableSystemConsole", false);

	action = settings_menu->addAction(tr("Log To &Debug Console"));
	action->setCheckable(true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, action, "Logging", "EnableDebugConsole", false);

	action = settings_menu->addAction(tr("Log To &File"));
	action->setCheckable(true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, action, "Logging", "EnableFileLogging", false);

	settings_menu->addSeparator();
#endif

	action = settings_menu->addAction(tr("Attach To &Main Window"));
	action->setCheckable(true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, action, "Logging", "AttachLogWindowToMainWindow", true);

	action = settings_menu->addAction(tr("Show &Timestamps"));
	action->setCheckable(true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, action, "Logging", "EnableTimestamps", true);

	settings_menu->addSeparator();

	// TODO: Log Level

	m_text = new QPlainTextEdit(this);
	m_text->setReadOnly(true);
	m_text->setUndoRedoEnabled(false);
	m_text->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
	m_text->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_text->setWordWrapMode(QTextOption::WrapAnywhere);

#if defined(_WIN32)
	QFont font("Consolas");
	font.setPointSize(10);
#elif defined(__APPLE__)
	QFont font("Monaco");
	font.setPointSize(11);
#else
	QFont font("Monospace");
	font.setStyleHint(QFont::TypeWriter);
#endif
	m_text->setFont(font);

	setCentralWidget(m_text);
}

void LogWindow::onClearTriggered()
{
	m_text->clear();
}

void LogWindow::onSaveTriggered()
{
	const QString path = QFileDialog::getSaveFileName(this, tr("Select Log File"), QString(), tr("Log Files (*.txt)"));
	if (path.isEmpty())
		return;

	QFile file(path);
	if (!file.open(QFile::WriteOnly | QFile::Text))
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to open file for writing."));
		return;
	}

	file.write(m_text->toPlainText().toUtf8());
	file.close();

	appendMessage(LOGLEVEL_INFO, Color_Default, tr("Log was written to %1.\n").arg(path));
}

void LogWindow::logCallback(LOGLEVEL level, ConsoleColors color, std::string_view message)
{
	std::unique_lock lock(s_log_mutex);
	if (!g_log_window)
		return;

	// I don't like the memory allocations here either...
	QString qmessage;
	qmessage.reserve(message.length() + 1);
	qmessage.append(QUtf8StringView(message.data(), message.length()));
	qmessage.append(QChar('\n'));

	if (g_emu_thread->isOnUIThread())
	{
		g_log_window->appendMessage(static_cast<u32>(level), static_cast<u32>(color), qmessage);
	}
	else
	{
		QMetaObject::invokeMethod(g_log_window, "appendMessage", Qt::QueuedConnection,
			Q_ARG(quint32, static_cast<u32>(level)), Q_ARG(quint32, static_cast<u32>(color)),
			Q_ARG(const QString&, qmessage));
	}
}

void LogWindow::closeEvent(QCloseEvent* event)
{
	if (!m_destroying)
	{
		event->ignore();
		return;
	}
	Log::SetHostOutputLevel(LOGLEVEL_NONE, nullptr);

	saveSize();

	QMainWindow::closeEvent(event);
}

void LogWindow::appendMessage(quint32 level, quint32 color, const QString& message)
{
	QTextCursor temp_cursor = m_text->textCursor();
	QScrollBar* scrollbar = m_text->verticalScrollBar();
	const bool cursor_at_end = temp_cursor.atEnd();
	const bool scroll_at_end = scrollbar->sliderPosition() == scrollbar->maximum();

	temp_cursor.movePosition(QTextCursor::End);

	{
		static constexpr const QColor qcolors[2][ConsoleColors_Count] = {
			// Light theme
			{
				QColor(0, 0, 0), // Color_Default
				QColor(0, 0, 0), // Color_Black
				QColor(128, 0, 0), // Color_Red
				QColor(0, 128, 0), // Color_Green
				QColor(0, 0, 128), // Color_Blue
				QColor(160, 0, 160), // Color_Magenta
				QColor(160, 120, 0), // Color_Orange
				QColor(108, 108, 108), // Color_Gray

				QColor(128, 180, 180), // Color_Cyan
				QColor(180, 180, 128), // Color_Yellow
				QColor(160, 160, 160), // Color_White

				QColor(0, 0, 0), // Color_StrongBlack
				QColor(128, 0, 0), // Color_StrongRed
				QColor(0, 128, 0), // Color_StrongGreen
				QColor(0, 0, 128), // Color_StrongBlue
				QColor(160, 0, 160), // Color_StrongMagenta
				QColor(160, 120, 0), // Color_StrongOrange
				QColor(108, 108, 108), // Color_StrongGray

				QColor(128, 180, 180), // Color_StrongCyan
				QColor(180, 180, 128), // Color_StrongYellow
				QColor(160, 160, 160), // Color_StrongWhite
			},
			// Dark theme
			{
				QColor(208, 208, 208), // Color_Default
				QColor(255, 255, 255), // Color_Black
				QColor(180, 0, 0), // Color_Red
				QColor(0, 160, 0), // Color_Green
				QColor(32, 32, 204), // Color_Blue
				QColor(160, 0, 160), // Color_Magenta
				QColor(160, 120, 0), // Color_Orange
				QColor(128, 128, 128), // Color_Gray
				QColor(128, 180, 180), // Color_Cyan
				QColor(180, 180, 128), // Color_Yellow
				QColor(160, 160, 160), // Color_White
				QColor(255, 255, 255), // Color_StrongBlack
				QColor(180, 0, 0), // Color_StrongRed
				QColor(0, 160, 0), // Color_StrongGreen
				QColor(32, 32, 204), // Color_StrongBlue
				QColor(160, 0, 160), // Color_StrongMagenta
				QColor(160, 120, 0), // Color_StrongOrange
				QColor(128, 128, 128), // Color_StrongGray
				QColor(128, 180, 180), // Color_StrongCyan
				QColor(180, 180, 128), // Color_StrongYellow
				QColor(160, 160, 160), // Color_StrongWhite
			},
		};

		static constexpr const QColor timestamp_color = QColor(0xcc, 0xcc, 0xcc);

		QTextCharFormat format = temp_cursor.charFormat();

		if (Log::AreTimestampsEnabled())
		{
			const float message_time = Log::GetCurrentMessageTime();
			const QString qtimestamp = QStringLiteral("[%1] ").arg(message_time, 10, 'f', 4);
			format.setForeground(QBrush(timestamp_color));
			temp_cursor.setCharFormat(format);
			temp_cursor.insertText(qtimestamp);
		}

		const bool dark = static_cast<u32>(QtHost::IsDarkApplicationTheme());

		// message has \n already
		format.setForeground(QBrush(qcolors[static_cast<u32>(dark)][color]));
		temp_cursor.setCharFormat(format);
		temp_cursor.insertText(message);
	}

	if (cursor_at_end)
	{
		if (scroll_at_end)
		{
			m_text->setTextCursor(temp_cursor);
			scrollbar->setSliderPosition(scrollbar->maximum());
		}
		else
		{
			// Can't let changing the cursor affect the scroll bar...
			const int pos = scrollbar->sliderPosition();
			m_text->setTextCursor(temp_cursor);
			scrollbar->setSliderPosition(pos);
		}
	}
}

void LogWindow::saveSize()
{
	const int current_width = Host::GetBaseIntSettingValue("UI", "LogWindowWidth", DEFAULT_WIDTH);
	const int current_height = Host::GetBaseIntSettingValue("UI", "LogWindowHeight", DEFAULT_HEIGHT);
	const QSize wsize = size();

	bool changed = false;
	if (current_width != wsize.width())
	{
		Host::SetBaseIntSettingValue("UI", "LogWindowWidth", wsize.width());
		changed = true;
	}
	if (current_height != wsize.height())
	{
		Host::SetBaseIntSettingValue("UI", "LogWindowHeight", wsize.height());
		changed = true;
	}

	if (changed)
		Host::CommitBaseSettingChanges();
}

void LogWindow::restoreSize()
{
	const int width = Host::GetBaseIntSettingValue("UI", "LogWindowWidth", DEFAULT_WIDTH);
	const int height = Host::GetBaseIntSettingValue("UI", "LogWindowHeight", DEFAULT_HEIGHT);
	resize(width, height);
}
