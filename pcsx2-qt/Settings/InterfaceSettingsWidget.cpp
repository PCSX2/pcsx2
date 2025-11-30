// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "InterfaceSettingsWidget.h"
#include "AutoUpdaterDialog.h"
#include "Common.h"
#include "Host.h"
#include "MainWindow.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"
#include "QtHost.h"

#include <QtCore/QLocale>

const char* InterfaceSettingsWidget::THEME_NAMES[] = {
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Native"),
//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
#ifdef _WIN32
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Classic Windows"),
#endif
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Fusion [Light/Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Dark Fusion (Gray) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Dark Fusion (Blue) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Grey Matter (Gray) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Untouched Lagoon (Grayish Green/-Blue ) [Light]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Baby Pastel (Pink) [Light]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Pizza Time! (Brown-ish/Creamy White) [Light]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "PCSX2 (White/Blue) [Light]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Scarlet Devil (Red/Purple) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Violet Angel (Blue/Purple) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Cobalt Sky (Blue) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "AMOLED (Black) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Ruby (Black/Red) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Sapphire (Black/Blue) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Emerald (Black/Green) [Dark]"),
	//: "custom.qss" must be kept as-is.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "custom.qss [Drop in PCSX2 Folder]"),
	nullptr};

const char* InterfaceSettingsWidget::THEME_VALUES[] = {
	"",
#ifdef _WIN32
	"windowsvista",
#endif
	"fusion",
	"darkfusion",
	"darkfusionblue",
	"GreyMatter",
	"UntouchedLagoon",
	"BabyPastel",
	"PizzaBrown",
	"PCSX2Blue",
	"ScarletDevilRed",
	"VioletAngelPurple",
	"CobaltSky",
	"AMOLED",
	"Ruby",
	"Sapphire",
	"Emerald",
	"Custom",
	nullptr};

const char* InterfaceSettingsWidget::BACKGROUND_SCALE_NAMES[] = {
	"fit",
	"fill",
	"stretch",
	"center",
	"tile",
	nullptr};

const char* InterfaceSettingsWidget::IMAGE_FILE_FILTER = QT_TRANSLATE_NOOP("InterfaceSettingsWidget",
	"Supported Image Types (*.bmp *.gif *.jpg *.jpeg *.png *.webp)");

InterfaceSettingsWidget::InterfaceSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.inhibitScreensaver, "EmuCore", "InhibitScreensaver", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.confirmShutdown, "UI", "ConfirmShutdown", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnFocusLoss, "UI", "PauseOnFocusLoss", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnControllerDisconnection, "UI", "PauseOnControllerDisconnection", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.promptOnStateLoadSaveFailure, "UI", "PromptOnStateLoadSaveFailure", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.discordPresence, "EmuCore", "EnableDiscordPresence", false);

#ifdef __linux__ // Mouse locking is only supported on X11
	const bool mouse_lock_supported = QGuiApplication::platformName().toLower() == "xcb";
#else
	const bool mouse_lock_supported = true;
#endif

	if (mouse_lock_supported)
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.mouseLock, "EmuCore", "EnableMouseLock", false);
		connect(m_ui.mouseLock, &QCheckBox::checkStateChanged, [](Qt::CheckState state) {
			if (state == Qt::Checked)
				Common::AttachMousePositionCb([](int x, int y) { g_main_window->checkMousePosition(x, y); });
			else
				Common::DetachMousePositionCb();
		});
	}
	else
	{
		m_ui.mouseLock->setEnabled(false);
	}

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.startFullscreen, "UI", "StartFullscreen", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.doubleClickTogglesFullscreen, "UI", "DoubleClickTogglesFullscreen",
		true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hideMouseCursor, "UI", "HideMouseCursor", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.renderToSeparateWindow, "UI", "RenderToSeparateWindow", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hideMainWindow, "UI", "HideMainWindowWhenRunning", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableWindowResizing, "UI", "DisableWindowResize", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.startFullscreenUI, "UI", "StartBigPictureMode", false);
	connect(m_ui.renderToSeparateWindow, &QCheckBox::checkStateChanged, this, &InterfaceSettingsWidget::onRenderToSeparateWindowChanged);

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.theme, "UI", "Theme", THEME_NAMES, THEME_VALUES,
		QtHost::GetDefaultThemeName(), "InterfaceSettingsWidget");
	connect(m_ui.theme, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { emit themeChanged(); });

	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.backgroundOpacity, "UI", "GameListBackgroundOpacity", 100.0f);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.backgroundScale, "UI", "GameListBackgroundMode", BACKGROUND_SCALE_NAMES, QtUtils::ScalingMode::Fit);
	connect(m_ui.backgroundBrowse, &QPushButton::clicked, [this]() { onSetGameListBackgroundTriggered(); });
	connect(m_ui.backgroundReset, &QPushButton::clicked, [this]() { onClearGameListBackgroundTriggered(); });
	connect(m_ui.backgroundOpacity, &QSpinBox::editingFinished, [this]() { emit backgroundChanged(); });
	connect(m_ui.backgroundScale, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { emit backgroundChanged(); });

	populateLanguages();
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.language, "UI", "Language", QtHost::GetDefaultLanguage());
	connect(m_ui.language, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { emit languageChanged(); });

	// Per-game settings is special, we don't want to bind it if we're editing per-game settings.
	if (!dialog()->isPerGameSettings())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnStart, "UI", "StartPaused", false);
	}

	if (!dialog()->isPerGameSettings() && AutoUpdaterDialog::isSupported())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.autoUpdateEnabled, "AutoUpdater", "CheckAtStartup", true);
		dialog()->registerWidgetHelp(m_ui.autoUpdateEnabled, tr("Enable Automatic Update Check"), tr("Checked"),
			tr("Automatically checks for updates to the program on startup. Updates can be deferred "
			   "until later or skipped entirely."));

		m_ui.autoUpdateTag->addItems(AutoUpdaterDialog::getTagList());
		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.autoUpdateTag, "AutoUpdater", "UpdateTag",
			AutoUpdaterDialog::getDefaultTag());

		//: Variable %1 shows the version number and variable %2 shows a timestamp.
		m_ui.autoUpdateCurrentVersion->setText(tr("%1 (%2)").arg(AutoUpdaterDialog::getCurrentVersion()).arg(AutoUpdaterDialog::getCurrentVersionDate()));
		connect(m_ui.checkForUpdates, &QPushButton::clicked, this, []() { g_main_window->checkForUpdates(true, true); });
	}
	else
	{
		m_ui.verticalLayout->removeWidget(m_ui.automaticUpdaterGroup);
		m_ui.automaticUpdaterGroup->hide();
	}

	if (dialog()->isPerGameSettings())
	{
		// language/theme doesn't make sense to have in per-game settings
		m_ui.verticalLayout->removeWidget(m_ui.appearancesGroup);
		m_ui.appearancesGroup->hide();

		// start paused doesn't make sense, because settings are applied after ELF load.
		m_ui.pauseOnStart->setEnabled(false);
	}

	dialog()->registerWidgetHelp(
		m_ui.inhibitScreensaver, tr("Inhibit Screensaver"), tr("Checked"),
		tr("Prevents the screen saver from activating and the host from sleeping while emulation is running."));
	dialog()->registerWidgetHelp(
		m_ui.confirmShutdown, tr("Confirm Shutdown"), tr("Checked"),
		tr("Determines whether a prompt will be displayed to confirm shutting down the virtual machine "
		   "when the hotkey is pressed."));
	dialog()->registerWidgetHelp(m_ui.pauseOnStart, tr("Pause On Start"), tr("Unchecked"),
		tr("Pauses the emulator when a game is started."));
	dialog()->registerWidgetHelp(m_ui.pauseOnFocusLoss, tr("Pause On Focus Loss"), tr("Unchecked"),
		tr("Pauses the emulator when you minimize the window or switch to another application, "
		   "and unpauses when you switch back."));
	dialog()->registerWidgetHelp(m_ui.pauseOnControllerDisconnection, tr("Pause On Controller Disconnection"),
		tr("Unchecked"), tr("Pauses the emulator when a controller with bindings is disconnected."));
	dialog()->registerWidgetHelp(m_ui.promptOnStateLoadSaveFailure, tr("Pause On State Load/Save Failure"),
		tr("Checked"), tr("Display a modal dialog when a save state load/save operation fails."));
	dialog()->registerWidgetHelp(m_ui.startFullscreen, tr("Start Fullscreen"), tr("Unchecked"),
		tr("Automatically switches to fullscreen mode when a game is started."));
	dialog()->registerWidgetHelp(m_ui.hideMouseCursor, tr("Hide Cursor In Fullscreen"), tr("Unchecked"),
		tr("Hides the mouse pointer/cursor when the emulator is in fullscreen mode."));
	dialog()->registerWidgetHelp(
		m_ui.renderToSeparateWindow, tr("Render To Separate Window"), tr("Unchecked"),
		tr("Renders the game to a separate window, instead of the main window. If unchecked, the game will display over the game list."));
	dialog()->registerWidgetHelp(
		m_ui.hideMainWindow, tr("Hide Main Window When Running"), tr("Unchecked"),
		tr("Hides the main window (with the game list) when a game is running. Requires Render To Separate Window to be enabled."));
	dialog()->registerWidgetHelp(
		m_ui.discordPresence, tr("Enable Discord Presence"), tr("Unchecked"),
		tr("Shows the game you are currently playing as part of your profile in Discord."));
	dialog()->registerWidgetHelp(
		m_ui.mouseLock, tr("Enable Mouse Lock"), tr("Unchecked"),
		tr("Locks the mouse cursor to the windows when PCSX2 is in focus and all other windows are closed.<br><b>Unavailable on Linux Wayland.</b><br><b>Requires accessibility permissions on macOS.</b>"));
	dialog()->registerWidgetHelp(
		m_ui.doubleClickTogglesFullscreen, tr("Double-Click Toggles Fullscreen"), tr("Checked"),
		tr("Allows switching in and out of fullscreen mode by double-clicking the game window."));
	dialog()->registerWidgetHelp(
		m_ui.disableWindowResizing, tr("Disable Window Resizing"), tr("Unchecked"),
		tr("Prevents the main window from being resized."));
	dialog()->registerWidgetHelp(
		m_ui.startFullscreenUI, tr("Start Big Picture Mode"), tr("Unchecked"),
		tr("Automatically starts Big Picture Mode instead of the regular Qt interface when PCSX2 launches."));
	dialog()->registerWidgetHelp(
		m_ui.backgroundBrowse, tr("Game List Background"), tr("None"),
		tr("Enable an animated/static background on the game list (where you launch your games).<br>"
		   "This background is only visible in the library and will be hidden once a game is launched. It will also be paused when it's not in focus."));
	dialog()->registerWidgetHelp(
		m_ui.backgroundReset, tr("Disable/Reset Game List Background"), tr("None"),
		tr("Disable and reset the currently applied game list background."));
	dialog()->registerWidgetHelp(
		m_ui.backgroundOpacity, tr("Game List Background Opacity"), tr("100%"),
		tr("Sets the opacity of the custom background."));
	dialog()->registerWidgetHelp(
		m_ui.backgroundScale, tr("Background Image Scaling"), tr("Fit"),
		tr("Select how to display the background image: <br><br>Fit (Preserve aspect ratio, fit to screen)"
		   "<br>Fill (Preserve aspect ratio, fill the screen) <br>Stretch (Ignore aspect ratio) <br>Center (Centers the image without any scaling) <br>Tile (Repeat the image to fill the screen)"));

	onRenderToSeparateWindowChanged();
}

InterfaceSettingsWidget::~InterfaceSettingsWidget() = default;

void InterfaceSettingsWidget::updatePromptOnStateLoadSaveFailureCheckbox(Qt::CheckState state)
{
	QSignalBlocker blocker(m_ui.promptOnStateLoadSaveFailure);
	m_ui.promptOnStateLoadSaveFailure->setCheckState(state);
}

void InterfaceSettingsWidget::onRenderToSeparateWindowChanged()
{
	m_ui.hideMainWindow->setEnabled(m_ui.renderToSeparateWindow->isChecked());
}

void InterfaceSettingsWidget::populateLanguages()
{
	for (const std::pair<QString, QString>& it : QtHost::GetAvailableLanguageList())
	{
		QIcon flag_icon = QtUtils::GetFlagIconForLanguage(it.second);
		if (!flag_icon.isNull())
			m_ui.language->addItem(flag_icon, it.first, it.second);
		else
			m_ui.language->addItem(it.first, it.second);
	}
}

void InterfaceSettingsWidget::onSetGameListBackgroundTriggered()
{
	const QString path = QDir::toNativeSeparators(
		QFileDialog::getOpenFileName(this, tr("Select Background Image"), QString(), qApp->translate("InterfaceSettingsWidget", IMAGE_FILE_FILTER)));

	if (path.isEmpty())
		return;

	std::string relative_path = Path::MakeRelative(QDir::toNativeSeparators(path).toStdString(), EmuFolders::DataRoot);
	Host::SetBaseStringSettingValue("UI", "GameListBackgroundPath", relative_path.c_str());

	Host::CommitBaseSettingChanges();
	emit backgroundChanged();
}

void InterfaceSettingsWidget::onClearGameListBackgroundTriggered()
{
	Host::RemoveBaseSettingValue("UI", "GameListBackgroundPath");
	Host::CommitBaseSettingChanges();
	emit backgroundChanged();
}
