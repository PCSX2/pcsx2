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

#include <QtCore/QDateTime>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

#include "common/Assertions.h"
#include "common/FileSystem.h"

#include "pcsx2/CDVD/CDVDaccess.h"
#include "pcsx2/Frontend/GameList.h"
#include "pcsx2/GSDumpReplayer.h"
#include "pcsx2/HostDisplay.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/Recording/InputRecording.h"

#include "AboutDialog.h"
#include "AutoUpdaterDialog.h"
#include "DisplayWidget.h"
#include "EmuThread.h"
#include "GameList/GameListRefreshThread.h"
#include "GameList/GameListWidget.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/GameListSettingsWidget.h"
#include "Settings/InterfaceSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "svnrev.h"
#include "Tools/InputRecording/NewInputRecordingDlg.h"


static constexpr char DISC_IMAGE_FILTER[] =
	QT_TRANSLATE_NOOP("MainWindow", "All File Types (*.bin *.iso *.cue *.chd *.cso *.gz *.elf *.irx *.m3u *.gs *.gs.xz *.gs.zst *.dump);;"
									"Single-Track Raw Images (*.bin *.iso);;"
									"Cue Sheets (*.cue);;"
									"MAME CHD Images (*.chd);;"
									"CSO Images (*.cso);;"
									"GZ Images (*.gz);;"
									"ELF Executables (*.elf);;"
									"IRX Executables (*.irx);;"
									"Playlists (*.m3u);;"
									"GS Dumps (*.gs *.gs.xz *.gs.zst);;"
									"Block Dumps (*.dump)");

const char* MainWindow::DEFAULT_THEME_NAME = "darkfusion";

MainWindow* g_main_window = nullptr;

MainWindow::MainWindow(const QString& unthemed_style_name)
	: m_unthemed_style_name(unthemed_style_name)
{
	pxAssert(!g_main_window);
	g_main_window = this;
}

MainWindow::~MainWindow()
{
	// we compare here, since recreate destroys the window later
	if (g_main_window == this)
		g_main_window = nullptr;
}

void MainWindow::initialize()
{
	setIconThemeFromSettings();
	m_ui.setupUi(this);
	setupAdditionalUi();
	setStyleFromSettings();
	connectSignals();
	connectVMThreadSignals(g_emu_thread);

	restoreStateFromConfig();
	switchToGameListView();
	updateWindowTitle();
	updateSaveStateMenus(QString(), QString(), 0);
}

void MainWindow::setupAdditionalUi()
{
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

	m_game_list_widget = new GameListWidget(this);
	m_game_list_widget->initialize();
	m_ui.actionGridViewShowTitles->setChecked(m_game_list_widget->getShowGridCoverTitles());
	setCentralWidget(m_game_list_widget);

	m_status_progress_widget = new QProgressBar(m_ui.statusBar);
	m_status_progress_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_status_progress_widget->setFixedSize(140, 16);
	m_status_progress_widget->hide();

	m_status_gs_widget = new QLabel(m_ui.statusBar);
	m_status_gs_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_status_gs_widget->setFixedHeight(16);
	m_status_gs_widget->hide();

	m_status_fps_widget = new QLabel(m_ui.statusBar);
	m_status_fps_widget->setAlignment(Qt::AlignRight);
	m_status_fps_widget->setFixedHeight(16);
	m_status_fps_widget->hide();

	for (u32 scale = 0; scale <= 10; scale++)
	{
		QAction* action = m_ui.menuWindowSize->addAction((scale == 0) ? tr("Internal Resolution") : tr("%1x Scale").arg(scale));
		connect(action, &QAction::triggered, [scale]() { g_emu_thread->requestDisplaySize(static_cast<float>(scale)); });
	}

	updateEmulationActions(false, false);
}

void MainWindow::connectSignals()
{
	connect(m_ui.actionStartFile, &QAction::triggered, this, &MainWindow::onStartFileActionTriggered);
	connect(m_ui.actionStartBios, &QAction::triggered, this, &MainWindow::onStartBIOSActionTriggered);
	connect(m_ui.actionChangeDisc, &QAction::triggered, [this] { m_ui.menuChangeDisc->exec(QCursor::pos()); });
	connect(m_ui.actionChangeDiscFromFile, &QAction::triggered, this, &MainWindow::onChangeDiscFromFileActionTriggered);
	connect(m_ui.actionChangeDiscFromDevice, &QAction::triggered, this, &MainWindow::onChangeDiscFromDeviceActionTriggered);
	connect(m_ui.actionChangeDiscFromGameList, &QAction::triggered, this, &MainWindow::onChangeDiscFromGameListActionTriggered);
	connect(m_ui.menuChangeDisc, &QMenu::aboutToShow, this, &MainWindow::onChangeDiscMenuAboutToShow);
	connect(m_ui.menuChangeDisc, &QMenu::aboutToHide, this, &MainWindow::onChangeDiscMenuAboutToHide);
	connect(m_ui.actionPowerOff, &QAction::triggered, this, [this]() { requestShutdown(true, true); });
	connect(m_ui.actionPowerOffWithoutSaving, &QAction::triggered, this, [this]() { requestShutdown(false, false); });
	connect(m_ui.actionLoadState, &QAction::triggered, this, [this]() { m_ui.menuLoadState->exec(QCursor::pos()); });
	connect(m_ui.actionSaveState, &QAction::triggered, this, [this]() { m_ui.menuSaveState->exec(QCursor::pos()); });
	connect(m_ui.actionExit, &QAction::triggered, this, &MainWindow::close);
	connect(m_ui.actionScreenshot, &QAction::triggered, this, &MainWindow::onScreenshotActionTriggered);
	connect(m_ui.menuLoadState, &QMenu::aboutToShow, this, &MainWindow::onLoadStateMenuAboutToShow);
	connect(m_ui.menuSaveState, &QMenu::aboutToShow, this, &MainWindow::onSaveStateMenuAboutToShow);
	connect(m_ui.actionSettings, &QAction::triggered, [this]() { doSettings(); });
	connect(m_ui.actionInterfaceSettings, &QAction::triggered, [this]() { doSettings("Interface"); });
	connect(m_ui.actionGameListSettings, &QAction::triggered, [this]() { doSettings("Game List"); });
	connect(m_ui.actionEmulationSettings, &QAction::triggered, [this]() { doSettings("Emulation"); });
	connect(m_ui.actionBIOSSettings, &QAction::triggered, [this]() { doSettings("BIOS"); });
	connect(m_ui.actionSystemSettings, &QAction::triggered, [this]() { doSettings("System"); });
	connect(m_ui.actionGraphicsSettings, &QAction::triggered, [this]() { doSettings("Graphics"); });
	connect(m_ui.actionAudioSettings, &QAction::triggered, [this]() { doSettings("Audio"); });
	connect(m_ui.actionMemoryCardSettings, &QAction::triggered, [this]() { doSettings("Memory Cards"); });
	connect(m_ui.actionDEV9Settings, &QAction::triggered, [this]() { doSettings("Network & HDD"); });
	connect(
		m_ui.actionControllerSettings, &QAction::triggered, [this]() { doControllerSettings(ControllerSettingsDialog::Category::GlobalSettings); });
	connect(m_ui.actionHotkeySettings, &QAction::triggered, [this]() { doControllerSettings(ControllerSettingsDialog::Category::HotkeySettings); });
	connect(
		m_ui.actionAddGameDirectory, &QAction::triggered, [this]() { getSettingsDialog()->getGameListSettingsWidget()->addSearchDirectory(this); });
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
	connect(m_ui.actionCheckForUpdates, &QAction::triggered, this, &MainWindow::onCheckForUpdatesActionTriggered);
	connect(m_ui.actionOpenDataDirectory, &QAction::triggered, this, &MainWindow::onToolsOpenDataDirectoryTriggered);
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

	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionViewStatusBarVerbose, "UI", "VerboseStatusBar", false);

	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableSystemConsole, "Logging", "EnableSystemConsole", false);
	connect(m_ui.actionEnableSystemConsole, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
#ifndef PCSX2_DEVBUILD
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableVerboseLogging, "Logging", "EnableVerbose", false);
	connect(m_ui.actionEnableVerboseLogging, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
#else
	// Dev builds always have verbose logging.
	m_ui.actionEnableVerboseLogging->setChecked(true);
	m_ui.actionEnableVerboseLogging->setEnabled(false);
#endif
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableEEConsoleLogging, "Logging", "EnableEEConsole", true);
	connect(m_ui.actionEnableEEConsoleLogging, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableIOPConsoleLogging, "Logging", "EnableIOPConsole", true);
	connect(m_ui.actionEnableIOPConsoleLogging, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableFileLogging, "Logging", "EnableFileLogging", false);
	connect(m_ui.actionEnableFileLogging, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableLogTimestamps, "Logging", "EnableTimestamps", true);
	connect(m_ui.actionEnableLogTimestamps, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionEnableCDVDVerboseReads, "EmuCore", "CdvdVerboseReads", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionSaveBlockDump, "EmuCore", "CdvdDumpBlocks", false);
	connect(m_ui.actionSaveBlockDump, &QAction::toggled, this, &MainWindow::onBlockDumpActionToggled);

	connect(m_ui.actionSaveGSDump, &QAction::triggered, this, &MainWindow::onSaveGSDumpActionTriggered);

	// Input Recording
	connect(m_ui.actionInputRecNew, &QAction::triggered, this, &MainWindow::onInputRecNewActionTriggered);
	connect(m_ui.actionInputRecPlay, &QAction::triggered, this, &MainWindow::onInputRecPlayActionTriggered);
	connect(m_ui.actionInputRecStop, &QAction::triggered, this, &MainWindow::onInputRecStopActionTriggered);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionInputRecConsoleLogs, "Logging", "EnableInputRecordingLogs", false);
	connect(m_ui.actionInputRecConsoleLogs, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.actionInputRecControllerLogs, "Logging", "EnableControllerLogs", false);
	connect(m_ui.actionInputRecControllerLogs, &QAction::triggered, this, &MainWindow::onLoggingOptionChanged); 

	// These need to be queued connections to stop crashing due to menus opening/closing and switching focus.
	connect(m_game_list_widget, &GameListWidget::refreshProgress, this, &MainWindow::onGameListRefreshProgress);
	connect(m_game_list_widget, &GameListWidget::refreshComplete, this, &MainWindow::onGameListRefreshComplete);
	connect(m_game_list_widget, &GameListWidget::selectionChanged, this, &MainWindow::onGameListSelectionChanged, Qt::QueuedConnection);
	connect(m_game_list_widget, &GameListWidget::entryActivated, this, &MainWindow::onGameListEntryActivated, Qt::QueuedConnection);
	connect(
		m_game_list_widget, &GameListWidget::entryContextMenuRequested, this, &MainWindow::onGameListEntryContextMenuRequested, Qt::QueuedConnection);
	connect(m_game_list_widget, &GameListWidget::addGameDirectoryRequested, this,
		[this]() { getSettingsDialog()->getGameListSettingsWidget()->addSearchDirectory(this); });
}

void MainWindow::connectVMThreadSignals(EmuThread* thread)
{
	connect(thread, &EmuThread::onCreateDisplayRequested, this, &MainWindow::createDisplay, Qt::BlockingQueuedConnection);
	connect(thread, &EmuThread::onUpdateDisplayRequested, this, &MainWindow::updateDisplay, Qt::BlockingQueuedConnection);
	connect(thread, &EmuThread::onDestroyDisplayRequested, this, &MainWindow::destroyDisplay, Qt::BlockingQueuedConnection);
	connect(thread, &EmuThread::onResizeDisplayRequested, this, &MainWindow::displayResizeRequested);
	connect(thread, &EmuThread::onVMStarting, this, &MainWindow::onVMStarting);
	connect(thread, &EmuThread::onVMStarted, this, &MainWindow::onVMStarted);
	connect(thread, &EmuThread::onVMPaused, this, &MainWindow::onVMPaused);
	connect(thread, &EmuThread::onVMResumed, this, &MainWindow::onVMResumed);
	connect(thread, &EmuThread::onVMStopped, this, &MainWindow::onVMStopped);
	connect(thread, &EmuThread::onGameChanged, this, &MainWindow::onGameChanged);
	connect(thread, &EmuThread::onPerformanceMetricsUpdated, this, &MainWindow::onPerformanceMetricsUpdated);

	connect(m_ui.actionReset, &QAction::triggered, thread, &EmuThread::resetVM);
	connect(m_ui.actionPause, &QAction::toggled, thread, &EmuThread::setVMPaused);
	connect(m_ui.actionFullscreen, &QAction::triggered, thread, &EmuThread::toggleFullscreen);
	connect(m_ui.actionToggleSoftwareRendering, &QAction::triggered, thread, &EmuThread::toggleSoftwareRendering);
	connect(m_ui.actionReloadPatches, &QAction::triggered, thread, &EmuThread::reloadPatches);

	static constexpr GSRendererType renderers[] = {
#ifdef _WIN32
		GSRendererType::DX11, GSRendererType::DX12,
#endif
		GSRendererType::OGL, GSRendererType::VK, GSRendererType::SW, GSRendererType::Null};
	for (GSRendererType renderer : renderers)
	{
		connect(m_ui.menuDebugSwitchRenderer->addAction(QString::fromUtf8(Pcsx2Config::GSOptions::GetRendererName(renderer))), &QAction::triggered,
			[renderer] { g_emu_thread->switchRenderer(renderer); });
	}
}

void MainWindow::recreate()
{
	if (m_vm_valid)
		requestShutdown(false, true, true);

	close();
	g_main_window = nullptr;

	MainWindow* new_main_window = new MainWindow(m_unthemed_style_name);
	new_main_window->initialize();
	new_main_window->refreshGameList(false);
	new_main_window->show();
	deleteLater();
}

void MainWindow::setStyleFromSettings()
{
	const std::string theme(Host::GetBaseStringSettingValue("UI", "Theme", DEFAULT_THEME_NAME));

	if (theme == "fusion")
	{
		qApp->setPalette(QApplication::style()->standardPalette());
		qApp->setStyleSheet(QString());
		qApp->setStyle(QStyleFactory::create("Fusion"));
	}
	else if (theme == "UntouchedLagoon")
	{
		// Custom pallete by RedDevilus, Tame (Light/Washed out) Green as main color and Grayish Blue as complimentary.
		// Alternative white theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor black(25, 25, 25);
		const QColor teal(0, 128, 128);
		const QColor tameTeal(160, 190, 185);
		const QColor grayBlue(160, 180, 190);

		QPalette standardPalette;
		standardPalette.setColor(QPalette::Window, tameTeal);
		standardPalette.setColor(QPalette::WindowText, black);
		standardPalette.setColor(QPalette::Base, grayBlue);
		standardPalette.setColor(QPalette::AlternateBase, tameTeal);
		standardPalette.setColor(QPalette::ToolTipBase, tameTeal);
		standardPalette.setColor(QPalette::ToolTipText, grayBlue);
		standardPalette.setColor(QPalette::Text, black);
		standardPalette.setColor(QPalette::Button, tameTeal);
		standardPalette.setColor(QPalette::ButtonText, Qt::white);
		standardPalette.setColor(QPalette::Link, black);
		standardPalette.setColor(QPalette::Highlight, teal);
		standardPalette.setColor(QPalette::HighlightedText, Qt::white);

		standardPalette.setColor(QPalette::Active, QPalette::Button, tameTeal.darker());
		standardPalette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::white);
		standardPalette.setColor(QPalette::Disabled, QPalette::WindowText, Qt::white);
		standardPalette.setColor(QPalette::Disabled, QPalette::Text, Qt::white);
		standardPalette.setColor(QPalette::Disabled, QPalette::Light, tameTeal);

		qApp->setPalette(standardPalette);

		qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
	}
	else if (theme == "BabyPastel")
	{
		// Custom pallete by RedDevilus, Blue as main color and blue as complimentary.
		// Alternative light theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(150, 150, 150);
		const QColor black(25, 25, 25);
		const QColor pink(255, 174, 201);
		const QColor brightPink(255, 230, 255);
		const QColor congoPink(255, 127, 121);
		const QColor blue(221, 225, 239);

		QPalette standardPalette;
		standardPalette.setColor(QPalette::Window, pink);
		standardPalette.setColor(QPalette::WindowText, black);
		standardPalette.setColor(QPalette::Base, brightPink);
		standardPalette.setColor(QPalette::AlternateBase, blue);
		standardPalette.setColor(QPalette::ToolTipBase, pink);
		standardPalette.setColor(QPalette::ToolTipText, brightPink);
		standardPalette.setColor(QPalette::Text, black);
		standardPalette.setColor(QPalette::Button, pink);
		standardPalette.setColor(QPalette::ButtonText, black);
		standardPalette.setColor(QPalette::Link, black);
		standardPalette.setColor(QPalette::Highlight, congoPink);
		standardPalette.setColor(QPalette::HighlightedText, black);

		standardPalette.setColor(QPalette::Active, QPalette::Button, pink);
		standardPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		standardPalette.setColor(QPalette::Disabled, QPalette::WindowText, congoPink);
		standardPalette.setColor(QPalette::Disabled, QPalette::Text, blue);
		standardPalette.setColor(QPalette::Disabled, QPalette::Light, gray);

		qApp->setPalette(standardPalette);

		qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
	}
	else if (theme == "PCSX2Blue")
	{
		// Custom pallete by RedDevilus, White as main color and Blue as complimentary.
		// Alternative light theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor black(25, 25, 25);
		const QColor darkBlue(73, 97, 177);
		const QColor blue(106, 156, 255);
		const QColor lightBlue(130, 155, 241);

		QPalette standardPalette;
		standardPalette.setColor(QPalette::Window, lightBlue);
		standardPalette.setColor(QPalette::WindowText, black);
		standardPalette.setColor(QPalette::Base, darkBlue);
		standardPalette.setColor(QPalette::AlternateBase, lightBlue);
		standardPalette.setColor(QPalette::ToolTipBase, lightBlue);
		standardPalette.setColor(QPalette::ToolTipText, Qt::white);
		standardPalette.setColor(QPalette::Text, Qt::white);
		standardPalette.setColor(QPalette::Button, blue);
		standardPalette.setColor(QPalette::ButtonText, Qt::white);
		standardPalette.setColor(QPalette::Link, darkBlue);
		standardPalette.setColor(QPalette::Highlight, Qt::white);
		standardPalette.setColor(QPalette::HighlightedText, black);

		standardPalette.setColor(QPalette::Active, QPalette::Button, blue.darker());
		standardPalette.setColor(QPalette::Disabled, QPalette::ButtonText, darkBlue);
		standardPalette.setColor(QPalette::Disabled, QPalette::WindowText, darkBlue);
		standardPalette.setColor(QPalette::Disabled, QPalette::Text, black);
		standardPalette.setColor(QPalette::Disabled, QPalette::Light, darkBlue);

		qApp->setPalette(standardPalette);

		qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
	}
	else if (theme == "darkfusion")
	{
		// adapted from https://gist.github.com/QuantumCD/6245215
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor lighterGray(75, 75, 75);
		const QColor darkGray(53, 53, 53);
		const QColor gray(128, 128, 128);
		const QColor black(25, 25, 25);
		const QColor blue(198, 238, 255);

		QPalette darkPalette;
		darkPalette.setColor(QPalette::Window, darkGray);
		darkPalette.setColor(QPalette::WindowText, Qt::white);
		darkPalette.setColor(QPalette::Base, black);
		darkPalette.setColor(QPalette::AlternateBase, darkGray);
		darkPalette.setColor(QPalette::ToolTipBase, darkGray);
		darkPalette.setColor(QPalette::ToolTipText, Qt::white);
		darkPalette.setColor(QPalette::Text, Qt::white);
		darkPalette.setColor(QPalette::Button, darkGray);
		darkPalette.setColor(QPalette::ButtonText, Qt::white);
		darkPalette.setColor(QPalette::Link, blue);
		darkPalette.setColor(QPalette::Highlight, lighterGray);
		darkPalette.setColor(QPalette::HighlightedText, Qt::white);

		darkPalette.setColor(QPalette::Active, QPalette::Button, gray.darker());
		darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::Light, darkGray);

		qApp->setPalette(darkPalette);

		qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
	}
	else if (theme == "darkfusionblue")
	{
		// adapted from https://gist.github.com/QuantumCD/6245215
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor darkGray(53, 53, 53);
		const QColor gray(128, 128, 128);
		const QColor black(25, 25, 25);
		const QColor blue(198, 238, 255);
		const QColor blue2(0, 88, 208);

		QPalette darkPalette;
		darkPalette.setColor(QPalette::Window, darkGray);
		darkPalette.setColor(QPalette::WindowText, Qt::white);
		darkPalette.setColor(QPalette::Base, black);
		darkPalette.setColor(QPalette::AlternateBase, darkGray);
		darkPalette.setColor(QPalette::ToolTipBase, blue2);
		darkPalette.setColor(QPalette::ToolTipText, Qt::white);
		darkPalette.setColor(QPalette::Text, Qt::white);
		darkPalette.setColor(QPalette::Button, darkGray);
		darkPalette.setColor(QPalette::ButtonText, Qt::white);
		darkPalette.setColor(QPalette::Link, blue);
		darkPalette.setColor(QPalette::Highlight, blue2);
		darkPalette.setColor(QPalette::HighlightedText, Qt::white);

		darkPalette.setColor(QPalette::Active, QPalette::Button, gray.darker());
		darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::Light, darkGray);

		qApp->setPalette(darkPalette);

		qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
	}
	else if (theme == "ScarletDevilRed")
	{
		// Custom pallete by RedDevilus, Red as main color and Purple as complimentary.
		// Alternative dark theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor darkRed(80, 45, 69);
		const QColor purplishRed(120, 45, 69);
		const QColor brightRed(200, 45, 69);

		QPalette darkPalette;
		darkPalette.setColor(QPalette::Window, darkRed);
		darkPalette.setColor(QPalette::WindowText, Qt::white);
		darkPalette.setColor(QPalette::Base, purplishRed);
		darkPalette.setColor(QPalette::AlternateBase, darkRed);
		darkPalette.setColor(QPalette::ToolTipBase, darkRed);
		darkPalette.setColor(QPalette::ToolTipText, Qt::white);
		darkPalette.setColor(QPalette::Text, Qt::white);
		darkPalette.setColor(QPalette::Button, darkRed);
		darkPalette.setColor(QPalette::ButtonText, Qt::white);
		darkPalette.setColor(QPalette::Link, brightRed);
		darkPalette.setColor(QPalette::Highlight, brightRed);
		darkPalette.setColor(QPalette::HighlightedText, Qt::white);

		darkPalette.setColor(QPalette::Active, QPalette::Button, purplishRed.darker());
		darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, brightRed);
		darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, brightRed);
		darkPalette.setColor(QPalette::Disabled, QPalette::Text, brightRed);
		darkPalette.setColor(QPalette::Disabled, QPalette::Light, darkRed);

		qApp->setPalette(darkPalette);

		qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
	}
	else
	{
		qApp->setPalette(QApplication::style()->standardPalette());
		qApp->setStyleSheet(QString());
		qApp->setStyle(m_unthemed_style_name);
	}
}

void MainWindow::setIconThemeFromSettings()
{
	const std::string theme(Host::GetBaseStringSettingValue("UI", "Theme", DEFAULT_THEME_NAME));
	QString icon_theme;

	if (theme == "darkfusion" || theme == "darkfusionblue" || theme == "dualtoneOrangeBlue" || theme == "ScarletDevilRed")
		icon_theme = QStringLiteral("white");
	else
		icon_theme = QStringLiteral("black");

	QIcon::setThemeName(icon_theme);
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

	std::string old_directory(Host::GetBaseStringSettingValue("EmuCore", "BlockDumpSaveDirectory", ""));
	if (old_directory.empty())
		old_directory = FileSystem::GetWorkingDirectory();

	// prompt for a location to save
	const QString new_dir(
		QFileDialog::getExistingDirectory(this, tr("Select location to save block dump:"),
			QString::fromStdString(old_directory)));
	if (new_dir.isEmpty())
	{
		// disable it again
		m_ui.actionSaveBlockDump->setChecked(false);
		return;
	}

	QtHost::SetBaseStringSettingValue("EmuCore", "BlockDumpSaveDirectory", new_dir.toUtf8().constData());
}

void MainWindow::saveStateToConfig()
{
	{
		const QByteArray geometry = saveGeometry();
		const QByteArray geometry_b64 = geometry.toBase64();
		const std::string old_geometry_b64 = Host::GetBaseStringSettingValue("UI", "MainWindowGeometry");
		if (old_geometry_b64 != geometry_b64.constData())
			QtHost::SetBaseStringSettingValue("UI", "MainWindowGeometry", geometry_b64.constData());
	}

	{
		const QByteArray state = saveState();
		const QByteArray state_b64 = state.toBase64();
		const std::string old_state_b64 = Host::GetBaseStringSettingValue("UI", "MainWindowState");
		if (old_state_b64 != state_b64.constData())
			QtHost::SetBaseStringSettingValue("UI", "MainWindowState", state_b64.constData());
	}
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

void MainWindow::updateEmulationActions(bool starting, bool running)
{
	const bool starting_or_running = starting || running;

	m_ui.actionStartFile->setDisabled(starting_or_running);
	m_ui.actionStartDisc->setDisabled(starting_or_running);
	m_ui.actionStartBios->setDisabled(starting_or_running);

	m_ui.actionPowerOff->setEnabled(running);
	m_ui.actionPowerOffWithoutSaving->setEnabled(running);
	m_ui.actionReset->setEnabled(running);
	m_ui.actionPause->setEnabled(running);
	m_ui.actionChangeDisc->setEnabled(running);
	m_ui.actionScreenshot->setEnabled(running);
	m_ui.actionViewSystemDisplay->setEnabled(starting_or_running);
	m_ui.menuChangeDisc->setEnabled(running);

	m_ui.actionSaveState->setEnabled(running);
	m_ui.menuSaveState->setEnabled(running);
	m_ui.menuWindowSize->setEnabled(starting_or_running);

	m_ui.actionFullscreen->setEnabled(starting_or_running);
	m_ui.actionViewGameProperties->setEnabled(running);

	m_game_list_widget->setDisabled(starting && !running);

	if (!starting && !running)
		m_ui.actionPause->setChecked(false);

	// scanning needs to be disabled while running
	m_ui.actionScanForNewGames->setDisabled(starting_or_running);
	m_ui.actionRescanAllGames->setDisabled(starting_or_running);
}

void MainWindow::updateStatusBarWidgetVisibility()
{
	auto Update = [this](QWidget* widget, bool visible, int stretch)
	{
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

	Update(m_status_gs_widget, m_vm_valid && !m_vm_paused, 1);
	Update(m_status_fps_widget, m_vm_valid, 0);
}

void MainWindow::updateWindowTitle()
{
	QString suffix(QtHost::GetAppConfigSuffix());
	QString main_title(QtHost::GetAppNameAndVersion() + suffix);
	QString display_title(m_current_game_name + suffix);

	if (!m_vm_valid || m_current_game_name.isEmpty())
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
}

void MainWindow::setProgressBar(int current, int total)
{
	m_status_progress_widget->setValue(current);
	m_status_progress_widget->setMaximum(total);

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
	return (centralWidget() == m_game_list_widget);
}

bool MainWindow::isRenderingFullscreen() const
{
	HostDisplay* display = Host::GetHostDisplay();
	if (!display || !m_display_widget)
		return false;

	return (m_display_widget->parent() != this && (m_display_widget->isFullScreen() || display->IsFullscreen()));
}

bool MainWindow::isRenderingToMain() const
{
	return (m_display_widget && m_display_widget->parent() == this);
}

void MainWindow::switchToGameListView()
{
	if (centralWidget() == m_game_list_widget)
	{
		m_game_list_widget->setFocus();
		return;
	}

	if (m_vm_valid)
	{
		m_was_paused_on_surface_loss = m_vm_paused;
		if (!m_vm_paused)
			g_emu_thread->setVMPaused(true);

		// switch to surfaceless. we have to wait until the display widget is gone before we swap over.
		g_emu_thread->setSurfaceless(true);
		while (m_display_widget)
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
	}

	pxAssertMsg(!centralWidget(), "Should not have a central widget at game list switch time");
	takeCentralWidget();
	setCentralWidget(m_game_list_widget);
	m_game_list_widget->setVisible(true);
	m_game_list_widget->setFocus();
}

void MainWindow::switchToEmulationView()
{
	if (!m_vm_valid || (m_display_widget && centralWidget() == m_display_widget))
		return;

	// we're no longer surfaceless! this will call back to UpdateDisplay(), which will swap the widget out.
	g_emu_thread->setSurfaceless(false);

	// resume if we weren't paused at switch time
	if (m_vm_paused && !m_was_paused_on_surface_loss)
		g_emu_thread->setVMPaused(false);

	if (m_display_widget)
		m_display_widget->setFocus();
}

void MainWindow::refreshGameList(bool invalidate_cache)
{
	// can't do this while the VM is running because of CDVD
	if (m_vm_valid)
		return;

	m_game_list_widget->refresh(invalidate_cache);
}

void MainWindow::cancelGameListRefresh()
{
	m_game_list_widget->cancelRefresh();
}

void MainWindow::invalidateSaveStateCache()
{
	m_save_states_invalidated = true;
}

void MainWindow::reportError(const QString& title, const QString& message)
{
	QMessageBox::critical(this, title, message);
}

void MainWindow::runOnUIThread(const std::function<void()>& func)
{
	func();
}

bool MainWindow::requestShutdown(bool allow_confirm /* = true */, bool allow_save_to_state /* = true */, bool block_until_done /* = false */)
{
	if (!VMManager::HasValidVM())
		return true;

	// if we don't have a crc, we can't save state
	allow_save_to_state &= (m_current_game_crc != 0);
	bool save_state = allow_save_to_state && EmuConfig.SaveStateOnShutdown;

	// only confirm on UI thread because we need to display a msgbox
	if (allow_confirm && !GSDumpReplayer::IsReplayingDump() && Host::GetBaseBoolSettingValue("UI", "ConfirmShutdown", true))
	{
		VMLock lock(pauseAndLockVM());

		QMessageBox msgbox(lock.getDialogParent());
		msgbox.setIcon(QMessageBox::Question);
		msgbox.setWindowTitle(tr("Confirm Shutdown"));
		msgbox.setText("Are you sure you want to shut down the virtual machine?");

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
	}

	g_emu_thread->shutdownVM(save_state);

	if (block_until_done || QtHost::InBatchMode())
	{
		// we need to yield here, since the display gets destroyed
		while (VMManager::GetState() != VMState::Shutdown)
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
	}

	if (QtHost::InBatchMode())
	{
		// closing the window should shut down everything.
		close();
	}

	return true;
}

void MainWindow::requestExit()
{
	// this is block, because otherwise closeEvent() will also prompt
	if (!requestShutdown(true, true, true))
		return;

	close();
}

void Host::InvalidateSaveStateCache()
{
	QMetaObject::invokeMethod(g_main_window, &MainWindow::invalidateSaveStateCache, Qt::QueuedConnection);
}

void MainWindow::onGameListRefreshProgress(const QString& status, int current, int total)
{
	m_ui.statusBar->showMessage(status);
	setProgressBar(current, total);
}

void MainWindow::onGameListRefreshComplete()
{
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

	if (m_vm_valid)
	{
		// change disc on double click
		doDiscChange(QString::fromStdString(entry->path));
		return;
	}

	// we might still be saving a resume state...
	VMManager::WaitForSaveStateFlush();

	const std::optional<bool> resume = promptForResumeState(
		QString::fromStdString(VMManager::GetSaveStateFileName(entry->serial.c_str(), entry->crc, -1)));
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
		action->setEnabled(!entry->serial.empty());
		if (action->isEnabled())
			connect(action, &QAction::triggered, [entry]() { SettingsDialog::openGamePropertiesDialog(entry, entry->serial, entry->crc); });

		action = menu.addAction(tr("Open Containing Directory..."));
		connect(action, &QAction::triggered, [this, entry]() {
			const QFileInfo fi(QString::fromStdString(entry->path));
			QtUtils::OpenURL(this, QUrl::fromLocalFile(fi.absolutePath()));
		});

		action = menu.addAction(tr("Set Cover Image..."));
		connect(action, &QAction::triggered, [this, entry]() { setGameListEntryCoverImage(entry); });

		connect(menu.addAction(tr("Exclude From List")), &QAction::triggered,
			[this, entry]() { getSettingsDialog()->getGameListSettingsWidget()->addExcludedPath(entry->path); });

		menu.addSeparator();

		if (!m_vm_valid)
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
				// TODO: Hook this up once it's implemented.
				action = menu.addAction(tr("Boot and Debug"));
			}

			menu.addSeparator();
			populateLoadStateMenu(&menu, QString::fromStdString(entry->path), QString::fromStdString(entry->serial), entry->crc);
		}
		else
		{
			action = menu.addAction(tr("Change Disc"));
			connect(action, &QAction::triggered, [this, entry]() {
				g_emu_thread->changeDisc(QString::fromStdString(entry->path));
				switchToEmulationView();
			});
			QtUtils::MarkActionAsDefault(action);
		}

		menu.addSeparator();
	}

	connect(menu.addAction(tr("Add Search Directory...")), &QAction::triggered,
		[this]() { getSettingsDialog()->getGameListSettingsWidget()->addSearchDirectory(this); });

	menu.exec(point);
}

void MainWindow::onStartFileActionTriggered()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Select Disc Image"), QString(), tr(DISC_IMAGE_FILTER), nullptr));
	if (path.isEmpty())
		return;

	doStartDisc(path);
}

void MainWindow::onStartBIOSActionTriggered()
{
	std::shared_ptr<VMBootParameters> params = std::make_shared<VMBootParameters>();
	g_emu_thread->startVM(std::move(params));
}

void MainWindow::onChangeDiscFromFileActionTriggered()
{
	VMLock lock(pauseAndLockVM());
	QString filename = QFileDialog::getOpenFileName(lock.getDialogParent(), tr("Select Disc Image"), QString(), tr(DISC_IMAGE_FILTER), nullptr);
	if (filename.isEmpty())
		return;

	g_emu_thread->changeDisc(filename);
}

void MainWindow::onChangeDiscFromGameListActionTriggered()
{
	m_was_disc_change_request = true;
	switchToGameListView();
}

void MainWindow::onChangeDiscFromDeviceActionTriggered()
{
	// TODO
}

void MainWindow::onChangeDiscMenuAboutToShow()
{
	// TODO: This is where we would populate the playlist if there is one.
}

void MainWindow::onChangeDiscMenuAboutToHide() {}

void MainWindow::onLoadStateMenuAboutToShow()
{
	if (m_save_states_invalidated)
		updateSaveStateMenus(m_current_disc_path, m_current_game_serial, m_current_game_crc);
}

void MainWindow::onSaveStateMenuAboutToShow()
{
	if (m_save_states_invalidated)
		updateSaveStateMenus(m_current_disc_path, m_current_game_serial, m_current_game_crc);
}

void MainWindow::onViewToolbarActionToggled(bool checked)
{
	QtHost::SetBaseBoolSettingValue("UI", "ShowToolbar", checked);
	m_ui.toolBar->setVisible(checked);
}

void MainWindow::onViewLockToolbarActionToggled(bool checked)
{
	QtHost::SetBaseBoolSettingValue("UI", "LockToolbar", checked);
	m_ui.toolBar->setMovable(!checked);
}

void MainWindow::onViewStatusBarActionToggled(bool checked)
{
	QtHost::SetBaseBoolSettingValue("UI", "ShowStatusBar", checked);
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
	if (m_vm_valid)
		switchToEmulationView();
}

void MainWindow::onViewGamePropertiesActionTriggered()
{
	if (!m_vm_valid)
		return;

	// prefer to use a game list entry, if we have one, that way the summary is populated
	if (!m_current_disc_path.isEmpty())
	{
		auto lock = GameList::GetLock();
		const GameList::Entry* entry = GameList::GetEntryForPath(m_current_disc_path.toUtf8().constData());
		if (entry)
		{
			SettingsDialog::openGamePropertiesDialog(entry, entry->serial, entry->crc);
			return;
		}
	}

	// open properties for the current running file (isn't in the game list)
	if (m_current_game_crc != 0)
		SettingsDialog::openGamePropertiesDialog(nullptr, m_current_game_serial.toStdString(), m_current_game_crc);
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

void MainWindow::onCheckForUpdatesActionTriggered()
{
	// Wipe out the last version, that way it displays the update if we've previously skipped it.
	QtHost::RemoveBaseSettingValue("AutoUpdater", "LastVersion");
	checkForUpdates(true);
}

void MainWindow::checkForUpdates(bool display_message)
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
			message =
				tr("<p>Sorry, you are trying to update a PCSX2 version which is not an official GitHub release. To "
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

	checkForUpdates(false);
}

void MainWindow::onToolsOpenDataDirectoryTriggered()
{
	const QString path(QString::fromStdString(EmuFolders::DataRoot));
	QtUtils::OpenURL(this, QUrl::fromLocalFile(path));
}

void MainWindow::onThemeChanged()
{
	setStyleFromSettings();
	setIconThemeFromSettings();
	recreate();
}

void MainWindow::onThemeChangedFromSettings()
{
	// reopen the settings dialog after recreating
	onThemeChanged();
	g_main_window->doSettings();
}

void MainWindow::onLoggingOptionChanged()
{
	QtHost::UpdateLogging();
}

void MainWindow::onInputRecNewActionTriggered()
{
	const bool wasPaused = m_vm_paused;
	const bool wasRunning = m_vm_valid;
	if (wasRunning && !wasPaused)
	{
		VMManager::SetPaused(true);
	}

	NewInputRecordingDlg dlg(this);
	const auto result = dlg.exec();

	if (result == QDialog::Accepted)
	{
		if (g_InputRecording.Create(
				dlg.getFilePath(),
				dlg.getInputRecType() == InputRecording::Type::FROM_SAVESTATE,
				dlg.getAuthorName()))
		{
			return;
		}
	}

	if (wasRunning && !wasPaused)
	{
		VMManager::SetPaused(false);
	}
}

#include "pcsx2/Recording/InputRecordingControls.h"

void MainWindow::onInputRecPlayActionTriggered()
{
	const bool wasPaused = m_vm_paused;

	if (!wasPaused)
		g_InputRecordingControls.PauseImmediately();

	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setWindowTitle("Select a File");
	dialog.setNameFilter(tr("Input Recording Files (*.p2m2)"));
	QStringList fileNames;
	if (dialog.exec())
	{
		fileNames = dialog.selectedFiles();
	}

	if (fileNames.length() > 0)
	{
		if (g_InputRecording.IsActive())
		{
			g_InputRecording.Stop();
		}
		if (g_InputRecording.Play(fileNames.first().toStdString()))
		{
			return;
		}
	}

	if (!wasPaused)
	{
		g_InputRecordingControls.Resume();
	}
}

void MainWindow::onInputRecStopActionTriggered()
{
	if (g_InputRecording.IsActive())
	{
		g_InputRecording.Stop();
	}
}

void MainWindow::onInputRecOpenSettingsTriggered()
{
	// TODO - Vaser - Implement
}

void MainWindow::onVMStarting()
{
	m_vm_valid = true;
	updateEmulationActions(true, false);
	updateWindowTitle();

	// prevent loading state until we're fully initialized
	updateSaveStateMenus(QString(), QString(), 0);
}

void MainWindow::onVMStarted()
{
	m_vm_valid = true;
	m_was_disc_change_request = false;
	updateEmulationActions(true, true);
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
}

void MainWindow::onVMPaused()
{
	// update UI
	{
		QSignalBlocker sb(m_ui.actionPause);
		m_ui.actionPause->setChecked(true);
	}

	m_vm_paused = true;
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
	m_status_fps_widget->setText(tr("Paused"));
}

void MainWindow::onVMResumed()
{
	// update UI
	{
		QSignalBlocker sb(m_ui.actionPause);
		m_ui.actionPause->setChecked(false);
	}

	m_vm_paused = false;
	m_was_disc_change_request = false;
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
	m_status_fps_widget->setText(m_last_fps_status);
	if (m_display_widget)
		m_display_widget->setFocus();
}

void MainWindow::onVMStopped()
{
	m_vm_valid = false;
	m_vm_paused = false;
	m_last_fps_status = QString();
	updateEmulationActions(false, false);
	updateWindowTitle();
	updateStatusBarWidgetVisibility();
	switchToGameListView();
}

void MainWindow::onGameChanged(const QString& path, const QString& serial, const QString& name, quint32 crc)
{
	m_current_disc_path = path;
	m_current_game_serial = serial;
	m_current_game_name = name;
	m_current_game_crc = crc;
	updateWindowTitle();
	updateSaveStateMenus(path, serial, crc);
}

void MainWindow::onPerformanceMetricsUpdated(const QString& fps_stat, const QString& gs_stat)
{
	m_last_fps_status = fps_stat;
	m_status_fps_widget->setText(m_last_fps_status);
	m_status_gs_widget->setText(gs_stat);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (!requestShutdown(true, true, true))
	{
		event->ignore();
		return;
	}

	saveStateToConfig();
	QMainWindow::closeEvent(event);
}

static QString getFilenameFromMimeData(const QMimeData* md)
{
	QString filename;
	if (md->hasUrls())
	{
		// only one url accepted
		const QList<QUrl> urls(md->urls());
		if (urls.size() == 1)
			filename = urls.front().toLocalFile();
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
	const QString filename(getFilenameFromMimeData(event->mimeData()));
	const std::string filename_str(filename.toStdString());
	if (VMManager::IsSaveStateFileName(filename_str))
	{
		// can't load a save state without a current VM 
		if (m_vm_valid)
		{
			event->acceptProposedAction();
			g_emu_thread->loadState(filename);
		}
		else
		{
			QMessageBox::critical(this, tr("Load State Failed"), tr("Cannot load a save state without a running VM."));
		}
	}
	else if (VMManager::IsLoadableFileName(filename_str))
	{
		// if we're already running, do a disc change, otherwise start
		event->acceptProposedAction();
		if (m_vm_valid)
			doDiscChange(filename);
		else
			doStartDisc(filename);
	}	
}

DisplayWidget* MainWindow::createDisplay(bool fullscreen, bool render_to_main)
{
	DevCon.WriteLn("createDisplay(%u, %u)", static_cast<u32>(fullscreen), static_cast<u32>(render_to_main));

	HostDisplay* host_display = Host::GetHostDisplay();
	if (!host_display)
		return nullptr;

	const std::string fullscreen_mode(Host::GetBaseStringSettingValue("EmuCore/GS", "FullscreenMode", ""));
	const bool is_exclusive_fullscreen = (fullscreen && !fullscreen_mode.empty() && host_display->SupportsFullscreen());

	QWidget* container;
	if (DisplayContainer::IsNeeded(fullscreen, render_to_main))
	{
		m_display_container = new DisplayContainer();
		m_display_widget = new DisplayWidget(m_display_container);
		m_display_container->setDisplayWidget(m_display_widget);
		container = m_display_container;
	}
	else
	{
		m_display_widget = new DisplayWidget((!fullscreen && render_to_main) ? this : nullptr);
		container = m_display_widget;
	}

	if (fullscreen || !render_to_main)
	{
		container->setWindowTitle(windowTitle());
		container->setWindowIcon(windowIcon());
	}

	if (fullscreen)
	{
		if (!is_exclusive_fullscreen)
			container->showFullScreen();
		else
			container->showNormal();
	}
	else if (!render_to_main)
	{
		restoreDisplayWindowGeometryFromConfig();
		container->showNormal();
	}
	else
	{
		m_game_list_widget->setVisible(false);
		takeCentralWidget();
		setCentralWidget(m_display_widget);
	}

	// we need the surface visible.. this might be able to be replaced with something else
	QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	std::optional<WindowInfo> wi = m_display_widget->getWindowInfo();
	if (!wi.has_value())
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to get window info from widget"));
		destroyDisplayWidget();
		return nullptr;
	}

	g_emu_thread->connectDisplaySignals(m_display_widget);

	if (!host_display->CreateRenderDevice(wi.value(), Host::GetStringSettingValue("EmuCore/GS", "Adapter", ""), EmuConfig.GetEffectiveVsyncMode(),
			Host::GetBoolSettingValue("EmuCore/GS", "ThreadedPresentation", false), Host::GetBoolSettingValue("EmuCore/GS", "UseDebugDevice", false)))
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to create host display device context."));
		destroyDisplayWidget();
		return nullptr;
	}

	if (is_exclusive_fullscreen)
		setDisplayFullscreen(fullscreen_mode);

	updateWindowTitle();
	m_display_widget->setFocus();

	host_display->DoneRenderContextCurrent();
	return m_display_widget;
}

DisplayWidget* MainWindow::updateDisplay(bool fullscreen, bool render_to_main, bool surfaceless)
{
	DevCon.WriteLn("updateDisplay() fullscreen=%s render_to_main=%s surfaceless=%s",
		fullscreen ? "true" : "false", render_to_main ? "true" : "false", surfaceless ? "true" : "false");

	HostDisplay* host_display = Host::GetHostDisplay();
	QWidget* container = m_display_container ? static_cast<QWidget*>(m_display_container) : static_cast<QWidget*>(m_display_widget);
	const bool is_fullscreen = isRenderingFullscreen();
	const bool is_rendering_to_main = isRenderingToMain();
	const std::string fullscreen_mode(Host::GetBaseStringSettingValue("EmuCore/GS", "FullscreenMode", ""));
	const bool is_exclusive_fullscreen = (fullscreen && !fullscreen_mode.empty() && host_display->SupportsFullscreen());
	const bool changing_surfaceless = (!m_display_widget != surfaceless);
	if (fullscreen == is_fullscreen && is_rendering_to_main == render_to_main && !changing_surfaceless)
		return m_display_widget;

	// Skip recreating the surface if we're just transitioning between fullscreen and windowed with render-to-main off.
	// .. except on Wayland, where everything tends to break if you don't recreate.
	const bool has_container = (m_display_container != nullptr);
	const bool needs_container = DisplayContainer::IsNeeded(fullscreen, render_to_main);
	if (!is_rendering_to_main && !render_to_main && !is_exclusive_fullscreen && has_container == needs_container && !needs_container && !changing_surfaceless)
	{
		DevCon.WriteLn("Toggling to %s without recreating surface", (fullscreen ? "fullscreen" : "windowed"));
		if (host_display->IsFullscreen())
			host_display->SetFullscreen(false, 0, 0, 0.0f);

		// since we don't destroy the display widget, we need to save it here
		if (!is_fullscreen && !is_rendering_to_main)
			saveDisplayWindowGeometryToConfig();

		if (fullscreen)
		{
			container->showFullScreen();
		}
		else
		{
			restoreDisplayWindowGeometryFromConfig();
			container->showNormal();
		}

		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		return m_display_widget;
	}

	host_display->DestroyRenderSurface();

	destroyDisplayWidget();

	// if we're going to surfaceless, we're done here
	if (surfaceless)
		return nullptr;

	if (DisplayContainer::IsNeeded(fullscreen, render_to_main))
	{
		m_display_container = new DisplayContainer();
		m_display_widget = new DisplayWidget(m_display_container);
		m_display_container->setDisplayWidget(m_display_widget);
		container = m_display_container;
	}
	else
	{
		m_display_widget = new DisplayWidget((!fullscreen && render_to_main) ? this : nullptr);
		container = m_display_widget;
	}

	if (fullscreen || !render_to_main)
	{
		container->setWindowTitle(windowTitle());
		container->setWindowIcon(windowIcon());

		// make sure the game list widget is still visible
		if (centralWidget() != m_game_list_widget && !fullscreen)
		{
			setCentralWidget(m_game_list_widget);
			m_game_list_widget->setVisible(true);
		}
	}

	if (fullscreen)
	{
		if (!is_exclusive_fullscreen)
			container->showFullScreen();
		else
			container->showNormal();
	}
	else if (!render_to_main)
	{
		restoreDisplayWindowGeometryFromConfig();
		container->showNormal();
	}
	else
	{
		m_game_list_widget->setVisible(false);
		takeCentralWidget();
		setCentralWidget(m_display_widget);
		m_display_widget->setFocus();
	}

	// we need the surface visible.. this might be able to be replaced with something else
	QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	std::optional<WindowInfo> wi = m_display_widget->getWindowInfo();
	if (!wi.has_value())
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to get new window info from widget"));
		destroyDisplayWidget();
		return nullptr;
	}

	g_emu_thread->connectDisplaySignals(m_display_widget);

	if (!host_display->ChangeRenderWindow(wi.value()))
		pxFailRel("Failed to recreate surface on new widget.");

	if (is_exclusive_fullscreen)
		setDisplayFullscreen(fullscreen_mode);

	updateWindowTitle();
	m_display_widget->setFocus();

	QSignalBlocker blocker(m_ui.actionFullscreen);
	m_ui.actionFullscreen->setChecked(fullscreen);
	return m_display_widget;
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
		getDisplayContainer()->resize(QSize(std::max<qint32>(width, 1), std::max<qint32>(height, 1)));
		return;
	}

	// we are rendering to the main window. we have to add in the extra height from the toolbar/status bar.
	const s32 extra_height = this->height() - m_display_widget->height();
	resize(QSize(std::max<qint32>(width, 1), std::max<qint32>(height + extra_height, 1)));
}

void MainWindow::destroyDisplay()
{
	destroyDisplayWidget();

	// switch back to game list view, we're not going back to display, so we can't use switchToGameListView().
	if (centralWidget() != m_game_list_widget)
	{
		takeCentralWidget();
		setCentralWidget(m_game_list_widget);
		m_game_list_widget->setVisible(true);
		m_game_list_widget->setFocus();
	}	
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
		QtHost::SetBaseStringSettingValue("UI", "DisplayWindowGeometry", geometry_b64.constData());
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

void MainWindow::destroyDisplayWidget()
{
	if (!m_display_widget)
		return;

	if (!isRenderingFullscreen() && !isRenderingToMain())
		saveDisplayWindowGeometryToConfig();

	if (m_display_container)
		m_display_container->removeDisplayWidget();

	if (m_display_widget == centralWidget())
		takeCentralWidget();

	if (m_display_widget)
	{
		m_display_widget->deleteLater();
		m_display_widget = nullptr;
	}

	if (m_display_container)
	{
		m_display_container->deleteLater();
		m_display_container = nullptr;
	}
}

void MainWindow::setDisplayFullscreen(const std::string& fullscreen_mode)
{
	u32 width, height;
	float refresh_rate;
	if (HostDisplay::ParseFullscreenMode(fullscreen_mode, &width, &height, &refresh_rate))
	{
		if (Host::GetHostDisplay()->SetFullscreen(true, width, height, refresh_rate))
		{
			Host::AddOSDMessage("Acquired exclusive fullscreen.", 10.0f);
		}
		else
		{
			Host::AddOSDMessage("Failed to acquire exclusive fullscreen.", 10.0f);
		}
	}
}

SettingsDialog* MainWindow::getSettingsDialog()
{
	if (!m_settings_dialog)
	{
		m_settings_dialog = new SettingsDialog(this);
		connect(
			m_settings_dialog->getInterfaceSettingsWidget(), &InterfaceSettingsWidget::themeChanged, this, &MainWindow::onThemeChangedFromSettings);
	}

	return m_settings_dialog;
}

void MainWindow::doSettings(const char* category /* = nullptr */)
{
	SettingsDialog* dlg = getSettingsDialog();
	if (!dlg->isVisible())
	{
		dlg->setModal(false);
		dlg->show();
	}

	if (category)
		dlg->setCategory(category);
}

ControllerSettingsDialog* MainWindow::getControllerSettingsDialog()
{
	if (!m_controller_settings_dialog)
		m_controller_settings_dialog = new ControllerSettingsDialog(this);

	return m_controller_settings_dialog;
}

void MainWindow::doControllerSettings(ControllerSettingsDialog::Category category)
{
	ControllerSettingsDialog* dlg = getControllerSettingsDialog();
	if (!dlg->isVisible())
	{
		dlg->setModal(false);
		dlg->show();
	}

	if (category != ControllerSettingsDialog::Category::Count)
		dlg->setCategory(category);
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
	const QString filename(QFileDialog::getOpenFileName(this, tr("Select Cover Image"), QString(), tr("All Cover Image Types (*.jpg *.jpeg *.png)")));
	if (filename.isEmpty())
		return;

	if (!GameList::GetCoverImagePathForEntry(entry).empty())
	{
		if (QMessageBox::question(this, tr("Cover Already Exists"), tr("A cover image for this game already exists, do you wish to replace it?"),
				QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}
	}

	const QString new_filename(QString::fromStdString(GameList::GetNewCoverImagePathForEntry(entry, filename.toUtf8().constData())));
	if (new_filename.isEmpty())
		return;

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

	m_game_list_widget->refreshGridCovers();
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
	if (m_vm_valid)
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
	if (m_vm_valid)
	{
		if (!filename.isEmpty() && m_current_disc_path != filename)
			g_emu_thread->changeDisc(m_current_disc_path);
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

	QAction* action = menu->addAction(is_right_click_menu ? tr("Load State File...") : tr("Load From File..."));
	connect(action, &QAction::triggered, [this, filename]() {
		const QString path(QFileDialog::getOpenFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s)")));
		if (path.isEmpty())
			return;

		loadSaveStateFile(filename, path);
	});

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
		}
	}

	for (s32 i = 1; i <= NUM_SAVE_STATE_SLOTS; i++)
	{
		FILESYSTEM_STAT_DATA sd;
		state_filename = VMManager::GetSaveStateFileName(game_serial_utf8.constData(), crc, i);
		if (!FileSystem::StatFile(state_filename.c_str(), &sd))
			continue;

		action = menu->addAction(tr("Save Slot %1 (%2)").arg(i).arg(formatTimestampForSaveStateMenu(sd.ModificationTime)));
		connect(action, &QAction::triggered, [this, i]() { loadSaveStateSlot(i); });
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
	for (s32 i = 1; i <= NUM_SAVE_STATE_SLOTS; i++)
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

void MainWindow::updateSaveStateMenus(const QString& filename, const QString& serial, quint32 crc)
{
	const bool load_enabled = !serial.isEmpty();
	const bool save_enabled = !serial.isEmpty() && m_vm_valid;
	m_ui.menuLoadState->clear();
	m_ui.menuLoadState->setEnabled(load_enabled);
	m_ui.actionLoadState->setEnabled(load_enabled);
	m_ui.menuSaveState->clear();
	m_ui.menuSaveState->setEnabled(save_enabled);
	m_ui.actionSaveState->setEnabled(save_enabled);
	m_save_states_invalidated = false;
	if (load_enabled)
		populateLoadStateMenu(m_ui.menuLoadState, filename, serial, crc);
	if (save_enabled)
		populateSaveStateMenu(m_ui.menuSaveState, serial, crc);
}

void MainWindow::doStartDisc(const QString& path)
{
	if (m_vm_valid)
		return;

	std::shared_ptr<VMBootParameters> params = std::make_shared<VMBootParameters>();
	params->filename = path.toStdString();

	// we might still be saving a resume state...
	VMManager::WaitForSaveStateFlush();

	const std::optional<bool> resume(
		promptForResumeState(
			QString::fromStdString(VMManager::GetSaveStateFileName(params->filename.c_str(), -1))));
	if (!resume.has_value())
		return;
	else if (resume.value())
		params->state_index = -1;

	g_emu_thread->startVM(std::move(params));
}

void MainWindow::doDiscChange(const QString& path)
{
	bool reset_system = false;
	if (!m_was_disc_change_request)
	{
		const int choice = QMessageBox::question(this, tr("Confirm Disc Change"), tr("Do you want to swap discs or boot the new image (via system reset)?"),
			tr("Swap Disc"), tr("Reset"), tr("Cancel"), 0, 2);
		if (choice == 2)
			return;
		reset_system = (choice != 0);
	}

	switchToEmulationView();

	g_emu_thread->changeDisc(path);
	if (reset_system)
		g_emu_thread->resetVM();
}

MainWindow::VMLock MainWindow::pauseAndLockVM()
{
	const bool was_fullscreen = isRenderingFullscreen();
	const bool was_paused = m_vm_paused;

	// We use surfaceless rather than switching out of fullscreen, because
	// we're paused, so we're not going to be rendering anyway.
	if (was_fullscreen)
		g_emu_thread->setSurfaceless(true);
	if (!was_paused)
		g_emu_thread->setVMPaused(true);

	// We want to parent dialogs to the display widget, except if we were fullscreen,
	// since it's going to get destroyed by the surfaceless call above.
	QWidget* dialog_parent = was_fullscreen ? static_cast<QWidget*>(this) : getDisplayContainer();

	return VMLock(dialog_parent, was_paused, was_fullscreen);
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
	lock.m_was_paused = false;
	lock.m_was_fullscreen = false;
}

MainWindow::VMLock::~VMLock()
{
	if (m_was_fullscreen)
		g_emu_thread->setSurfaceless(false);
	if (!m_was_paused)
		g_emu_thread->setVMPaused(false);
}

