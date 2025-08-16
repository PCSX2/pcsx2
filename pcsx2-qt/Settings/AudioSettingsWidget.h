// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_AudioSettingsWidget.h"

#include "SettingsWidget.h"

enum class AudioBackend : u8;
enum class AudioExpansionMode : u8;

class AudioSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	AudioSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~AudioSettingsWidget();

private Q_SLOTS:
	void onExpansionModeChanged();
	void onSyncModeChanged();

	void updateDriverNames();
	void updateDeviceNames();
	void updateLatencyLabel();
	void updateVolumeLabel();
	void onMinimalOutputLatencyChanged();
	void onOutputVolumeChanged(int new_value);
	void onFastForwardVolumeChanged(int new_value);
	void onOutputMutedChanged(int new_state);

	void onExpansionSettingsClicked();
	void onStretchSettingsClicked();

private:
	AudioBackend getEffectiveBackend() const;
	AudioExpansionMode getEffectiveExpansionMode() const;
	u32 getEffectiveExpansionBlockSize() const;
	void resetVolume(bool fast_forward);

	Ui::AudioSettingsWidget m_ui;
	u32 m_output_device_latency = 0;
};
