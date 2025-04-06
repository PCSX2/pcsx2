// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugUserInterfaceSettingsWidget.h"

#include "SettingWidgetBinder.h"
#include "Debugger/DebuggerWindow.h"

static const char* s_drop_indicators[] = {
	QT_TRANSLATE_NOOP("DebugUserInterfaceSettingsWidget", "Classic"),
	QT_TRANSLATE_NOOP("DebugUserInterfaceSettingsWidget", "Segmented"),
	QT_TRANSLATE_NOOP("DebugUserInterfaceSettingsWidget", "Minimalistic"),
	nullptr,
};

DebugUserInterfaceSettingsWidget::DebugUserInterfaceSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.refreshInterval, "Debugger/UserInterface", "RefreshInterval", 1000);
	connect(m_ui.refreshInterval, &QSpinBox::valueChanged, this, []() {
		if (g_debugger_window)
			g_debugger_window->updateFromSettings();
	});
	dialog->registerWidgetHelp(
		m_ui.refreshInterval, tr("Refresh Interval"), tr("1000ms"),
		tr("The amount of time to wait between subsequent attempts to update the user interface to reflect the state "
		   "of the virtual machine."));

	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.showOnStartup, "Debugger/UserInterface", "ShowOnStartup", false);
	dialog->registerWidgetHelp(
		m_ui.showOnStartup, tr("Show On Startup"), tr("Unchecked"),
		tr("Open the debugger window automatically when PCSX2 starts."));

	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.saveWindowGeometry, "Debugger/UserInterface", "SaveWindowGeometry", true);
	dialog->registerWidgetHelp(
		m_ui.saveWindowGeometry, tr("Save Window Geometry"), tr("Checked"),
		tr("Save the position and size of the debugger window when it is closed so that it can be restored later."));

	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif,
		m_ui.dropIndicator,
		"Debugger/UserInterface",
		"DropIndicatorStyle",
		s_drop_indicators,
		s_drop_indicators,
		s_drop_indicators[0],
		"DebugUserInterfaceSettingsWidget");
	dialog->registerWidgetHelp(
		m_ui.dropIndicator, tr("Drop Indicator Style"), tr("Classic"),
		tr("Choose how the drop indicators that appear when you drag dock windows in the debugger are styled. "
		   "You will have to restart the debugger for this option to take effect."));
}
