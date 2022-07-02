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
#include "pcsx2/Frontend/LogSink.h"
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
	qRegisterMetaType<CDVD_SourceType>();
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
	// Use $XDG_CONFIG_HOME/PCSX2 if it exists.
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home && Path::IsAbsolute(xdg_config_home))
	{
		EmuFolders::DataRoot = Path::Combine(xdg_config_home, "PCSX2");
	}
	else
	{
		// Use ~/PCSX2 for non-XDG, and ~/.config/PCSX2 for XDG.
		// Maybe we should drop the former when Qt goes live.
		const char* home_dir = getenv("HOME");
		if (home_dir)
		{
#ifndef XDG_STD
			EmuFolders::DataRoot = Path::Combine(home_dir, "PCSX2");
#else
			// ~/.config should exist, but just in case it doesn't and this is a fresh profile..
			const std::string config_dir(Path::Combine(home_dir, ".config"));
			if (!FileSystem::DirectoryExists(config_dir.c_str()))
				FileSystem::CreateDirectoryPath(config_dir.c_str(), false);

			EmuFolders::DataRoot = Path::Combine(config_dir, "PCSX2");
#endif
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
	Host::UpdateLogging();
	return true;
}

void QtHost::SetDefaultConfig()
{
	EmuConfig = Pcsx2Config();
	EmuFolders::SetDefaults();
	EmuFolders::EnsureFoldersExist();
	VMManager::SetHardwareDependentDefaultSettings(EmuConfig);

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
