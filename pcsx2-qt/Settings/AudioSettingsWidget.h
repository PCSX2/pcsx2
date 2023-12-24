// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_AudioSettingsWidget.h"

class SettingsWindow;

class AudioSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	AudioSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~AudioSettingsWidget();

private Q_SLOTS:
	void expansionModeChanged();
	void outputModuleChanged();
	void outputBackendChanged();
	void updateDevices();
	void volumeChanged(int value);
	void volumeContextMenuRequested(const QPoint& pt);
	void updateTargetLatencyRange();
	void updateLatencyLabels();
	void onMinimalOutputLatencyStateChanged();
	void resetTimestretchDefaults();

private:
	void populateOutputModules();
	void updateVolumeLabel();

	SettingsWindow* m_dialog;
	Ui::AudioSettingsWidget m_ui;
	u32 m_output_device_latency = 0;
};
