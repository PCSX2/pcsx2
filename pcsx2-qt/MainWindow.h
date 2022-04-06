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
	static const char* DEFAULT_THEME_NAME;

public:
	explicit MainWindow(const QString& unthemed_style_name);
	~MainWindow();

	void initialize();
	void connectVMThreadSignals(EmuThread* thread);

public Q_SLOTS:
	void refreshGameList(bool invalidate_cache);
	void invalidateSaveStateCache();
	void reportError(const QString& title, const QString& message);
	void runOnUIThread(const std::function<void()>& func);
	bool requestShutdown(bool allow_confirm = true, bool allow_save_to_state = true, bool block_until_done = false);
	void requestExit();

private Q_SLOTS:
	DisplayWidget* createDisplay(bool fullscreen, bool render_to_main);
	DisplayWidget* updateDisplay(bool fullscreen, bool render_to_main);
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

	void loadSaveStateSlot(s32 slot);
	void loadSaveStateFile(const QString& filename, const QString& state_filename);
	void populateLoadStateMenu(QMenu* menu, const QString& filename, const QString& serial, quint32 crc);
	void populateSaveStateMenu(QMenu* menu, const QString& serial, quint32 crc);
	void updateSaveStateMenus(const QString& filename, const QString& serial, quint32 crc);

	Ui::MainWindow m_ui;

	QString m_unthemed_style_name;

	GameListWidget* m_game_list_widget = nullptr;
	DisplayWidget* m_display_widget = nullptr;
	DisplayContainer* m_display_container = nullptr;

	SettingsDialog* m_settings_dialog = nullptr;
	ControllerSettingsDialog* m_controller_settings_dialog = nullptr;

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
	bool m_was_focused_on_container_switch = false;

	QString m_last_fps_status;
};

extern MainWindow* g_main_window;
