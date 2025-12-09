// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_AchievementSettingsWidget.h"

#include "SettingsWidget.h"

class AchievementSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	explicit AchievementSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~AchievementSettingsWidget();

private Q_SLOTS:
	void updateEnableState();
	void onHardcoreModeStateChanged();
	void onAchievementsNotificationDurationSliderChanged();
	void onLeaderboardsNotificationDurationSliderChanged();
	void onLoginLogoutPressed();
	void onViewProfilePressed();
	void onAchievementsRefreshed(quint32 id, const QString& game_info_string);

private:
	void updateLoginState();
	static const char* AUDIO_FILE_FILTER;

	Ui::AchievementSettingsWidget m_ui;
};
