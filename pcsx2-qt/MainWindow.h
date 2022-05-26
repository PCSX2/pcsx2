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

#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <functional>
#include <optional>

#include "Settings/ControllerSettingsDialog.h"
#include "Settings/SettingsDialog.h"
#include "ui_MainWindow.h"

class QProgressBar;

class AutoUpdaterDialog;
class DisplayWidget;
class DisplayContainer;
class GameListWidget;
class ControllerSettingsDialog;

class EmuThread;

namespace GameList
{
	struct Entry;
}

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

		/// Returns the parent widget, which can be used for any popup dialogs.
		__fi QWidget* getDialogParent() const { return m_dialog_parent; }

		/// Cancels any pending unpause/fullscreen transition.
		/// Call when you're going to destroy the VM anyway.
		void cancelResume();

	private:
		VMLock(QWidget* dialog_parent, bool was_paused, bool was_fullscreen);
		friend MainWindow;

		QWidget* m_dialog_parent;
		bool m_was_paused;
		bool m_was_fullscreen;
	};

	/// Default theme name for the platform.
	static const char* DEFAULT_THEME_NAME;

public:
	explicit MainWindow(const QString& unthemed_style_name);
	~MainWindow();

	void initialize();
	void connectVMThreadSignals(EmuThread* thread);
	void startupUpdateCheck();

	/// Locks the VM by pausing it, while a popup dialog is displayed.
	VMLock pauseAndLockVM();

public Q_SLOTS:
	void checkForUpdates(bool display_message);
	void refreshGameList(bool invalidate_cache);
	void cancelGameListRefresh();
	void invalidateSaveStateCache();
	void reportError(const QString& title, const QString& message);
	void runOnUIThread(const std::function<void()>& func);
	bool requestShutdown(bool allow_confirm = true, bool allow_save_to_state = true, bool block_until_done = false);
	void requestExit();

private Q_SLOTS:
	void onUpdateCheckComplete();

	DisplayWidget* createDisplay(bool fullscreen, bool render_to_main);
	DisplayWidget* updateDisplay(bool fullscreen, bool render_to_main, bool surfaceless);
	void displayResizeRequested(qint32 width, qint32 height);
	void destroyDisplay();
	void focusDisplayWidget();

	void onGameListRefreshComplete();
	void onGameListRefreshProgress(const QString& status, int current, int total);
	void onGameListSelectionChanged();
	void onGameListEntryActivated();
	void onGameListEntryContextMenuRequested(const QPoint& point);

	void onStartFileActionTriggered();
	void onStartBIOSActionTriggered();
	void onChangeDiscFromFileActionTriggered();
	void onChangeDiscFromGameListActionTriggered();
	void onChangeDiscFromDeviceActionTriggered();
	void onChangeDiscMenuAboutToShow();
	void onChangeDiscMenuAboutToHide();
	void onLoadStateMenuAboutToShow();
	void onSaveStateMenuAboutToShow();
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
	void onCheckForUpdatesActionTriggered();
	void onToolsOpenDataDirectoryTriggered();
	void onThemeChanged();
	void onThemeChangedFromSettings();
	void onLoggingOptionChanged();
	void onScreenshotActionTriggered();
	void onSaveGSDumpActionTriggered();
	void onBlockDumpActionToggled(bool checked);

	// Input Recording
	void onInputRecNewActionTriggered();
	void onInputRecPlayActionTriggered();
	void onInputRecStopActionTriggered();
	void onInputRecOpenSettingsTriggered();

	void onVMStarting();
	void onVMStarted();
	void onVMPaused();
	void onVMResumed();
	void onVMStopped();

	void onGameChanged(const QString& path, const QString& serial, const QString& name, quint32 crc);
	void onPerformanceMetricsUpdated(const QString& fps_stat, const QString& gs_stat);

	void recreate();

protected:
	void closeEvent(QCloseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private:
	enum : s32
	{
		NUM_SAVE_STATE_SLOTS = 10,
	};

	void setupAdditionalUi();
	void connectSignals();
	void setStyleFromSettings();
	void setIconThemeFromSettings();

	void saveStateToConfig();
	void restoreStateFromConfig();

	void updateEmulationActions(bool starting, bool running);
	void updateStatusBarWidgetVisibility();
	void updateWindowTitle();
	void setProgressBar(int current, int total);
	void clearProgressBar();

	bool isShowingGameList() const;
	bool isRenderingFullscreen() const;
	bool isRenderingToMain() const;
	void switchToGameListView();
	void switchToEmulationView();

	QWidget* getDisplayContainer() const;
	void saveDisplayWindowGeometryToConfig();
	void restoreDisplayWindowGeometryFromConfig();
	void destroyDisplayWidget();
	void setDisplayFullscreen(const std::string& fullscreen_mode);

	SettingsDialog* getSettingsDialog();
	void doSettings(const char* category = nullptr);

	ControllerSettingsDialog* getControllerSettingsDialog();
	void doControllerSettings(ControllerSettingsDialog::Category category = ControllerSettingsDialog::Category::Count);

	void startGameListEntry(const GameList::Entry* entry, std::optional<s32> save_slot = std::nullopt,
		std::optional<bool> fast_boot = std::nullopt);
	void setGameListEntryCoverImage(const GameList::Entry* entry);

	std::optional<bool> promptForResumeState(const QString& save_state_path);
	void loadSaveStateSlot(s32 slot);
	void loadSaveStateFile(const QString& filename, const QString& state_filename);
	void populateLoadStateMenu(QMenu* menu, const QString& filename, const QString& serial, quint32 crc);
	void populateSaveStateMenu(QMenu* menu, const QString& serial, quint32 crc);
	void updateSaveStateMenus(const QString& filename, const QString& serial, quint32 crc);
	void doStartDisc(const QString& path);
	void doDiscChange(const QString& path);

	Ui::MainWindow m_ui;

	QString m_unthemed_style_name;

	GameListWidget* m_game_list_widget = nullptr;
	DisplayWidget* m_display_widget = nullptr;
	DisplayContainer* m_display_container = nullptr;

	SettingsDialog* m_settings_dialog = nullptr;
	ControllerSettingsDialog* m_controller_settings_dialog = nullptr;
	AutoUpdaterDialog* m_auto_updater_dialog = nullptr;

	QProgressBar* m_status_progress_widget = nullptr;
	QLabel* m_status_gs_widget = nullptr;
	QLabel* m_status_fps_widget = nullptr;

	QString m_current_disc_path;
	QString m_current_game_serial;
	QString m_current_game_name;
	quint32 m_current_game_crc;
	bool m_vm_valid = false;
	bool m_vm_paused = false;
	bool m_save_states_invalidated = false;
	bool m_was_paused_on_surface_loss = false;
	bool m_was_disc_change_request = false;

	QString m_last_fps_status;
};

extern MainWindow* g_main_window;
