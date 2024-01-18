// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/WindowInfo.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <functional>
#include <optional>

#include "Tools/InputRecording/InputRecordingViewer.h"
#include "Settings/ControllerSettingsWindow.h"
#include "Settings/SettingsWindow.h"
#include "Debugger/DebuggerWindow.h"
#include "ui_MainWindow.h"

class QProgressBar;

class AutoUpdaterDialog;
class DisplayWidget;
class DisplayContainer;
class GameListWidget;
class ControllerSettingsWindow;

class EmuThread;

namespace Achievements
{
	enum class LoginRequestReason;
}

namespace GameList
{
	struct Entry;
}

enum class CDVD_SourceType : uint8_t;

class MainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	/// This class is a scoped lock on the VM, which prevents it from running while
	/// the object exists. Its purpose is to be used for blocking/modal popup boxes,
	/// where the VM needs to exit fullscreen temporarily.
	class VMLock
	{
	public:
		VMLock(VMLock&& lock);
		VMLock(const VMLock&) = delete;
		~VMLock();

		VMLock& operator=(VMLock&& lock);
		VMLock& operator=(const VMLock&) = delete;

		/// Returns the parent widget, which can be used for any popup dialogs.
		__fi QWidget* getDialogParent() const { return m_dialog_parent; }

		/// Cancels any pending unpause/fullscreen transition.
		/// Call when you're going to destroy the VM anyway.
		void cancelResume();

	private:
		VMLock(QWidget* dialog_parent, bool was_paused, bool was_exclusive_fullscreen);
		friend MainWindow;

		QWidget* m_dialog_parent;
		bool m_was_paused;
		bool m_was_fullscreen;
	};

	/// Default filter for opening a file.
	static const char* OPEN_FILE_FILTER;

	/// Default filter for opening a disc image.
	static const char* DISC_IMAGE_FILTER;

public:
	MainWindow();
	~MainWindow();

	void initialize();
	void connectVMThreadSignals(EmuThread* thread);
	void startupUpdateCheck();
	void resetSettings(bool ui);

	/// Locks the VM by pausing it, while a popup dialog is displayed.
	VMLock pauseAndLockVM();

	/// Accessors for the status bar widgets, updated by the emulation thread.
	__fi QLabel* getStatusVerboseWidget() const { return m_status_verbose_widget; }
	__fi QLabel* getStatusRendererWidget() const { return m_status_renderer_widget; }
	__fi QLabel* getStatusResolutionWidget() const { return m_status_resolution_widget; }
	__fi QLabel* getStatusFPSWidget() const { return m_status_fps_widget; }
	__fi QLabel* getStatusVPSWidget() const { return m_status_vps_widget; }

	/// Rescans a single file. NOTE: Happens on UI thread.
	void rescanFile(const std::string& path);

	void openDebugger();

public Q_SLOTS:
	void checkForUpdates(bool display_message, bool force_check);
	void refreshGameList(bool invalidate_cache);
	void cancelGameListRefresh();
	void reportError(const QString& title, const QString& message);
	bool confirmMessage(const QString& title, const QString& message);
	void runOnUIThread(const std::function<void()>& func);
	void requestReset();
	bool requestShutdown(bool allow_confirm = true, bool allow_save_to_state = true, bool default_save_to_state = true);
	void requestExit(bool allow_confirm = true);
	void checkForSettingChanges();
	std::optional<WindowInfo> getWindowInfo();

private Q_SLOTS:
	void onUpdateCheckComplete();

	std::optional<WindowInfo> acquireRenderWindow(bool recreate_window, bool fullscreen, bool render_to_main, bool surfaceless);
	void displayResizeRequested(qint32 width, qint32 height);
	void mouseModeRequested(bool relative_mode, bool hide_cursor);
	void releaseRenderWindow();
	void focusDisplayWidget();

	void onGameListRefreshComplete();
	void onGameListRefreshProgress(const QString& status, int current, int total);
	void onGameListSelectionChanged();
	void onGameListEntryActivated();
	void onGameListEntryContextMenuRequested(const QPoint& point);

	void onStartFileActionTriggered();
	void onStartDiscActionTriggered();
	void onStartBIOSActionTriggered();
	void onChangeDiscFromFileActionTriggered();
	void onChangeDiscFromGameListActionTriggered();
	void onChangeDiscFromDeviceActionTriggered();
	void onRemoveDiscActionTriggered();
	void onChangeDiscMenuAboutToShow();
	void onChangeDiscMenuAboutToHide();
	void onLoadStateMenuAboutToShow();
	void onSaveStateMenuAboutToShow();
	void onStartFullscreenUITriggered();
	void onFullscreenUIStateChange(bool running);
	void onViewToolbarActionToggled(bool checked);
	void onViewLockToolbarActionToggled(bool checked);
	void onViewStatusBarActionToggled(bool checked);
	void onViewGameListActionTriggered();
	void onViewGameGridActionTriggered();
	void onViewSystemDisplayTriggered();
	void onViewGamePropertiesActionTriggered();
	void onGitHubRepositoryActionTriggered();
	void onSupportForumsActionTriggered();
	void onDiscordServerActionTriggered();
	void onAboutActionTriggered();
	void onToolsOpenDataDirectoryTriggered();
	void onToolsCoverDownloaderTriggered();
	void onToolsEditCheatsPatchesTriggered(bool cheats);
	void onCreateMemoryCardOpenRequested();
	void updateTheme();
	void reloadThemeSpecificImages();
	void updateLanguage();
	void onScreenshotActionTriggered();
	void onSaveGSDumpActionTriggered();
	void onBlockDumpActionToggled(bool checked);
	void onShowAdvancedSettingsToggled(bool checked);
	void onToolsVideoCaptureToggled(bool checked);
	void onSettingsTriggeredFromToolbar();

	// Input Recording
	void onInputRecNewActionTriggered();
	void onInputRecPlayActionTriggered();
	void onInputRecStopActionTriggered();
	void onInputRecOpenSettingsTriggered();
	void onInputRecOpenViewer();

	void onVMStarting();
	void onVMStarted();
	void onVMPaused();
	void onVMResumed();
	void onVMStopped();

	void onGameChanged(const QString& title, const QString& elf_override, const QString& disc_path,
		const QString& serial, quint32 disc_crc, quint32 crc);

	void onCaptureStarted(const QString& filename);
	void onCaptureStopped();

	void onAchievementsLoginRequested(Achievements::LoginRequestReason reason);
	void onAchievementsLoginSucceeded(const QString& display_name, quint32 points, quint32 sc_points, quint32 unread_messages);
	void onAchievementsHardcoreModeChanged(bool enabled);

protected:
	void showEvent(QShowEvent* event) override;
	void closeEvent(QCloseEvent* event) override;
	void changeEvent(QEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void moveEvent(QMoveEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

#ifdef _WIN32
	bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private:
	void setupAdditionalUi();
	void connectSignals();
	void createRendererSwitchMenu();
	void recreate();
	void recreateSettings();
	void destroySubWindows();

	void registerForDeviceNotifications();
	void unregisterForDeviceNotifications();

	void saveStateToConfig();
	void restoreStateFromConfig();

	void updateEmulationActions(bool starting, bool running, bool stopping);
	void updateDisplayRelatedActions(bool has_surface, bool render_to_main, bool fullscreen);
	void updateGameDependentActions();
	void updateStatusBarWidgetVisibility();
	void updateAdvancedSettingsVisibility();
	void updateWindowTitle();
	void updateWindowState(bool force_visible = false);
	void setProgressBar(int current, int total);
	void clearProgressBar();

	bool isShowingGameList() const;
	bool isRenderingFullscreen() const;
	bool isRenderingToMain() const;
	bool shouldHideMouseCursor() const;
	bool shouldHideMainWindow() const;
	void switchToGameListView();
	void switchToEmulationView();

	bool shouldAbortForMemcardBusy(const VMLock& lock);

	QWidget* getContentParent();
	QWidget* getDisplayContainer() const;
	void saveDisplayWindowGeometryToConfig();
	void restoreDisplayWindowGeometryFromConfig();
	void createDisplayWidget(bool fullscreen, bool render_to_main);
	void destroyDisplayWidget(bool show_game_list);
	void updateDisplayWidgetCursor();

	SettingsWindow* getSettingsWindow();
	void doSettings(const char* category = nullptr);

	InputRecordingViewer* getInputRecordingViewer();
	void updateInputRecordingActions(bool started);

	DebuggerWindow* getDebuggerWindow();

	void doControllerSettings(ControllerSettingsWindow::Category category = ControllerSettingsWindow::Category::Count);

	QString getDiscDevicePath(const QString& title);

	void startGameListEntry(
		const GameList::Entry* entry, std::optional<s32> save_slot = std::nullopt, std::optional<bool> fast_boot = std::nullopt);
	void setGameListEntryCoverImage(const GameList::Entry* entry);
	void clearGameListEntryPlayTime(const GameList::Entry* entry);

	std::optional<bool> promptForResumeState(const QString& save_state_path);
	void loadSaveStateSlot(s32 slot);
	void loadSaveStateFile(const QString& filename, const QString& state_filename);
	void populateLoadStateMenu(QMenu* menu, const QString& filename, const QString& serial, quint32 crc);
	void populateSaveStateMenu(QMenu* menu, const QString& serial, quint32 crc);
	void doStartFile(std::optional<CDVD_SourceType> source, const QString& path);
	void doDiscChange(CDVD_SourceType source, const QString& path);

	Ui::MainWindow m_ui;

	GameListWidget* m_game_list_widget = nullptr;
	DisplayWidget* m_display_widget = nullptr;
	DisplayContainer* m_display_container = nullptr;

	SettingsWindow* m_settings_window = nullptr;
	ControllerSettingsWindow* m_controller_settings_window = nullptr;
	InputRecordingViewer* m_input_recording_viewer = nullptr;
	AutoUpdaterDialog* m_auto_updater_dialog = nullptr;

	DebuggerWindow* m_debugger_window = nullptr;

	QProgressBar* m_status_progress_widget = nullptr;
	QLabel* m_status_verbose_widget = nullptr;
	QLabel* m_status_renderer_widget = nullptr;
	QLabel* m_status_fps_widget = nullptr;
	QLabel* m_status_vps_widget = nullptr;
	QLabel* m_status_resolution_widget = nullptr;

	QMenu* m_settings_toolbar_menu = nullptr;

	bool m_display_created = false;
	bool m_relative_mouse_mode = false;
	bool m_hide_mouse_cursor = false;
	bool m_was_paused_on_surface_loss = false;
	bool m_was_disc_change_request = false;
	bool m_is_closing = false;
	bool m_is_temporarily_windowed = false;

	QString m_last_fps_status;

#ifdef _WIN32
	void* m_device_notification_handle = nullptr;
#endif
};

extern MainWindow* g_main_window;
