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

#include <cmath>
#include <csignal>

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/CrashHandler.h"
#include "common/Exceptions.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "pcsx2/CDVD/CDVD.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/Counters.h"
#include "pcsx2/DebugTools/Debug.h"
#include "pcsx2/Frontend/CommonHost.h"
#include "pcsx2/Frontend/FullscreenUI.h"
#include "pcsx2/Frontend/GameList.h"
#include "pcsx2/Frontend/InputManager.h"
#include "pcsx2/Frontend/ImGuiManager.h"
#include "pcsx2/Frontend/LogSink.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/GSDumpReplayer.h"
#include "pcsx2/HostDisplay.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/PAD/Host/PAD.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/VMManager.h"

#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtGui/QClipboard>
#include <QtGui/QInputMethod>

#include "fmt/core.h"

#include "DisplayWidget.h"
#include "GameList/GameListWidget.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "svnrev.h"

#ifdef ENABLE_ACHIEVEMENTS
#include "Frontend/Achievements.h"
#endif

static constexpr u32 SETTINGS_SAVE_DELAY = 1000;

EmuThread* g_emu_thread = nullptr;

//////////////////////////////////////////////////////////////////////////
// Local function declarations
//////////////////////////////////////////////////////////////////////////
namespace QtHost
{
	static void PrintCommandLineVersion();
	static void PrintCommandLineHelp(const std::string_view& progname);
	static std::shared_ptr<VMBootParameters>& AutoBoot(std::shared_ptr<VMBootParameters>& autoboot);
	static bool ParseCommandLineOptions(const QStringList& args, std::shared_ptr<VMBootParameters>& autoboot);
	static bool InitializeConfig();
	static void SaveSettings();
	static void HookSignals();
} // namespace QtHost

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
static bool s_test_config_and_exit = false;

//////////////////////////////////////////////////////////////////////////
// CPU Thread
//////////////////////////////////////////////////////////////////////////

EmuThread::EmuThread(QThread* ui_thread)
	: QThread()
	, m_ui_thread(ui_thread)
{
}

EmuThread::~EmuThread() = default;

bool EmuThread::isOnEmuThread() const
{
	return QThread::currentThread() == this;
}

void EmuThread::start()
{
	pxAssertRel(!g_emu_thread, "Emu thread does not exist");

	g_emu_thread = new EmuThread(QThread::currentThread());
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

	if (VMManager::HasValidVM())
		return;

	m_run_fullscreen_ui = true;
	if (fullscreen)
		m_is_fullscreen = true;

	if (!GetMTGS().WaitForOpen())
	{
		m_run_fullscreen_ui = false;
		return;
	}

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
		while (g_host_display)
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);

		return;
	}

	if (!g_host_display)
		return;

	pxAssertRel(!VMManager::HasValidVM(), "VM is not valid at FSUI shutdown time");
	m_run_fullscreen_ui = false;
	GetMTGS().WaitForClose();
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
	CommonHost::CPUThreadInitialize();

	// Start background polling because the VM won't do it for us.
	createBackgroundControllerPollTimer();
	startBackgroundControllerPollTimer();

	// Main CPU thread loop.
	while (!m_shutdown_flag.load())
	{
		if (!VMManager::HasValidVM())
		{
			m_event_loop->exec();
			continue;
		}

		executeVM();
	}

	// Teardown in reverse order.
	stopBackgroundControllerPollTimer();
	destroyBackgroundControllerPollTimer();
	CommonHost::CPUThreadShutdown();

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

void EmuThread::executeVM()
{
	for (;;)
	{
		switch (VMManager::GetState())
		{
			case VMState::Initializing:
				pxFailRel("Shouldn't be in the starting state state");
				continue;

			case VMState::Paused:
				m_event_loop->exec();
				continue;

			case VMState::Running:
				m_event_loop->processEvents(QEventLoop::AllEvents);
				VMManager::Execute();
				continue;

			case VMState::Stopping:
				destroyVM();
				m_event_loop->processEvents(QEventLoop::AllEvents);
				return;

			default:
				continue;
		}
	}
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
	InputManager::PollSources();
}

void EmuThread::toggleFullscreen()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::toggleFullscreen, Qt::QueuedConnection);
		return;
	}

	setFullscreen(!m_is_fullscreen);
}

void EmuThread::setFullscreen(bool fullscreen)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "setFullscreen", Qt::QueuedConnection, Q_ARG(bool, fullscreen));
		return;
	}

	if (!GetMTGS().IsOpen() || m_is_fullscreen == fullscreen)
		return;

	// This will call back to us on the MTGS thread.
	m_is_fullscreen = fullscreen;
	GetMTGS().UpdateDisplayWindow();
	GetMTGS().WaitGS();

	// If we're using exclusive fullscreen, the refresh rate may have changed.
	UpdateVSyncRate();
}

void EmuThread::setSurfaceless(bool surfaceless)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "setSurfaceless", Qt::QueuedConnection, Q_ARG(bool, surfaceless));
		return;
	}

	if (!GetMTGS().IsOpen() || m_is_surfaceless == surfaceless)
		return;

	// If we went surfaceless and were running the fullscreen UI, stop MTGS running idle.
	// Otherwise, we'll keep trying to present to nothing.
	GetMTGS().SetRunIdle(!surfaceless && m_run_fullscreen_ui);

	// This will call back to us on the MTGS thread.
	m_is_surfaceless = surfaceless;
	GetMTGS().UpdateDisplayWindow();
	GetMTGS().WaitGS();
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

	Host::Internal::UpdateEmuFolders();
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
	CommonHost::LoadSettings(si, lock);
	g_emu_thread->loadSettings(si, lock);
}

void EmuThread::checkForSettingChanges(const Pcsx2Config& old_config)
{
	QMetaObject::invokeMethod(g_main_window, &MainWindow::checkForSettingChanges, Qt::QueuedConnection);

	if (g_host_display)
	{
		const bool render_to_main = shouldRenderToMain();
		if (!m_is_fullscreen && m_is_rendering_to_main != render_to_main)
		{
			m_is_rendering_to_main = render_to_main;
			GetMTGS().UpdateDisplayWindow();
			GetMTGS().WaitGS();
		}
	}

	updatePerformanceMetrics(true);
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
	CommonHost::CheckForSettingsChanges(old_config);
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

	GetMTGS().ToggleSoftwareRendering();
}

void EmuThread::switchRenderer(GSRendererType renderer)
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, "switchRenderer", Qt::QueuedConnection, Q_ARG(GSRendererType, renderer));
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	GetMTGS().SwitchRenderer(renderer);
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

void EmuThread::reloadPatches()
{
	if (!isOnEmuThread())
	{
		QMetaObject::invokeMethod(this, &EmuThread::reloadPatches, Qt::QueuedConnection);
		return;
	}

	if (!VMManager::HasValidVM())
		return;

	VMManager::ReloadPatches(true, true);
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
	if (!g_host_display)
		return;

	GetMTGS().ResizeDisplayWindow(width, height, scale);
}

void EmuThread::onApplicationStateChanged(Qt::ApplicationState state)
{
	// NOTE: This is executed on the emu thread, not UI thread.
	if (!m_pause_on_focus_loss || !VMManager::HasValidVM())
		return;

	const bool focus_loss = (state != Qt::ApplicationActive);
	if (focus_loss)
	{
		if (!m_was_paused_by_focus_loss && VMManager::GetState() == VMState::Running)
		{
			m_was_paused_by_focus_loss = true;
			VMManager::SetPaused(true);
		}
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

	GetMTGS().PresentCurrentFrame();
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

	GetMTGS().RunOnGSThread([gsdump_frames]() { GSQueueSnapshot(std::string(), gsdump_frames); });
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

	GetMTGS().RunOnGSThread([path = path.toStdString()]() {
		GSBeginCapture(std::move(path));
	});
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

	GetMTGS().RunOnGSThread(&GSEndCapture);
}

void EmuThread::updateDisplay()
{
	pxAssertRel(!isOnEmuThread(), "Not on emu thread");

	// finished with the display for now
	g_host_display->DoneCurrent();

	// but we should get it back after this call
	onUpdateDisplayRequested(m_is_fullscreen, !m_is_fullscreen && m_is_rendering_to_main, m_is_surfaceless);
	if (!g_host_display->MakeCurrent())
	{
		pxFailRel("Failed to recreate context after updating");
		return;
	}
}

bool EmuThread::acquireHostDisplay(RenderAPI api, bool clear_state_on_fail)
{
	pxAssertRel(!g_host_display, "Host display does not exist on create");
	m_is_rendering_to_main = shouldRenderToMain();
	m_is_surfaceless = false;

	g_host_display = HostDisplay::CreateForAPI(api);
	if (!g_host_display)
		return false;

	DisplayWidget* widget = emit onCreateDisplayRequested(m_is_fullscreen, m_is_rendering_to_main);
	if (!widget)
	{
		g_host_display.reset();
		return false;
	}

	connectDisplaySignals(widget);

	if (!g_host_display->MakeCurrent())
	{
		Console.Error("Failed to make render context current");
		releaseHostDisplay(clear_state_on_fail);
		return false;
	}

	if (!g_host_display->SetupDevice() || !ImGuiManager::Initialize())
	{
		Console.Error("Failed to initialize device/imgui");
		releaseHostDisplay(clear_state_on_fail);
		return false;
	}

	Console.WriteLn(Color_StrongGreen, "%s Graphics Driver Info:", HostDisplay::RenderAPIToString(g_host_display->GetRenderAPI()));
	Console.Indent().WriteLn(g_host_display->GetDriverInfo());

	if (m_run_fullscreen_ui && !ImGuiManager::InitializeFullscreenUI())
	{
		Console.Error("Failed to initialize fullscreen UI");
		releaseHostDisplay(clear_state_on_fail);
		m_run_fullscreen_ui = false;
		return false;
	}

	return true;
}

void EmuThread::releaseHostDisplay(bool clear_state)
{
	ImGuiManager::Shutdown(clear_state);

	g_host_display.reset();
	emit onDestroyDisplayRequested();
}

bool Host::AcquireHostDisplay(RenderAPI api, bool clear_state_on_fail)
{
	return g_emu_thread->acquireHostDisplay(api, clear_state_on_fail);
}

void Host::ReleaseHostDisplay(bool clear_state)
{
	g_emu_thread->releaseHostDisplay(clear_state);
}

bool Host::BeginPresentFrame(bool frame_skip)
{
	if (!g_host_display->BeginPresent(frame_skip))
	{
		// if we're skipping a frame, we need to reset imgui's state, since
		// we won't be calling EndPresentFrame().
		ImGuiManager::NewFrame();
		return false;
	}

	return true;
}

void Host::EndPresentFrame()
{
	if (GSDumpReplayer::IsReplayingDump())
		GSDumpReplayer::RenderUI();

	FullscreenUI::Render();
	ImGuiManager::RenderOSD();
	g_host_display->EndPresent();
	ImGuiManager::NewFrame();
}

void Host::ResizeHostDisplay(u32 new_window_width, u32 new_window_height, float new_window_scale)
{
	g_host_display->ResizeWindow(new_window_width, new_window_height, new_window_scale);
	ImGuiManager::WindowResized();
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
	g_emu_thread->onResizeDisplayRequested(width, height);
}

void Host::UpdateHostDisplay()
{
	g_emu_thread->updateDisplay();
	ImGuiManager::WindowResized();
}

void Host::OnVMStarting()
{
	CommonHost::OnVMStarting();
	g_emu_thread->stopBackgroundControllerPollTimer();
	emit g_emu_thread->onVMStarting();
}

void Host::OnVMStarted()
{
	CommonHost::OnVMStarted();
	emit g_emu_thread->onVMStarted();
}

void Host::OnVMDestroyed()
{
	CommonHost::OnVMDestroyed();
	emit g_emu_thread->onVMStopped();
	g_emu_thread->startBackgroundControllerPollTimer();
}

void Host::OnVMPaused()
{
	CommonHost::OnVMPaused();
	g_emu_thread->startBackgroundControllerPollTimer();
	emit g_emu_thread->onVMPaused();
}

void Host::OnVMResumed()
{
	CommonHost::OnVMResumed();

	// exit the event loop when we eventually return
	g_emu_thread->getEventLoop()->quit();
	g_emu_thread->stopBackgroundControllerPollTimer();

	// if we were surfaceless (view->game list, system->unpause), get our display widget back
	if (g_emu_thread->isSurfaceless())
		g_emu_thread->setSurfaceless(false);

	emit g_emu_thread->onVMResumed();
}

void Host::OnGameChanged(const std::string& disc_path, const std::string& elf_override, const std::string& game_serial,
	const std::string& game_name, u32 game_crc)
{
	CommonHost::OnGameChanged(disc_path, elf_override, game_serial, game_name, game_crc);
	emit g_emu_thread->onGameChanged(QString::fromStdString(disc_path), QString::fromStdString(elf_override),
		QString::fromStdString(game_serial), QString::fromStdString(game_name), game_crc);
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
			gs_stat = QStringLiteral("%1 | EE: %2% | VU: %3% | GS: %4%")
						  .arg(gs_stat_str.c_str())
						  .arg(PerformanceMetrics::GetCPUThreadUsage(), 0, 'f', 0)
						  .arg(PerformanceMetrics::GetVUThreadUsage(), 0, 'f', 0)
						  .arg(PerformanceMetrics::GetGSThreadUsage(), 0, 'f', 0);
		}
		else
		{
			gs_stat = QStringLiteral("%1 | EE: %2% | GS: %3%")
						  .arg(gs_stat_str.c_str())
						  .arg(PerformanceMetrics::GetCPUThreadUsage(), 0, 'f', 0)
						  .arg(PerformanceMetrics::GetGSThreadUsage(), 0, 'f', 0);
		}

		QMetaObject::invokeMethod(g_main_window->getStatusVerboseWidget(), "setText", Qt::QueuedConnection, Q_ARG(const QString&, gs_stat));
	}

	const GSRendererType renderer = GSConfig.Renderer;
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

#ifdef ENABLE_ACHIEVEMENTS
void Host::OnAchievementsRefreshed()
{
	u32 game_id = 0;
	u32 achievement_count = 0;
	u32 max_points = 0;

	QString game_info;

	if (Achievements::HasActiveGame())
	{
		game_id = Achievements::GetGameID();
		achievement_count = Achievements::GetAchievementCount();
		max_points = Achievements::GetMaximumPointsForGame();

		game_info = qApp->translate("EmuThread", "Game ID: %1\n"
												 "Game Title: %2\n"
												 "Achievements: %5 (%6)\n\n")
						.arg(game_id)
						.arg(QString::fromStdString(Achievements::GetGameTitle()))
						.arg(achievement_count)
						.arg(qApp->translate("EmuThread", "%n points", "", max_points));

		const std::string rich_presence_string(Achievements::GetRichPresenceString());
		if (!rich_presence_string.empty())
			game_info.append(QString::fromStdString(rich_presence_string));
		else
			game_info.append(qApp->translate("EmuThread", "Rich presence inactive or unsupported."));
	}
	else
	{
		game_info = qApp->translate("EmuThread", "Game not loaded or no RetroAchievements available.");
	}

	emit g_emu_thread->onAchievementsRefreshed(game_id, game_info, achievement_count, max_points);
}
#endif

void Host::CPUThreadVSync()
{
	g_emu_thread->getEventLoop()->processEvents(QEventLoop::AllEvents);
	CommonHost::CPUThreadVSync();
}

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
	if (g_emu_thread->isOnEmuThread())
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

void Host::RequestExit(bool save_state_if_running)
{
	if (VMManager::HasValidVM())
		g_emu_thread->shutdownVM(save_state_if_running);

	QMetaObject::invokeMethod(g_main_window, "requestExit", Qt::QueuedConnection);
}

void Host::RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state)
{
	if (!VMManager::HasValidVM())
		return;

	// Run it on the host thread, that way we get the confirm prompt (if enabled).
	QMetaObject::invokeMethod(g_main_window, "requestShutdown", Qt::QueuedConnection, Q_ARG(bool, allow_confirm),
		Q_ARG(bool, allow_save_state), Q_ARG(bool, default_save_state), Q_ARG(bool, false));
}

bool Host::IsFullscreen()
{
	return g_emu_thread->isFullscreen();
}

void Host::SetFullscreen(bool enabled)
{
	g_emu_thread->setFullscreen(enabled);
}

alignas(16) static SysMtgsThread s_mtgs_thread;

SysMtgsThread& GetMTGS()
{
	return s_mtgs_thread;
}

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
				QStringLiteral("Settings failed to load, or are the incorrect version. Clicking Yes will reset all settings to defaults. "
							   "Do you want to continue?")) != QMessageBox::Yes)
		{
			return false;
		}

		CommonHost::SetDefaultSettings(*s_base_settings_interface, true, true, true, true, true);
		SaveSettings();
	}

	CommonHost::SetBlockSystemConsole(QtHost::InNoGUIMode());
	CommonHost::LoadStartupSettings();
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
		ret = QStringLiteral(
			"PCSX2 " APPNAME_STRINGIZE(PCSX2_VersionHi) "." APPNAME_STRINGIZE(PCSX2_VersionMid) "." APPNAME_STRINGIZE(PCSX2_VersionLo));
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

std::optional<std::time_t> Host::GetResourceFileTimestamp(const char* filename)
{
	const std::string path(Path::Combine(EmuFolders::Resources, filename));
	FILESYSTEM_STAT_DATA sd;
	if (!FileSystem::StatFile(filename, &sd))
		return std::nullopt;

	return sd.ModificationTime;
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

void Host::SetRelativeMouseMode(bool enabled)
{
	emit g_emu_thread->onRelativeMouseModeRequested(enabled);
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

#ifdef __linux__
	// Ignore SIGCHLD by default on Linux, since we kick off aplay asynchronously.
	struct sigaction sa_chld = {};
	sigemptyset(&sa_chld.sa_mask);
	sa_chld.sa_flags = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa_chld, nullptr);
#endif
}

void QtHost::PrintCommandLineVersion()
{
	CommonHost::InitializeEarlyConsole();
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
	std::fprintf(stderr, "  -disc <path>: Uses the specified host DVD drive as a source.\n");
	std::fprintf(stderr, "  -logfile <path>: Writes the application log to path instead of emulog.txt.\n");
	std::fprintf(stderr, "  -bios: Starts the BIOS (System Menu/OSDSYS).\n");
	std::fprintf(stderr, "  -fastboot: Force fast boot for provided filename.\n");
	std::fprintf(stderr, "  -slowboot: Force slow boot for provided filename.\n");
	std::fprintf(stderr, "  -state <index>: Loads specified save state by index.\n");
	std::fprintf(stderr, "  -statefile <filename>: Loads state from the specified filename.\n");
	std::fprintf(stderr, "  -fullscreen: Enters fullscreen mode immediately after starting.\n");
	std::fprintf(stderr, "  -nofullscreen: Prevents fullscreen mode from triggering if enabled.\n");
	std::fprintf(stderr, "  -earlyconsolelog: Forces logging of early console messages to console.\n");
	std::fprintf(stderr, "  -testconfig: Initializes configuration and checks version, then exits.\n");
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
			else if (CHECK_ARG_PARAM(QStringLiteral("-disc")))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::Disc;
				AutoBoot(autoboot)->filename = (++it)->toStdString();
				continue;
			}
			else if (CHECK_ARG_PARAM(QStringLiteral("-logfile")))
			{
				CommonHost::SetFileLogPath((++it)->toStdString());
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
				CommonHost::InitializeEarlyConsole();
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
		CommonHost::InitializeEarlyConsole();
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

static void RegisterTypes()
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
	if (!QtHost::ParseCommandLineOptions(app.arguments(), autoboot))
		return EXIT_FAILURE;

	// Bail out if we can't find any config.
	if (!QtHost::InitializeConfig())
		return EXIT_FAILURE;

	// Are we just setting up the configuration?
	if (s_test_config_and_exit)
		return EXIT_SUCCESS;

	// Set theme before creating any windows.
	MainWindow::updateApplicationTheme();
	MainWindow* main_window = new MainWindow();

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

	// Ensure config is written. Prevents destruction order issues.
	if (s_base_settings_interface->IsDirty())
		s_base_settings_interface->Save();

	// Ensure emulog is flushed.
	if (emuLog)
	{
		std::fclose(emuLog);
		emuLog = nullptr;
	}

	return result;
}
