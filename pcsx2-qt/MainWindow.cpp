// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AboutDialog.h"
#include "AutoUpdaterDialog.h"
#include "CoverDownloadDialog.h"
#include "DisplayWidget.h"
#include "GameList/GameListRefreshThread.h"
#include "GameList/GameListWidget.h"
#include "LogWindow.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "Settings/AchievementLoginDialog.h"
#include "Settings/ControllerSettingsWindow.h"
#include "Settings/GameListSettingsWidget.h"
#include "Settings/InterfaceSettingsWidget.h"
#include "Settings/MemoryCardCreateDialog.h"
#include "Tools/InputRecording/InputRecordingViewer.h"
#include "Tools/InputRecording/NewInputRecordingDlg.h"
#include "svnrev.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/CDVD/CDVDdiscReader.h"
#include "pcsx2/GS.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/GSDumpReplayer.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/MTGS.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/Recording/InputRecording.h"
#include "pcsx2/Recording/InputRecordingControls.h"
#include "pcsx2/SIO/Sio.h"

#include "common/Assertions.h"
#include "common/CocoaTools.h"
#include "common/FileSystem.h"

#include <QtCore/QDateTime>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <Dbt.h>
#endif

const char* MainWindow::OPEN_FILE_FILTER =
	QT_TRANSLATE_NOOP("MainWindow", "All File Types (*.bin *.iso *.cue *.mdf *.chd *.cso *.zso *.gz *.elf *.irx *.gs *.gs.xz *.gs.zst *.dump);;"
									"Single-Track Raw Images (*.bin *.iso);;"
									"Cue Sheets (*.cue);;"
									"Media Descriptor File (*.mdf);;"
									"MAME CHD Images (*.chd);;"
									"CSO Images (*.cso);;"
									"ZSO Images (*.zso);;"
									"GZ Images (*.gz);;"
									"ELF Executables (*.elf);;"
									"IRX Executables (*.irx);;"
									"GS Dumps (*.gs *.gs.xz *.gs.zst);;"
									"Block Dumps (*.dump)");

const char* MainWindow::DISC_IMAGE_FILTER = QT_TRANSLATE_NOOP("MainWindow", "All File Types (*.bin *.iso *.cue *.mdf *.chd *.cso *.zso *.gz *.dump);;"
																			"Single-Track Raw Images (*.bin *.iso);;"
																			"Cue Sheets (*.cue);;"
																			"Media Descriptor File (*.mdf);;"
																			"MAME CHD Images (*.chd);;"
																			"CSO Images (*.cso);;"
																			"ZSO Images (*.zso);;"
																			"GZ Images (*.gz);;"
																			"Block Dumps (*.dump)");

MainWindow* g_main_window = nullptr;

#if defined(_WIN32) || defined(__APPLE__)
static const bool s_use_central_widget = false;
#else
// Qt Wayland is broken. Any sort of stacked widget usage fails to update,
// leading to broken window resizes, no display rendering, etc. So, we mess
// with the central widget instead. Which we can't do on xorg, because it
// breaks window resizing there...
static bool s_use_central_widget = false;
#endif

// UI thread VM validity.
static bool s_vm_valid = false;
static bool s_vm_paused = false;
static QString s_current_title;
static QString s_current_elf_override;
static QString s_current_disc_path;
static QString s_current_disc_serial;
static quint32 s_current_disc_crc;
static quint32 s_current_running_crc;

MainWindow::MainWindow()
{
	pxAssert(!g_main_window);
	g_main_window = this;

#if !defined(_WIN32) && !defined(__APPLE__)
	s_use_central_widget = DisplayContainer::isRunningOnWayland();
#endif
}

MainWindow::~MainWindow()
{
	// make sure the game list isn't refreshing, because it's on a separate thread
	cancelGameListRefresh();
	destroySubWindows();

	// we compare here, since recreate destroys the window later
	if (g_main_window == this)
		g_main_window = nullptr;
#ifdef _WIN32
	unregisterForDeviceNotifications();
#endif
#ifdef __APPLE__
	CocoaTools::RemoveThemeChangeHandler(this);
#endif
}

void MainWindow::initialize()
{
#ifdef __APPLE__
	CocoaTools::AddThemeChangeHandler(this, [](void* ctx) {
		// This handler is called *before* the style change has propagated far enough for Qt to see it
		// Use RunOnUIThread to delay until it has
		QtHost::RunOnUIThread([ctx = static_cast<MainWindow*>(ctx)] {
			ctx->updateTheme(); // Qt won't notice the style change without us touching the palette in some way
		});
	});
#endif
	m_ui.setupUi(this);
	setupAdditionalUi();
	connectSignals();
	connectVMThreadSignals(g_emu_thread);

	restoreStateFromConfig();
	switchToGameListView();
	updateWindowTitle();
	updateGameDependentActions();

#ifdef _WIN32
	registerForDeviceNotifications();
#endif
}

// TODO: Figure out how to set this in the .ui file
/// Marks the icons for all actions in the given menu as mask icons
/// This means macOS's menubar renderer will ignore color values and use only the alpha in the image.
/// The color value will instead be taken from the system theme.
/// Since the menubar follows the OS's dark/light mode and not our current theme's, this prevents problems where a theme mismatch puts white icons in light mode or dark icons in dark mode.
static void makeIconsMasks(QWidget* menu)
{
	for (QAction* action : menu->actions())
	{
		if (!action->icon().isNull())
		{
			QIcon icon = action->icon();
			icon.setIsMask(true);
			action->setIcon(icon);
		}
		if (action->menu())
			makeIconsMasks(action->menu());
	}
}

QWidget* MainWindow::getContentParent()
{
	return s_use_central_widget ? static_cast<QWidget*>(this) : static_cast<QWidget*>(m_ui.mainContainer);
}

void MainWindow::setupAdditionalUi()
{
	makeIconsMasks(menuBar());
	updateAdvancedSettingsVisibility();

	const bool toolbar_visible = Host::GetBaseBoolSettingValue("UI", "ShowToolbar", false);
	m_ui.actionViewToolbar->setChecked(toolbar_visible);
	m_ui.toolBar->setVisible(toolbar_visible);

	const bool toolbars_locked = Host::GetBaseBoolSettingValue("UI", "LockToolbar", false);
	m_ui.actionViewLockToolbar->setChecked(toolbars_locked);
	m_ui.toolBar->setMovable(!toolbars_locked);
	m_ui.toolBar->setContextMenuPolicy(Qt::PreventContextMenu);

	const bool status_bar_visible = Host::GetBaseBoolSettingValue("UI", "ShowStatusBar", true);
	m_ui.actionViewStatusBar->setChecked(status_bar_visible);
	m_ui.statusBar->setVisible(status_bar_visible);

	m_game_list_widget = new GameListWidget(getContentParent());
	m_game_list_widget->initialize();
	m_ui.actionGridViewShowTitles->setChecked(m_game_list_widget->getShowGridCoverTitles());
	if (s_use_central_widget)
	{
		m_ui.mainContainer = nullptr; // setCentralWidget() will delete this
		setCentralWidget(m_game_list_widget);
	}
	else
	{
		m_ui.mainContainer->addWidget(m_game_list_widget);
	}

	m_status_progress_widget = new QProgressBar(m_ui.statusBar);
	m_status_progress_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_status_progress_widget->setFixedSize(140, 16);
	m_status_progress_widget->setMinimum(0);
	m_status_progress_widget->setMaximum(100);
	m_status_progress_widget->hide();

	m_status_verbose_widget = new QLabel(m_ui.statusBar);
	m_status_verbose_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_status_verbose_widget->setFixedHeight(16);
	m_status_verbose_widget->hide();

	m_status_renderer_widget = new QLabel(m_ui.statusBar);
	m_status_renderer_widget->setFixedHeight(16);
	m_status_renderer_widget->setFixedSize(65, 16);
	m_status_renderer_widget->hide();

	m_status_resolution_widget = new QLabel(m_ui.statusBar);
	m_status_resolution_widget->setFixedHeight(16);
	m_status_resolution_widget->setFixedSize(70, 16);
	m_status_resolution_widget->hide();

	m_status_fps_widget = new QLabel(m_ui.statusBar);
	m_status_fps_widget->setFixedSize(85, 16);
	m_status_fps_widget->hide();

	m_status_vps_widget = new QLabel(m_ui.statusBar);
	m_status_vps_widget->setFixedSize(125, 16);
	m_status_vps_widget->hide();

	m_settings_toolbar_menu = new QMenu(m_ui.toolBar);
	m_settings_toolbar_menu->addAction(m_ui.actionSettings);
	m_settings_toolbar_menu->addAction(m_ui.actionViewGameProperties);

	for (u32 scale = 0; scale <= 10; scale++)
	{
		QAction* action = m_ui.menuWindowSize->addAction((scale == 0) ? tr("Internal Resolution") : tr("%1x Scale").arg(scale));
		connect(action, &QAction::triggered, [scale]() { g_emu_thread->requestDisplaySize(static_cast<float>(scale)); });
	}

	updateEmulationActions(false, false, false);
	updateDisplayRelatedActions(false, false, false);

#ifdef ENABLE_RAINTEGRATION
	if (Achievements::IsUsingRAIntegration())
	{
		QMenu* raMenu = new QMenu(QStringLiteral("RAIntegration"), m_ui.menuTools);
		connect(raMenu, &QMenu::aboutToShow, this, [this, raMenu]() {
			raMenu->clear();

			const auto items = Achievements::RAIntegration::GetMenuItems();
			for (const auto& [id, title, checked] : items)
			{
				if (id == 0)
				{
					raMenu->addSeparator();
					continue;
				}

				QAction* raAction = raMenu->addAction(QString::fromUtf8(title));
				if (checked)
				{
					raAction->setCheckable(true);
					raAction->setChecked(checked);
				}

				connect(raAction, &QAction::triggered, this,
					[id = id]() { Host::RunOnCPUThread([id]() { Achievements::RAIntegration::ActivateMenuItem(id); }, false); });
			}
		});
		m_ui.menuTools->insertMenu(m_ui.menuInputRecording->menuAction(), raMenu);
	}
#endif
}

void MainWindow::connectSignals()
{
	connect(m_ui.actionStartFile, &QAction::triggered, this, &MainWindow::onStartFileActionTriggered);
	connect(m_ui.actionStartDisc, &QAction::triggered, this, &MainWindow::onStartDiscActionTriggered);
	connect(m_ui.actionStartBios, &QAction::triggered, this, &MainWindow::onStartBIOSActionTriggered);
	connect(m_ui.actionChangeDiscFromFile, &QAction::triggered, this, &MainWindow::onChangeDiscFromFileActionTriggered);
	connect(m_ui.actionChangeDiscFromDevice, &QAction::triggered, this, &MainWindow::onChangeDiscFromDeviceActionTriggered);
	connect(m_ui.actionChangeDiscFromGameList, &QAction::triggered, this, &MainWindow::onChangeDiscFromGameListActionTriggered);
	connect(m_ui.actionRemoveDisc, &QAction::triggered, this, &MainWindow::onRemoveDiscActionTriggered);
	connect(m_ui.menuChangeDisc, &QMenu::aboutToShow, this, &MainWindow::onChangeDiscMenuAboutToShow);
	connect(m_ui.menuChangeDisc, &QMenu::aboutToHide, this, &MainWindow::onChangeDiscMenuAboutToHide);
	connect(m_ui.actionPowerOff, &QAction::triggered, this, [this]() { requestShutdown(true, true, EmuConfig.SaveStateOnShutdown); });
	connect(m_ui.actionPowerOffWithoutSaving, &QAction::triggered, this, [this]() { requestShutdown(false, false, false); });
	connect(m_ui.actionStartFullscreenUI, &QAction::triggered, this, &MainWindow::onStartFullscreenUITriggered);
	connect(m_ui.actionToolbarStartFullscreenUI, &QAction::triggered, this, &MainWindow::onStartFullscreenUITriggered);
	connect(m_ui.actionToolbarStartFile, &QAction::triggered, this, &MainWindow::onStartFileActionTriggered);
	connect(m_ui.actionToolbarStartDisc, &QAction::triggered, this, &MainWindow::onStartDiscActionTriggered);
	connect(m_ui.actionToolbarStartBios, &QAction::triggered, this, &MainWindow::onStartBIOSActionTriggered);
	connect(m_ui.actionToolbarChangeDisc, &QAction::triggered, [this] { m_ui.menuChangeDisc->exec(QCursor::pos()); });
	connect(m_ui.actionToolbarPowerOff, &QAction::triggered, this, [this]() { requestShutdown(true, true, EmuConfig.SaveStateOnShutdown); });
	connect(m_ui.actionToolbarLoadState, &QAction::triggered, this, [this]() { m_ui.menuLoadState->exec(QCursor::pos()); });
	connect(m_ui.actionToolbarSaveState, &QAction::triggered, this, [this]() { m_ui.menuSaveState->exec(QCursor::pos()); });
	connect(m_ui.actionToolbarSettings, &QAction::triggered, this, &MainWindow::onSettingsTriggeredFromToolbar);
	connect(m_ui.actionToolbarControllerSettings, &QAction::triggered,
		[this]() { doControllerSettings(ControllerSettingsWindow::Category::GlobalSettings); });
	connect(m_ui.actionToolbarScreenshot, &QAction::triggered, this, &MainWindow::onScreenshotActionTriggered);
	connect(m_ui.actionExit, &QAction::triggered, this, &MainWindow::close);
	connect(m_ui.actionScreenshot, &QAction::triggered, this, &MainWindow::onScreenshotActionTriggered);
	connect(m_ui.menuLoadState, &QMenu::aboutToShow, this, &MainWindow::onLoadStateMenuAboutToShow);
	connect(m_ui.menuSaveState, &QMenu::aboutToShow, this, &MainWindow::onSaveStateMenuAboutToShow);
	connect(m_ui.actionSettings, &QAction::triggered, [this]() { doSettings(); });
	connect(m_ui.actionInterfaceSettings, &QAction::triggered, [this]() { doSettings("Interface"); });
	connect(m_ui.actionGameListSettings, &QAction::triggered, [this]() { doSettings("Game List"); });
	connect(m_ui.actionEmulationSettings, &QAction::triggered, [this]() { doSettings("Emulation"); });
	connect(m_ui.actionBIOSSettings, &QAction::triggered, [this]() { doSettings("BIOS"); });
	connect(m_ui.actionGraphicsSettings, &QAction::triggered, [this]() { doSettings("Graphics"); });
	connect(m_ui.actionAudioSettings, &QAction::triggered, [this]() { doSettings("Audio"); });
	connect(m_ui.actionMemoryCardSettings, &QAction::triggered, [this]() { doSettings("Memory Cards"); });
	connect(m_ui.actionDEV9Settings, &QAction::triggered, [this]() { doSettings("Network & HDD"); });
	connect(m_ui.actionFolderSettings, &QAction::triggered, [this]() { doSettings("Folders"); });
	connect(m_ui.actionAchievementSettings, &QAction::triggered, [this]() { doSettings("Achievements"); });
	connect(m_ui.actionControllerSettings, &QAction::triggered,
		[this]() { doControllerSettings(ControllerSettingsWindow::Category::GlobalSettings); });
	connect(m_ui.actionHotkeySettings, &QAction::triggered,
		[this]() { doControllerSettings(ControllerSettingsWindow::Category::HotkeySettings); });
	connect(m_ui.actionAddGameDirectory, &QAction::triggered,
		[this]() { getSettingsWindow()->getGameListSettingsWidget()->addSearchDirectory(this); });
	connect(m_ui.actionScanForNewGames, &QAction::triggered, [this]() { refreshGameList(false); });
	connect(m_ui.actionRescanAllGames, &QAction::triggered, [this]() { refreshGameList(true); });
	connect(m_ui.actionViewToolbar, &QAction::toggled, this, &MainWindow::onViewToolbarActionToggled);
	connect(m_ui.actionViewLockToolbar, &QAction::toggled, this, &MainWindow::onViewLockToolbarActionToggled);
	connect(m_ui.actionViewStatusBar, &QAction::toggled, this, &MainWindow::onViewStatusBarActionToggled);
	connect(m_ui.actionViewGameList, &QAction::triggered, this, &MainWindow::onViewGameListActionTriggered);
	connect(m_ui.actionViewGameGrid, &QAction::triggered, this, &MainWindow::onViewGameGridActionTriggered);
	connect(m_ui.actionViewSystemDisplay, &QAction::triggered, this, &MainWindow::onViewSystemDisplayTriggered);
	connect(m_ui.actionViewGameProperties, &QAction::triggered, this, &MainWindow::onViewGamePropertiesActionTriggered);
	connect(m_ui.actionGitHubRepository, &QAction::triggered, this, &MainWindow::onGitHubRepositoryActionTriggered);
	connect(m_ui.actionSupportForums, &QAction::triggered, this, &MainWindow::onSupportForumsActionTriggered);
	connect(m_ui.actionDiscordServer, &QAction::triggered, this, &MainWindow::onDiscordServerActionTriggered);
	connect(m_ui.actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
	connect(m_ui.actionAbout, &QAction::triggered, this, &MainWindow::onAboutActionTriggered);
	connect(m_ui.actionCheckForUpdates, &QAction::triggered, this, [this]() { checkForUpdates(true, true); });
	connect(m_ui.actionOpenDataDirectory, &QAction::triggered, this, &MainWindow::onToolsOpenDataDirectoryTriggered);
	connect(m_ui.actionCoverDownloader, &QAction::triggered, this, &MainWindow::onToolsCoverDownloaderTriggered);
	connect(m_ui.actionGridViewShowTitles, &QAction::triggered, m_game_list_widget, &GameListWidget::setShowCoverTitles);
	connect(m_ui.actionGridViewZoomIn, &QAction::triggered, m_game_list_widget, [this]() {
		if (isShowingGameList())
			m_game_list_widget->gridZoomIn();
	});
	connect(m_ui.actionGridViewZoomOut, &QAction::triggered, m_game_list_widget, [this]() {
		if (isShowingGameList())
			m_game_list_widget->gridZoomOut();
	});
	connect(m_ui.actionGridViewRefreshCovers, &QAction::triggered, m_game_list_widget, &GameListWidget::refreshGridCovers);
	connect(m_game_list_widget, &GameListWidget::layoutChange, this, [this]() {
		QSignalBlocker sb(m_ui.actionGridViewShowTitles);
		m_ui.actionGridViewShowTitles->setChecked(m_game_list_widget->getShowGridCoverTitles());
	});

	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionViewStatusBarVerbose, "UI", "VerboseStatusBar", false);

	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableSystemConsole, "Logging", "EnableSystemConsole", false);

	if (Log::IsDebugOutputAvailable())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableDebugConsole, "Logging", "EnableDebugConsole", false);
	}
	else
	{
		m_ui.menuTools->removeAction(m_ui.actionEnableDebugConsole);
		m_ui.actionEnableDebugConsole->deleteLater();
		m_ui.actionEnableDebugConsole = nullptr;
	}

#ifndef PCSX2_DEVBUILD
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableVerboseLogging, "Logging", "EnableVerbose", false);
#else
	// Dev builds always have verbose logging.
	m_ui.actionEnableVerboseLogging->setChecked(true);
	m_ui.actionEnableVerboseLogging->setEnabled(false);
#endif
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableEEConsoleLogging, "Logging", "EnableEEConsole", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableIOPConsoleLogging, "Logging", "EnableIOPConsole", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableLogWindow, "Logging", "EnableLogWindow", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableFileLogging, "Logging", "EnableFileLogging", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableLogTimestamps, "Logging", "EnableTimestamps", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableCDVDVerboseReads, "EmuCore", "CdvdVerboseReads", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionSaveBlockDump, "EmuCore", "CdvdDumpBlocks", false);
	m_ui.actionShowAdvancedSettings->setChecked(QtHost::ShouldShowAdvancedSettings());
	connect(m_ui.actionSaveBlockDump, &QAction::toggled, this, &MainWindow::onBlockDumpActionToggled);
	connect(m_ui.actionShowAdvancedSettings, &QAction::toggled, this, &MainWindow::onShowAdvancedSettingsToggled);
	connect(m_ui.actionSaveGSDump, &QAction::triggered, this, &MainWindow::onSaveGSDumpActionTriggered);
	connect(m_ui.actionToolsVideoCapture, &QAction::toggled, this, &MainWindow::onToolsVideoCaptureToggled);
	connect(m_ui.actionEditPatches, &QAction::triggered, this, [this]() { onToolsEditCheatsPatchesTriggered(false); });
	connect(m_ui.actionEditCheats, &QAction::triggered, this, [this]() { onToolsEditCheatsPatchesTriggered(true); });

	// Input Recording
	connect(m_ui.actionInputRecNew, &QAction::triggered, this, &MainWindow::onInputRecNewActionTriggered);
	connect(m_ui.actionInputRecPlay, &QAction::triggered, this, &MainWindow::onInputRecPlayActionTriggered);
	connect(m_ui.actionInputRecStop, &QAction::triggered, this, &MainWindow::onInputRecStopActionTriggered);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionInputRecConsoleLogs, "Logging", "EnableInputRecordingLogs", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionInputRecControllerLogs, "Logging", "EnableControllerLogs", false);
	connect(m_ui.actionInputRecOpenViewer, &QAction::triggered, this, &MainWindow::onInputRecOpenViewer);

	// These need to be queued connections to stop crashing due to menus opening/closing and switching focus.
	connect(m_game_list_widget, &GameListWidget::refreshProgress, this, &MainWindow::onGameListRefreshProgress);
	connect(m_game_list_widget, &GameListWidget::refreshComplete, this, &MainWindow::onGameListRefreshComplete);
	connect(m_game_list_widget, &GameListWidget::selectionChanged, this, &MainWindow::onGameListSelectionChanged, Qt::QueuedConnection);
	connect(m_game_list_widget, &GameListWidget::entryActivated, this, &MainWindow::onGameListEntryActivated, Qt::QueuedConnection);
	connect(m_game_list_widget, &GameListWidget::entryContextMenuRequested, this, &MainWindow::onGameListEntryContextMenuRequested,
		Qt::QueuedConnection);
	connect(m_game_list_widget, &GameListWidget::addGameDirectoryRequested, this,
		[this]() { getSettingsWindow()->getGameListSettingsWidget()->addSearchDirectory(this); });

	createRendererSwitchMenu();
}

void MainWindow::connectVMThreadSignals(EmuThread* thread)
{
	connect(thread, &EmuThread::messageConfirmed, this, &MainWindow::confirmMessage, Qt::BlockingQueuedConnection);
	connect(thread, &EmuThread::onAcquireRenderWindowRequested, this, &MainWindow::acquireRenderWindow, Qt::BlockingQueuedConnection);
	connect(thread, &EmuThread::onReleaseRenderWindowRequested, this, &MainWindow::releaseRenderWindow, Qt::BlockingQueuedConnection);
	connect(thread, &EmuThread::onResizeRenderWindowRequested, this, &MainWindow::displayResizeRequested);
	connect(thread, &EmuThread::onMouseModeRequested, this, &MainWindow::mouseModeRequested);
	connect(thread, &EmuThread::onFullscreenUIStateChange, this, &MainWindow::onFullscreenUIStateChange);
	connect(thread, &EmuThread::onVMStarting, this, &MainWindow::onVMStarting);
	connect(thread, &EmuThread::onVMStarted, this, &MainWindow::onVMStarted);
	connect(thread, &EmuThread::onVMPaused, this, &MainWindow::onVMPaused);
	connect(thread, &EmuThread::onVMResumed, this, &MainWindow::onVMResumed);
	connect(thread, &EmuThread::onVMStopped, this, &MainWindow::onVMStopped);
	connect(thread, &EmuThread::onGameChanged, this, &MainWindow::onGameChanged);
	connect(thread, &EmuThread::onCaptureStarted, this, &MainWindow::onCaptureStarted);
	connect(thread, &EmuThread::onCaptureStopped, this, &MainWindow::onCaptureStopped);
	connect(thread, &EmuThread::onAchievementsLoginRequested, this, &MainWindow::onAchievementsLoginRequested);
	connect(thread, &EmuThread::onAchievementsLoginSucceeded, this, &MainWindow::onAchievementsLoginSucceeded);
	connect(thread, &EmuThread::onAchievementsHardcoreModeChanged, this, &MainWindow::onAchievementsHardcoreModeChanged);
	connect(thread, &EmuThread::onCoverDownloaderOpenRequested, this, &MainWindow::onToolsCoverDownloaderTriggered);
	connect(thread, &EmuThread::onCreateMemoryCardOpenRequested, this, &MainWindow::onCreateMemoryCardOpenRequested);

	connect(m_ui.actionReset, &QAction::triggered, this, &MainWindow::requestReset);
	connect(m_ui.actionPause, &QAction::toggled, thread, &EmuThread::setVMPaused);
	connect(m_ui.actionFullscreen, &QAction::triggered, thread, &EmuThread::toggleFullscreen);
	connect(m_ui.actionToolbarReset, &QAction::triggered, this, &MainWindow::requestReset);
	connect(m_ui.actionToolbarPause, &QAction::toggled, thread, &EmuThread::setVMPaused);
	connect(m_ui.actionToolbarFullscreen, &QAction::triggered, thread, &EmuThread::toggleFullscreen);
	connect(m_ui.actionToggleSoftwareRendering, &QAction::triggered, thread, &EmuThread::toggleSoftwareRendering);
	connect(m_ui.actionDebugger, &QAction::triggered, this, &MainWindow::openDebugger);
	connect(m_ui.actionReloadPatches, &QAction::triggered, thread, &EmuThread::reloadPatches);
}

void MainWindow::createRendererSwitchMenu()
{
	static constexpr const GSRendererType renderers[] = {
		GSRendererType::Auto,
#if defined(_WIN32)
		GSRendererType::DX11,
		GSRendererType::DX12,
#elif defined(__APPLE__)
		GSRendererType::Metal,
#endif
#ifdef ENABLE_OPENGL
		GSRendererType::OGL,
#endif
#ifdef ENABLE_VULKAN
		GSRendererType::VK,
#endif
		GSRendererType::SW,
		GSRendererType::Null,
	};
	const GSRendererType current_renderer = static_cast<GSRendererType>(
		Host::GetBaseIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto)));
	for (const GSRendererType renderer : renderers)
	{
		QAction* action = m_ui.menuDebugSwitchRenderer->addAction(
			QString::fromUtf8(Pcsx2Config::GSOptions::GetRendererName(renderer)));
		action->setCheckable(true);
		action->setChecked(current_renderer == renderer);
		connect(action,
			&QAction::triggered, [this, action, renderer] {
				Host::SetBaseIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(renderer));
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();

				// clear all others
				for (QObject* obj : m_ui.menuDebugSwitchRenderer->children())
				{
					if (QAction* act = qobject_cast<QAction*>(obj); act && act != action)
						act->setChecked(false);
				}
			});
	}
}

void MainWindow::recreate()
{
	const bool was_display_created = m_display_created;
	if (was_display_created)
	{
		g_emu_thread->setSurfaceless(true);
		while (m_display_widget || !g_emu_thread->isSurfaceless())
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);

		m_display_created = false;
	}

	// We need to close input sources, because e.g. DInput uses our window handle.
	g_emu_thread->closeInputSources();

	close();
	g_main_window = nullptr;

	MainWindow* new_main_window = new MainWindow();
	pxAssert(g_main_window == new_main_window);
	new_main_window->initialize();
	new_main_window->refreshGameList(false);
	new_main_window->show();
	deleteLater();

	// Recreate log window as well. Then make sure we're still on top.
	LogWindow::updateSettings();
	new_main_window->raise();
	new_main_window->activateWindow();

	// Reload the sources we just closed.
	g_emu_thread->reloadInputSources();

	if (was_display_created)
	{
		g_emu_thread->setSurfaceless(false);
		g_main_window->updateEmulationActions(false, s_vm_valid, Achievements::IsHardcoreModeActive());
		g_main_window->onFullscreenUIStateChange(g_emu_thread->isRunningFullscreenUI());
	}
}

void MainWindow::recreateSettings()
{
	QString current_category;
	if (m_settings_window)
	{
		const bool was_visible = m_settings_window->isVisible();

		current_category = m_settings_window->getCategory();
		m_settings_window->hide();
		m_settings_window->deleteLater();
		m_settings_window = nullptr;

		if (!was_visible)
			return;
	}

	doSettings(current_category.toUtf8().constData());
}

void MainWindow::resetSettings(bool ui)
{
	Host::RequestResetSettings(false, true, false, false, ui);

	if (ui)
	{
		// UI reset includes theme (and eventually language).
		// Just updating the theme here, when there's no change, causes Qt to get very confused..
		// So, we'll just tear down everything and recreate. We'll need to do that for language
		// resets eventaully anyway.
		recreate();
	}

	// g_main_window here for recreate() case above.
	g_main_window->recreateSettings();
}

void MainWindow::destroySubWindows()
{
	if (m_debugger_window)
	{
		m_debugger_window->close();
		m_debugger_window->deleteLater();
		m_debugger_window = nullptr;
	}

	if (m_controller_settings_window)
	{
		m_controller_settings_window->close();
		m_controller_settings_window->deleteLater();
		m_controller_settings_window = nullptr;
	}

	if (m_settings_window)
	{
		m_settings_window->close();
		m_settings_window->deleteLater();
		m_settings_window = nullptr;
	}

	SettingsWindow::closeGamePropertiesDialogs();

	LogWindow::destroy();
}

void MainWindow::onScreenshotActionTriggered()
{
	g_emu_thread->queueSnapshot(0);
}

void MainWindow::onSaveGSDumpActionTriggered()
{
	g_emu_thread->queueSnapshot(1);
}

void MainWindow::onBlockDumpActionToggled(bool checked)
{
	if (!checked)
		return;

	std::string old_directory = Host::GetBaseStringSettingValue("EmuCore", "BlockDumpSaveDirectory", "");
	if (old_directory.empty())
		old_directory = FileSystem::GetWorkingDirectory();

	// prompt for a location to save
	const QString new_dir(QDir::toNativeSeparators(
		QFileDialog::getExistingDirectory(this, tr("Select location to save block dump:"), QString::fromStdString(old_directory))));
	if (new_dir.isEmpty())
	{
		// disable it again
		m_ui.actionSaveBlockDump->setChecked(false);
		return;
	}

	Host::SetBaseStringSettingValue("EmuCore", "BlockDumpSaveDirectory", new_dir.toUtf8().constData());
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();
}

void MainWindow::onShowAdvancedSettingsToggled(bool checked)
{
	if (checked && !Host::GetBaseBoolSettingValue("UI", "AdvancedSettingsWarningShown", false))
	{
		QCheckBox* cb = new QCheckBox(tr("Do not show again"));
		QMessageBox mb(this);
		mb.setWindowTitle(tr("Show Advanced Settings"));
		mb.setText(tr("Changing advanced settings can have unpredictable effects on games, including graphical glitches, lock-ups, and "
					  "even corrupted save files. "
					  "We do not recommend changing advanced settings unless you know what you are doing, and the implications of changing "
					  "each setting.\n\n"
					  "The PCSX2 team will not provide any support for configurations that modify these settings, you are on your own.\n\n"
					  "Are you sure you want to continue?"));
		mb.setIcon(QMessageBox::Warning);
		mb.addButton(QMessageBox::Yes);
		mb.addButton(QMessageBox::No);
		mb.setDefaultButton(QMessageBox::No);
		mb.setCheckBox(cb);

		if (mb.exec() == QMessageBox::No)
		{
			QSignalBlocker sb(m_ui.actionShowAdvancedSettings);
			m_ui.actionShowAdvancedSettings->setChecked(false);
			return;
		}

		if (cb->isChecked())
		{
			Host::SetBaseBoolSettingValue("UI", "AdvancedSettingsWarningShown", true);
			Host::CommitBaseSettingChanges();
		}
	}

	Host::SetBaseBoolSettingValue("UI", "ShowAdvancedSettings", checked);
	Host::CommitBaseSettingChanges();

	updateAdvancedSettingsVisibility();

	// just recreate the entire settings window, it's easier.
	if (m_settings_window)
		recreateSettings();
}

void MainWindow::updateAdvancedSettingsVisibility()
{
	const bool enabled = QtHost::ShouldShowAdvancedSettings();

	m_ui.menuDebug->menuAction()->setVisible(enabled);

	m_ui.actionEnableSystemConsole->setVisible(enabled);
	if (m_ui.actionEnableDebugConsole)
		m_ui.actionEnableDebugConsole->setVisible(enabled);
	m_ui.actionEnableVerboseLogging->setVisible(enabled);
}

void MainWindow::onToolsVideoCaptureToggled(bool checked)
{
	if (!s_vm_valid)
		return;

	// Reset the checked state, we'll get updated by the GS thread.
	QSignalBlocker sb(m_ui.actionToolsVideoCapture);
	m_ui.actionToolsVideoCapture->setChecked(!checked);

	if (!checked)
	{
		g_emu_thread->endCapture();
		return;
	}

	const QString container(QString::fromStdString(
		Host::GetStringSettingValue("EmuCore/GS", "CaptureContainer", Pcsx2Config::GSOptions::DEFAULT_CAPTURE_CONTAINER)));
	const QString filter(tr("%1 Files (*.%2)").arg(container.toUpper()).arg(container));

	QString path(QStringLiteral("%1.%2").arg(QString::fromStdString(GSGetBaseVideoFilename())).arg(container));
	path = QFileDialog::getSaveFileName(this, tr("Video Capture"), path, filter);
	if (path.isEmpty())
		return;

	g_emu_thread->beginCapture(path);
}

void MainWindow::onCaptureStarted(const QString& filename)
{
	if (!s_vm_valid)
		return;

	QSignalBlocker sb(m_ui.actionToolsVideoCapture);
	m_ui.actionToolsVideoCapture->setChecked(true);
}

void MainWindow::onCaptureStopped()
{
	if (!s_vm_valid)
		return;

	QSignalBlocker sb(m_ui.actionToolsVideoCapture);
	m_ui.actionToolsVideoCapture->setChecked(false);
}

void MainWindow::onAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
	auto lock = pauseAndLockVM();

	AchievementLoginDialog dlg(lock.getDialogParent(), reason);
	dlg.exec();
}

void MainWindow::onAchievementsLoginSucceeded(const QString& display_name, quint32 points, quint32 sc_points, quint32 unread_messages)
{
	const QString message =
		tr("RA: Logged in as %1 (%2, %3 softcore). %4 unread messages.").arg(display_name).arg(points).arg(sc_points).arg(unread_messages);
	m_ui.statusBar->showMessage(message);
}

void MainWindow::onAchievementsHardcoreModeChanged(bool enabled)
{
	// disable debugger while hardcore mode is active
	m_ui.actionDebugger->setDisabled(enabled);
	if (enabled)
	{
		if (m_debugger_window)
		{
			m_debugger_window->close();
			m_debugger_window->deleteLater();
			m_debugger_window = nullptr;
		}
	}
}

void MainWindow::onSettingsTriggeredFromToolbar()
{
	if (s_vm_valid)
	{
		m_settings_toolbar_menu->exec(QCursor::pos());
	}
	else
	{
		doSettings();
	}
}

void MainWindow::saveStateToConfig()
{
	if (!isVisible())
		return;

	bool changed = false;

	const QByteArray geometry(saveGeometry());
	const QByteArray geometry_b64(geometry.toBase64());
	const std::string old_geometry_b64(Host::GetBaseStringSettingValue("UI", "MainWindowGeometry"));
	if (old_geometry_b64 != geometry_b64.constData())
	{
		Host::SetBaseStringSettingValue("UI", "MainWindowGeometry", geometry_b64.constData());
		changed = true;
	}

	const QByteArray state(saveState());
	const QByteArray state_b64(state.toBase64());
	const std::string old_state_b64(Host::GetBaseStringSettingValue("UI", "MainWindowState"));
	if (old_state_b64 != state_b64.constData())
	{
		Host::SetBaseStringSettingValue("UI", "MainWindowState", state_b64.constData());
		changed = true;
	}

	if (changed)
		Host::CommitBaseSettingChanges();
}

void MainWindow::restoreStateFromConfig()
{
	{
		const std::string geometry_b64 = Host::GetBaseStringSettingValue("UI", "MainWindowGeometry");
		const QByteArray geometry = QByteArray::fromBase64(QByteArray::fromStdString(geometry_b64));
		if (!geometry.isEmpty())
			restoreGeometry(geometry);
	}

	{
		const std::string state_b64 = Host::GetBaseStringSettingValue("UI", "MainWindowState");
		const QByteArray state = QByteArray::fromBase64(QByteArray::fromStdString(state_b64));
		if (!state.isEmpty())
			restoreState(state);

		{
			QSignalBlocker sb(m_ui.actionViewToolbar);
			m_ui.actionViewToolbar->setChecked(!m_ui.toolBar->isHidden());
		}
		{
			QSignalBlocker sb(m_ui.actionViewStatusBar);
			m_ui.actionViewStatusBar->setChecked(!m_ui.statusBar->isHidden());
		}
	}
}

void MainWindow::updateEmulationActions(bool starting, bool running, bool stopping)
{
	const bool starting_or_running = starting || running;

	m_ui.actionStartFile->setDisabled(starting_or_running || stopping);
	m_ui.actionStartDisc->setDisabled(starting_or_running || stopping);
	m_ui.actionStartBios->setDisabled(starting_or_running || stopping);
	m_ui.actionToolbarStartFile->setDisabled(starting_or_running || stopping);
	m_ui.actionToolbarStartDisc->setDisabled(starting_or_running || stopping);
	m_ui.actionToolbarStartBios->setDisabled(starting_or_running || stopping);
	m_ui.actionStartFullscreenUI->setDisabled(starting_or_running || stopping);
	m_ui.actionToolbarStartFullscreenUI->setDisabled(starting_or_running || stopping);

	m_ui.actionPowerOff->setEnabled(running);
	m_ui.actionPowerOffWithoutSaving->setEnabled(running);
	m_ui.actionReset->setEnabled(running);
	m_ui.actionPause->setEnabled(running);
	m_ui.actionScreenshot->setEnabled(running);
	m_ui.menuChangeDisc->setEnabled(running);
	m_ui.menuSaveState->setEnabled(running);

	m_ui.actionToolbarPowerOff->setEnabled(running);
	m_ui.actionToolbarReset->setEnabled(running);
	m_ui.actionToolbarPause->setEnabled(running);
	m_ui.actionToolbarScreenshot->setEnabled(running);
	m_ui.actionToolbarChangeDisc->setEnabled(running);
	m_ui.actionToolbarSaveState->setEnabled(running);

	m_ui.actionViewGameProperties->setEnabled(running);

	m_ui.actionToolsVideoCapture->setEnabled(running);
	if (!running && m_ui.actionToolsVideoCapture->isChecked())
	{
		QSignalBlocker sb(m_ui.actionToolsVideoCapture);
		m_ui.actionToolsVideoCapture->setChecked(false);
	}

	m_game_list_widget->setDisabled(starting && !running);

	if (!starting && !running)
	{
		{
			QSignalBlocker sb(m_ui.actionPause);
			m_ui.actionPause->setChecked(false);
		}

		{
			QSignalBlocker sb(m_ui.actionToolbarPause);
			m_ui.actionToolbarPause->setChecked(false);
		}
	}

	// scanning needs to be disabled while running
	m_ui.actionScanForNewGames->setDisabled(starting_or_running || stopping);
	m_ui.actionRescanAllGames->setDisabled(starting_or_running || stopping);
}

void MainWindow::updateDisplayRelatedActions(bool has_surface, bool render_to_main, bool fullscreen)
{
	// rendering to main, or switched to gamelist/grid
	m_ui.actionViewSystemDisplay->setEnabled((has_surface && render_to_main) || (!has_surface && MTGS::IsOpen()));
	m_ui.menuWindowSize->setEnabled(has_surface && !fullscreen);
	m_ui.actionFullscreen->setEnabled(has_surface);
	m_ui.actionToolbarFullscreen->setEnabled(has_surface);

	{
		QSignalBlocker blocker(m_ui.actionFullscreen);
		m_ui.actionFullscreen->setChecked(fullscreen);
	}

	{
		QSignalBlocker blocker(m_ui.actionToolbarFullscreen);
		m_ui.actionToolbarFullscreen->setChecked(fullscreen);
	}
}

void MainWindow::updateStatusBarWidgetVisibility()
{
	auto Update = [this](QWidget* widget, bool visible, int stretch) {
		if (widget->isVisible())
		{
			m_ui.statusBar->removeWidget(widget);
			widget->hide();
		}

		if (visible)
		{
			m_ui.statusBar->addPermanentWidget(widget, stretch);
			widget->show();
		}
	};

	Update(m_status_verbose_widget, s_vm_valid, 1);
	Update(m_status_renderer_widget, s_vm_valid, 0);
	Update(m_status_resolution_widget, s_vm_valid, 0);
	Update(m_status_fps_widget, s_vm_valid, 0);
	Update(m_status_vps_widget, s_vm_valid, 0);
}

void MainWindow::updateWindowTitle()
{
	QString suffix(QtHost::GetAppConfigSuffix());
	QString main_title(QtHost::GetAppNameAndVersion() + suffix);
	QString display_title(s_current_title + suffix);

	if (!s_vm_valid || s_current_title.isEmpty())
		display_title = main_title;
	else if (isRenderingToMain())
		main_title = display_title;

	if (windowTitle() != main_title)
		setWindowTitle(main_title);

	if (m_display_widget && !isRenderingToMain())
	{
		QWidget* container = m_display_container ? static_cast<QWidget*>(m_display_container) : static_cast<QWidget*>(m_display_widget);
		if (container->windowTitle() != display_title)
			container->setWindowTitle(display_title);
	}

	if (g_log_window)
		g_log_window->updateWindowTitle();
}

void MainWindow::updateWindowState(bool force_visible)
{
	// Skip all of this when we're closing, since we don't want to make ourselves visible and cancel it.
	if (m_is_closing)
		return;

	const bool hide_window = !isRenderingToMain() && shouldHideMainWindow();
	const bool disable_resize = Host::GetBoolSettingValue("UI", "DisableWindowResize", false);
	const bool has_window = s_vm_valid || m_display_widget;

	// Need to test both valid and display widget because of startup (vm invalid while window is created).
	const bool visible = force_visible || !hide_window || !has_window;
	if (isVisible() != visible)
		setVisible(visible);

	// No point changing realizability if we're not visible.
	const bool resizeable = force_visible || !disable_resize || !has_window;
	if (visible)
		QtUtils::SetWindowResizeable(this, resizeable);

	// Update the display widget too if rendering separately.
	if (m_display_widget && !isRenderingToMain())
		QtUtils::SetWindowResizeable(getDisplayContainer(), resizeable);
}

void MainWindow::setProgressBar(int current, int total)
{
	const int value = (total != 0) ? ((current * 100) / total) : 0;
	if (m_status_progress_widget->value() != value)
		m_status_progress_widget->setValue(value);

	if (m_status_progress_widget->isVisible())
		return;

	m_status_progress_widget->show();
	m_ui.statusBar->addPermanentWidget(m_status_progress_widget);
}

void MainWindow::clearProgressBar()
{
	if (!m_status_progress_widget->isVisible())
		return;

	m_status_progress_widget->hide();
	m_ui.statusBar->removeWidget(m_status_progress_widget);
}

bool MainWindow::isShowingGameList() const
{
	if (s_use_central_widget)
		return (centralWidget() == m_game_list_widget);
	else
		return (m_ui.mainContainer->currentIndex() == 0);
}

bool MainWindow::isRenderingFullscreen() const
{
	if (!MTGS::IsOpen() || !m_display_widget)
		return false;

	return getDisplayContainer()->isFullScreen();
}

bool MainWindow::isRenderingToMain() const
{
	if (s_use_central_widget)
		return (m_display_widget && centralWidget() == m_display_widget);
	else
		return (m_display_widget && m_ui.mainContainer->indexOf(m_display_widget) == 1);
}

bool MainWindow::shouldHideMouseCursor() const
{
	return ((isRenderingFullscreen() && Host::GetBoolSettingValue("UI", "HideMouseCursor", false)) ||
			m_relative_mouse_mode || m_hide_mouse_cursor);
}

bool MainWindow::shouldHideMainWindow() const
{
	// NOTE: We can't use isRenderingToMain() here, because this happens post-fullscreen-switch.
	return Host::GetBoolSettingValue("UI", "HideMainWindowWhenRunning", false) ||
		   (g_emu_thread->shouldRenderToMain() && (isRenderingFullscreen() || m_is_temporarily_windowed)) ||
		   QtHost::InNoGUIMode();
}

bool MainWindow::shouldAbortForMemcardBusy(const VMLock& lock)
{
	if (MemcardBusy::IsBusy() && !GSDumpReplayer::IsReplayingDump())
	{
		const QMessageBox::StandardButton res = QMessageBox::question(
			lock.getDialogParent(),
			tr("WARNING: Memory Card Busy"),
			tr("WARNING: Your memory card is still writing data. Shutting down now will IRREVERSIBLY DESTROY YOUR MEMORY CARD. It is strongly recommended to resume your game and let it finish writing to your memory card.\n\nDo you wish to shutdown anyways and IRREVERSIBLY DESTROY YOUR MEMORY CARD?"));

		if (res != QMessageBox::Yes)
		{
			return true;
		}
	}

	return false;
}

void MainWindow::switchToGameListView()
{
	if (isShowingGameList())
	{
		m_game_list_widget->setFocus();
		return;
	}

	if (m_display_created)
	{
		m_was_paused_on_surface_loss = s_vm_paused;
		if (!s_vm_paused)
			g_emu_thread->setVMPaused(true);

		// switch to surfaceless. we have to wait until the display widget is gone before we swap over.
		g_emu_thread->setSurfaceless(true);
		while (m_display_widget)
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
	}
}

void MainWindow::switchToEmulationView()
{
	if (!m_display_created || !isShowingGameList())
		return;

	// we're no longer surfaceless! this will call back to UpdateDisplay(), which will swap the widget out.
	g_emu_thread->setSurfaceless(false);

	// resume if we weren't paused at switch time
	if (s_vm_paused && !m_was_paused_on_surface_loss)
		g_emu_thread->setVMPaused(false);

	if (m_display_widget)
		m_display_widget->setFocus();
}

void MainWindow::refreshGameList(bool invalidate_cache)
{
	// can't do this while the VM is running because of CDVD
	if (s_vm_valid)
		return;

	m_game_list_widget->refresh(invalidate_cache);
}

void MainWindow::cancelGameListRefresh()
{
	m_game_list_widget->cancelRefresh();
}

void MainWindow::reportError(const QString& title, const QString& message)
{
	QMessageBox::critical(this, title, message);
}

bool MainWindow::confirmMessage(const QString& title, const QString& message)
{
	VMLock lock(pauseAndLockVM());
	return (QMessageBox::question(this, title, message) == QMessageBox::Yes);
}

void MainWindow::runOnUIThread(const std::function<void()>& func)
{
	func();
}

void MainWindow::requestReset()
{
	if (!s_vm_valid)
		return;

	const auto lock = pauseAndLockVM();

	// Check if memcard is busy, deny request if so
	if (shouldAbortForMemcardBusy(lock))
	{
		return;
	}

	g_emu_thread->resetVM();
}

bool MainWindow::requestShutdown(bool allow_confirm, bool allow_save_to_state, bool default_save_to_state)
{
	if (!s_vm_valid)
		return true;

	// If we don't have a crc, we can't save state.
	allow_save_to_state &= (s_current_disc_crc != 0);
	bool save_state = allow_save_to_state && default_save_to_state;
	VMLock lock(pauseAndLockVM());

	// Check if memcard is busy, deny request if so.
	if (shouldAbortForMemcardBusy(lock))
	{
		return false;
	}

	// Only confirm on UI thread because we need to display a msgbox.
	if (!m_is_closing && allow_confirm && !GSDumpReplayer::IsReplayingDump() && Host::GetBoolSettingValue("UI", "ConfirmShutdown", true))
	{
		QMessageBox msgbox(lock.getDialogParent());
		msgbox.setIcon(QMessageBox::Question);
		msgbox.setWindowTitle(tr("Confirm Shutdown"));
		msgbox.setText(tr("Are you sure you want to shut down the virtual machine?"));

		QCheckBox* save_cb = new QCheckBox(tr("Save State For Resume"), &msgbox);
		save_cb->setChecked(save_state);
		save_cb->setEnabled(allow_save_to_state);
		msgbox.setCheckBox(save_cb);
		msgbox.addButton(QMessageBox::Yes);
		msgbox.addButton(QMessageBox::No);
		msgbox.setDefaultButton(QMessageBox::Yes);
		if (msgbox.exec() != QMessageBox::Yes)
			return false;

		save_state = save_cb->isChecked();

		// Don't switch back to fullscreen when we're shutting down anyway.
		lock.cancelResume();
	}

	// This is a little bit annoying. Qt will close everything down if we don't have at least one window visible,
	// but we might not be visible because the user is using render-to-separate and hide. We don't want to always
	// reshow the main window during display updates, because otherwise fullscreen transitions and renderer switches
	// would briefly show and then hide the main window. So instead, we do it on shutdown, here. Except if we're in
	// batch mode, when we're going to exit anyway.
	if (!isRenderingToMain() && isHidden() && !QtHost::InBatchMode() && !g_emu_thread->isRunningFullscreenUI())
		updateWindowState(true);

	// Clear the VM valid state early. That way we can't do anything in the UI if we take a while to shut down.
	if (s_vm_valid)
	{
		s_vm_valid = false;
		updateEmulationActions(false, false, true);
		updateDisplayRelatedActions(false, false, false);
	}

	// Now we can actually shut down the VM.
	g_emu_thread->shutdownVM(save_state);
	return true;
}

void MainWindow::requestExit(bool allow_confirm)
{
	// requestShutdown() clears this flag.
	const bool vm_was_valid = QtHost::IsVMValid();

	// this is block, because otherwise closeEvent() will also prompt
	if (!requestShutdown(allow_confirm, true, EmuConfig.SaveStateOnShutdown))
		return;

	// VM stopped signal won't have fired yet, so queue an exit if we still have one.
	// Otherwise, immediately exit, because there's no VM to exit us later.
	if (vm_was_valid)
		m_is_closing = true;
	else
		QGuiApplication::quit();
}

void MainWindow::checkForSettingChanges()
{
	if (m_display_widget)
		updateDisplayWidgetCursor();

	updateWindowState();

	LogWindow::updateSettings();
}

std::optional<WindowInfo> MainWindow::getWindowInfo()
{
	if (!m_display_widget || isRenderingToMain())
		return QtUtils::GetWindowInfoForWidget(this);
	else if (QWidget* widget = getDisplayContainer())
		return QtUtils::GetWindowInfoForWidget(widget);
	else
		return std::nullopt;
}

void MainWindow::onGameListRefreshProgress(const QString& status, int current, int total)
{
	m_ui.statusBar->showMessage(status);
	setProgressBar(current, total);
}

void MainWindow::onGameListRefreshComplete()
{
	m_ui.statusBar->clearMessage();
	clearProgressBar();
}

void MainWindow::onGameListSelectionChanged()
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = m_game_list_widget->getSelectedEntry();
	if (!entry)
		return;

	m_ui.statusBar->showMessage(QString::fromStdString(entry->path));
}

void MainWindow::onGameListEntryActivated()
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = m_game_list_widget->getSelectedEntry();
	if (!entry)
		return;

	if (s_vm_valid)
	{
		// change disc on double click
		if (!entry->IsDisc())
		{
			QMessageBox::critical(this, tr("Error"), tr("You must select a disc to change discs."));
			return;
		}

		doDiscChange(CDVD_SourceType::Iso, QString::fromStdString(entry->path));
		return;
	}

	// we might still be saving a resume state...
	VMManager::WaitForSaveStateFlush();

	const std::optional<bool> resume =
		promptForResumeState(QString::fromStdString(VMManager::GetSaveStateFileName(entry->serial.c_str(), entry->crc, -1)));
	if (!resume.has_value())
	{
		// cancelled
		return;
	}

	// only resume if the option is enabled, and we have one for this game
	startGameListEntry(entry, resume.value() ? std::optional<s32>(-1) : std::optional<s32>(), std::nullopt);
}

void MainWindow::onGameListEntryContextMenuRequested(const QPoint& point)
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = m_game_list_widget->getSelectedEntry();

	QMenu menu;

	if (entry)
	{
		QAction* action = menu.addAction(tr("Properties..."));
		action->setEnabled(!entry->serial.empty() || entry->type == GameList::EntryType::ELF);
		if (action->isEnabled())
		{
			connect(action, &QAction::triggered, [entry]() {
				SettingsWindow::openGamePropertiesDialog(entry, entry->title,
					(entry->type != GameList::EntryType::ELF) ? entry->serial : std::string(),
					entry->crc);
			});
		}

		//: Refers to the directory where a game is contained.
		action = menu.addAction(tr("Open Containing Directory..."));
		connect(action, &QAction::triggered, [this, entry]() {
			const QFileInfo fi(QString::fromStdString(entry->path));
			QtUtils::OpenURL(this, QUrl::fromLocalFile(fi.absolutePath()));
		});

		action = menu.addAction(tr("Set Cover Image..."));
		connect(action, &QAction::triggered, [this, entry]() { setGameListEntryCoverImage(entry); });

		connect(menu.addAction(tr("Exclude From List")), &QAction::triggered,
			[this, entry]() { getSettingsWindow()->getGameListSettingsWidget()->addExcludedPath(entry->path); });

		connect(menu.addAction(tr("Reset Play Time")), &QAction::triggered, [this, entry]() { clearGameListEntryPlayTime(entry); });

		menu.addSeparator();

		if (!s_vm_valid)
		{
			action = menu.addAction(tr("Default Boot"));
			connect(action, &QAction::triggered, [this, entry]() { startGameListEntry(entry); });

			// Make bold to indicate it's the default choice when double-clicking
			if (!VMManager::HasSaveStateInSlot(entry->serial.c_str(), entry->crc, -1))
				QtUtils::MarkActionAsDefault(action);

			action = menu.addAction(tr("Fast Boot"));
			connect(action, &QAction::triggered, [this, entry]() { startGameListEntry(entry, std::nullopt, true); });

			action = menu.addAction(tr("Full Boot"));
			connect(action, &QAction::triggered, [this, entry]() { startGameListEntry(entry, std::nullopt, false); });

			if (m_ui.menuDebug->menuAction()->isVisible())
			{
				action = menu.addAction(tr("Boot and Debug"));
				connect(action, &QAction::triggered, [this, entry]() {
					DebugInterface::setPauseOnEntry(true);
					startGameListEntry(entry);
					getDebuggerWindow()->show();
				});
			}

			menu.addSeparator();
			populateLoadStateMenu(&menu, QString::fromStdString(entry->path), QString::fromStdString(entry->serial), entry->crc);
		}
		else if (entry->IsDisc())
		{
			action = menu.addAction(tr("Change Disc"));
			connect(action, &QAction::triggered, [this, entry]() {
				g_emu_thread->changeDisc(CDVD_SourceType::Iso, QString::fromStdString(entry->path));
				switchToEmulationView();
			});
			QtUtils::MarkActionAsDefault(action);
		}

		menu.addSeparator();
	}

	connect(menu.addAction(tr("Add Search Directory...")), &QAction::triggered,
		[this]() { getSettingsWindow()->getGameListSettingsWidget()->addSearchDirectory(this); });

	menu.exec(point);
}

void MainWindow::onStartFileActionTriggered()
{
	const QString path(
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Start File"), QString(), tr(OPEN_FILE_FILTER), nullptr)));
	if (path.isEmpty())
		return;

	doStartFile(std::nullopt, path);
}

void MainWindow::onStartDiscActionTriggered()
{
	QString path(getDiscDevicePath(tr("Start Disc")));
	if (path.isEmpty())
		return;

	doStartFile(CDVD_SourceType::Disc, path);
}

void MainWindow::onStartBIOSActionTriggered()
{
	std::shared_ptr<VMBootParameters> params = std::make_shared<VMBootParameters>();
	g_emu_thread->startVM(std::move(params));
}

void MainWindow::onChangeDiscFromFileActionTriggered()
{
	VMLock lock(pauseAndLockVM());
	QString filename =
		QFileDialog::getOpenFileName(lock.getDialogParent(), tr("Select Disc Image"), QString(), tr(DISC_IMAGE_FILTER), nullptr);
	if (filename.isEmpty())
		return;

	g_emu_thread->changeDisc(CDVD_SourceType::Iso, filename);
}

void MainWindow::onChangeDiscFromGameListActionTriggered()
{
	m_was_disc_change_request = true;
	switchToGameListView();
}

void MainWindow::onChangeDiscFromDeviceActionTriggered()
{
	QString path(getDiscDevicePath(tr("Change Disc")));
	if (path.isEmpty())
		return;

	g_emu_thread->changeDisc(CDVD_SourceType::Disc, path);
}

void MainWindow::onRemoveDiscActionTriggered()
{
	g_emu_thread->changeDisc(CDVD_SourceType::NoDisc, QString());
}

void MainWindow::onChangeDiscMenuAboutToShow()
{
	// TODO: This is where we would populate the playlist if there is one.
}

void MainWindow::onChangeDiscMenuAboutToHide()
{
}

void MainWindow::onLoadStateMenuAboutToShow()
{
	m_ui.menuLoadState->clear();
	populateLoadStateMenu(m_ui.menuLoadState, s_current_disc_path, s_current_disc_serial, s_current_disc_crc);
}

void MainWindow::onSaveStateMenuAboutToShow()
{
	m_ui.menuSaveState->clear();
	populateSaveStateMenu(m_ui.menuSaveState, s_current_disc_serial, s_current_disc_crc);
}

void MainWindow::onStartFullscreenUITriggered()
{
	if (m_display_widget)
		g_emu_thread->stopFullscreenUI();
	else
		g_emu_thread->startFullscreenUI(Host::GetBaseBoolSettingValue("UI", "StartFullscreen", false));
}

void MainWindow::onFullscreenUIStateChange(bool running)
{
	m_ui.actionStartFullscreenUI->setText(running ? tr("Stop Big Picture Mode") : tr("Start Big Picture Mode"));
	m_ui.actionToolbarStartFullscreenUI->setText(running ? tr("Exit Big Picture", "In Toolbar") : tr("Big Picture", "In Toolbar"));
}

void MainWindow::onViewToolbarActionToggled(bool checked)
{
	Host::SetBaseBoolSettingValue("UI", "ShowToolbar", checked);
	Host::CommitBaseSettingChanges();
	m_ui.toolBar->setVisible(checked);
}

void MainWindow::onViewLockToolbarActionToggled(bool checked)
{
	Host::SetBaseBoolSettingValue("UI", "LockToolbar", checked);
	Host::CommitBaseSettingChanges();
	m_ui.toolBar->setMovable(!checked);
}

void MainWindow::onViewStatusBarActionToggled(bool checked)
{
	Host::SetBaseBoolSettingValue("UI", "ShowStatusBar", checked);
	Host::CommitBaseSettingChanges();
	m_ui.statusBar->setVisible(checked);
}

void MainWindow::onViewGameListActionTriggered()
{
	switchToGameListView();
	m_game_list_widget->showGameList();
}

void MainWindow::onViewGameGridActionTriggered()
{
	switchToGameListView();
	m_game_list_widget->showGameGrid();
}

void MainWindow::onViewSystemDisplayTriggered()
{
	if (m_display_created)
		switchToEmulationView();
}

void MainWindow::onViewGamePropertiesActionTriggered()
{
	if (!s_vm_valid)
		return;

	// prefer to use a game list entry, if we have one, that way the summary is populated
	if (!s_current_disc_path.isEmpty() || !s_current_elf_override.isEmpty())
	{
		auto lock = GameList::GetLock();
		const QString& path = (s_current_elf_override.isEmpty() ? s_current_disc_path : s_current_elf_override);
		const GameList::Entry* entry = GameList::GetEntryForPath(path.toUtf8().constData());
		if (entry)
		{
			SettingsWindow::openGamePropertiesDialog(
				entry, entry->title, s_current_elf_override.isEmpty() ? entry->serial : std::string(), entry->crc);
			return;
		}
	}

	// open properties for the current running file (isn't in the game list)
	if (s_current_disc_crc == 0)
	{
		QMessageBox::critical(this, tr("Game Properties"), tr("Game properties is unavailable for the current game."));
		return;
	}

	// can't use serial for ELFs, because they might have a disc set
	if (s_current_elf_override.isEmpty())
	{
		SettingsWindow::openGamePropertiesDialog(
			nullptr, s_current_title.toStdString(), s_current_disc_serial.toStdString(), s_current_disc_crc);
	}
	else
	{
		SettingsWindow::openGamePropertiesDialog(
			nullptr, s_current_title.toStdString(), std::string(), s_current_disc_crc);
	}
}

void MainWindow::onGitHubRepositoryActionTriggered()
{
	QtUtils::OpenURL(this, AboutDialog::getGitHubRepositoryUrl());
}

void MainWindow::onSupportForumsActionTriggered()
{
	QtUtils::OpenURL(this, AboutDialog::getSupportForumsUrl());
}

void MainWindow::onDiscordServerActionTriggered()
{
	QtUtils::OpenURL(this, AboutDialog::getDiscordServerUrl());
}

void MainWindow::onAboutActionTriggered()
{
	AboutDialog about(this);
	about.exec();
}

void MainWindow::checkForUpdates(bool display_message, bool force_check)
{
	if (!AutoUpdaterDialog::isSupported())
	{
		if (display_message)
		{
			QMessageBox mbox(this);
			mbox.setWindowTitle(tr("Updater Error"));
			mbox.setTextFormat(Qt::RichText);

			QString message;
#ifdef _WIN32
			message = tr("<p>Sorry, you are trying to update a PCSX2 version which is not an official GitHub release. To "
						 "prevent incompatibilities, the auto-updater is only enabled on official builds.</p>"
						 "<p>To obtain an official build, please download from the link below:</p>"
						 "<p><a href=\"https://pcsx2.net/downloads/\">https://pcsx2.net/downloads/</a></p>");
#else
			message = tr("Automatic updating is not supported on the current platform.");
#endif

			mbox.setText(message);
			mbox.setIcon(QMessageBox::Critical);
			mbox.exec();
		}

		return;
	}

	if (m_auto_updater_dialog)
		return;

	if (force_check)
	{
		// Wipe out the last version, that way it displays the update if we've previously skipped it.
		Host::RemoveBaseSettingValue("AutoUpdater", "LastVersion");
		Host::CommitBaseSettingChanges();
	}

	m_auto_updater_dialog = new AutoUpdaterDialog(this);
	connect(m_auto_updater_dialog, &AutoUpdaterDialog::updateCheckCompleted, this, &MainWindow::onUpdateCheckComplete);
	m_auto_updater_dialog->queueUpdateCheck(display_message);
}

void MainWindow::onUpdateCheckComplete()
{
	if (!m_auto_updater_dialog)
		return;

	m_auto_updater_dialog->deleteLater();
	m_auto_updater_dialog = nullptr;
}

void MainWindow::startupUpdateCheck()
{
	if (!Host::GetBaseBoolSettingValue("AutoUpdater", "CheckAtStartup", true))
		return;

	checkForUpdates(false, false);
}

void MainWindow::onToolsOpenDataDirectoryTriggered()
{
	const QString path(QString::fromStdString(EmuFolders::DataRoot));
	QtUtils::OpenURL(this, QUrl::fromLocalFile(path));
}

void MainWindow::onToolsCoverDownloaderTriggered()
{
	// This can be invoked via big picture, so exit fullscreen.
	VMLock lock(pauseAndLockVM());
	CoverDownloadDialog dlg(lock.getDialogParent());
	connect(&dlg, &CoverDownloadDialog::coverRefreshRequested, m_game_list_widget, &GameListWidget::refreshGridCovers);
	dlg.exec();
}

void MainWindow::onToolsEditCheatsPatchesTriggered(bool cheats)
{
	if (s_current_disc_serial.isEmpty() || s_current_running_crc == 0)
		return;

	const std::string path = Patch::GetPnachFilename(s_current_disc_serial.toStdString(), s_current_running_crc, cheats);
	if (!FileSystem::FileExists(path.c_str()))
	{
		if (QMessageBox::question(this, tr("Confirm File Creation"),
				tr("The pnach file '%1' does not currently exist. Do you want to create it?")
					.arg(QtUtils::StringViewToQString(Path::GetFileName(path))),
				QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}

		if (!FileSystem::WriteStringToFile(path.c_str(), std::string_view()))
		{
			QMessageBox::critical(this, tr("Error"), tr("Failed to create '%1'.").arg(QString::fromStdString(path)));
			return;
		}
	}

	QtUtils::OpenURL(this, QUrl::fromLocalFile(QString::fromStdString(path)));
}

void MainWindow::onCreateMemoryCardOpenRequested()
{
	// This can be invoked via big picture, so exit fullscreen.
	VMLock lock(pauseAndLockVM());
	MemoryCardCreateDialog dlg(lock.getDialogParent());
	dlg.exec();
}

void MainWindow::updateTheme()
{
	QtHost::UpdateApplicationTheme();
	reloadThemeSpecificImages();
}

void MainWindow::reloadThemeSpecificImages()
{
	m_game_list_widget->reloadThemeSpecificImages();
}

void MainWindow::updateLanguage()
{
	// Remove the settings window, so it doesn't mess with any popups that happen (e.g. font download).
	destroySubWindows();

	QtHost::InstallTranslator(this);
	recreate();
}

void MainWindow::onInputRecNewActionTriggered()
{
	const bool wasPaused = s_vm_paused;
	const bool wasRunning = s_vm_valid;
	if (wasRunning && !wasPaused)
	{
		g_emu_thread->setVMPaused(true);
	}

	NewInputRecordingDlg dlg(this);
	const auto result = dlg.exec();

	if (result == QDialog::Accepted)
	{
		Host::RunOnCPUThread(
			[&, filePath = dlg.getFilePath(), fromSavestate = dlg.getInputRecType() == InputRecording::Type::FROM_SAVESTATE,
				authorName = dlg.getAuthorName()]() {
				if (g_InputRecording.create(filePath, fromSavestate, authorName))
				{
					QtHost::RunOnUIThread([&]() {
						m_ui.actionInputRecNew->setEnabled(false);
						m_ui.actionInputRecStop->setEnabled(true);
						m_ui.actionReset->setEnabled(!g_InputRecording.isTypeSavestate());
						m_ui.actionToolbarReset->setEnabled(!g_InputRecording.isTypeSavestate());
					});
				}
			});
	}

	if (wasRunning && !wasPaused)
	{
		g_emu_thread->setVMPaused(false);
	}
}

void MainWindow::onInputRecPlayActionTriggered()
{
	const bool wasPaused = s_vm_paused;

	if (!wasPaused)
	{
		g_emu_thread->setVMPaused(true);
	}

	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setWindowTitle("Select a File");
	dialog.setNameFilter(tr("Input Recording Files (*.p2m2)"));
	QStringList fileNames;
	if (dialog.exec())
	{
		fileNames = dialog.selectedFiles();
	}
	else
	{
		if (!wasPaused)
		{
			g_emu_thread->setVMPaused(false);
			return;
		}
	}

	if (fileNames.length() > 0)
	{
		if (g_InputRecording.isActive())
		{
			Host::RunOnCPUThread([]() { g_InputRecording.stop(); });
			m_ui.actionInputRecStop->setEnabled(false);
		}
		Host::RunOnCPUThread([&, filename = fileNames.first().toStdString()]() {
			if (g_InputRecording.play(filename))
			{
				QtHost::RunOnUIThread([&]() {
					m_ui.actionInputRecNew->setEnabled(false);
					m_ui.actionInputRecStop->setEnabled(true);
					m_ui.actionReset->setEnabled(!g_InputRecording.isTypeSavestate());
					m_ui.actionToolbarReset->setEnabled(!g_InputRecording.isTypeSavestate());
				});
			}
		});
	}
}

void MainWindow::onInputRecStopActionTriggered()
{
	if (g_InputRecording.isActive())
	{
		Host::RunOnCPUThread([&]() {
			g_InputRecording.stop();
			QtHost::RunOnUIThread([&]() {
				m_ui.actionInputRecNew->setEnabled(true);
				m_ui.actionInputRecStop->setEnabled(false);
				m_ui.actionReset->setEnabled(true);
				m_ui.actionToolbarReset->setEnabled(true);
			});
		});
	}
}

void MainWindow::onInputRecOpenSettingsTriggered()
{
	// TODO - Vaser - Implement
}

InputRecordingViewer* MainWindow::getInputRecordingViewer()
{
	if (!m_input_recording_viewer)
	{
		m_input_recording_viewer = new InputRecordingViewer(this);
	}

	return m_input_recording_viewer;
}

void MainWindow::updateInputRecordingActions(bool started)
{
	m_ui.actionInputRecNew->setEnabled(started);
	m_ui.actionInputRecPlay->setEnabled(started);
}

void MainWindow::onInputRecOpenViewer()
{
	InputRecordingViewer* viewer = getInputRecordingViewer();
	if (!viewer->isVisible())
	{
		viewer->show();
	}
}

void MainWindow::onVMStarting()
{
	s_vm_valid = true;
	updateEmulationActions(true, false, false);
	updateWindowTitle();
}

void MainWindow::onVMStarted()
{
	s_vm_valid = true;
	m_was_disc_change_request = false;
	updateEmulationActions(true, true, false);
	updateGameDependentActions();
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
	updateInputRecordingActions(true);
}

void MainWindow::onVMPaused()
{
	// update UI
	{
		QSignalBlocker sb(m_ui.actionPause);
		m_ui.actionPause->setChecked(true);
	}
	{
		QSignalBlocker sb(m_ui.actionToolbarPause);
		m_ui.actionToolbarPause->setChecked(true);
	}

	s_vm_paused = true;
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
	m_last_fps_status = m_status_verbose_widget->text();
	m_status_verbose_widget->setText(tr("Paused"));
	if (m_display_widget)
		updateDisplayWidgetCursor();
}

void MainWindow::onVMResumed()
{
	// update UI
	{
		QSignalBlocker sb(m_ui.actionPause);
		m_ui.actionPause->setChecked(false);
	}
	{
		QSignalBlocker sb(m_ui.actionToolbarPause);
		m_ui.actionToolbarPause->setChecked(false);
	}

	s_vm_paused = false;
	m_was_disc_change_request = false;
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
	m_status_verbose_widget->setText(m_last_fps_status);
	m_last_fps_status = QString();
	if (m_display_widget)
	{
		updateDisplayWidgetCursor();
		m_display_widget->setFocus();
	}
}

void MainWindow::onVMStopped()
{
	s_vm_valid = false;
	s_vm_paused = false;
	m_last_fps_status = QString();
	updateEmulationActions(false, false, false);
	updateGameDependentActions();
	updateWindowTitle();
	updateWindowState();
	updateStatusBarWidgetVisibility();
	updateInputRecordingActions(false);

	// If we're closing or in batch mode, quit the whole application now.
	if (m_is_closing || QtHost::InBatchMode())
	{
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
		QCoreApplication::quit();
		return;
	}

	if (m_display_widget)
		updateDisplayWidgetCursor();
	else
		switchToGameListView();

	// reload played time
	if (m_game_list_widget->isShowingGameList())
		m_game_list_widget->refresh(false);
}

void MainWindow::onGameChanged(const QString& title, const QString& elf_override, const QString& disc_path,
	const QString& serial, quint32 disc_crc, quint32 crc)
{
	s_current_title = title;
	s_current_elf_override = elf_override;
	s_current_disc_path = disc_path;
	s_current_disc_serial = serial;
	s_current_disc_crc = disc_crc;
	s_current_running_crc = crc;
	updateWindowTitle();
	updateGameDependentActions();
}

void MainWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	// This is a bit silly, but for some reason resizing *before* the window is shown
	// gives the incorrect sizes for columns, if you set the style before setting up
	// the rest of the window... so, instead, let's just force it to be resized on show.
	if (isShowingGameList())
		m_game_list_widget->resizeTableViewColumnsToFit();

#ifdef ENABLE_RAINTEGRATION
	if (Achievements::IsUsingRAIntegration())
		Achievements::RAIntegration::MainWindowChanged((void*)winId());
#endif
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	// If there's no VM, we can just exit as normal.
	if (!s_vm_valid || !m_display_created)
	{
		saveStateToConfig();
		if (m_display_created)
			g_emu_thread->stopFullscreenUI();
		destroySubWindows();
		QMainWindow::closeEvent(event);
		return;
	}

	// But if there is, we have to cancel the action, regardless of whether we ended exiting
	// or not. The window still needs to be visible while GS is shutting down.
	event->ignore();

	// Exit cancelled?
	if (!requestShutdown(true, true, EmuConfig.SaveStateOnShutdown))
		return;

	// Application will be exited in VM stopped handler.
	m_is_closing = true;
}

void MainWindow::changeEvent(QEvent* event)
{
	QMainWindow::changeEvent(event);

	if (event->type() == QEvent::StyleChange)
	{
		QtHost::SetIconThemeFromStyle();
		reloadThemeSpecificImages();
	}
}

static QString getFilenameFromMimeData(const QMimeData* md)
{
	QString filename;
	if (md->hasUrls())
	{
		// only one url accepted
		const QList<QUrl> urls(md->urls());
		if (urls.size() == 1)
			filename = QDir::toNativeSeparators(urls.front().toLocalFile());
	}

	return filename;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	const std::string filename(getFilenameFromMimeData(event->mimeData()).toStdString());

	// allow save states being dragged in
	if (!VMManager::IsLoadableFileName(filename) && !VMManager::IsSaveStateFileName(filename))
		return;

	event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
	const auto mcLock = pauseAndLockVM();

	// Check if memcard is busy, deny request if so
	if (shouldAbortForMemcardBusy(mcLock))
	{
		return;
	}

	const QString filename(getFilenameFromMimeData(event->mimeData()));
	const std::string filename_str(filename.toStdString());
	if (VMManager::IsSaveStateFileName(filename_str))
	{
		event->acceptProposedAction();

		// can't load a save state without a current VM
		if (s_vm_valid)
			g_emu_thread->loadState(filename);
		else
			QMessageBox::critical(this, tr("Load State Failed"), tr("Cannot load a save state without a running VM."));

		return;
	}

	if (!VMManager::IsLoadableFileName(filename_str))
		return;

	// if we're already running, do a disc change, otherwise start
	if (!s_vm_valid)
	{
		event->acceptProposedAction();
		doStartFile(std::nullopt, filename);
		return;
	}

	if (VMManager::IsDiscFileName(filename_str) || VMManager::IsBlockDumpFileName(filename_str))
	{
		event->acceptProposedAction();
		doDiscChange(CDVD_SourceType::Iso, filename);
	}
	else if (VMManager::IsElfFileName(filename_str))
	{
		const auto lock = pauseAndLockVM();

		event->acceptProposedAction();

		if (QMessageBox::question(this, tr("Confirm Reset"),
				tr("The new ELF cannot be loaded without resetting the virtual machine. Do you want to reset the virtual machine now?")) !=
			QMessageBox::Yes)
		{
			return;
		}

		g_emu_thread->setELFOverride(filename);
		switchToEmulationView();
	}
	else if (VMManager::IsGSDumpFileName(filename_str))
	{
		event->acceptProposedAction();

		if (!GSDumpReplayer::IsReplayingDump())
		{
			QMessageBox::critical(this, tr("Error"), tr("Cannot change from game to GS dump without shutting down first."));
			return;
		}

		g_emu_thread->changeGSDump(filename);
		switchToEmulationView();
	}
}

void MainWindow::moveEvent(QMoveEvent* event)
{
	QMainWindow::moveEvent(event);

	if (g_log_window && g_log_window->isAttachedToMainWindow())
		g_log_window->reattachToMainWindow();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
	QMainWindow::resizeEvent(event);

	if (g_log_window && g_log_window->isAttachedToMainWindow())
		g_log_window->reattachToMainWindow();
}

void MainWindow::registerForDeviceNotifications()
{
#ifdef _WIN32
	// We use these notifications to detect when a controller is connected or disconnected.
	DEV_BROADCAST_DEVICEINTERFACE_W filter = {sizeof(DEV_BROADCAST_DEVICEINTERFACE_W), DBT_DEVTYP_DEVICEINTERFACE};
	m_device_notification_handle =
		RegisterDeviceNotificationW((HANDLE)winId(), &filter, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
#endif
}

void MainWindow::unregisterForDeviceNotifications()
{
#ifdef _WIN32
	if (!m_device_notification_handle)
		return;

	UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(m_device_notification_handle));
	m_device_notification_handle = nullptr;
#endif
}

#ifdef _WIN32

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
	static constexpr const char win_type[] = "windows_generic_MSG";
	if (eventType == QByteArray(win_type, sizeof(win_type) - 1))
	{
		const MSG* msg = static_cast<const MSG*>(message);
		if (msg->message == WM_DEVICECHANGE && msg->wParam == DBT_DEVNODES_CHANGED)
		{
			g_emu_thread->reloadInputDevices();
			*result = 1;
			return true;
		}
	}

	return QMainWindow::nativeEvent(eventType, message, result);
}

#endif

std::optional<WindowInfo> MainWindow::acquireRenderWindow(bool recreate_window, bool fullscreen, bool render_to_main, bool surfaceless)
{
	DevCon.WriteLn("acquireRenderWindow() recreate=%s fullscreen=%s render_to_main=%s surfaceless=%s", recreate_window ? "true" : "false",
		fullscreen ? "true" : "false", render_to_main ? "true" : "false", surfaceless ? "true" : "false");

	QWidget* container = m_display_container ? static_cast<QWidget*>(m_display_container) : static_cast<QWidget*>(m_display_widget);
	const bool is_fullscreen = isRenderingFullscreen();
	const bool is_rendering_to_main = isRenderingToMain();
	const bool changing_surfaceless = (!m_display_widget != surfaceless);
	if (m_display_created && !recreate_window && fullscreen == is_fullscreen && is_rendering_to_main == render_to_main &&
		!changing_surfaceless)
	{
		return m_display_widget ? m_display_widget->getWindowInfo() : WindowInfo();
	}

	// Skip recreating the surface if we're just transitioning between fullscreen and windowed with render-to-main off.
	// .. except on Wayland, where everything tends to break if you don't recreate.
	const bool has_container = (m_display_container != nullptr);
	const bool needs_container = DisplayContainer::isNeeded(fullscreen, render_to_main);
	if (m_display_created && !recreate_window && !is_rendering_to_main && !render_to_main && has_container == needs_container &&
		!needs_container && !changing_surfaceless)
	{
		DevCon.WriteLn("Toggling to %s without recreating surface", (fullscreen ? "fullscreen" : "windowed"));

		// since we don't destroy the display widget, we need to save it here
		if (!is_fullscreen && !is_rendering_to_main)
			saveDisplayWindowGeometryToConfig();

		if (fullscreen)
		{
			container->showFullScreen();
		}
		else
		{
			if (m_is_temporarily_windowed && g_emu_thread->shouldRenderToMain())
				container->setGeometry(geometry());
			else
				restoreDisplayWindowGeometryFromConfig();
			container->showNormal();
		}

		updateDisplayWidgetCursor();
		m_display_widget->setFocus();
		updateWindowState();

		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		return m_display_widget->getWindowInfo();
	}

	destroyDisplayWidget(surfaceless);
	m_display_created = true;

	// if we're going to surfaceless, we're done here
	if (surfaceless)
		return WindowInfo();

	createDisplayWidget(fullscreen, render_to_main);

	std::optional<WindowInfo> wi = m_display_widget->getWindowInfo();
	if (!wi.has_value())
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to get window info from widget"));
		destroyDisplayWidget(true);
		return std::nullopt;
	}

	g_emu_thread->connectDisplaySignals(m_display_widget);

	updateWindowTitle();
	updateWindowState();

	updateDisplayWidgetCursor();
	m_display_widget->setFocus();

	return wi;
}

void MainWindow::createDisplayWidget(bool fullscreen, bool render_to_main)
{
	// If we're rendering to main and were hidden (e.g. coming back from fullscreen),
	// make sure we're visible before trying to add ourselves. Otherwise Wayland breaks.
	if (!fullscreen && render_to_main && !isVisible())
	{
		setVisible(true);
		QGuiApplication::sync();
	}

	QWidget* container;
	if (DisplayContainer::isNeeded(fullscreen, render_to_main))
	{
		m_display_container = new DisplayContainer();
		m_display_widget = new DisplayWidget(m_display_container);
		m_display_container->setDisplayWidget(m_display_widget);
		container = m_display_container;
	}
	else
	{
		m_display_widget = new DisplayWidget((!fullscreen && render_to_main) ? getContentParent() : nullptr);
		container = m_display_widget;
	}

	if (fullscreen || !render_to_main)
	{
		container->setWindowTitle(windowTitle());
		container->setWindowIcon(windowIcon());
	}

	if (fullscreen)
	{
		// Don't risk doing this on Wayland, it really doesn't like window state changes,
		// and positioning has no effect anyway.
		if (!s_use_central_widget)
		{
			if (isVisible() && g_emu_thread->shouldRenderToMain())
				container->move(pos());
			else
				restoreDisplayWindowGeometryFromConfig();
		}

		container->showFullScreen();
	}
	else if (!render_to_main)
	{
		if (m_is_temporarily_windowed && g_emu_thread->shouldRenderToMain())
			container->setGeometry(geometry());
		else
			restoreDisplayWindowGeometryFromConfig();
		container->showNormal();
	}
	else if (s_use_central_widget)
	{
		m_game_list_widget->setVisible(false);
		takeCentralWidget();
		m_game_list_widget->setParent(this); // takeCentralWidget() removes parent
		setCentralWidget(m_display_widget);
		m_display_widget->setFocus();
		update();
	}
	else
	{
		pxAssertRel(m_ui.mainContainer->count() == 1, "Has no display widget");
		m_ui.mainContainer->addWidget(container);
		m_ui.mainContainer->setCurrentIndex(1);
	}

	updateDisplayRelatedActions(true, render_to_main, fullscreen);

	// We need the surface visible.
	QGuiApplication::sync();
}

void MainWindow::displayResizeRequested(qint32 width, qint32 height)
{
	if (!m_display_widget)
		return;

	// unapply the pixel scaling factor for hidpi
	const float dpr = devicePixelRatioF();
	width = static_cast<qint32>(std::max(static_cast<int>(std::lroundf(static_cast<float>(width) / dpr)), 1));
	height = static_cast<qint32>(std::max(static_cast<int>(std::lroundf(static_cast<float>(height) / dpr)), 1));

	if (m_display_container || !m_display_widget->parent())
	{
		// no parent - rendering to separate window. easy.
		QtUtils::ResizePotentiallyFixedSizeWindow(getDisplayContainer(), width, height);
		return;
	}

	// we are rendering to the main window. we have to add in the extra height from the toolbar/status bar.
	const s32 extra_height = this->height() - m_display_widget->height();
	QtUtils::ResizePotentiallyFixedSizeWindow(this, width, height + extra_height);
}

void MainWindow::mouseModeRequested(bool relative_mode, bool hide_cursor)
{
	if (m_relative_mouse_mode == relative_mode && m_hide_mouse_cursor == hide_cursor)
		return;

	m_relative_mouse_mode = relative_mode;
	m_hide_mouse_cursor = hide_cursor;
	if (m_display_widget && !s_vm_paused)
		updateDisplayWidgetCursor();
}

void MainWindow::releaseRenderWindow()
{
	// Now we can safely destroy the display window.
	destroyDisplayWidget(true);
	m_display_created = false;

	m_ui.actionViewSystemDisplay->setEnabled(false);
	m_ui.actionFullscreen->setEnabled(false);
}

void MainWindow::destroyDisplayWidget(bool show_game_list)
{
	if (!m_display_widget)
		return;

	if (!isRenderingFullscreen() && !isRenderingToMain())
		saveDisplayWindowGeometryToConfig();

	if (m_display_container)
		m_display_container->removeDisplayWidget();

	if (isRenderingToMain())
	{
		if (s_use_central_widget)
		{
			pxAssertRel(centralWidget() == m_display_widget, "Display widget is currently central");
			takeCentralWidget();
			if (show_game_list)
			{
				m_game_list_widget->setVisible(true);
				setCentralWidget(m_game_list_widget);
				m_game_list_widget->resizeTableViewColumnsToFit();
			}
		}
		else
		{
			pxAssertRel(m_ui.mainContainer->indexOf(m_display_widget) == 1, "Display widget in stack");
			m_ui.mainContainer->removeWidget(m_display_widget);
			if (show_game_list)
			{
				m_ui.mainContainer->setCurrentIndex(0);
				m_game_list_widget->resizeTableViewColumnsToFit();
			}
		}
	}

	if (m_display_widget)
	{
		m_display_widget->destroy();
		m_display_widget = nullptr;
	}

	if (m_display_container)
	{
		m_display_container->deleteLater();
		m_display_container = nullptr;
	}

	updateDisplayRelatedActions(false, false, false);
}

void MainWindow::updateDisplayWidgetCursor()
{
	pxAssertRel(m_display_widget, "Should have a display widget");
	m_display_widget->updateRelativeMode(s_vm_valid && !s_vm_paused && m_relative_mouse_mode);
	m_display_widget->updateCursor(s_vm_valid && !s_vm_paused && shouldHideMouseCursor());
}

void MainWindow::focusDisplayWidget()
{
	if (!m_display_widget || centralWidget() != m_display_widget)
		return;

	m_display_widget->setFocus();
}

QWidget* MainWindow::getDisplayContainer() const
{
	return (m_display_container ? static_cast<QWidget*>(m_display_container) : static_cast<QWidget*>(m_display_widget));
}

void MainWindow::saveDisplayWindowGeometryToConfig()
{
	QWidget* container = getDisplayContainer();
	if (container->windowState() & Qt::WindowFullScreen)
	{
		// if we somehow ended up here, don't save the fullscreen state to the config
		return;
	}

	const QByteArray geometry = getDisplayContainer()->saveGeometry();
	const QByteArray geometry_b64 = geometry.toBase64();
	const std::string old_geometry_b64 = Host::GetBaseStringSettingValue("UI", "DisplayWindowGeometry");
	if (old_geometry_b64 != geometry_b64.constData())
	{
		Host::SetBaseStringSettingValue("UI", "DisplayWindowGeometry", geometry_b64.constData());
		Host::CommitBaseSettingChanges();
	}
}

void MainWindow::restoreDisplayWindowGeometryFromConfig()
{
	const std::string geometry_b64 = Host::GetBaseStringSettingValue("UI", "DisplayWindowGeometry");
	const QByteArray geometry = QByteArray::fromBase64(QByteArray::fromStdString(geometry_b64));
	QWidget* container = getDisplayContainer();
	if (!geometry.isEmpty())
	{
		container->restoreGeometry(geometry);

		// make sure we're not loading a dodgy config which had fullscreen set...
		container->setWindowState(container->windowState() & ~(Qt::WindowFullScreen | Qt::WindowActive));
	}
	else
	{
		// default size
		container->resize(640, 480);
	}
}

SettingsWindow* MainWindow::getSettingsWindow()
{
	if (!m_settings_window)
	{
		m_settings_window = new SettingsWindow();
		connect(m_settings_window->getInterfaceSettingsWidget(), &InterfaceSettingsWidget::themeChanged, this, &MainWindow::updateTheme);
		connect(m_settings_window->getInterfaceSettingsWidget(), &InterfaceSettingsWidget::languageChanged, this, [this]() {
			// reopen settings dialog after it applies
			updateLanguage();
			// If you doSettings now, on macOS, the window will somehow end up underneath the main window that was created above
			// Delay it slightly...
			QtHost::RunOnUIThread([] {
				g_main_window->doSettings("Interface");
			});
		});
		connect(m_settings_window->getGameListSettingsWidget(), &GameListSettingsWidget::preferEnglishGameListChanged, this, [] {
			g_main_window->m_game_list_widget->refreshGridCovers();
		});
	}

	return m_settings_window;
}

void MainWindow::doSettings(const char* category /* = nullptr */)
{
	SettingsWindow* dlg = getSettingsWindow();
	if (!dlg->isVisible())
	{
		dlg->show();
	}
	else
	{
		dlg->raise();
		dlg->activateWindow();
		dlg->setFocus();
	}

	if (category)
		dlg->setCategory(category);
}

DebuggerWindow* MainWindow::getDebuggerWindow()
{
	if (!m_debugger_window)
		// Don't pass us (this) as the parent, otherwise the window is always on top of the mainwindow (on windows at least)
		m_debugger_window = new DebuggerWindow(nullptr);

	return m_debugger_window;
}

void MainWindow::openDebugger()
{
	DebuggerWindow* dwnd = getDebuggerWindow();
	dwnd->isVisible() ? dwnd->hide() : dwnd->show();
}

void MainWindow::doControllerSettings(ControllerSettingsWindow::Category category)
{
	if (!m_controller_settings_window)
		m_controller_settings_window = new ControllerSettingsWindow();

	if (!m_controller_settings_window->isVisible())
	{
		m_controller_settings_window->show();
	}
	else
	{
		m_controller_settings_window->raise();
		m_controller_settings_window->activateWindow();
		m_controller_settings_window->setFocus();
	}


	if (category != ControllerSettingsWindow::Category::Count)
		m_controller_settings_window->setCategory(category);
}

QString MainWindow::getDiscDevicePath(const QString& title)
{
	QString ret;

	const std::vector<std::string> devices(GetOpticalDriveList());
	if (devices.empty())
	{
		QMessageBox::critical(this, title,
			tr("Could not find any CD/DVD-ROM devices. Please ensure you have a drive connected and "
			   "sufficient permissions to access it."));
		return ret;
	}

	// if there's only one, select it automatically
	if (devices.size() == 1)
	{
		ret = QString::fromStdString(devices.front());
		return ret;
	}

	QStringList input_options;
	for (const std::string& name : devices)
		input_options.append(QString::fromStdString(name));

	QInputDialog input_dialog(this);
	input_dialog.setWindowTitle(title);
	input_dialog.setLabelText(tr("Select disc drive:"));
	input_dialog.setInputMode(QInputDialog::TextInput);
	input_dialog.setOptions(QInputDialog::UseListViewForComboBoxItems);
	input_dialog.setComboBoxEditable(false);
	input_dialog.setComboBoxItems(std::move(input_options));
	if (input_dialog.exec() == 0)
		return ret;

	ret = input_dialog.textValue();
	return ret;
}

void MainWindow::startGameListEntry(const GameList::Entry* entry, std::optional<s32> save_slot, std::optional<bool> fast_boot)
{
	std::shared_ptr<VMBootParameters> params = std::make_shared<VMBootParameters>();
	params->fast_boot = fast_boot;

	GameList::FillBootParametersForEntry(params.get(), entry);

	if (save_slot.has_value() && !entry->serial.empty())
	{
		std::string state_filename = VMManager::GetSaveStateFileName(entry->serial.c_str(), entry->crc, save_slot.value());
		if (!FileSystem::FileExists(state_filename.c_str()))
		{
			QMessageBox::critical(this, tr("Error"), tr("This save state does not exist."));
			return;
		}

		params->save_state = std::move(state_filename);
	}

	g_emu_thread->startVM(std::move(params));
}

void MainWindow::setGameListEntryCoverImage(const GameList::Entry* entry)
{
	const QString filename = QDir::toNativeSeparators(
		QFileDialog::getOpenFileName(this, tr("Select Cover Image"), QString(), tr("All Cover Image Types (*.jpg *.jpeg *.png *.webp)")));
	if (filename.isEmpty())
		return;

	const QString old_filename = QString::fromStdString(GameList::GetCoverImagePathForEntry(entry));
	const QString new_filename = QString::fromStdString(GameList::GetNewCoverImagePathForEntry(entry, filename.toUtf8().constData()));
	if (new_filename.isEmpty())
		return;

	if (!old_filename.isEmpty())
	{
		if (QFileInfo(old_filename) == QFileInfo(filename))
		{
			QMessageBox::critical(this, tr("Copy Error"), tr("You must select a different file to the current cover image."));
			return;
		}

		if (QMessageBox::question(this, tr("Cover Already Exists"),
				tr("A cover image for this game already exists, do you wish to replace it?"), QMessageBox::Yes,
				QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}
	}

	if (QFile::exists(new_filename) && !QFile::remove(new_filename))
	{
		QMessageBox::critical(this, tr("Copy Error"), tr("Failed to remove existing cover '%1'").arg(new_filename));
		return;
	}

	if (!QFile::copy(filename, new_filename))
	{
		QMessageBox::critical(this, tr("Copy Error"), tr("Failed to copy '%1' to '%2'").arg(filename).arg(new_filename));
		return;
	}

	if (!old_filename.isEmpty() && old_filename != new_filename && !QFile::remove(old_filename))
	{
		QMessageBox::critical(this, tr("Copy Error"), tr("Failed to remove '%1'").arg(old_filename));
		return;
	}

	m_game_list_widget->refreshGridCovers();
}

void MainWindow::clearGameListEntryPlayTime(const GameList::Entry* entry)
{
	if (QMessageBox::question(this, tr("Confirm Reset"),
			tr("Are you sure you want to reset the play time for '%1'?\n\nThis action cannot be undone.")
				.arg(QString::fromStdString(entry->title))) != QMessageBox::Yes)
	{
		return;
	}

	GameList::ClearPlayedTimeForSerial(entry->serial);
	m_game_list_widget->refresh(false);
}

std::optional<bool> MainWindow::promptForResumeState(const QString& save_state_path)
{
	if (save_state_path.isEmpty())
		return false;

	QFileInfo fi(save_state_path);
	if (!fi.exists())
		return false;

	QMessageBox msgbox(this);
	msgbox.setIcon(QMessageBox::Question);
	msgbox.setWindowTitle(tr("Load Resume State"));
	msgbox.setText(
		tr("A resume save state was found for this game, saved at:\n\n%1.\n\nDo you want to load this state, or start from a fresh boot?")
			.arg(fi.lastModified().toLocalTime().toString()));

	QPushButton* load = msgbox.addButton(tr("Load State"), QMessageBox::AcceptRole);
	QPushButton* boot = msgbox.addButton(tr("Fresh Boot"), QMessageBox::RejectRole);
	QPushButton* delboot = msgbox.addButton(tr("Delete And Boot"), QMessageBox::RejectRole);
	msgbox.addButton(QMessageBox::Cancel);
	msgbox.setDefaultButton(load);
	msgbox.exec();

	QAbstractButton* clicked = msgbox.clickedButton();
	if (load == clicked)
	{
		return true;
	}
	else if (boot == clicked)
	{
		return false;
	}
	else if (delboot == clicked)
	{
		if (!QFile::remove(save_state_path))
			QMessageBox::critical(this, tr("Error"), tr("Failed to delete save state file '%1'.").arg(save_state_path));

		return false;
	}

	return std::nullopt;
}

void MainWindow::loadSaveStateSlot(s32 slot)
{
	if (s_vm_valid)
	{
		// easy when we're running
		g_emu_thread->loadStateFromSlot(slot);
		return;
	}
	else
	{
		// we're not currently running, therefore we must've right clicked in the game list
		const GameList::Entry* entry = m_game_list_widget->getSelectedEntry();
		if (!entry)
			return;

		startGameListEntry(entry, slot, std::nullopt);
	}
}

void MainWindow::loadSaveStateFile(const QString& filename, const QString& state_filename)
{
	if (s_vm_valid)
	{
		if (!filename.isEmpty() && s_current_disc_path != filename)
			g_emu_thread->changeDisc(CDVD_SourceType::Iso, s_current_disc_path);
		g_emu_thread->loadState(state_filename);
	}
	else
	{
		std::shared_ptr<VMBootParameters> params = std::make_shared<VMBootParameters>();
		params->filename = filename.toStdString();
		params->save_state = state_filename.toStdString();
		g_emu_thread->startVM(std::move(params));
	}
}

static QString formatTimestampForSaveStateMenu(time_t timestamp)
{
	const QDateTime qtime(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(timestamp)));
	return qtime.toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat));
}

void MainWindow::populateLoadStateMenu(QMenu* menu, const QString& filename, const QString& serial, quint32 crc)
{
	if (serial.isEmpty())
		return;

	const bool is_right_click_menu = (menu != m_ui.menuLoadState);
	bool has_any_states = false;

	QAction* action = menu->addAction(is_right_click_menu ? tr("Load State File...") : tr("Load From File..."));
	connect(action, &QAction::triggered, [this, filename]() {
		const QString path(QFileDialog::getOpenFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s *.p2s.backup)")));
		if (path.isEmpty())
			return;

		loadSaveStateFile(filename, path);
	});

	QAction* delete_save_states_action = menu->addAction(tr("Delete Save States..."));

	// don't include undo in the right click menu
	if (!is_right_click_menu)
	{
		QAction* load_undo_state = menu->addAction(tr("Undo Load State"));
		load_undo_state->setEnabled(false); // CanUndoLoadState()
		// connect(load_undo_state, &QAction::triggered, this, &QtHostInterface::undoLoadState);
		menu->addSeparator();
	}

	const QByteArray game_serial_utf8(serial.toUtf8());
	std::string state_filename;
	FILESYSTEM_STAT_DATA sd;
	if (is_right_click_menu)
	{
		state_filename = VMManager::GetSaveStateFileName(game_serial_utf8.constData(), crc, -1);
		if (FileSystem::StatFile(state_filename.c_str(), &sd))
		{
			action = menu->addAction(tr("Resume (%2)").arg(formatTimestampForSaveStateMenu(sd.ModificationTime)));
			connect(action, &QAction::triggered, [this]() { loadSaveStateSlot(-1); });

			// Make bold to indicate it's the default choice when double-clicking
			QtUtils::MarkActionAsDefault(action);
			has_any_states = true;
		}
	}

	for (s32 i = 1; i <= VMManager::NUM_SAVE_STATE_SLOTS; i++)
	{
		FILESYSTEM_STAT_DATA sd;
		state_filename = VMManager::GetSaveStateFileName(game_serial_utf8.constData(), crc, i);
		if (!FileSystem::StatFile(state_filename.c_str(), &sd))
			continue;

		action = menu->addAction(tr("Load Slot %1 (%2)").arg(i).arg(formatTimestampForSaveStateMenu(sd.ModificationTime)));
		connect(action, &QAction::triggered, [this, i]() { loadSaveStateSlot(i); });
		has_any_states = true;
	}

	delete_save_states_action->setEnabled(has_any_states);
	if (has_any_states)
	{
		connect(delete_save_states_action, &QAction::triggered, this, [this, serial, crc] {
			if (QMessageBox::warning(this, tr("Delete Save States"),
					tr("Are you sure you want to delete all save states for %1?\n\nThe saves will not be recoverable.").arg(serial),
					QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
			{
				return;
			}

			const u32 deleted = VMManager::DeleteSaveStates(serial.toUtf8().constData(), crc, true);
			QMessageBox::information(this, tr("Delete Save States"), tr("%1 save states deleted.").arg(deleted));
		});
	}
}

void MainWindow::populateSaveStateMenu(QMenu* menu, const QString& serial, quint32 crc)
{
	if (serial.isEmpty())
		return;

	connect(menu->addAction(tr("Save To File...")), &QAction::triggered, [this]() {
		const QString path(QFileDialog::getSaveFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s)")));
		if (path.isEmpty())
			return;

		g_emu_thread->saveState(path);
	});

	menu->addSeparator();

	const QByteArray game_serial_utf8(serial.toUtf8());
	for (s32 i = 1; i <= VMManager::NUM_SAVE_STATE_SLOTS; i++)
	{
		std::string filename(VMManager::GetSaveStateFileName(game_serial_utf8.constData(), crc, i));
		FILESYSTEM_STAT_DATA sd;
		QString timestamp;
		if (FileSystem::StatFile(filename.c_str(), &sd))
			timestamp = formatTimestampForSaveStateMenu(sd.ModificationTime);
		else
			timestamp = tr("Empty");

		QString title(tr("Save Slot %1 (%2)").arg(i).arg(timestamp));
		connect(menu->addAction(title), &QAction::triggered, [i]() { g_emu_thread->saveStateToSlot(i); });
	}
}

void MainWindow::updateGameDependentActions()
{
	const bool valid_serial_and_crc = (s_vm_valid && !s_current_disc_serial.isEmpty() && s_current_disc_crc != 0);
	m_ui.menuLoadState->setEnabled(valid_serial_and_crc);
	m_ui.actionToolbarLoadState->setEnabled(valid_serial_and_crc);
	m_ui.menuSaveState->setEnabled(valid_serial_and_crc);
	m_ui.actionToolbarSaveState->setEnabled(valid_serial_and_crc);

	const bool can_use_pnach = (s_vm_valid && !s_current_disc_serial.isEmpty() && s_current_running_crc != 0);
	m_ui.actionEditCheats->setEnabled(can_use_pnach);
	m_ui.actionEditPatches->setEnabled(can_use_pnach);
	m_ui.actionReloadPatches->setEnabled(s_vm_valid);
}

void MainWindow::doStartFile(std::optional<CDVD_SourceType> source, const QString& path)
{
	if (s_vm_valid)
		return;

	std::shared_ptr<VMBootParameters> params = std::make_shared<VMBootParameters>();
	params->source_type = source;
	params->filename = path.toStdString();

	// we might still be saving a resume state...
	VMManager::WaitForSaveStateFlush();

	// GetSaveStateFileName() might temporarily mount the ISO to get the serial.
	cancelGameListRefresh();

	const std::optional<bool> resume(
		promptForResumeState(QString::fromStdString(VMManager::GetSaveStateFileName(params->filename.c_str(), -1))));
	if (!resume.has_value())
		return;
	else if (resume.value())
		params->state_index = -1;

	g_emu_thread->startVM(std::move(params));
}

void MainWindow::doDiscChange(CDVD_SourceType source, const QString& path)
{
	const auto lock = pauseAndLockVM();

	// Check if memcard is busy, deny request if so
	if (shouldAbortForMemcardBusy(lock))
	{
		return;
	}

	bool reset_system = false;
	if (!m_was_disc_change_request)
	{
		QMessageBox message(QMessageBox::Question, tr("Confirm Disc Change"),
			tr("Do you want to swap discs or boot the new image (via system reset)?"), QMessageBox::NoButton, this);
		message.addButton(tr("Swap Disc"), QMessageBox::ActionRole);
		QPushButton* reset_button = message.addButton(tr("Reset"), QMessageBox::ActionRole);
		QPushButton* cancel_button = message.addButton(QMessageBox::Cancel);
		message.setDefaultButton(cancel_button);
		message.exec();

		if (message.clickedButton() == cancel_button)
			return;
		reset_system = (message.clickedButton() == reset_button);
	}

	switchToEmulationView();

	g_emu_thread->changeDisc(source, path);
	if (reset_system)
	{
		// Clearing ELF override will reset the system.
		if (!s_current_elf_override.isEmpty())
			g_emu_thread->setELFOverride(QString());
		else
			g_emu_thread->resetVM();
	}
}

MainWindow::VMLock MainWindow::pauseAndLockVM()
{
	// To switch out of fullscreen when displaying a popup, or not to?
	// For Windows, with driver's direct scanout, what renders behind tends to be hit and miss.
	// We can't draw anything over exclusive fullscreen, so get out of it in that case.
	// Wayland's a pain as usual, we need to recreate the window, which means there'll be a brief
	// period when there's no window, and Qt might shut us down. So avoid it there.
	// On MacOS, it forces a workspace switch, which is kinda jarring.
#ifndef __APPLE__
	const bool was_fullscreen = g_emu_thread->isFullscreen() && !s_use_central_widget;
#else
	const bool was_fullscreen = false;
#endif
	const bool was_paused = s_vm_paused;

	if (!was_paused)
		g_emu_thread->setVMPaused(true);

	// We need to switch out of exclusive fullscreen before we can display our popup.
	// However, we do not want to switch back to render-to-main, the window might have generated this event.
	if (was_fullscreen)
	{
		// m_is_temporarily_windowed needs to be set, so that we don't show the main window just for this popup.
		pxAssertRel(!g_main_window->m_is_temporarily_windowed, "Not already temporarily windowed");
		g_main_window->m_is_temporarily_windowed = true;

		g_emu_thread->setFullscreen(false, false);

		// Container could change... thanks Wayland.
		QWidget* container;
		while (QtHost::IsVMValid() && (g_emu_thread->isFullscreen() ||
										  !(container = getDisplayContainer()) || container->isFullScreen()))
		{
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
	}

	// Now we'll either have a borderless window, or a regular window (if we were exclusive fullscreen).
	QWidget* dialog_parent = getDisplayContainer();

	// Ensure main window is visible.
	if (!g_main_window->isVisible())
		g_main_window->show();
	g_main_window->raise();
	g_main_window->activateWindow();

	return VMLock(dialog_parent, was_paused, was_fullscreen);
}

void MainWindow::rescanFile(const std::string& path)
{
	m_game_list_widget->rescanFile(path);
}

MainWindow::VMLock::VMLock(QWidget* dialog_parent, bool was_paused, bool was_fullscreen)
	: m_dialog_parent(dialog_parent)
	, m_was_paused(was_paused)
	, m_was_fullscreen(was_fullscreen)
{
}

MainWindow::VMLock::VMLock(VMLock&& lock)
	: m_dialog_parent(lock.m_dialog_parent)
	, m_was_paused(lock.m_was_paused)
	, m_was_fullscreen(lock.m_was_fullscreen)
{
	lock.m_dialog_parent = nullptr;
	lock.m_was_paused = true;
	lock.m_was_fullscreen = false;
}

MainWindow::VMLock::~VMLock()
{
	if (m_was_fullscreen)
	{
		g_main_window->m_is_temporarily_windowed = false;
		g_emu_thread->setFullscreen(true, true);
	}

	if (!m_was_paused)
		g_emu_thread->setVMPaused(false);
}

MainWindow::VMLock& MainWindow::VMLock::operator=(VMLock&& lock)
{
	m_dialog_parent = lock.m_dialog_parent;
	m_was_paused = lock.m_was_paused;
	m_was_fullscreen = lock.m_was_fullscreen;

	lock.m_dialog_parent = nullptr;
	lock.m_was_paused = true;
	lock.m_was_fullscreen = false;

	return *this;
}

void MainWindow::VMLock::cancelResume()
{
	m_was_paused = true;
	m_was_fullscreen = false;
	g_main_window->m_is_temporarily_windowed = false;
}

bool QtHost::IsVMValid()
{
	return s_vm_valid;
}

bool QtHost::IsVMPaused()
{
	return s_vm_paused;
}

const QString& QtHost::GetCurrentGameTitle()
{
	return s_current_title;
}

const QString& QtHost::GetCurrentGameSerial()
{
	return s_current_disc_serial;
}

const QString& QtHost::GetCurrentGamePath()
{
	return s_current_disc_path;
}
