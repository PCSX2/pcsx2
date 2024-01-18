// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AutoUpdaterDialog.h"
#include "DisplayWidget.h"
#include "GameList/GameListWidget.h"
#include "LogWindow.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtProgressCallback.h"
#include "QtUtils.h"
#include "SetupWizardDialog.h"
#include "svnrev.h"

#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVD.h"
#include "pcsx2/Counters.h"
#include "pcsx2/DebugTools/Debug.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/GSDumpReplayer.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/ImGui/ImGuiOverlays.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/MTGS.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/VMManager.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/FileSystem.h"
#include "common/HTTPDownloader.h"
#include "common/Path.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtGui/QClipboard>
#include <QtGui/QInputMethod>

#include "fmt/core.h"

#include <cmath>
#include <csignal>

static constexpr u32 SETTINGS_SAVE_DELAY = 1000;
static constexpr const char* RUNTIME_RESOURCES_URL =
	"https://github.com/PCSX2/pcsx2-windows-dependencies/releases/download/runtime-resources/";

EmuThread* g_emu_thread = nullptr;

//////////////////////////////////////////////////////////////////////////
// Local function declarations
//////////////////////////////////////////////////////////////////////////
namespace QtHost
{
	static void InitializeEarlyConsole();
	static void PrintCommandLineVersion();
	static void PrintCommandLineHelp(const std::string_view& progname);
	static std::shared_ptr<VMBootParameters>& AutoBoot(std::shared_ptr<VMBootParameters>& autoboot);
	static bool ParseCommandLineOptions(const QStringList& args, std::shared_ptr<VMBootParameters>& autoboot);
	static bool InitializeConfig();
	static void SaveSettings();
	static void HookSignals();
	static void RegisterTypes();
	static bool RunSetupWizard();
	static std::optional<bool> DownloadFile(QWidget* parent, const QString& title, std::string url, std::vector<u8>* data);
} // namespace QtHost

//////////////////////////////////////////////////////////////////////////
// Local variable declarations
//////////////////////////////////////////////////////////////////////////
static std::unique_ptr<QTimer> s_settings_save_timer;
static std::unique_ptr<INISettingsInterface> s_base_settings_interface;
static bool s_batch_mode = false;
static bool s_nogui_mode = false;
static bool s_start_fullscreen_ui = false;
static bool s_start_fullscreen_ui_fullscreen = false;
static bool s_test_config_and_exit = false;
static bool s_run_setup_wizard = false;
static bool s_cleanup_after_update = false;
static bool s_boot_and_debug = false;

//////////////////////////////////////////////////////////////////////////
// CPU Thread
//////////////////////////////////////////////////////////////////////////

EmuThread::EmuThread(QThread* ui_thread)
	: QThread()
	, m_ui_thread(ui_thread)
{
}

EmuThread::~EmuThread() = default;

void EmuThread::start()
{
	pxAssertRel(!g_emu_thread, "Emu thread does not exist");

	g_emu_thread = new EmuThread(QThread::currentThread());
	g_emu_thread->setStackSize(VMManager::EMU_THREAD_STACK_SIZE);
	g_emu_thread->QThread::start();
	g_emu_thread->m_started_semaphore.acquire();
	g_emu_thread->moveToThread(g_emu_thread);
}

void EmuThread::stop()
{
	pxAssertRel(g_emu_thread, "Emu thread exists");
	pxAssertRel(!g_emu_thread->isOnEmuThread(), "Not called on the emu thread");

	QMetaObject::invokeMethod(g_emu_thread, &EmuThread::stopInThread, Qt::QueuedConnection);
	while (g_emu_thread->isRunning())
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
}

void EmuThread::stopInThread()
{
	if (VMManager::HasValidVM())
		destroyVM();

	if (m_run_fullscreen_ui)
		stopFullscreenUI();

	m_event_loop->quit();
	m_shutdown_flag.store(true);
}

bool EmuThread::confirmMessage(const QString& title, const QString& message)
{
	if (!isOnEmuThread())
	{
		// This is definitely deadlock risky, but unlikely to happen (why would GS be confirming?).
		bool result = false;
		QMetaObject::invokeMethod(g_emu_thread, "confirmMessage", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, result),
			Q_ARG(const QString&, title), Q_ARG(const QString&, message));
		return result;
	}

	// Easy if there's no VM.
	if (!VMManager::HasValidVM())
		return emit messageConfirmed(title, message);

	// Preemptively pause/set surfaceless on the emu thread, because it can't run while the popup is open.
	const bool was_paused = (VMManager::GetState() == VMState::Paused);
	const bool was_fullscreen = isFullscreen();
	if (!was_paused)
		VMManager::SetPaused(true);
	if (was_fullscreen)
		setSurfaceless(true);

	// This won't return until the user confirms one way or another.
	const bool result = emit messageConfirmed(title, message);

	// Resume VM after confirming.
	if (was_fullscreen)
		setSurfaceless(false);
	if (!was_paused)
		VMManager::SetPaused(false);

	return result;
}

void EmuThread::startFullscreenUI(bool fullscreen)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "startFullscreenUI", Qt::QueuedConnection, Q_ARG(bool, fullscreen));
		return;
	}

	if (VMManager::HasValidVM() || MTGS::IsOpen())
		return;

	// this should just set the flag so it gets automatically started
	ImGuiManager::InitializeFullscreenUI();
	m_run_fullscreen_ui = true;
	m_is_rendering_to_main = shouldRenderToMain();
	m_is_fullscreen = fullscreen;

	if (!MTGS::WaitForOpen())
	{
		m_run_fullscreen_ui = false;
		return;
	}

	emit onFullscreenUIStateChange(true);

	// poll more frequently so we don't lose events
	stopBackgroundControllerPollTimer();
	startBackgroundControllerPollTimer();
}

void EmuThread::stopFullscreenUI()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::stopFullscreenUI, Qt::QueuedConnection);

		// wait until the host display is gone
		while (!QtHost::IsVMValid() && MTGS::IsOpen())
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);

		return;
	}

	if (m_run_fullscreen_ui)
	{
		m_run_fullscreen_ui = false;
		emit onFullscreenUIStateChange(false);
	}

	if (MTGS::IsOpen() && !VMManager::HasValidVM())
		MTGS::WaitForClose();
}

void EmuThread::startVM(std::shared_ptr<VMBootParameters> boot_params)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "startVM", Qt::QueuedConnection, Q_ARG(std::shared_ptr<VMBootParameters>, boot_params));
		return;
	}

	pxAssertRel(!VMManager::HasValidVM(), "VM is shut down");

	// Determine whether to start fullscreen or not.
	m_is_rendering_to_main = shouldRenderToMain();
	if (boot_params->fullscreen.has_value())
		m_is_fullscreen = boot_params->fullscreen.value();
	else
		m_is_fullscreen = Host::GetBaseBoolSettingValue("UI", "StartFullscreen", false);

	if (!VMManager::Initialize(*boot_params))
		return;

	if (!Host::GetBoolSettingValue("UI", "StartPaused", false))
	{
		// This will come back and call OnVMResumed().
		VMManager::SetState(VMState::Running);
	}
	else
	{
		// When starting paused, redraw the window, so there's at least something there.
		redrawDisplayWindow();
		Host::OnVMPaused();
	}

	m_event_loop->quit();
}

void EmuThread::resetVM()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::resetVM, Qt::QueuedConnection);
		return;
	}

	VMManager::Reset();
}

void EmuThread::setVMPaused(bool paused)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "setVMPaused", Qt::QueuedConnection, Q_ARG(bool, paused));
		return;
	}

	VMManager::SetPaused(paused);
}

void EmuThread::shutdownVM(bool save_state /* = true */)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "shutdownVM", Qt::QueuedConnection, Q_ARG(bool, save_state));
		return;
	}

	const VMState state = VMManager::GetState();
	if (state == VMState::Paused)
		m_event_loop->quit();
	else if (state != VMState::Running)
		return;

	m_save_state_on_shutdown = save_state;
	VMManager::SetState(VMState::Stopping);
}

void EmuThread::loadState(const QString& filename)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "loadState", Qt::QueuedConnection, Q_ARG(const QString&, filename));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::LoadState(filename.toUtf8().constData());
}

void EmuThread::loadStateFromSlot(qint32 slot)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "loadStateFromSlot", Qt::QueuedConnection, Q_ARG(qint32, slot));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::LoadStateFromSlot(slot);
}

void EmuThread::saveState(const QString& filename)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "saveState", Qt::QueuedConnection, Q_ARG(const QString&, filename));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	if (!VMManager::SaveState(filename.toUtf8().constData()))
	{
		// this one is usually the result of a user-chosen path, so we can display a message box safely here
		Console.Error("Failed to save state");
	}
}

void EmuThread::saveStateToSlot(qint32 slot)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "saveStateToSlot", Qt::QueuedConnection, Q_ARG(qint32, slot));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::SaveStateToSlot(slot);
}

void EmuThread::run()
{
	// Qt-specific initialization.
	m_event_loop = new QEventLoop();
	m_started_semaphore.release();
	connectSignals();

	// Common host initialization (VM setup, etc).
	if (!VMManager::Internal::CPUThreadInitialize())
	{
		VMManager::Internal::CPUThreadShutdown();
		QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
		return;
	}

	// Start background polling because the VM won't do it for us.
	createBackgroundControllerPollTimer();
	startBackgroundControllerPollTimer();

	// Main CPU thread loop.
	while (!m_shutdown_flag.load())
	{
		switch (VMManager::GetState())
		{
			case VMState::Initializing:
				pxFailRel("Shouldn't be in the starting state");
				continue;

			case VMState::Shutdown:
			case VMState::Paused:
				m_event_loop->exec();
				continue;

			case VMState::Running:
				m_event_loop->processEvents(QEventLoop::AllEvents);
				VMManager::Execute();
				continue;

			case VMState::Resetting:
				VMManager::Reset();
				continue;

			case VMState::Stopping:
				destroyVM();
				continue;

			default:
				continue;
		}
	}

	// Teardown in reverse order.
	stopBackgroundControllerPollTimer();
	destroyBackgroundControllerPollTimer();
	VMManager::Internal::CPUThreadShutdown();

	// Move back to the UI thread, since we're no longer running.
	moveToThread(m_ui_thread);
	deleteLater();
}

void EmuThread::destroyVM()
{
	m_last_speed = 0.0f;
	m_last_game_fps = 0.0f;
	m_last_video_fps = 0.0f;
	m_last_internal_width = 0;
	m_last_internal_height = 0;
	m_was_paused_by_focus_loss = false;
	VMManager::Shutdown(m_save_state_on_shutdown);
	m_save_state_on_shutdown = false;
}

void EmuThread::createBackgroundControllerPollTimer()
{
	pxAssert(!m_background_controller_polling_timer);
	m_background_controller_polling_timer = new QTimer(this);
	m_background_controller_polling_timer->setSingleShot(false);
	m_background_controller_polling_timer->setTimerType(Qt::CoarseTimer);
	connect(m_background_controller_polling_timer, &QTimer::timeout, this, &EmuThread::doBackgroundControllerPoll);
}

void EmuThread::destroyBackgroundControllerPollTimer()
{
	delete m_background_controller_polling_timer;
	m_background_controller_polling_timer = nullptr;
}

void EmuThread::startBackgroundControllerPollTimer()
{
	if (m_background_controller_polling_timer->isActive())
		return;

	m_background_controller_polling_timer->start(
		FullscreenUI::IsInitialized() ? FULLSCREEN_UI_CONTROLLER_POLLING_INTERVAL : BACKGROUND_CONTROLLER_POLLING_INTERVAL);
}

void EmuThread::stopBackgroundControllerPollTimer()
{
	if (!m_background_controller_polling_timer->isActive())
		return;

	m_background_controller_polling_timer->stop();
}

void EmuThread::doBackgroundControllerPoll()
{
	VMManager::IdlePollUpdate();
}

void EmuThread::toggleFullscreen()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::toggleFullscreen, Qt::QueuedConnection);
		return;
	}

	setFullscreen(!m_is_fullscreen, true);
}

void EmuThread::setFullscreen(bool fullscreen, bool allow_render_to_main)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "setFullscreen", Qt::QueuedConnection, Q_ARG(bool, fullscreen), Q_ARG(bool, allow_render_to_main));
		return;
	}

	if (!MTGS::IsOpen() || m_is_fullscreen == fullscreen)
		return;

	// This will call back to us on the MTGS thread.
	m_is_fullscreen = fullscreen;
	m_is_rendering_to_main = allow_render_to_main && shouldRenderToMain();
	MTGS::UpdateDisplayWindow();
	MTGS::WaitGS();

	// If we're using exclusive fullscreen, the refresh rate may have changed.
	VMManager::UpdateTargetSpeed();
}

void EmuThread::setSurfaceless(bool surfaceless)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "setSurfaceless", Qt::QueuedConnection, Q_ARG(bool, surfaceless));
		return;
	}

	if (!MTGS::IsOpen() || m_is_surfaceless == surfaceless)
		return;

	// This will call back to us on the MTGS thread.
	m_is_surfaceless = surfaceless;
	MTGS::UpdateDisplayWindow();
	MTGS::WaitGS();
}

void EmuThread::applySettings()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::applySettings, Qt::QueuedConnection);
		return;
	}

	VMManager::ApplySettings();
}

void EmuThread::reloadGameSettings()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadGameSettings, Qt::QueuedConnection);
		return;
	}

	// this will skip applying settings when they're not active
	VMManager::ReloadGameSettings();
}

void EmuThread::updateEmuFolders()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::updateEmuFolders, Qt::QueuedConnection);
		return;
	}

	VMManager::Internal::UpdateEmuFolders();
}

void EmuThread::connectSignals()
{
	connect(qApp, &QGuiApplication::applicationStateChanged, this, &EmuThread::onApplicationStateChanged);
}

void EmuThread::loadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
	m_verbose_status = si.GetBoolValue("UI", "VerboseStatusBar", false);
	m_pause_on_focus_loss = si.GetBoolValue("UI", "PauseOnFocusLoss", false);
}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
	g_emu_thread->loadSettings(si, lock);
}

void EmuThread::checkForSettingChanges(const Pcsx2Config& old_config)
{
	if (g_main_window)
	{
		QMetaObject::invokeMethod(g_main_window, &MainWindow::checkForSettingChanges, Qt::QueuedConnection);
		updatePerformanceMetrics(true);
	}

	if (MTGS::IsOpen())
	{
		const bool render_to_main = shouldRenderToMain();
		if (!m_is_fullscreen && m_is_rendering_to_main != render_to_main)
		{
			m_is_rendering_to_main = render_to_main;
			MTGS::UpdateDisplayWindow();
			MTGS::WaitGS();
		}
	}
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
	g_emu_thread->checkForSettingChanges(old_config);
}

bool EmuThread::shouldRenderToMain() const
{
	return !Host::GetBoolSettingValue("UI", "RenderToSeparateWindow", false) && !QtHost::InNoGUIMode();
}

void EmuThread::toggleSoftwareRendering()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::toggleSoftwareRendering, Qt::QueuedConnection);
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	MTGS::ToggleSoftwareRendering();
}

void EmuThread::changeDisc(CDVD_SourceType source, const QString& path)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "changeDisc", Qt::QueuedConnection, Q_ARG(CDVD_SourceType, source), Q_ARG(const QString&, path));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::ChangeDisc(source, path.toStdString());
}

void EmuThread::setELFOverride(const QString& path)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "setELFOverride", Qt::QueuedConnection, Q_ARG(const QString&, path));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::SetELFOverride(path.toStdString());
}

void EmuThread::changeGSDump(const QString& path)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "changeGSDump", Qt::QueuedConnection, Q_ARG(const QString&, path));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::ChangeGSDump(path.toStdString());
}

void EmuThread::reloadPatches()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadPatches, Qt::QueuedConnection);
		return;
	}

	VMManager::ReloadPatches(true, false, true, true);
}

void EmuThread::reloadInputSources()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadInputSources, Qt::QueuedConnection);
		return;
	}

	std::unique_lock<std::mutex> lock = Host::GetSettingsLock();
	SettingsInterface* si = Host::GetSettingsInterface();
	SettingsInterface* bindings_si = Host::GetSettingsInterfaceForBindings();
	InputManager::ReloadSources(*si, lock);

	// skip loading bindings if we're not running, since it'll get done on startup anyway
	if (VMManager::HasValidVM())
		InputManager::ReloadBindings(*si, *bindings_si);
}

void EmuThread::reloadInputBindings()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadInputBindings, Qt::QueuedConnection);
		return;
	}

	// skip loading bindings if we're not running, since it'll get done on startup anyway
	if (!VMManager::HasValidVM())
		return;

	auto lock = Host::GetSettingsLock();
	SettingsInterface* si = Host::GetSettingsInterface();
	SettingsInterface* bindings_si = Host::GetSettingsInterfaceForBindings();
	InputManager::ReloadBindings(*si, *bindings_si);
}

void EmuThread::reloadInputDevices()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadInputDevices, Qt::QueuedConnection);
		return;
	}

	InputManager::ReloadDevices();
}

void EmuThread::closeInputSources()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadInputDevices, Qt::BlockingQueuedConnection);
		return;
	}

	InputManager::CloseSources();
}

void EmuThread::requestDisplaySize(float scale)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "requestDisplaySize", Qt::QueuedConnection, Q_ARG(float, scale));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::RequestDisplaySize(scale);
}

void EmuThread::enumerateInputDevices()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::enumerateInputDevices, Qt::QueuedConnection);
		return;
	}

	const std::vector<std::pair<std::string, std::string>> devs(InputManager::EnumerateDevices());
	QList<QPair<QString, QString>> qdevs;
	qdevs.reserve(devs.size());
	for (const std::pair<std::string, std::string>& dev : devs)
		qdevs.emplace_back(QString::fromStdString(dev.first), QString::fromStdString(dev.second));

	onInputDevicesEnumerated(qdevs);
}

void EmuThread::enumerateVibrationMotors()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::enumerateVibrationMotors, Qt::QueuedConnection);
		return;
	}

	const std::vector<InputBindingKey> motors(InputManager::EnumerateMotors());
	QList<InputBindingKey> qmotors;
	qmotors.reserve(motors.size());
	for (InputBindingKey key : motors)
		qmotors.push_back(key);

	onVibrationMotorsEnumerated(qmotors);
}

void EmuThread::connectDisplaySignals(DisplayWidget* widget)
{
	widget->disconnect(this);

	connect(widget, &DisplayWidget::windowResizedEvent, this, &EmuThread::onDisplayWindowResized);
	connect(widget, &DisplayWidget::windowRestoredEvent, this, &EmuThread::redrawDisplayWindow);
}

void EmuThread::onDisplayWindowResized(int width, int height, float scale)
{
	if (!MTGS::IsOpen())
		return;

	MTGS::ResizeDisplayWindow(width, height, scale);
}

void EmuThread::onApplicationStateChanged(Qt::ApplicationState state)
{
	// NOTE: This is executed on the emu thread, not UI thread.
	if (!VMManager::HasValidVM())
		return;

	const bool focus_loss = (state != Qt::ApplicationActive);
	if (focus_loss)
	{
		if (m_pause_on_focus_loss && !m_was_paused_by_focus_loss && VMManager::GetState() == VMState::Running)
		{
			m_was_paused_by_focus_loss = true;
			VMManager::SetPaused(true);
		}

		// Clear the state of all keyboard binds.
		// That way, if we had a key held down, and lost focus, the bind won't be stuck enabled because we never
		// got the key release message, because it happened in another window which "stole" the event.
		InputManager::ClearBindStateFromSource(InputManager::MakeHostKeyboardKey(0));
	}
	else
	{
		if (m_was_paused_by_focus_loss)
		{
			m_was_paused_by_focus_loss = false;
			if (VMManager::GetState() == VMState::Paused)
				VMManager::SetPaused(false);
		}
	}
}

void EmuThread::redrawDisplayWindow()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::redrawDisplayWindow, Qt::QueuedConnection);
		return;
	}

	// If we're running, we're going to re-present anyway.
	if (!VMManager::HasValidVM() || VMManager::GetState() == VMState::Running)
		return;

	MTGS::PresentCurrentFrame();
}

void EmuThread::runOnCPUThread(const std::function<void()>& func)
{
	func();
}

void EmuThread::queueSnapshot(quint32 gsdump_frames)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "queueSnapshot", Qt::QueuedConnection, Q_ARG(quint32, gsdump_frames));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	MTGS::RunOnGSThread([gsdump_frames]() { GSQueueSnapshot(std::string(), gsdump_frames); });
}

void EmuThread::beginCapture(const QString& path)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "beginCapture", Qt::QueuedConnection, Q_ARG(const QString&, path));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	MTGS::RunOnGSThread([path = path.toStdString()]() {
		GSBeginCapture(std::move(path));
	});

	// Sync GS thread. We want to start adding audio at the same time as video.
	// TODO: This could be up to 64 frames behind... use the pts to adjust it.
	MTGS::WaitGS(false, false, false);
}

void EmuThread::endCapture()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "endCapture", Qt::QueuedConnection);
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	MTGS::RunOnGSThread(&GSEndCapture);
}

std::optional<WindowInfo> EmuThread::acquireRenderWindow(bool recreate_window)
{
	// Check if we're wanting to get exclusive fullscreen. This should be safe to read, since we're going to be calling from the GS thread.
	m_is_exclusive_fullscreen = m_is_fullscreen && GSWantsExclusiveFullscreen();
	const bool window_fullscreen = m_is_fullscreen && !m_is_exclusive_fullscreen;
	const bool render_to_main = !m_is_exclusive_fullscreen && !window_fullscreen && m_is_rendering_to_main;

	return emit onAcquireRenderWindowRequested(recreate_window, window_fullscreen, render_to_main, m_is_surfaceless);
}

void EmuThread::releaseRenderWindow()
{
	emit onReleaseRenderWindowRequested();
}

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
	return g_emu_thread->acquireRenderWindow(recreate_window);
}

void Host::ReleaseRenderWindow()
{
	return g_emu_thread->releaseRenderWindow();
}

void Host::BeginPresentFrame()
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
	g_emu_thread->onResizeRenderWindowRequested(width, height);
}

void Host::OnVMStarting()
{
	g_emu_thread->stopBackgroundControllerPollTimer();
	emit g_emu_thread->onVMStarting();
}

void Host::OnVMStarted()
{
	emit g_emu_thread->onVMStarted();
}

void Host::OnVMDestroyed()
{
	emit g_emu_thread->onVMStopped();
	g_emu_thread->startBackgroundControllerPollTimer();
}

void Host::OnVMPaused()
{
	g_emu_thread->startBackgroundControllerPollTimer();
	emit g_emu_thread->onVMPaused();
}

void Host::OnVMResumed()
{
	// exit the event loop when we eventually return
	g_emu_thread->getEventLoop()->quit();
	g_emu_thread->stopBackgroundControllerPollTimer();

	// if we were surfaceless (view->game list, system->unpause), get our display widget back
	if (g_emu_thread->isSurfaceless())
		g_emu_thread->setSurfaceless(false);

	emit g_emu_thread->onVMResumed();
}

void Host::OnGameChanged(const std::string& title, const std::string& elf_override, const std::string& disc_path,
	const std::string& disc_serial, u32 disc_crc, u32 current_crc)
{
	emit g_emu_thread->onGameChanged(QString::fromStdString(title), QString::fromStdString(elf_override),
		QString::fromStdString(disc_path), QString::fromStdString(disc_serial), disc_crc, current_crc);
}

void EmuThread::updatePerformanceMetrics(bool force)
{
	if (m_verbose_status && VMManager::HasValidVM())
	{
		std::string gs_stat_str;
		GSgetTitleStats(gs_stat_str);

		QString gs_stat;
		if (THREAD_VU1)
		{
			gs_stat = tr("Slot: %1 | %2 | EE: %3% | VU: %4% | GS: %5%")
						  .arg(SaveStateSelectorUI::GetCurrentSlot())
						  .arg(gs_stat_str.c_str())
						  .arg(PerformanceMetrics::GetCPUThreadUsage(), 0, 'f', 0)
						  .arg(PerformanceMetrics::GetVUThreadUsage(), 0, 'f', 0)
						  .arg(PerformanceMetrics::GetGSThreadUsage(), 0, 'f', 0);
		}
		else
		{
			gs_stat = tr("Slot: %1 | %2 | EE: %3% | GS: %4%")
						  .arg(SaveStateSelectorUI::GetCurrentSlot())
						  .arg(gs_stat_str.c_str())
						  .arg(PerformanceMetrics::GetCPUThreadUsage(), 0, 'f', 0)
						  .arg(PerformanceMetrics::GetGSThreadUsage(), 0, 'f', 0);
		}

		QMetaObject::invokeMethod(g_main_window->getStatusVerboseWidget(), "setText", Qt::QueuedConnection, Q_ARG(const QString&, gs_stat));
	}

	const GSRendererType renderer = GSGetCurrentRenderer(); // Reading from GS thread, therefore racey, but it's just visual.
	const float speed = std::round(PerformanceMetrics::GetSpeed());
	const float gfps = std::round(PerformanceMetrics::GetInternalFPS());
	const float vfps = std::round(PerformanceMetrics::GetFPS());
	int iwidth, iheight;
	GSgetInternalResolution(&iwidth, &iheight);

	if (iwidth != m_last_internal_width || iheight != m_last_internal_height || speed != m_last_speed || gfps != m_last_game_fps ||
		vfps != m_last_video_fps || renderer != m_last_renderer || force)
	{
		if (iwidth == 0 && iheight == 0)
		{
			// if we don't have width/height yet, we're not going to have fps either.
			// and we'll probably be <100% due to compiling. so just leave it blank for now.
			QString blank;
			QMetaObject::invokeMethod(
				g_main_window->getStatusRendererWidget(), "setText", Qt::QueuedConnection, Q_ARG(const QString&, blank));
			QMetaObject::invokeMethod(
				g_main_window->getStatusResolutionWidget(), "setText", Qt::QueuedConnection, Q_ARG(const QString&, blank));
			QMetaObject::invokeMethod(g_main_window->getStatusFPSWidget(), "setText", Qt::QueuedConnection, Q_ARG(const QString&, blank));
			QMetaObject::invokeMethod(g_main_window->getStatusVPSWidget(), "setText", Qt::QueuedConnection, Q_ARG(const QString&, blank));
			return;
		}
		else
		{
			if (renderer != m_last_renderer || force)
			{
				QMetaObject::invokeMethod(g_main_window->getStatusRendererWidget(), "setText", Qt::QueuedConnection,
					Q_ARG(const QString&, QString::fromUtf8(Pcsx2Config::GSOptions::GetRendererName(renderer))));
				m_last_renderer = renderer;
			}
			if (iwidth != m_last_internal_width || iheight != m_last_internal_height || force)
			{
				QMetaObject::invokeMethod(g_main_window->getStatusResolutionWidget(), "setText", Qt::QueuedConnection,
					Q_ARG(const QString&, tr("%1x%2").arg(iwidth).arg(iheight)));
				m_last_internal_width = iwidth;
				m_last_internal_height = iheight;
			}

			if (gfps != m_last_game_fps || force)
			{
				QMetaObject::invokeMethod(g_main_window->getStatusFPSWidget(), "setText", Qt::QueuedConnection,
					Q_ARG(const QString&, tr("Game: %1 FPS").arg(gfps, 0, 'f', 0)));
				m_last_game_fps = gfps;
			}

			if (speed != m_last_speed || vfps != m_last_video_fps || force)
			{
				QMetaObject::invokeMethod(g_main_window->getStatusVPSWidget(), "setText", Qt::QueuedConnection,
					Q_ARG(const QString&, tr("Video: %1 FPS (%2%)").arg(vfps, 0, 'f', 0).arg(speed, 0, 'f', 0)));
				m_last_speed = speed;
				m_last_video_fps = vfps;
			}
		}
	}
}

void Host::OnPerformanceMetricsUpdated()
{
	g_emu_thread->updatePerformanceMetrics(false);
}

void Host::OnSaveStateLoading(const std::string_view& filename)
{
	emit g_emu_thread->onSaveStateLoading(QtUtils::StringViewToQString(filename));
}

void Host::OnSaveStateLoaded(const std::string_view& filename, bool was_successful)
{
	emit g_emu_thread->onSaveStateLoaded(QtUtils::StringViewToQString(filename), was_successful);
}

void Host::OnSaveStateSaved(const std::string_view& filename)
{
	emit g_emu_thread->onSaveStateSaved(QtUtils::StringViewToQString(filename));
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
	emit g_emu_thread->onAchievementsLoginRequested(reason);
}

void Host::OnAchievementsLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
	emit g_emu_thread->onAchievementsLoginSucceeded(QString::fromUtf8(username), points, sc_points, unread_messages);
}

void Host::OnAchievementsRefreshed()
{
	u32 game_id = 0;

	QString game_info;

	if (Achievements::HasActiveGame())
	{
		game_id = Achievements::GetGameID();

		game_info = qApp
						->translate("EmuThread", "Game: %1 (%2)\n")
						.arg(QString::fromStdString(Achievements::GetGameTitle()))
						.arg(game_id);

		const std::string& rich_presence_string = Achievements::GetRichPresenceString();
		if (!rich_presence_string.empty())
			game_info.append(QString::fromStdString(StringUtil::Ellipsise(rich_presence_string, 128)));
		else
			game_info.append(qApp->translate("EmuThread", "Rich presence inactive or unsupported."));
	}
	else
	{
		game_info = qApp->translate("EmuThread", "Game not loaded or no RetroAchievements available.");
	}

	emit g_emu_thread->onAchievementsRefreshed(game_id, game_info);
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
	emit g_emu_thread->onAchievementsHardcoreModeChanged(enabled);
}

void Host::OnCoverDownloaderOpenRequested()
{
	emit g_emu_thread->onCoverDownloaderOpenRequested();
}

void Host::OnCreateMemoryCardOpenRequested()
{
	emit g_emu_thread->onCreateMemoryCardOpenRequested();
}

void Host::VSyncOnCPUThread()
{
	g_emu_thread->getEventLoop()->processEvents(QEventLoop::AllEvents);
}

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
	if (block && g_emu_thread->isOnEmuThread())
	{
		// probably shouldn't ever happen, but just in case..
		function();
		return;
	}

	QMetaObject::invokeMethod(g_emu_thread, "runOnCPUThread", block ? Qt::BlockingQueuedConnection : Qt::QueuedConnection,
		Q_ARG(const std::function<void()>&, std::move(function)));
}

void Host::RefreshGameListAsync(bool invalidate_cache)
{
	QMetaObject::invokeMethod(g_main_window, "refreshGameList", Qt::QueuedConnection, Q_ARG(bool, invalidate_cache));
}

void Host::CancelGameListRefresh()
{
	QMetaObject::invokeMethod(g_main_window, "cancelGameListRefresh", Qt::BlockingQueuedConnection);
}

void Host::RequestExit(bool allow_confirm)
{
	QMetaObject::invokeMethod(g_main_window, "requestExit", Qt::QueuedConnection, Q_ARG(bool, allow_confirm));
}

void Host::RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state)
{
	if (!VMManager::HasValidVM())
		return;

	// This is a bit messy here - we want to shut down immediately (in case it was requested by the game),
	// but we also need to exit-on-shutdown for batch mode. So, if we're running on the CPU thread, destroy
	// the VM, then request the main window to exit.
	if (allow_confirm || !g_emu_thread->isOnEmuThread())
	{
		// Run it on the host thread, that way we get the confirm prompt (if enabled).
		QMetaObject::invokeMethod(g_main_window, "requestShutdown", Qt::QueuedConnection, Q_ARG(bool, allow_confirm),
			Q_ARG(bool, allow_save_state), Q_ARG(bool, default_save_state));
	}
	else
	{
		// Change state to stopping -> return -> shut down VM.
		g_emu_thread->shutdownVM(allow_save_state && default_save_state);

		// This will probably call shutdownVM() again, but by the time it runs, we'll have already shut down
		// and it'll be a noop.
		if (QtHost::InBatchMode())
			QMetaObject::invokeMethod(g_main_window, "requestExit", Qt::QueuedConnection, Q_ARG(bool, false));
	}
}

bool Host::IsFullscreen()
{
	return g_emu_thread->isFullscreen();
}

void Host::SetFullscreen(bool enabled)
{
	g_emu_thread->setFullscreen(enabled, true);
}

void Host::OnCaptureStarted(const std::string& filename)
{
	emit g_emu_thread->onCaptureStarted(QString::fromStdString(filename));
}

void Host::OnCaptureStopped()
{
	emit g_emu_thread->onCaptureStopped();
}

bool QtHost::InitializeConfig()
{
	if (!EmuFolders::InitializeCriticalFolders())
	{
		QMessageBox::critical(nullptr, QStringLiteral("PCSX2"),
			QStringLiteral("One or more critical directories are missing, your installation may be incomplete."));
		return false;
	}

	// Write crash dumps to the data directory, since that'll be accessible for certain.
	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	const std::string path(Path::Combine(EmuFolders::Settings, "PCSX2.ini"));
	s_run_setup_wizard = s_run_setup_wizard || !FileSystem::FileExists(path.c_str());
	Console.WriteLn("Loading config from %s.", path.c_str());

	s_base_settings_interface = std::make_unique<INISettingsInterface>(std::move(path));
	Host::Internal::SetBaseSettingsLayer(s_base_settings_interface.get());
	if (!s_base_settings_interface->Load() || !VMManager::Internal::CheckSettingsVersion())
	{
		// If the config file doesn't exist, assume this is a new install and don't prompt to overwrite.
		if (FileSystem::FileExists(s_base_settings_interface->GetFileName().c_str()) &&
			QMessageBox::question(nullptr, QStringLiteral("PCSX2"),
				QStringLiteral("Settings failed to load, or are the incorrect version. Clicking Yes will reset all settings to defaults. "
							   "Do you want to continue?")) != QMessageBox::Yes)
		{
			return false;
		}

		VMManager::SetDefaultSettings(*s_base_settings_interface, true, true, true, true, true);

		// Don't save if we're running the setup wizard. We want to run it next time if they don't finish it.
		if (!s_run_setup_wizard)
			SaveSettings();
	}

	// Setup wizard was incomplete last time?
	s_run_setup_wizard =
		s_run_setup_wizard || s_base_settings_interface->GetBoolValue("UI", "SetupWizardIncomplete", false);

	// TODO: -nogui console block?

	VMManager::Internal::LoadStartupSettings();
	InstallTranslator(nullptr);
	return true;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
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
	si.SetBoolValue("UI", "PreferEnglishGameList", false);
	si.SetStringValue("UI", "Theme", QtHost::GetDefaultThemeName());
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

bool QtHost::ShouldShowAdvancedSettings()
{
	return Host::GetBaseBoolSettingValue("UI", "ShowAdvancedSettings", false);
}

void QtHost::RunOnUIThread(const std::function<void()>& func, bool block /*= false*/)
{
	// main window always exists, so it's fine to attach it to that.
	QMetaObject::invokeMethod(g_main_window, "runOnUIThread", block ? Qt::BlockingQueuedConnection : Qt::QueuedConnection,
		Q_ARG(const std::function<void()>&, func));
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	{
		auto lock = Host::GetSettingsLock();
		VMManager::SetDefaultSettings(*s_base_settings_interface.get(), folders, core, controllers, hotkeys, ui);
	}
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();
	if (folders)
		g_emu_thread->updateEmuFolders();

	return true;
}

QString QtHost::GetAppNameAndVersion()
{
	return QStringLiteral("PCSX2 " GIT_REV);
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

QIcon QtHost::GetAppIcon()
{
	return QIcon(QStringLiteral(":/icons/AppIcon64.png"));
}

QString QtHost::GetResourcesBasePath()
{
	return QString::fromStdString(EmuFolders::Resources);
}

std::string QtHost::GetRuntimeDownloadedResourceURL(std::string_view name)
{
	return fmt::format("{}/{}", RUNTIME_RESOURCES_URL, HTTPDownloader::URLEncode(name));
}

std::optional<bool> QtHost::DownloadFile(QWidget* parent, const QString& title, std::string url, std::vector<u8>* data)
{
	static constexpr u32 HTTP_POLL_INTERVAL = 10;

	std::unique_ptr<HTTPDownloader> http = HTTPDownloader::Create(Host::GetHTTPUserAgent());
	if (!http)
	{
		QMessageBox::critical(parent, qApp->translate("EmuThread", "Error"), qApp->translate("EmuThread", "Failed to create HTTPDownloader."));
		return false;
	}

	std::optional<bool> download_result;
	const std::string::size_type url_file_part_pos = url.rfind('/');
	QtModalProgressCallback progress(parent);
	progress.GetDialog().setLabelText(
		qApp->translate("EmuThread", "Downloading %1...").arg(QtUtils::StringViewToQString(
			std::string_view(url).substr((url_file_part_pos != std::string::npos) ? (url_file_part_pos + 1) : 0))));
	progress.GetDialog().setWindowTitle(title);
	progress.GetDialog().setWindowIcon(GetAppIcon());
	progress.SetCancellable(true);

	http->CreateRequest(
		std::move(url), [parent, data, &download_result](s32 status_code, const std::string&, std::vector<u8> hdata) {
			if (status_code == HTTPDownloader::HTTP_STATUS_CANCELLED)
				return;

			if (status_code != HTTPDownloader::HTTP_STATUS_OK)
			{
				QMessageBox::critical(parent, qApp->translate("EmuThread", "Error"),
					qApp->translate("EmuThread", "Download failed with HTTP status code %1.").arg(status_code));
				download_result = false;
				return;
			}

			if (hdata.empty())
			{
				QMessageBox::critical(parent, qApp->translate("EmuThread", "Error"),
					qApp->translate("EmuThread", "Download failed: Data is empty.").arg(status_code));

			download_result = false;
			return;
			}

			*data = std::move(hdata);
			download_result = true;
		},
		&progress);

	// Block until completion.
	while (http->HasAnyRequests())
	{
		QApplication::processEvents(QEventLoop::AllEvents, HTTP_POLL_INTERVAL);
		http->PollRequests();
	}

	return download_result;
}

bool QtHost::DownloadFile(QWidget* parent, const QString& title, std::string url, const std::string& path)
{
	std::vector<u8> data;
	if (!DownloadFile(parent, title, std::move(url), &data).value_or(false) || data.empty())
		return false;

	// Directory may not exist. Create it.
	const std::string directory(Path::GetDirectory(path));
	if ((!directory.empty() && !FileSystem::DirectoryExists(directory.c_str()) &&
			!FileSystem::CreateDirectoryPath(directory.c_str(), true)) ||
		!FileSystem::WriteBinaryFile(path.c_str(), data.data(), data.size()))
	{
		QMessageBox::critical(parent, qApp->translate("EmuThread", "Error"),
			qApp->translate("EmuThread", "Failed to write '%1'.").arg(QString::fromStdString(path)));
		return false;
	}

	return true;
}

void Host::ReportErrorAsync(const std::string_view& title, const std::string_view& message)
{
	if (!title.empty() && !message.empty())
	{
		Console.Error(
			"ReportErrorAsync: %.*s: %.*s", static_cast<int>(title.size()), title.data(), static_cast<int>(message.size()), message.data());
	}
	else if (!message.empty())
	{
		Console.Error("ReportErrorAsync: %.*s", static_cast<int>(message.size()), message.data());
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
	QtHost::RunOnUIThread([url = QtUtils::StringViewToQString(url)]() { QtUtils::OpenURL(g_main_window, QUrl(url)); });
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

void Host::BeginTextInput()
{
	QInputMethod* method = qApp->inputMethod();
	if (method)
		QMetaObject::invokeMethod(method, "show", Qt::QueuedConnection);
}

void Host::EndTextInput()
{
	QInputMethod* method = qApp->inputMethod();
	if (method)
		QMetaObject::invokeMethod(method, "hide", Qt::QueuedConnection);
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
	std::optional<WindowInfo> ret;
	QMetaObject::invokeMethod(g_main_window, &MainWindow::getWindowInfo, Qt::BlockingQueuedConnection, &ret);
	return ret;
}

void Host::OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name)
{
	emit g_emu_thread->onInputDeviceConnected(identifier.empty() ? QString() : QString::fromUtf8(identifier.data(), identifier.size()),
		device_name.empty() ? QString() : QString::fromUtf8(device_name.data(), device_name.size()));
}

void Host::OnInputDeviceDisconnected(const std::string_view& identifier)
{
	emit g_emu_thread->onInputDeviceDisconnected(identifier.empty() ? QString() : QString::fromUtf8(identifier.data(), identifier.size()));
}

void Host::SetMouseMode(bool relative_mode, bool hide_cursor)
{
	emit g_emu_thread->onMouseModeRequested(relative_mode, hide_cursor);
}

//////////////////////////////////////////////////////////////////////////
// Hotkeys
//////////////////////////////////////////////////////////////////////////

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()


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
		QMetaObject::invokeMethod(g_main_window, "requestExit", Qt::QueuedConnection, Q_ARG(bool, false));
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

#ifdef _WIN32

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	if (dwCtrlType != CTRL_C_EVENT)
		return FALSE;

	SignalHandler(SIGTERM);
	return TRUE;
}

#endif

void QtHost::HookSignals()
{
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);

#if defined(_WIN32)
	SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
#elif defined(__linux__)
	// Ignore SIGCHLD by default on Linux, since we kick off aplay asynchronously.
	struct sigaction sa_chld = {};
	sigemptyset(&sa_chld.sa_mask);
	sa_chld.sa_flags = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa_chld, nullptr);
#endif
}

void QtHost::InitializeEarlyConsole()
{
	Log::SetConsoleOutputLevel(LOGLEVEL_DEBUG);
}

void QtHost::PrintCommandLineVersion()
{
	InitializeEarlyConsole();
	std::fprintf(stderr, "%s\n", (GetAppNameAndVersion() + GetAppConfigSuffix()).toUtf8().constData());
	std::fprintf(stderr, "https://pcsx2.net/\n");
	std::fprintf(stderr, "\n");
}

void QtHost::PrintCommandLineHelp(const std::string_view& progname)
{
	PrintCommandLineVersion();
	fmt::print(stderr, "Usage: {} [parameters] [--] [boot filename]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "  -version: Displays version information and exits.\n");
	std::fprintf(stderr, "  -batch: Enables batch mode (exits after shutting down).\n");
	std::fprintf(stderr, "  -nogui: Hides main window while running (implies batch mode).\n");
	std::fprintf(stderr, "  -elf <file>: Overrides the boot ELF with the specified filename.\n");
	std::fprintf(stderr, "  -gameargs <string>: passes the specified quoted space-delimited string of launch arguments.\n");
	std::fprintf(stderr, "  -disc <path>: Uses the specified host DVD drive as a source.\n");
	std::fprintf(stderr, "  -logfile <path>: Writes the application log to path instead of emulog.txt.\n");
	std::fprintf(stderr, "  -bios: Starts the BIOS (System Menu/OSDSYS).\n");
	std::fprintf(stderr, "  -fastboot: Force fast boot for provided filename.\n");
	std::fprintf(stderr, "  -slowboot: Force slow boot for provided filename.\n");
	std::fprintf(stderr, "  -state <index>: Loads specified save state by index.\n");
	std::fprintf(stderr, "  -statefile <filename>: Loads state from the specified filename.\n");
	std::fprintf(stderr, "  -fullscreen: Enters fullscreen mode immediately after starting.\n");
	std::fprintf(stderr, "  -nofullscreen: Prevents fullscreen mode from triggering if enabled.\n");
	std::fprintf(stderr, "  -bigpicture: Forces PCSX2 to use the Big Picture mode (useful for controller-only and couch play).\n");
	std::fprintf(stderr, "  -earlyconsolelog: Forces logging of early console messages to console.\n");
	std::fprintf(stderr, "  -testconfig: Initializes configuration and checks version, then exits.\n");
	std::fprintf(stderr, "  -setupwizard: Forces initial setup wizard to run.\n");
	std::fprintf(stderr, "  -debugger: Open debugger and break on entry point.\n");
#ifdef ENABLE_RAINTEGRATION
	std::fprintf(stderr, "  -raintegration: Use RAIntegration instead of built-in achievement support.\n");
#endif
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

bool QtHost::ParseCommandLineOptions(const QStringList& args, std::shared_ptr<VMBootParameters>& autoboot)
{
	bool no_more_args = false;

	if (args.empty())
	{
		// Nothing to do here.
		return true;
	}

	for (auto it = std::next(args.begin()); it != args.end(); ++it)
	{
		if (!no_more_args)
		{
#define CHECK_ARG(str) (*it == str)
#define CHECK_ARG_PARAM(str) (*it == str && std::next(it) != args.end())

			if (CHECK_ARG(QStringLiteral("-help")))
			{
				PrintCommandLineHelp(args.front().toStdString());
				return false;
			}
			else if (CHECK_ARG(QStringLiteral("-version")))
			{
				PrintCommandLineVersion();
				return false;
			}
			else if (CHECK_ARG(QStringLiteral("-batch")))
			{
				s_batch_mode = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-nogui")))
			{
				s_batch_mode = true;
				s_nogui_mode = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-fastboot")))
			{
				AutoBoot(autoboot)->fast_boot = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-slowboot")))
			{
				AutoBoot(autoboot)->fast_boot = false;
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-state")))
			{
				AutoBoot(autoboot)->state_index = (++it)->toInt();
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-statefile")))
			{
				AutoBoot(autoboot)->save_state = (++it)->toStdString();
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-elf")))
			{
				AutoBoot(autoboot)->elf_override = (++it)->toStdString();
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-gameargs")))
			{
				EmuConfig.CurrentGameArgs = (++it)->toStdString();
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-disc")))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::Disc;
				AutoBoot(autoboot)->filename = (++it)->toStdString();
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-logfile")))
			{
				VMManager::Internal::SetFileLogPath((++it)->toStdString());
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-bios")))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::NoDisc;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-fullscreen")))
			{
				AutoBoot(autoboot)->fullscreen = true;
				s_start_fullscreen_ui_fullscreen = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-nofullscreen")))
			{
				AutoBoot(autoboot)->fullscreen = false;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-earlyconsolelog")))
			{
				InitializeEarlyConsole();
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-bigpicture")))
			{
				s_start_fullscreen_ui = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-testconfig")))
			{
				s_test_config_and_exit = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-setupwizard")))
			{
				s_run_setup_wizard = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-debugger")))
			{
				s_boot_and_debug = true;
				continue;
			}
			else if (CHECK_ARG(QStringLiteral("-updatecleanup")))
			{
				s_cleanup_after_update = AutoUpdaterDialog::isSupported();
				continue;
			}
#ifdef ENABLE_RAINTEGRATION
			else if (CHECK_ARG(QStringLiteral("-raintegration")))
			{
				Achievements::SwitchToRAIntegration();
				continue;
			}
#endif
			else if (CHECK_ARG(QStringLiteral("--")))
			{
				no_more_args = true;
				continue;
			}
			else if ((*it)[0] == '-')
			{
				QMessageBox::critical(nullptr, QStringLiteral("Error"), QStringLiteral("Unknown parameter: '%1'").arg(*it));
				return false;
			}

#undef CHECK_ARG
#undef CHECK_ARG_PARAM
		}

		if (!AutoBoot(autoboot)->filename.empty())
			AutoBoot(autoboot)->filename += ' ';

		AutoBoot(autoboot)->filename += it->toStdString();
	}

	// check autoboot parameters, if we set something like fullscreen without a bios
	// or disc, we don't want to actually start.
	if (autoboot && !autoboot->source_type.has_value() && autoboot->filename.empty() && autoboot->elf_override.empty())
	{
		Console.Warning("Skipping autoboot due to no boot parameters.");
		autoboot.reset();
	}

	// if we don't have autoboot, we definitely don't want batch mode (because that'll skip
	// scanning the game list).
	if (s_batch_mode && !s_start_fullscreen_ui && !autoboot)
	{
		QMessageBox::critical(nullptr, QStringLiteral("Error"),
			s_nogui_mode ? QStringLiteral("Cannot use no-gui mode, because no boot filename was specified.") :
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

void QtHost::RegisterTypes()
{
	qRegisterMetaType<std::optional<bool>>();
	qRegisterMetaType<std::optional<WindowInfo>>("std::optional<WindowInfo>()");
	// Bit of fun with metatype names
	// On Windows, the real type name here is "std::function<void __cdecl(void)>"
	// Normally, the fact that we `Q_DECLARE_METATYPE(std::function<void()>);` in QtHost.h would make it also register under "std::function<void()>"
	// The metatype is a pointer to `QMetaTypeInterfaceWrapper<std::function<void()>>::metaType`, which contains a pointer to the function that would register the alternate name
	// But to anyone who can't see QtHost.h, that pointer should be null, opening us up to ODR violations
	// Turns out some of our automoc files also instantiate that metaType (with the null pointer), so if we try to rely on it, everything will break if we get unlucky with link order
	// Instead, manually register under the desired name:
	qRegisterMetaType<std::function<void()>>("std::function<void()>");
	qRegisterMetaType<std::shared_ptr<VMBootParameters>>();
	qRegisterMetaType<GSRendererType>();
	qRegisterMetaType<InputBindingKey>();
	qRegisterMetaType<CDVD_SourceType>();
	qRegisterMetaType<const GameList::Entry*>();
	qRegisterMetaType<Achievements::LoginRequestReason>();
}

bool QtHost::RunSetupWizard()
{
	// Set a flag in the config so that even though we created the ini, we'll run the wizard next time.
	Host::SetBaseBoolSettingValue("UI", "SetupWizardIncomplete", true);
	Host::CommitBaseSettingChanges();

	SetupWizardDialog dialog;
	if (dialog.exec() == QDialog::Rejected)
		return false;

	// Remove the flag.
	Host::SetBaseBoolSettingValue("UI", "SetupWizardIncomplete", false);
	Host::CommitBaseSettingChanges();
	return true;
}

int main(int argc, char* argv[])
{
	CrashHandler::Install();

	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	QtHost::RegisterTypes();

	QApplication app(argc, argv);

#ifndef _WIN32
	if (!PerformEarlyHardwareChecks())
		return EXIT_FAILURE;
#endif

	std::shared_ptr<VMBootParameters> autoboot;
	if (!QtHost::ParseCommandLineOptions(app.arguments(), autoboot))
		return EXIT_FAILURE;

	// Bail out if we can't find any config.
	if (!QtHost::InitializeConfig())
		return EXIT_FAILURE;

	// Are we just setting up the configuration?
	if (s_test_config_and_exit)
		return EXIT_SUCCESS;

	// Remove any previous-version remanants.
	if (s_cleanup_after_update)
		AutoUpdaterDialog::cleanupAfterUpdate();

	// Set theme before creating any windows.
	QtHost::UpdateApplicationTheme();

	// Start logging early.
	LogWindow::updateSettings();

	// Start up the CPU thread.
	QtHost::HookSignals();
	EmuThread::start();

	// Optionally run setup wizard.
	int result;
	if (s_run_setup_wizard && !QtHost::RunSetupWizard())
	{
		result = EXIT_FAILURE;
		goto shutdown_and_exit;
	}

	// Create all window objects, the emuthread might still be starting up at this point.
	g_main_window = new MainWindow();
	g_main_window->initialize();

	// When running in batch mode, ensure game list is loaded, but don't scan for any new files.
	if (!s_batch_mode)
		g_main_window->refreshGameList(false);
	else
		GameList::Refresh(false, true);

	// Don't bother showing the window in no-gui mode.
	if (!s_nogui_mode)
	{
		g_main_window->show();
		g_main_window->raise();
		g_main_window->activateWindow();
	}

	// Initialize big picture mode if requested.
	if (s_start_fullscreen_ui)
		g_emu_thread->startFullscreenUI(s_start_fullscreen_ui_fullscreen);

	if (s_boot_and_debug)
	{
		DebugInterface::setPauseOnEntry(true);
		g_main_window->openDebugger();
	}

	// Skip the update check if we're booting a game directly.
	if (autoboot)
		g_emu_thread->startVM(std::move(autoboot));
	else if (!s_nogui_mode)
		g_main_window->startupUpdateCheck();

	// This doesn't return until we exit.
	result = app.exec();

shutdown_and_exit:
	// Shutting down.
	EmuThread::stop();
	if (g_main_window)
	{
		g_main_window->close();
		delete g_main_window;
	}

	// Ensure config is written. Prevents destruction order issues.
	if (s_base_settings_interface->IsDirty())
		s_base_settings_interface->Save();

	return result;
}
