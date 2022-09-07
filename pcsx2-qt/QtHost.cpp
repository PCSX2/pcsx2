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

#include <csignal>

#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtGui/QClipboard>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/DebugTools/Debug.h"
#include "pcsx2/Frontend/CommonHost.h"
#include "pcsx2/Frontend/GameList.h"
#include "pcsx2/Frontend/INISettingsInterface.h"
#include "pcsx2/Frontend/LogSink.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/PAD/Host/PAD.h"

#include "EmuThread.h"
#include "GameList/GameListWidget.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "svnrev.h"

static constexpr u32 SETTINGS_SAVE_DELAY = 1000;

//////////////////////////////////////////////////////////////////////////
// Local function declarations
//////////////////////////////////////////////////////////////////////////
namespace QtHost {
static void PrintCommandLineVersion();
static void PrintCommandLineHelp(const char* progname);
static std::shared_ptr<VMBootParameters>& AutoBoot(std::shared_ptr<VMBootParameters>& autoboot);
static bool ParseCommandLineOptions(int argc, char* argv[], std::shared_ptr<VMBootParameters>& autoboot);
static bool InitializeConfig();
static void SaveSettings();
static void HookSignals();
}

//////////////////////////////////////////////////////////////////////////
// Local variable declarations
//////////////////////////////////////////////////////////////////////////
const IConsoleWriter* PatchesCon = &Console;
static std::unique_ptr<QTimer> s_settings_save_timer;
static std::unique_ptr<INISettingsInterface> s_base_settings_interface;
static bool s_batch_mode = false;
static bool s_nogui_mode = false;
static bool s_start_fullscreen_ui = false;
static bool s_start_fullscreen_ui_fullscreen = false;

//////////////////////////////////////////////////////////////////////////
// Initialization/Shutdown
//////////////////////////////////////////////////////////////////////////

bool QtHost::InitializeConfig()
{
	if (!CommonHost::InitializeCriticalFolders())
	{
		QMessageBox::critical(nullptr, QStringLiteral("PCSX2"),
			QStringLiteral("One or more critical directories are missing, your installation may be incomplete."));
		return false;
	}

	const std::string path(Path::Combine(EmuFolders::Settings, "PCSX2.ini"));
	Console.WriteLn("Loading config from %s.", path.c_str());

	s_base_settings_interface = std::make_unique<INISettingsInterface>(std::move(path));
	Host::Internal::SetBaseSettingsLayer(s_base_settings_interface.get());
	if (!s_base_settings_interface->Load() || !CommonHost::CheckSettingsVersion())
	{
		// If the config file doesn't exist, assume this is a new install and don't prompt to overwrite.
		if (FileSystem::FileExists(s_base_settings_interface->GetFileName().c_str()) &&
			QMessageBox::question(nullptr, QStringLiteral("PCSX2"),
				QStringLiteral("Settings failed to load, or are the incorrect version. Clicking Yes will reset all settings to defaults. Do you want to continue?")) != QMessageBox::Yes)
		{
			return false;
		}

		CommonHost::SetDefaultSettings(*s_base_settings_interface, true, true, true, true, true);
		SaveSettings();
	}

	CommonHost::LoadStartupSettings();
	Host::UpdateLogging(QtHost::InNoGUIMode());
	return true;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
	Host::SetDefaultLoggingSettings(si);

	si.SetBoolValue("UI", "InhibitScreensaver", true);
	si.SetBoolValue("UI", "ConfirmShutdown", true);
	si.SetBoolValue("UI", "StartPaused", false);
	si.SetBoolValue("UI", "PauseOnFocusLoss", false);
	si.SetBoolValue("UI", "StartFullscreen", false);
	si.SetBoolValue("UI", "DoubleClickTogglesFullscreen", true);
	si.SetBoolValue("UI", "HideMouseCursor", false);
	si.SetBoolValue("UI", "RenderToSeparateWindow", false);
	si.SetBoolValue("UI", "HideMainWindowWhenRunning", false);
	si.SetBoolValue("UI", "DisableWindowResize", false);
	si.SetStringValue("UI", "Theme", MainWindow::DEFAULT_THEME_NAME);
}

void QtHost::SaveSettings()
{
	pxAssertRel(!g_emu_thread->isOnEmuThread(), "Saving should happen on the UI thread.");

	{
		auto lock = Host::GetSettingsLock();
		if (!s_base_settings_interface->Save())
			Console.Error("Failed to save settings.");
	}

	s_settings_save_timer->deleteLater();
	s_settings_save_timer.release();
}

void Host::CommitBaseSettingChanges()
{
	if (!QtHost::IsOnUIThread())
	{
		QtHost::RunOnUIThread(&Host::CommitBaseSettingChanges);
		return;
	}

	auto lock = Host::GetSettingsLock();
	if (s_settings_save_timer)
		return;

	s_settings_save_timer = std::make_unique<QTimer>();
	s_settings_save_timer->connect(s_settings_save_timer.get(), &QTimer::timeout, &QtHost::SaveSettings);
	s_settings_save_timer->setSingleShot(true);
	s_settings_save_timer->start(SETTINGS_SAVE_DELAY);
}

bool QtHost::InBatchMode()
{
	return s_batch_mode;
}

bool QtHost::InNoGUIMode()
{
	return s_nogui_mode;
}

bool QtHost::IsOnUIThread()
{
	QThread* ui_thread = qApp->thread();
	return (QThread::currentThread() == ui_thread);
}

void QtHost::RunOnUIThread(const std::function<void()>& func, bool block /*= false*/)
{
	// main window always exists, so it's fine to attach it to that.
	QMetaObject::invokeMethod(g_main_window, "runOnUIThread",
		block ? Qt::BlockingQueuedConnection : Qt::QueuedConnection,
		Q_ARG(const std::function<void()>&, func));
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	{
		auto lock = Host::GetSettingsLock();
		CommonHost::SetDefaultSettings(*s_base_settings_interface.get(), folders, core, controllers, hotkeys, ui);
	}
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();
	if (folders)
		g_emu_thread->updateEmuFolders();

	return true;
}

QString QtHost::GetAppNameAndVersion()
{
	QString ret;
	if constexpr (!PCSX2_isReleaseVersion && GIT_TAGGED_COMMIT)
	{
		ret = QStringLiteral("PCSX2 Nightly - " GIT_TAG);
	}
	else if constexpr (PCSX2_isReleaseVersion)
	{
#define APPNAME_STRINGIZE(x) #x
		ret = QStringLiteral("PCSX2 "
			APPNAME_STRINGIZE(PCSX2_VersionHi) "."
			APPNAME_STRINGIZE(PCSX2_VersionMid) "."
			APPNAME_STRINGIZE(PCSX2_VersionLo));
#undef APPNAME_STRINGIZE
	}
	else
	{
		return QStringLiteral("PCSX2 " GIT_REV);
	}

	return ret;
}

QString QtHost::GetAppConfigSuffix()
{
#if defined(PCSX2_DEBUG)
	return QStringLiteral(" [Debug]");
#elif defined(PCSX2_DEVBUILD)
	return QStringLiteral(" [Devel]");
#else
	return QString();
#endif
}

QString QtHost::GetResourcesBasePath()
{
	return QString::fromStdString(EmuFolders::Resources);
}

std::optional<std::vector<u8>> Host::ReadResourceFile(const char* filename)
{
	const std::string path(Path::Combine(EmuFolders::Resources, filename));
	std::optional<std::vector<u8>> ret(FileSystem::ReadBinaryFile(path.c_str()));
	if (!ret.has_value())
		Console.Error("Failed to read resource file '%s'", filename);
	return ret;
}

std::optional<std::string> Host::ReadResourceFileToString(const char* filename)
{
	const std::string path(Path::Combine(EmuFolders::Resources, filename));
	std::optional<std::string> ret(FileSystem::ReadFileToString(path.c_str()));
	if (!ret.has_value())
		Console.Error("Failed to read resource file to string '%s'", filename);
	return ret;
}

void Host::ReportErrorAsync(const std::string_view& title, const std::string_view& message)
{
	if (!title.empty() && !message.empty())
	{
		Console.Error("ReportErrorAsync: %.*s: %.*s",
			static_cast<int>(title.size()), title.data(),
			static_cast<int>(message.size()), message.data());
	}
	else if (!message.empty())
	{
		Console.Error("ReportErrorAsync: %.*s",
			static_cast<int>(message.size()), message.data());
	}

	QMetaObject::invokeMethod(g_main_window, "reportError", Qt::QueuedConnection,
		Q_ARG(const QString&, title.empty() ? QString() : QString::fromUtf8(title.data(), title.size())),
		Q_ARG(const QString&, message.empty() ? QString() : QString::fromUtf8(message.data(), message.size())));
}

bool Host::ConfirmMessage(const std::string_view& title, const std::string_view& message)
{
	const QString qtitle(QString::fromUtf8(title.data(), title.size()));
	const QString qmessage(QString::fromUtf8(message.data(), message.size()));
	return g_emu_thread->confirmMessage(qtitle, qmessage);
}

void Host::OpenURL(const std::string_view& url)
{
	QtHost::RunOnUIThread([url = QtUtils::StringViewToQString(url)]() {
		QtUtils::OpenURL(g_main_window, QUrl(url));
	});
}

bool Host::CopyTextToClipboard(const std::string_view& text)
{
	QtHost::RunOnUIThread([text = QtUtils::StringViewToQString(text)]() {
		QClipboard* clipboard = QGuiApplication::clipboard();
		if (clipboard)
			clipboard->setText(text);
	});
	return true;
}

void Host::OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name)
{
	emit g_emu_thread->onInputDeviceConnected(
		identifier.empty() ? QString() : QString::fromUtf8(identifier.data(), identifier.size()),
		device_name.empty() ? QString() : QString::fromUtf8(device_name.data(), device_name.size()));
}

void Host::OnInputDeviceDisconnected(const std::string_view& identifier)
{
	emit g_emu_thread->onInputDeviceDisconnected(
		identifier.empty() ? QString() : QString::fromUtf8(identifier.data(), identifier.size()));
}

//////////////////////////////////////////////////////////////////////////
// Interface Stuff
//////////////////////////////////////////////////////////////////////////

static void SignalHandler(int signal)
{
	// First try the normal (graceful) shutdown/exit.
	static bool graceful_shutdown_attempted = false;
	if (!graceful_shutdown_attempted && g_main_window)
	{
		std::fprintf(stderr, "Received CTRL+C, attempting graceful shutdown. Press CTRL+C again to force.\n");
		graceful_shutdown_attempted = true;

		// This could be a bit risky invoking from a signal handler... hopefully it's okay.
		QMetaObject::invokeMethod(g_main_window, &MainWindow::requestExit, Qt::QueuedConnection);
		return;
	}

	std::signal(signal, SIG_DFL);

	// MacOS is missing std::quick_exit() despite it being C++11...
#ifndef __APPLE__
	std::quick_exit(1);
#else
	_Exit(1);
#endif
}

void QtHost::HookSignals()
{
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);
}

void QtHost::PrintCommandLineVersion()
{
	Host::InitializeEarlyConsole();
	std::fprintf(stderr, "%s\n", (GetAppNameAndVersion() + GetAppConfigSuffix()).toUtf8().constData());
	std::fprintf(stderr, "https://pcsx2.net/\n");
	std::fprintf(stderr, "\n");
}

void QtHost::PrintCommandLineHelp(const char* progname)
{
	PrintCommandLineVersion();
	std::fprintf(stderr, "Usage: %s [parameters] [--] [boot filename]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "  -version: Displays version information and exits.\n");
	std::fprintf(stderr, "  -batch: Enables batch mode (exits after shutting down).\n");
	std::fprintf(stderr, "  -nogui: Hides main window while running (implies batch mode).\n");
	std::fprintf(stderr, "  -elf <file>: Overrides the boot ELF with the specified filename.\n");
	std::fprintf(stderr, "  -disc <path>: Uses the specified host DVD drive as a source.\n");
	std::fprintf(stderr, "  -bios: Starts the BIOS (System Menu/OSDSYS).\n");
	std::fprintf(stderr, "  -fastboot: Force fast boot for provided filename.\n");
	std::fprintf(stderr, "  -slowboot: Force slow boot for provided filename.\n");
	std::fprintf(stderr, "  -state <index>: Loads specified save state by index.\n");
	std::fprintf(stderr, "  -statefile <filename>: Loads state from the specified filename.\n");
	std::fprintf(stderr, "  -fullscreen: Enters fullscreen mode immediately after starting.\n");
	std::fprintf(stderr, "  -nofullscreen: Prevents fullscreen mode from triggering if enabled.\n");
	std::fprintf(stderr, "  -earlyconsolelog: Forces logging of early console messages to console.\n");
	std::fprintf(stderr, "  --: Signals that no more arguments will follow and the remaining\n"
						 "    parameters make up the filename. Use when the filename contains\n"
						 "    spaces or starts with a dash.\n");
	std::fprintf(stderr, "\n");
}

std::shared_ptr<VMBootParameters>& QtHost::AutoBoot(std::shared_ptr<VMBootParameters>& autoboot)
{
	if (!autoboot)
		autoboot = std::make_shared<VMBootParameters>();

	return autoboot;
}

bool QtHost::ParseCommandLineOptions(int argc, char* argv[], std::shared_ptr<VMBootParameters>& autoboot)
{
	bool no_more_args = false;

	for (int i = 1; i < argc; i++)
	{
		if (!no_more_args)
		{
#define CHECK_ARG(str) !std::strcmp(argv[i], str)
#define CHECK_ARG_PARAM(str) (!std::strcmp(argv[i], str) && ((i + 1) < argc))

			if (CHECK_ARG("-help"))
			{
				PrintCommandLineHelp(argv[0]);
				return false;
			}
			else if (CHECK_ARG("-version"))
			{
				PrintCommandLineVersion();
				return false;
			}
			else if (CHECK_ARG("-batch"))
			{
				s_batch_mode = true;
				continue;
			}
			else if (CHECK_ARG("-nogui"))
			{
				s_batch_mode = true;
				s_nogui_mode = true;
				continue;
			}
			else if (CHECK_ARG("-fastboot"))
			{
				AutoBoot(autoboot)->fast_boot = true;
				continue;
			}
			else if (CHECK_ARG("-slowboot"))
			{
				AutoBoot(autoboot)->fast_boot = false;
				continue;
			}
			else if (CHECK_ARG_PARAM("-state"))
			{
				AutoBoot(autoboot)->state_index = std::atoi(argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("-statefile"))
			{
				AutoBoot(autoboot)->save_state = argv[++i];
				continue;
			}
			else if (CHECK_ARG_PARAM("-elf"))
			{
				AutoBoot(autoboot)->elf_override = argv[++i];
				continue;
			}
			else if (CHECK_ARG_PARAM("-disc"))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::Disc;
				AutoBoot(autoboot)->filename = argv[++i];
				continue;
			}
			else if (CHECK_ARG("-bios"))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::NoDisc;
				continue;
			}
			else if (CHECK_ARG("-fullscreen"))
			{
				AutoBoot(autoboot)->fullscreen = true;
				s_start_fullscreen_ui_fullscreen = true;
				continue;
			}
			else if (CHECK_ARG("-nofullscreen"))
			{
				AutoBoot(autoboot)->fullscreen = false;
				continue;
			}
			else if (CHECK_ARG("-earlyconsolelog"))
			{
				Host::InitializeEarlyConsole();
				continue;
			}
			else if (CHECK_ARG("-bigpicture"))
			{
				s_start_fullscreen_ui = true;
				continue;
			}
			else if (CHECK_ARG("--"))
			{
				no_more_args = true;
				continue;
			}
			else if (argv[i][0] == '-')
			{
				Host::InitializeEarlyConsole();
				std::fprintf(stderr, "Unknown parameter: '%s'", argv[i]);
				return false;
			}

#undef CHECK_ARG
#undef CHECK_ARG_PARAM
		}

		if (!AutoBoot(autoboot)->filename.empty())
			AutoBoot(autoboot)->filename += ' ';

		AutoBoot(autoboot)->filename += argv[i];
	}

	// check autoboot parameters, if we set something like fullscreen without a bios
	// or disc, we don't want to actually start.
	if (autoboot && !autoboot->source_type.has_value() && autoboot->filename.empty() && autoboot->elf_override.empty())
	{
		Host::InitializeEarlyConsole();
		Console.Warning("Skipping autoboot due to no boot parameters.");
		autoboot.reset();
	}

	// if we don't have autoboot, we definitely don't want batch mode (because that'll skip
	// scanning the game list).
	if (s_batch_mode && !s_start_fullscreen_ui && !autoboot)
	{
		QMessageBox::critical(nullptr, QStringLiteral("Error"), s_nogui_mode ?
			QStringLiteral("Cannot use no-gui mode, because no boot filename was specified.") :
			QStringLiteral("Cannot use batch mode, because no boot filename was specified."));
		return false;
	}

	return true;
}

#ifndef _WIN32

// See note in EarlyHardwareChecks.cpp as to why we don't do this on Windows.
static bool PerformEarlyHardwareChecks()
{
	// NOTE: No point translating this message, because the configuration isn't loaded yet, so we
	// won't know which language to use, and loading the configuration uses float instructions.
	const char* error;
	if (VMManager::PerformEarlyHardwareChecks(&error))
		return true;

	QMessageBox::critical(nullptr, QStringLiteral("Hardware Check Failed"), QString::fromUtf8(error));
	return false;
}

#endif

static void RegisterTypes()
{
	qRegisterMetaType<std::optional<bool>>();
	qRegisterMetaType<std::function<void()>>();
	qRegisterMetaType<std::shared_ptr<VMBootParameters>>();
	qRegisterMetaType<GSRendererType>();
	qRegisterMetaType<InputBindingKey>();
	qRegisterMetaType<CDVD_SourceType>();
	qRegisterMetaType<const GameList::Entry*>();
}

int main(int argc, char* argv[])
{
	CrashHandler::Install();

	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	RegisterTypes();

	QApplication app(argc, argv);

#ifndef _WIN32
	if (!PerformEarlyHardwareChecks())
		return EXIT_FAILURE;
#endif

	std::shared_ptr<VMBootParameters> autoboot;
	if (!QtHost::ParseCommandLineOptions(argc, argv, autoboot))
		return EXIT_FAILURE;

	// Bail out if we can't find any config.
	if (!QtHost::InitializeConfig())
		return EXIT_FAILURE;

	// Set theme before creating any windows.
	MainWindow::updateApplicationTheme();
	MainWindow* main_window = new MainWindow(QApplication::style()->objectName());

	// Start up the CPU thread.
	QtHost::HookSignals();
	EmuThread::start();

	// Create all window objects, the emuthread might still be starting up at this point.
	main_window->initialize();

	// When running in batch mode, ensure game list is loaded, but don't scan for any new files.
	if (!s_batch_mode)
		main_window->refreshGameList(false);
	else
		GameList::Refresh(false, true);

	// Don't bother showing the window in no-gui mode.
	if (!s_nogui_mode)
		main_window->show();

	// Initialize big picture mode if requested.
	if (s_start_fullscreen_ui)
		g_emu_thread->startFullscreenUI(s_start_fullscreen_ui_fullscreen);

	// Skip the update check if we're booting a game directly.
	if (autoboot)
		g_emu_thread->startVM(std::move(autoboot));
	else if (!s_nogui_mode)
		main_window->startupUpdateCheck();

	// This doesn't return until we exit.
	const int result = app.exec();

	// Shutting down.
	EmuThread::stop();
	if (g_main_window)
	{
		g_main_window->close();
		delete g_main_window;
	}

	// Ensure emulog is flushed.
	if (emuLog)
	{
		std::fclose(emuLog);
		emuLog = nullptr;
	}

	return result;
}
