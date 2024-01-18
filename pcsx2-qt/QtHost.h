// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "pcsx2/Host.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/VMManager.h"

#include <QtCore/QList>
#include <QtCore/QEventLoop>
#include <QtCore/QMetaType>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QSemaphore>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QThread>

#include <QtGui/QIcon>

class SettingsInterface;

class DisplayWidget;
struct VMBootParameters;

enum class CDVD_SourceType : uint8_t;

namespace Achievements
{
	enum class LoginRequestReason;
}

Q_DECLARE_METATYPE(std::shared_ptr<VMBootParameters>);
Q_DECLARE_METATYPE(std::optional<bool>);
Q_DECLARE_METATYPE(GSRendererType);
Q_DECLARE_METATYPE(InputBindingKey);
Q_DECLARE_METATYPE(CDVD_SourceType);
Q_DECLARE_METATYPE(Achievements::LoginRequestReason);

class EmuThread : public QThread
{
	Q_OBJECT

public:
	explicit EmuThread(QThread* ui_thread);
	~EmuThread();

	static void start();
	static void stop();

	__fi QEventLoop* getEventLoop() const { return m_event_loop; }
	__fi bool isFullscreen() const { return m_is_fullscreen; }
	__fi bool isExclusiveFullscreen() const { return m_is_exclusive_fullscreen; }
	__fi bool isRenderingToMain() const { return m_is_rendering_to_main; }
	__fi bool isSurfaceless() const { return m_is_surfaceless; }
	__fi bool isRunningFullscreenUI() const { return m_run_fullscreen_ui; }

	__fi bool isOnEmuThread() const { return (QThread::currentThread() == this); }
	__fi bool isOnUIThread() const { return (QThread::currentThread() == m_ui_thread); }
	bool shouldRenderToMain() const;

	/// Called back from the GS thread when the display state changes (e.g. fullscreen, render to main).
	std::optional<WindowInfo> acquireRenderWindow(bool recreate_window);
	void connectDisplaySignals(DisplayWidget* widget);
	void releaseRenderWindow();

	void startBackgroundControllerPollTimer();
	void stopBackgroundControllerPollTimer();
	void updatePerformanceMetrics(bool force);

public Q_SLOTS:
	bool confirmMessage(const QString& title, const QString& message);
	void loadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock);
	void checkForSettingChanges(const Pcsx2Config& old_config);
	void startFullscreenUI(bool fullscreen);
	void stopFullscreenUI();
	void startVM(std::shared_ptr<VMBootParameters> boot_params);
	void resetVM();
	void setVMPaused(bool paused);
	void shutdownVM(bool save_state = true);
	void loadState(const QString& filename);
	void loadStateFromSlot(qint32 slot);
	void saveState(const QString& filename);
	void saveStateToSlot(qint32 slot);
	void toggleFullscreen();
	void setFullscreen(bool fullscreen, bool allow_render_to_main);
	void setSurfaceless(bool surfaceless);
	void applySettings();
	void reloadGameSettings();
	void updateEmuFolders();
	void toggleSoftwareRendering();
	void changeDisc(CDVD_SourceType source, const QString& path);
	void setELFOverride(const QString& path);
	void changeGSDump(const QString& path);
	void reloadPatches();
	void reloadInputSources();
	void reloadInputBindings();
	void reloadInputDevices();
	void closeInputSources();
	void requestDisplaySize(float scale);
	void enumerateInputDevices();
	void enumerateVibrationMotors();
	void runOnCPUThread(const std::function<void()>& func);
	void queueSnapshot(quint32 gsdump_frames);
	void beginCapture(const QString& path);
	void endCapture();

Q_SIGNALS:
	bool messageConfirmed(const QString& title, const QString& message);

	std::optional<WindowInfo> onAcquireRenderWindowRequested(bool recreate_window, bool fullscreen, bool render_to_main, bool surfaceless);
	void onResizeRenderWindowRequested(qint32 width, qint32 height);
	void onReleaseRenderWindowRequested();
	void onMouseModeRequested(bool relative_mode, bool hide_cursor);
	void onFullscreenUIStateChange(bool running);

	/// Called when the VM is starting initialization, but has not been completed yet.
	void onVMStarting();

	/// Called when the VM is created.
	void onVMStarted();

	/// Called when the VM is paused.
	void onVMPaused();

	/// Called when the VM is resumed after being paused.
	void onVMResumed();

	/// Called when the VM is shut down or destroyed.
	void onVMStopped();

	/// Provided by the host; called when the running executable changes.
	void onGameChanged(const QString& title, const QString& elf_override, const QString& disc_path,
		const QString& serial, quint32 disc_crc, quint32 crc);

	void onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices);
	void onInputDeviceConnected(const QString& identifier, const QString& device_name);
	void onInputDeviceDisconnected(const QString& identifier);
	void onVibrationMotorsEnumerated(const QList<InputBindingKey>& motors);

	/// Called when a save state is loading, before the file is processed.
	void onSaveStateLoading(const QString& path);

	/// Called after a save state is successfully loaded. If the save state was invalid, was_successful will be false.
	void onSaveStateLoaded(const QString& path, bool was_successful);

	/// Called when a save state is being created/saved. The compression/write to disk is asynchronous, so this callback
	/// just signifies that the save has started, not necessarily completed.
	void onSaveStateSaved(const QString& path);

	/// Called when achievements login is requested.
	void onAchievementsLoginRequested(Achievements::LoginRequestReason reason);

	/// Called when achievements login succeeds. Also happens on startup.
	void onAchievementsLoginSucceeded(const QString& display_name, quint32 points, quint32 sc_points, quint32 unread_messages);

	/// Called when achievements are reloaded/refreshed (e.g. game change, login, option change).
	void onAchievementsRefreshed(quint32 id, const QString& game_info_string);

	/// Called when hardcore mode is enabled or disabled.
	void onAchievementsHardcoreModeChanged(bool enabled);

	/// Big Picture UI requests.
	void onCoverDownloaderOpenRequested();
	void onCreateMemoryCardOpenRequested();

	/// Called when video capture starts/stops.
	void onCaptureStarted(const QString& filename);
	void onCaptureStopped();

protected:
	void run();

private:
	/// Interval at which the controllers are polled when the system is not active.
	static constexpr u32 BACKGROUND_CONTROLLER_POLLING_INTERVAL = 100;

	/// Poll at half the vsync rate for FSUI to reduce the chance of getting a press+release in the same frame.
	static constexpr u32 FULLSCREEN_UI_CONTROLLER_POLLING_INTERVAL = 8;

	void destroyVM();

	void createBackgroundControllerPollTimer();
	void destroyBackgroundControllerPollTimer();
	void connectSignals();

private Q_SLOTS:
	void stopInThread();
	void doBackgroundControllerPoll();
	void onDisplayWindowResized(int width, int height, float scale);
	void onApplicationStateChanged(Qt::ApplicationState state);
	void redrawDisplayWindow();

private:
	QThread* m_ui_thread;
	QSemaphore m_started_semaphore;
	QEventLoop* m_event_loop = nullptr;
	QTimer* m_background_controller_polling_timer = nullptr;

	std::atomic_bool m_shutdown_flag{false};

	bool m_verbose_status = false;
	bool m_run_fullscreen_ui = false;
	bool m_is_rendering_to_main = false;
	bool m_is_fullscreen = false;
	bool m_is_exclusive_fullscreen = false;
	bool m_is_surfaceless = false;
	bool m_save_state_on_shutdown = false;
	bool m_pause_on_focus_loss = false;

	bool m_was_paused_by_focus_loss = false;

	float m_last_speed = 0.0f;
	float m_last_game_fps = 0.0f;
	float m_last_video_fps = 0.0f;
	int m_last_internal_width = 0;
	int m_last_internal_height = 0;
	GSRendererType m_last_renderer = GSRendererType::Null;
};

extern EmuThread* g_emu_thread;

namespace QtHost
{
	/// Default theme name for the platform.
	const char* GetDefaultThemeName();

	/// Default language for the platform.
	const char* GetDefaultLanguage();

	/// Sets application theme according to settings.
	void UpdateApplicationTheme();

	/// Returns true if the application theme is using dark colours.
	bool IsDarkApplicationTheme();

	/// Sets the icon theme, based on the current style (light/dark).
	void SetIconThemeFromStyle();

	/// Sets batch mode (exit after game shutdown).
	bool InBatchMode();

	/// Sets NoGUI mode (implys batch mode, does not display main window, exits on shutdown).
	bool InNoGUIMode();

	/// Returns true if the calling thread is the UI thread.
	bool IsOnUIThread();

	/// Returns true if advanced settings should be shown.
	bool ShouldShowAdvancedSettings();

	/// Executes a function on the UI thread.
	void RunOnUIThread(const std::function<void()>& func, bool block = false);

	/// Returns a list of supported languages and codes (suffixes for translation files).
	std::vector<std::pair<QString, QString>> GetAvailableLanguageList();

	/// Call when the language changes.
	void InstallTranslator(QWidget* dialog_parent);

	/// Returns the application name and version, optionally including debug/devel config indicator.
	QString GetAppNameAndVersion();

	/// Returns the debug/devel config indicator.
	QString GetAppConfigSuffix();

	/// Returns the main application icon.
	QIcon GetAppIcon();

	/// Returns the base path for resources. This may be : prefixed, if we're using embedded resources.
	QString GetResourcesBasePath();

	/// Returns the URL to a runtime-downloaded resource.
	std::string GetRuntimeDownloadedResourceURL(std::string_view name);

	/// Downloads the specified URL to the provided path.
	bool DownloadFile(QWidget* parent, const QString& title, std::string url, const std::string& path);

	/// VM state, safe to access on UI thread.
	bool IsVMValid();
	bool IsVMPaused();

	/// Accessors for game information.
	const QString& GetCurrentGameTitle();
	const QString& GetCurrentGameSerial();
	const QString& GetCurrentGamePath();

	/// Compare strings in the locale of the current UI language
	int LocaleSensitiveCompare(QStringView lhs, QStringView rhs);
} // namespace QtHost
