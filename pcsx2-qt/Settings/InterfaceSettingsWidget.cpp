// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "InterfaceSettingsWidget.h"
#include "AutoUpdaterDialog.h"
#include "MainWindow.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"
#include "QtHost.h"

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
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Ruby (Black/Red) [Dark]"),
	//: Ignore what Crowdin says in this string about "[Light]/[Dark]" being untouchable here, these are not variables in this case and must be translated.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Sapphire (Black/Blue) [Dark]"),
	//: "Custom.qss" must be kept as-is.
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Custom.qss [Drop in PCSX2 Folder]"),
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
	"Ruby",
	"Sapphire",
	"Custom",
	nullptr};

InterfaceSettingsWidget::InterfaceSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.inhibitScreensaver, "EmuCore", "InhibitScreensaver", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.confirmShutdown, "UI", "ConfirmShutdown", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveStateOnShutdown, "EmuCore", "SaveStateOnShutdown", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnFocusLoss, "UI", "PauseOnFocusLoss", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnControllerDisconnection, "UI", "PauseOnControllerDisconnection", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.backupSaveStates, "EmuCore", "BackupSavestate", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.discordPresence, "EmuCore", "EnableDiscordPresence", false);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.startFullscreen, "UI", "StartFullscreen", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.doubleClickTogglesFullscreen, "UI", "DoubleClickTogglesFullscreen",
		true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hideMouseCursor, "UI", "HideMouseCursor", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.renderToSeparateWindow, "UI", "RenderToSeparateWindow", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hideMainWindow, "UI", "HideMainWindowWhenRunning", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableWindowResizing, "UI", "DisableWindowResize", false);
	connect(m_ui.renderToSeparateWindow, &QCheckBox::checkStateChanged, this, &InterfaceSettingsWidget::onRenderToSeparateWindowChanged);

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.theme, "UI", "Theme", THEME_NAMES, THEME_VALUES,
		QtHost::GetDefaultThemeName(), "InterfaceSettingsWidget");
	connect(m_ui.theme, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { emit themeChanged(); });

	populateLanguages();
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.language, "UI", "Language", QtHost::GetDefaultLanguage());
	connect(m_ui.language, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { emit languageChanged(); });

	// Per-game settings is special, we don't want to bind it if we're editing per-game settings.
	if (!dialog->isPerGameSettings())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnStart, "UI", "StartPaused", false);
	}

	if (!dialog->isPerGameSettings() && AutoUpdaterDialog::isSupported())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.autoUpdateEnabled, "AutoUpdater", "CheckAtStartup", true);
		dialog->registerWidgetHelp(m_ui.autoUpdateEnabled, tr("Enable Automatic Update Check"), tr("Checked"),
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

	if (dialog->isPerGameSettings())
	{
		// language/theme doesn't make sense to have in per-game settings
		m_ui.verticalLayout->removeWidget(m_ui.preferencesGroup);
		m_ui.preferencesGroup->hide();

		// start paused doesn't make sense, because settings are applied after ELF load.
		m_ui.pauseOnStart->setEnabled(false);
	}

	dialog->registerWidgetHelp(
		m_ui.inhibitScreensaver, tr("Inhibit Screensaver"), tr("Checked"),
		tr("Prevents the screen saver from activating and the host from sleeping while emulation is running."));
	dialog->registerWidgetHelp(
		m_ui.confirmShutdown, tr("Confirm Shutdown"), tr("Checked"),
		tr("Determines whether a prompt will be displayed to confirm shutting down the virtual machine "
		   "when the hotkey is pressed."));
	dialog->registerWidgetHelp(m_ui.saveStateOnShutdown, tr("Save State On Shutdown"), tr("Unchecked"),
		tr("Automatically saves the emulator state when powering down or exiting. You can then "
		   "resume directly from where you left off next time."));
	dialog->registerWidgetHelp(m_ui.pauseOnStart, tr("Pause On Start"), tr("Unchecked"),
		tr("Pauses the emulator when a game is started."));
	dialog->registerWidgetHelp(m_ui.pauseOnFocusLoss, tr("Pause On Focus Loss"), tr("Unchecked"),
		tr("Pauses the emulator when you minimize the window or switch to another application, "
		   "and unpauses when you switch back."));
	dialog->registerWidgetHelp(m_ui.pauseOnControllerDisconnection, tr("Pause On Controller Disconnection"),
		tr("Unchecked"), tr("Pauses the emulator when a controller with bindings is disconnected."));
	dialog->registerWidgetHelp(m_ui.backupSaveStates, tr("Create Save State Backups"), tr("Checked"),
		//: Do not translate the ".backup" extension.
		tr("Creates a backup copy of a save state if it already exists when the save is created. The backup copy has a .backup suffix."));
	dialog->registerWidgetHelp(m_ui.startFullscreen, tr("Start Fullscreen"), tr("Unchecked"),
		tr("Automatically switches to fullscreen mode when a game is started."));
	dialog->registerWidgetHelp(m_ui.hideMouseCursor, tr("Hide Cursor In Fullscreen"), tr("Unchecked"),
		tr("Hides the mouse pointer/cursor when the emulator is in fullscreen mode."));
	dialog->registerWidgetHelp(
		m_ui.renderToSeparateWindow, tr("Render To Separate Window"), tr("Unchecked"),
		tr("Renders the game to a separate window, instead of the main window. If unchecked, the game will display over the top of the game list."));
	dialog->registerWidgetHelp(
		m_ui.hideMainWindow, tr("Hide Main Window When Running"), tr("Unchecked"),
		tr("Hides the main window (with the game list) when a game is running, requires Render To Separate Window to be enabled."));
	dialog->registerWidgetHelp(
		m_ui.discordPresence, tr("Enable Discord Presence"), tr("Unchecked"),
		tr("Shows the game you are currently playing as part of your profile in Discord."));
	dialog->registerWidgetHelp(
		m_ui.doubleClickTogglesFullscreen, tr("Double-Click Toggles Fullscreen"), tr("Checked"),
		tr("Allows switching in and out of fullscreen mode by double-clicking the game window."));
	dialog->registerWidgetHelp(
		m_ui.disableWindowResizing, tr("Disable Window Resizing"), tr("Unchecked"),
		tr("Prevents the main window from being resized."));

	onRenderToSeparateWindowChanged();
}

InterfaceSettingsWidget::~InterfaceSettingsWidget() = default;

void InterfaceSettingsWidget::onRenderToSeparateWindowChanged()
{
	m_ui.hideMainWindow->setEnabled(m_ui.renderToSeparateWindow->isChecked());
}

void InterfaceSettingsWidget::populateLanguages()
{
	for (const std::pair<QString, QString>& it : QtHost::GetAvailableLanguageList())
		m_ui.language->addItem(it.first, it.second);
}
