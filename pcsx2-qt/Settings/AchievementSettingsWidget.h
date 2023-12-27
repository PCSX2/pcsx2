// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <QtWidgets/QWidget>
#include "ui_AchievementSettingsWidget.h"

class SettingsWindow;

class AchievementSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit AchievementSettingsWidget(SettingsWindow* dialog, QWidget* parent);
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

	Ui::AchievementSettingsWidget m_ui;

	SettingsWindow* m_dialog;
};
