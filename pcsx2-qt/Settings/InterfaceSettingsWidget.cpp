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

#include "InterfaceSettingsWidget.h"
#include "AutoUpdaterDialog.h"
#include "MainWindow.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

static const char* THEME_NAMES[] = {QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Native [Light]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Fusion [Light]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Dark Fusion (Gray) [Dark]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Dark Fusion (Blue) [Dark]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Untouched Lagoon (Grayish Green/-Blue ) [Light]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Baby Pastel (Pink) [Light]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "PCSX2 (White/Blue) [Light]"),
	QT_TRANSLATE_NOOP("InterfaceSettingsWidget", "Scarlet Devil (Red/Purple) [Dark]"), nullptr};

static const char* THEME_VALUES[] = {"", "fusion", "darkfusion", "darkfusionblue", 
	"UntouchedLagoon", "BabyPastel", "PCSX2Blue", "ScarletDevilRed", nullptr};

InterfaceSettingsWidget::InterfaceSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.inhibitScreensaver, "UI", "InhibitScreensaver", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.confirmShutdown, "UI", "ConfirmShutdown", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveStateOnShutdown, "EmuCore", "SaveStateOnShutdown", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnStart, "UI", "StartPaused", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnFocusLoss, "UI", "PauseOnFocusLoss", false);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.startFullscreen, "UI", "StartFullscreen", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.doubleClickTogglesFullscreen, "UI", "DoubleClickTogglesFullscreen",
		true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hideMouseCursor, "UI", "HideMouseCursor", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.renderToMainWindow, "UI", "RenderToMainWindow", true);

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.theme, "UI", "Theme", THEME_NAMES, THEME_VALUES,
		MainWindow::DEFAULT_THEME_NAME);
	connect(m_ui.theme, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { emit themeChanged(); });

	dialog->registerWidgetHelp(
		m_ui.inhibitScreensaver, tr("Inhibit Screensaver"), tr("Checked"),
		tr("Prevents the screen saver from activating and the host from sleeping while emulation is running."));

	if (AutoUpdaterDialog::isSupported())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.autoUpdateEnabled, "AutoUpdater", "CheckAtStartup", true);
		dialog->registerWidgetHelp(m_ui.autoUpdateEnabled, tr("Enable Automatic Update Check"), tr("Checked"),
			tr("Automatically checks for updates to the program on startup. Updates can be deferred "
			   "until later or skipped entirely."));

		m_ui.autoUpdateTag->addItems(AutoUpdaterDialog::getTagList());
		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.autoUpdateTag, "AutoUpdater", "UpdateTag",
			AutoUpdaterDialog::getDefaultTag());

		m_ui.autoUpdateCurrentVersion->setText(tr("%1 (%2)").arg(AutoUpdaterDialog::getCurrentVersion()).arg(AutoUpdaterDialog::getCurrentVersionDate()));
		connect(m_ui.checkForUpdates, &QPushButton::clicked, this, []() { g_main_window->checkForUpdates(true); });
	}
	else
	{
		m_ui.verticalLayout->removeWidget(m_ui.automaticUpdaterGroup);
		m_ui.automaticUpdaterGroup->hide();
	}

	dialog->registerWidgetHelp(
		m_ui.confirmShutdown, tr("Confirm Shutdown"), tr("Checked"),
		tr("Determines whether a prompt will be displayed to confirm shutting down the virtual machine "
		   "when the hotkey is pressed."));
	dialog->registerWidgetHelp(m_ui.saveStateOnShutdown, tr("Save State On Shutdown"), tr("Checked"),
		tr("Automatically saves the emulator state when powering down or exiting. You can then "
		   "resume directly from where you left off next time."));
	dialog->registerWidgetHelp(m_ui.pauseOnStart, tr("Pause On Start"), tr("Unchecked"),
		tr("Pauses the emulator when a game is started."));
	dialog->registerWidgetHelp(m_ui.pauseOnFocusLoss, tr("Pause On Focus Loss"), tr("Unchecked"),
		tr("Pauses the emulator when you minimize the window or switch to another application, "
		   "and unpauses when you switch back."));
	dialog->registerWidgetHelp(m_ui.startFullscreen, tr("Start Fullscreen"), tr("Unchecked"),
		tr("Automatically switches to fullscreen mode when a game is started."));
	dialog->registerWidgetHelp(m_ui.hideMouseCursor, tr("Hide Cursor In Fullscreen"), tr("Checked"),
		tr("Hides the mouse pointer/cursor when the emulator is in fullscreen mode."));
	dialog->registerWidgetHelp(
		m_ui.renderToMainWindow, tr("Render To Main Window"), tr("Checked"),
		tr("Renders the display of the simulated console to the main window of the application, over "
		   "the game list. If unchecked, the display will render in a separate window."));
	
	// Not yet used, disable the options
	m_ui.pauseOnStart->setDisabled(true);
	m_ui.pauseOnFocusLoss->setDisabled(true);
	m_ui.disableWindowResizing->setDisabled(true);
	m_ui.hideMouseCursor->setDisabled(true);
	m_ui.language->setDisabled(true);
}

InterfaceSettingsWidget::~InterfaceSettingsWidget() = default;
