// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_OSDSettingsWidget.h"
#include "SettingsWidget.h"

#include <QtCore/QPointer>

class OSDFontPickerDialog;

class OSDSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	OSDSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~OSDSettingsWidget();

private Q_SLOTS:
	void onMessagesPosChanged();
	void onPerformancePosChanged();
	void onOsdShowSettingsToggled();
	void onBrowseOsdFontPathClicked();
	void onClearOsdFontPathClicked();
	void onSelectAllClicked();
	void onDeselectAllClicked();

private:
	void loadOsdFontPathSetting();
	void saveOsdFontPathSetting(const QString& path);

	Ui::OSDSettingsWidget m_ui;
	QPointer<OSDFontPickerDialog> m_font_picker;
};
