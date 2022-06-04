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
#include <QtWidgets/QMessageBox>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <KnownFolders.h>
#include <ShlObj.h>
#endif

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "pcsx2/DebugTools/Debug.h"
#include "pcsx2/Frontend/GameList.h"
#include "pcsx2/Frontend/INISettingsInterface.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/PAD/Host/PAD.h"

#include "EmuThread.h"
#include "GameList/GameListWidget.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "svnrev.h"

static constexpr u32 SETTINGS_VERSION = 1;
static constexpr u32 SETTINGS_SAVE_DELAY = 1000;

//////////////////////////////////////////////////////////////////////////
// Local function declarations
//////////////////////////////////////////////////////////////////////////
namespace QtHost {
static bool InitializeConfig();
static bool ShouldUsePortableMode();
static void SetAppRoot();
static void SetResourcesDirectory();
static void SetDataDirectory();
static void HookSignals();
static bool SetCriticalFolders();
static void SetDefaultConfig();
static void SaveSettings();
}

//////////////////////////////////////////////////////////////////////////
// Local variable declarations
//////////////////////////////////////////////////////////////////////////
const IConsoleWriter* PatchesCon = &Console;
static std::unique_ptr<QTimer> s_settings_save_timer;
static std::unique_ptr<INISettingsInterface> s_base_settings_interface;
static bool s_batch_mode = false;

//////////////////////////////////////////////////////////////////////////
// Initialization/Shutdown
//////////////////////////////////////////////////////////////////////////

bool QtHost::Initialize()
{
	qRegisterMetaType<std::optional<bool>>();
	qRegisterMetaType<std::function<void()>>();
	qRegisterMetaType<std::shared_ptr<VMBootParameters>>();
	qRegisterMetaType<GSRendererType>();
	qRegisterMetaType<InputBindingKey>();
	qRegisterMetaType<const GameList::Entry*>();

	if (!InitializeConfig())
	{
		// NOTE: No point translating this, because no config means the language won't be loaded anyway.
		QMessageBox::critical(nullptr, QStringLiteral("Error"), QStringLiteral("Failed to initialize config."));
		return false;
	}

	HookSignals();
	EmuThread::start();
	return true;
}

void QtHost::Shutdown()
{
	EmuThread::stop();
	if (g_main_window)
	{
		g_main_window->close();
		delete g_main_window;
	}

	if (emuLog)
	{
		std::fclose(emuLog);
		emuLog = nullptr;
	}
}

bool QtHost::SetCriticalFolders()
{
	SetAppRoot();
	SetResourcesDirectory();
	SetDataDirectory();

	// logging of directories in case something goes wrong super early
	Console.WriteLn("AppRoot Directory: %s", EmuFolders::AppRoot.c_str());
	Console.WriteLn("DataRoot Directory: %s", EmuFolders::DataRoot.c_str());
	Console.WriteLn("Resources Directory: %s", EmuFolders::Resources.c_str());

	// allow SetDataDirectory() to change settings directory (if we want to split config later on)
	if (EmuFolders::Settings.empty())
		EmuFolders::Settings = Path::Combine(EmuFolders::DataRoot, "inis");

	// Write crash dumps to the data directory, since that'll be accessible for certain.
	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	// the resources directory should exist, bail out if not
	if (!FileSystem::DirectoryExists(EmuFolders::Resources.c_str()))
	{
		QMessageBox::critical(nullptr, QStringLiteral("Error"),
			QStringLiteral("Resources directory is missing, your installation is incomplete."));
		return false;
	}

	return true;
}

bool QtHost::ShouldUsePortableMode()
{
	// Check whether portable.ini exists in the program directory.
	return FileSystem::FileExists(Path::Combine(EmuFolders::AppRoot, "portable.ini").c_str());
}

void QtHost::SetAppRoot()
{
	std::string program_path(FileSystem::GetProgramPath());
	Console.WriteLn("Program Path: %s", program_path.c_str());

	EmuFolders::AppRoot = Path::Canonicalize(Path::GetDirectory(program_path));
}

void QtHost::SetResourcesDirectory()
{
#ifndef __APPLE__
	// On Windows/Linux, these are in the binary directory.
	EmuFolders::Resources = Path::Combine(EmuFolders::AppRoot, "resources");
#else
	// On macOS, this is in the bundle resources directory.
	EmuFolders::Resources = Path::Canonicalize(Path::Combine(EmuFolders::AppRoot, "../Resources"));
#endif
}

void QtHost::SetDataDirectory()
{
	if (ShouldUsePortableMode())
	{
		EmuFolders::DataRoot = EmuFolders::AppRoot;
		return;
	}

#if defined(_WIN32)
	// On Windows, use My Documents\PCSX2 to match old installs.
	PWSTR documents_directory;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents_directory)))
	{
		if (std::wcslen(documents_directory) > 0)
			EmuFolders::DataRoot = Path::Combine(StringUtil::WideStringToUTF8String(documents_directory), "PCSX2");
		CoTaskMemFree(documents_directory);
	}
#elif defined(__linux__)
	// Check for $HOME/PCSX2 first, for legacy installs.
	const char* home_dir = getenv("HOME");
	const std::string legacy_dir(home_dir ? Path::Combine(home_dir, "PCSX2") : std::string());
	if (!legacy_dir.empty() && FileSystem::DirectoryExists(legacy_dir.c_str()))
	{
		EmuFolders::DataRoot = std::move(legacy_dir);
	}
	else
	{
		// otherwise, use $XDG_CONFIG_HOME/PCSX2.
		const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
		if (xdg_config_home && xdg_config_home[0] == '/' && FileSystem::DirectoryExists(xdg_config_home))
		{
			EmuFolders::DataRoot = Path::Combine(xdg_config_home, "PCSX2");
		}
		else if (!legacy_dir.empty())
		{
			// fall back to the legacy PCSX2-in-home.
			EmuFolders::DataRoot = std::move(legacy_dir);
		}
	}
#elif defined(__APPLE__)
	static constexpr char MAC_DATA_DIR[] = "Library/Application Support/PCSX2";
	const char* home_dir = getenv("HOME");
	if (home_dir)
		EmuFolders::DataRoot = Path::Combine(home_dir, MAC_DATA_DIR);
#endif

	// make sure it exists
	if (!EmuFolders::DataRoot.empty() && !FileSystem::DirectoryExists(EmuFolders::DataRoot.c_str()))
	{
		// we're in trouble if we fail to create this directory... but try to hobble on with portable
		if (!FileSystem::CreateDirectoryPath(EmuFolders::DataRoot.c_str(), false))
			EmuFolders::DataRoot.clear();
	}

	// couldn't determine the data directory? fallback to portable.
	if (EmuFolders::DataRoot.empty())
		EmuFolders::DataRoot = EmuFolders::AppRoot;
}

void QtHost::UpdateFolders()
{
	// TODO: This should happen with the VM thread paused.
	auto lock = Host::GetSettingsLock();
	EmuFolders::LoadConfig(*s_base_settings_interface.get());
	EmuFolders::EnsureFoldersExist();
}

bool QtHost::InitializeConfig()
{
	if (!SetCriticalFolders())
		return false;

	const std::string path(Path::Combine(EmuFolders::Settings, "PCSX2.ini"));
	Console.WriteLn("Loading config from %s.", path.c_str());
	s_base_settings_interface = std::make_unique<INISettingsInterface>(std::move(path));
	Host::Internal::SetBaseSettingsLayer(s_base_settings_interface.get());

	uint settings_version;
	if (!s_base_settings_interface->Load() ||
		!s_base_settings_interface->GetUIntValue("UI", "SettingsVersion", &settings_version) ||
		settings_version != SETTINGS_VERSION)
	{
		QMessageBox::critical(
			g_main_window, qApp->translate("QtHost", "Settings Reset"),
			qApp->translate("QtHost", "Settings do not exist or are the incorrect version, resetting to defaults."));
		SetDefaultConfig();
		s_base_settings_interface->Save();
	}

	// TODO: Handle reset to defaults if load fails.
	EmuFolders::LoadConfig(*s_base_settings_interface.get());
	EmuFolders::EnsureFoldersExist();
	QtHost::UpdateLogging();
	return true;
}

void QtHost::SetDefaultConfig()
{
	EmuConfig = Pcsx2Config();
	EmuFolders::SetDefaults();

	SettingsInterface& si = *s_base_settings_interface.get();
	si.SetUIntValue("UI", "SettingsVersion", SETTINGS_VERSION);

	{
		SettingsSaveWrapper wrapper(si);
		EmuConfig.LoadSave(wrapper);
	}

	EmuFolders::Save(si);
	PAD::SetDefaultConfig(si);
}

void QtHost::SetBaseBoolSettingValue(const char* section, const char* key, bool value)
{
	auto lock = Host::GetSettingsLock();
	s_base_settings_interface->SetBoolValue(section, key, value);
	QueueSettingsSave();
}

void QtHost::SetBaseIntSettingValue(const char* section, const char* key, int value)
{
	auto lock = Host::GetSettingsLock();
	s_base_settings_interface->SetIntValue(section, key, value);
	QueueSettingsSave();
}

void QtHost::SetBaseFloatSettingValue(const char* section, const char* key, float value)
{
	auto lock = Host::GetSettingsLock();
	s_base_settings_interface->SetFloatValue(section, key, value);
	QueueSettingsSave();
}

void QtHost::SetBaseStringSettingValue(const char* section, const char* key, const char* value)
{
	auto lock = Host::GetSettingsLock();
	s_base_settings_interface->SetStringValue(section, key, value);
	QueueSettingsSave();
}

void QtHost::SetBaseStringListSettingValue(const char* section, const char* key, const std::vector<std::string>& values)
{
	auto lock = Host::GetSettingsLock();
	s_base_settings_interface->SetStringList(section, key, values);
	QueueSettingsSave();
}

bool QtHost::AddBaseValueToStringList(const char* section, const char* key, const char* value)
{
	auto lock = Host::GetSettingsLock();
	if (!s_base_settings_interface->AddToStringList(section, key, value))
		return false;

	QueueSettingsSave();
	return true;
}

bool QtHost::RemoveBaseValueFromStringList(const char* section, const char* key, const char* value)
{
	auto lock = Host::GetSettingsLock();
	if (!s_base_settings_interface->RemoveFromStringList(section, key, value))
		return false;

	QueueSettingsSave();
	return true;
}

void QtHost::RemoveBaseSettingValue(const char* section, const char* key)
{
	auto lock = Host::GetSettingsLock();
	s_base_settings_interface->DeleteValue(section, key);
	QueueSettingsSave();
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

void QtHost::QueueSettingsSave()
{
	if (s_settings_save_timer)
		return;

	s_settings_save_timer = std::make_unique<QTimer>();
	s_settings_save_timer->connect(s_settings_save_timer.get(), &QTimer::timeout, SaveSettings);
	s_settings_save_timer->setSingleShot(true);
	s_settings_save_timer->start(SETTINGS_SAVE_DELAY);
}

bool QtHost::InBatchMode()
{
	return s_batch_mode;
}

void QtHost::SetBatchMode(bool enabled)
{
	s_batch_mode = enabled;
}

void QtHost::RunOnUIThread(const std::function<void()>& func, bool block /*= false*/)
{
	// main window always exists, so it's fine to attach it to that.
	QMetaObject::invokeMethod(g_main_window, "runOnUIThread",
		block ? Qt::BlockingQueuedConnection : Qt::QueuedConnection,
		Q_ARG(const std::function<void()>&, func));
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

// Used on both Windows and Linux.
#ifdef _WIN32
static const wchar_t s_console_colors[][ConsoleColors_Count] = {
#define CC(x) L ## x
#else
static const char s_console_colors[][ConsoleColors_Count] = {
#define CC(x) x
#endif
	CC("\033[0m"), // default
	CC("\033[30m\033[1m"), // black
	CC("\033[32m"), // green
	CC("\033[31m"), // red
	CC("\033[34m"), // blue
	CC("\033[35m"), // magenta
	CC("\033[35m"), // orange (FIXME)
	CC("\033[37m"), // gray
	CC("\033[36m"), // cyan
	CC("\033[33m"), // yellow
	CC("\033[37m"), // white
	CC("\033[30m\033[1m"), // strong black
	CC("\033[31m\033[1m"), // strong red
	CC("\033[32m\033[1m"), // strong green
	CC("\033[34m\033[1m"), // strong blue
	CC("\033[35m\033[1m"), // strong magenta
	CC("\033[35m\033[1m"), // strong orange (FIXME)
	CC("\033[37m\033[1m"), // strong gray
	CC("\033[36m\033[1m"), // strong cyan
	CC("\033[33m\033[1m"), // strong yellow
	CC("\033[37m\033[1m") // strong white
};
#undef CC

static Common::Timer::Value s_log_start_timestamp = Common::Timer::GetCurrentValue();
static bool s_log_timestamps = false;
static std::mutex s_log_mutex;

// Replacement for Console so we actually get output to our console window on Windows.
#ifdef _WIN32

static bool s_debugger_attached = false;
static bool s_console_handle_set = false;
static bool s_console_allocated = false;
static HANDLE s_console_handle = INVALID_HANDLE_VALUE;
static HANDLE s_old_console_stdin = NULL;
static HANDLE s_old_console_stdout = NULL;
static HANDLE s_old_console_stderr = NULL;

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	Console.WriteLn("Handler %u", dwCtrlType);
	if (dwCtrlType != CTRL_C_EVENT)
		return FALSE;

	SignalHandler(SIGTERM);
	return TRUE;
}

static bool EnableVirtualTerminalProcessing(HANDLE hConsole)
{
	if (hConsole == INVALID_HANDLE_VALUE)
		return false;

	DWORD old_mode;
	if (!GetConsoleMode(hConsole, &old_mode))
		return false;

	// already enabled?
	if (old_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
		return true;

	return SetConsoleMode(hConsole, old_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#endif

static void ConsoleQt_SetTitle(const char* title)
{
#ifdef _WIN32
	SetConsoleTitleW(StringUtil::UTF8StringToWideString(title).c_str());
#else
	std::fprintf(stdout, "\033]0;%s\007", title);
#endif
}

static void ConsoleQt_DoSetColor(ConsoleColors color)
{
#ifdef _WIN32
	if (s_console_handle == INVALID_HANDLE_VALUE)
		return;

	const wchar_t* colortext = s_console_colors[static_cast<u32>(color)];
	DWORD written;
	WriteConsoleW(s_console_handle, colortext, std::wcslen(colortext), &written, nullptr);
#else
	const char* colortext = s_console_colors[static_cast<u32>(color)];
	std::fputs(colortext, stdout);
#endif
}

static void ConsoleQt_DoWrite(const char* fmt)
{
	std::unique_lock lock(s_log_mutex);

#ifdef _WIN32
	if (s_console_handle != INVALID_HANDLE_VALUE || s_debugger_attached)
	{
		// TODO: Put this on the stack.
		std::wstring wfmt(StringUtil::UTF8StringToWideString(fmt));

		if (s_debugger_attached)
			OutputDebugStringW(wfmt.c_str());

		if (s_console_handle != INVALID_HANDLE_VALUE)
		{
			DWORD written;
			WriteConsoleW(s_console_handle, wfmt.c_str(), static_cast<DWORD>(wfmt.length()), &written, nullptr);
		}
	}
#else
	std::fputs(fmt, stdout);
	std::fputc('\n', stdout);
#endif

	if (emuLog)
	{
		std::fputs(fmt, emuLog);
	}
}

static void ConsoleQt_DoWriteLn(const char* fmt)
{
	std::unique_lock lock(s_log_mutex);

	// find time since start of process, but save a syscall if we're not writing timestamps
	float message_time = s_log_timestamps ?
							 static_cast<float>(
								 Common::Timer::ConvertValueToSeconds(Common::Timer::GetCurrentValue() - s_log_start_timestamp)) :
                             0.0f;

	// split newlines up
	const char* start = fmt;
	do
	{
		const char* end = std::strchr(start, '\n');

		std::string_view line;
		if (end)
		{
			line = std::string_view(start, end - start);
			start = end + 1;
		}
		else
		{
			line = std::string_view(start);
			start = nullptr;
		}

#ifdef _WIN32
		if (s_console_handle != INVALID_HANDLE_VALUE || s_debugger_attached)
		{
			// TODO: Put this on the stack.
			std::wstring wfmt(StringUtil::UTF8StringToWideString(line));

			if (s_debugger_attached)
			{
				// VS already timestamps logs (at least with the productivity power tools).
				if (!wfmt.empty())
					OutputDebugStringW(wfmt.c_str());
				OutputDebugStringW(L"\n");
			}

			if (s_console_handle != INVALID_HANDLE_VALUE)
			{
				DWORD written;
				if (s_log_timestamps)
				{
					wchar_t timestamp_text[128];
					const int timestamp_len = _swprintf(timestamp_text, L"[%10.4f] ", message_time);
					WriteConsoleW(s_console_handle, timestamp_text, static_cast<DWORD>(timestamp_len), &written, nullptr);
				}

				if (!wfmt.empty())
					WriteConsoleW(s_console_handle, wfmt.c_str(), static_cast<DWORD>(wfmt.length()), &written, nullptr);

				WriteConsoleW(s_console_handle, L"\n", 1, &written, nullptr);
			}
		}
#else
		if (s_log_timestamps)
		{
			std::fprintf(stdout, "[%10.4f] %.*s\n", message_time, static_cast<int>(line.length()), line.data());
		}
		else
		{
			if (!line.empty())
				std::fwrite(line.data(), line.length(), 1, stdout);
			std::fputc('\n', stdout);
		}
#endif

		if (emuLog)
		{
			if (s_log_timestamps)
			{
				std::fprintf(emuLog, "[%10.4f] %.*s\n", message_time, static_cast<int>(line.length()), line.data());
			}
			else
			{
				std::fwrite(line.data(), line.length(), 1, emuLog);
				std::fputc('\n', emuLog);
			}
		}
	} while (start);
}

static void ConsoleQt_Newline()
{
	ConsoleQt_DoWriteLn("");
}

static const IConsoleWriter ConsoleWriter_WinQt =
	{
		ConsoleQt_DoWrite,
		ConsoleQt_DoWriteLn,
		ConsoleQt_DoSetColor,

		ConsoleQt_DoWrite,
		ConsoleQt_Newline,
		ConsoleQt_SetTitle,
};

static void UpdateLoggingSinks(bool system_console, bool file_log)
{
#ifdef _WIN32
	const bool debugger_attached = IsDebuggerPresent();
	s_debugger_attached = debugger_attached;
	if (system_console)
	{
		if (!s_console_handle_set)
		{
			s_old_console_stdin = GetStdHandle(STD_INPUT_HANDLE);
			s_old_console_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
			s_old_console_stderr = GetStdHandle(STD_ERROR_HANDLE);

			bool handle_valid = (GetConsoleWindow() != NULL);
			if (!handle_valid)
			{
				s_console_allocated = AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole();
				handle_valid = (GetConsoleWindow() != NULL);
			}

			if (handle_valid)
			{
				s_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
				if (s_console_handle != INVALID_HANDLE_VALUE)
				{
					s_console_handle_set = true;
					SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

					// This gets us unix-style coloured output.
					EnableVirtualTerminalProcessing(GetStdHandle(STD_OUTPUT_HANDLE));
					EnableVirtualTerminalProcessing(GetStdHandle(STD_ERROR_HANDLE));

					// Redirect stdout/stderr.
					std::FILE* fp;
					freopen_s(&fp, "CONIN$", "r", stdin);
					freopen_s(&fp, "CONOUT$", "w", stdout);
					freopen_s(&fp, "CONOUT$", "w", stderr);
				}
			}
		}

		// just in case it fails
		system_console = s_console_handle_set;
	}
	else
	{
		if (s_console_handle_set)
		{
			s_console_handle_set = false;
			SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);

			// redirect stdout/stderr back to null.
			std::FILE* fp;
			freopen_s(&fp, "NUL:", "w", stderr);
			freopen_s(&fp, "NUL:", "w", stdout);
			freopen_s(&fp, "NUL:", "w", stdin);

			// release console and restore state
			SetStdHandle(STD_INPUT_HANDLE, s_old_console_stdin);
			SetStdHandle(STD_OUTPUT_HANDLE, s_old_console_stdout);
			SetStdHandle(STD_ERROR_HANDLE, s_old_console_stderr);
			s_old_console_stdin = NULL;
			s_old_console_stdout = NULL;
			s_old_console_stderr = NULL;
			if (s_console_allocated)
			{
				s_console_allocated = false;
				FreeConsole();
			}
		}
	}
#else
	const bool debugger_attached = false;
#endif

	if (file_log)
	{
		if (!emuLog)
		{
			emuLogName = Path::Combine(EmuFolders::Logs, "emulog.txt");
			emuLog = FileSystem::OpenCFile(emuLogName.c_str(), "wb");
			file_log = (emuLog != nullptr);
		}
	}
	else
	{
		if (emuLog)
		{
			std::fclose(emuLog);
			emuLog = nullptr;
			emuLogName = {};
		}
	}

	// Discard logs completely if there's no sinks.
	if (debugger_attached || system_console || file_log)
		Console_SetActiveHandler(ConsoleWriter_WinQt);
	else
		Console_SetActiveHandler(ConsoleWriter_Null);
}

void QtHost::InitializeEarlyConsole()
{
	UpdateLoggingSinks(true, false);
}

void QtHost::UpdateLogging()
{
	const bool system_console_enabled = Host::GetBaseBoolSettingValue("Logging", "EnableSystemConsole", false);
	const bool file_logging_enabled = Host::GetBaseBoolSettingValue("Logging", "EnableFileLogging", false);

	s_log_timestamps = Host::GetBaseBoolSettingValue("Logging", "EnableTimestamps", true);

	const bool any_logging_sinks = system_console_enabled || file_logging_enabled;
	DevConWriterEnabled = any_logging_sinks && (IsDevBuild || Host::GetBaseBoolSettingValue("Logging", "EnableVerbose", false));
	SysConsole.eeConsole.Enabled = any_logging_sinks && Host::GetBaseBoolSettingValue("Logging", "EnableEEConsole", false);
	SysConsole.iopConsole.Enabled = any_logging_sinks && Host::GetBaseBoolSettingValue("Logging", "EnableIOPConsole", false);
    
	// Input Recording Logs
	SysConsole.recordingConsole.Enabled = any_logging_sinks && Host::GetBaseBoolSettingValue("Logging", "EnableInputRecordingLogs", true);
	SysConsole.controlInfo.Enabled = any_logging_sinks && Host::GetBaseBoolSettingValue("Logging", "EnableControllerLogs", false);

	UpdateLoggingSinks(system_console_enabled, file_logging_enabled);
}
