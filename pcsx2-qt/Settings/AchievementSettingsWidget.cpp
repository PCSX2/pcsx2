// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "AchievementSettingsWidget.h"
#include "AchievementLoginDialog.h"
#include "MainWindow.h"
#include "SettingsWindow.h"
#include "SettingWidgetBinder.h"
#include "QtUtils.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/Host.h"

#include "common/StringUtil.h"

#include <QtCore/QDateTime>
#include <QtWidgets/QMessageBox>

const char* AUDIO_FILE_FILTER = QT_TRANSLATE_NOOP("MainWindow", "Audio Files (*.wav)");

AchievementSettingsWidget::AchievementSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enable, "Achievements", "Enabled", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hardcoreMode, "Achievements", "ChallengeMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.achievementNotifications, "Achievements", "Notifications", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.leaderboardNotifications, "Achievements", "LeaderboardNotifications", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.soundEffects, "Achievements", "SoundEffects", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.notificationSound, "Achievements", "InfoSound", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.unlockSound, "Achievements", "UnlockSound", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.lbSound, "Achievements", "LBSubmitSound", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.overlays, "Achievements", "Overlays", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.leaderboardOverlays, "Achievements", "LBOverlays", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.encoreMode, "Achievements", "EncoreMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.spectatorMode, "Achievements", "SpectatorMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.unofficialAchievements, "Achievements", "UnofficialTestMode",false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.achievementNotificationsDuration, "Achievements", "NotificationsDuration", Pcsx2Config::AchievementsOptions::DEFAULT_NOTIFICATION_DURATION);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.leaderboardNotificationsDuration, "Achievements", "LeaderboardsDuration", Pcsx2Config::AchievementsOptions::DEFAULT_LEADERBOARD_DURATION);

	SettingWidgetBinder::BindWidgetToFileSetting(sif, m_ui.notificationSoundPath, m_ui.notificationSoundBrowse, m_ui.notificationSoundOpen, m_ui.notificationSoundReset, "Achievements", "InfoSoundName", Path::Combine(EmuFolders::Resources, EmuConfig.Achievements.DEFAULT_INFO_SOUND_NAME), AUDIO_FILE_FILTER, true, false);
	SettingWidgetBinder::BindWidgetToFileSetting(sif, m_ui.unlockSoundPath, m_ui.unlockSoundBrowse, m_ui.unlockSoundOpen, m_ui.unlockSoundReset, "Achievements", "UnlockSoundName", Path::Combine(EmuFolders::Resources, EmuConfig.Achievements.DEFAULT_UNLOCK_SOUND_NAME), AUDIO_FILE_FILTER, true, false);
	SettingWidgetBinder::BindWidgetToFileSetting(sif, m_ui.lbSoundPath, m_ui.lbSoundBrowse, m_ui.lbSoundOpen, m_ui.lbSoundReset, "Achievements", "LBSubmitSoundName", Path::Combine(EmuFolders::Resources, EmuConfig.Achievements.DEFAULT_LBSUBMIT_SOUND_NAME), AUDIO_FILE_FILTER, true, false);

	dialog->registerWidgetHelp(m_ui.enable, tr("Enable Achievements"), tr("Unchecked"), tr("When enabled and logged in, PCSX2 will scan for achievements on startup."));
	dialog->registerWidgetHelp(m_ui.hardcoreMode, tr("Enable Hardcore Mode"), tr("Unchecked"), tr("\"Challenge\" mode for achievements, including leaderboard tracking. Disables save state, cheats, and slowdown functions."));
	dialog->registerWidgetHelp(m_ui.achievementNotifications, tr("Show Achievement Notifications"), tr("Checked"), tr("Displays popup messages on events such as achievement unlocks and game completion."));
	dialog->registerWidgetHelp(m_ui.leaderboardNotifications, tr("Show Leaderboard Notifications"), tr("Checked"), tr("Displays popup messages when starting, submitting, or failing a leaderboard challenge."));
	dialog->registerWidgetHelp(m_ui.soundEffects, tr("Enable Sound Effects"), tr("Checked"), tr("Plays sound effects for events such as achievement unlocks and leaderboard submissions."));
	dialog->registerWidgetHelp(m_ui.soundEffectsBox, tr("Custom Sound Effect"), tr("Any"), tr("Customize the sound effect that are played whenever you received a notification, earned an achievement or submitted an entry to the leaderboard."));
	dialog->registerWidgetHelp(m_ui.overlays, tr("Enable In-Game Overlays"), tr("Checked"), tr("Shows icons in the lower-right corner of the screen when a challenge/primed achievement is active."));
	dialog->registerWidgetHelp(m_ui.leaderboardOverlays, tr("Enable In-Game Leaderboard Overlays"), tr("Checked"), tr("Shows icons in the lower-right corner of the screen when leaderboard tracking is active."));
	dialog->registerWidgetHelp(m_ui.encoreMode, tr("Enable Encore Mode"), tr("Unchecked"),tr("When enabled, each session will behave as if no achievements have been unlocked."));
	dialog->registerWidgetHelp(m_ui.spectatorMode, tr("Enable Spectator Mode"), tr("Unchecked"), tr("When enabled, PCSX2 will assume all achievements are locked and not send any unlock notifications to the server."));
	dialog->registerWidgetHelp(m_ui.unofficialAchievements, tr("Test Unofficial Achievements"), tr("Unchecked"), tr("When enabled, PCSX2 will list achievements from unofficial sets. Please note that these achievements are not tracked by RetroAchievements, so they unlock every time."));

	connect(m_ui.enable, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.hardcoreMode, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.hardcoreMode, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::onHardcoreModeStateChanged);
	connect(m_ui.achievementNotifications, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.leaderboardNotifications, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.soundEffects, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.notificationSound, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.unlockSound, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.lbSound, &QCheckBox::checkStateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.achievementNotificationsDuration, &QSlider::valueChanged, this, &AchievementSettingsWidget::onAchievementsNotificationDurationSliderChanged);
	connect(m_ui.leaderboardNotificationsDuration, &QSlider::valueChanged, this, &AchievementSettingsWidget::onLeaderboardsNotificationDurationSliderChanged);

	if (!m_dialog->isPerGameSettings())
	{
		connect(m_ui.loginButton, &QPushButton::clicked, this, &AchievementSettingsWidget::onLoginLogoutPressed);
		connect(m_ui.viewProfile, &QPushButton::clicked, this, &AchievementSettingsWidget::onViewProfilePressed);
		connect(g_emu_thread, &EmuThread::onAchievementsRefreshed, this, &AchievementSettingsWidget::onAchievementsRefreshed);
		updateLoginState();

		// force a refresh of game info
		Host::RunOnCPUThread(Host::OnAchievementsRefreshed);
	}
	else
	{
		// remove login and game info, not relevant for per-game
		m_ui.verticalLayout->removeWidget(m_ui.gameInfoBox);
		m_ui.gameInfoBox->deleteLater();
		m_ui.gameInfoBox = nullptr;
		m_ui.verticalLayout->removeWidget(m_ui.loginBox);
		m_ui.loginBox->deleteLater();
		m_ui.loginBox = nullptr;

		// sound effects
		m_ui.verticalLayout->removeWidget(m_ui.soundEffectsBox);
		m_ui.soundEffectsBox->deleteLater();
		m_ui.soundEffectsBox = nullptr;
	}

	updateEnableState();
	onAchievementsNotificationDurationSliderChanged();
	onLeaderboardsNotificationDurationSliderChanged();
}

AchievementSettingsWidget::~AchievementSettingsWidget() = default;

void AchievementSettingsWidget::updateEnableState()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("Achievements", "Enabled", false);
	const bool notifications = enabled && m_dialog->getEffectiveBoolValue("Achievements", "Notifications", true);
	const bool lb_notifications = enabled && m_dialog->getEffectiveBoolValue("Achievements", "LeaderboardNotifications", true);
	const bool sound = m_dialog->getEffectiveBoolValue("Achievements", "SoundEffects", true);
	const bool info = enabled && sound && m_dialog->getEffectiveBoolValue("Achievements", "InfoSound", true);
	const bool unlock = enabled && sound && m_dialog->getEffectiveBoolValue("Achievements", "UnlockSound", true);
	const bool lbsound = enabled && sound && m_dialog->getEffectiveBoolValue("Achievements", "LBSubmitSound", true);
	m_ui.hardcoreMode->setEnabled(enabled);
	m_ui.achievementNotifications->setEnabled(enabled);
	m_ui.leaderboardNotifications->setEnabled(enabled);
	m_ui.achievementNotificationsDuration->setEnabled(notifications);
	m_ui.achievementNotificationsDurationLabel->setEnabled(notifications);
	m_ui.leaderboardNotificationsDuration->setEnabled(lb_notifications);
	m_ui.leaderboardNotificationsDurationLabel->setEnabled(lb_notifications);

	if (!m_dialog->isPerGameSettings())
	{
		m_ui.notificationSoundPath->setEnabled(info);
		m_ui.notificationSoundBrowse->setEnabled(info);
		m_ui.notificationSoundOpen->setEnabled(info);
		m_ui.notificationSoundReset->setEnabled(info);
		m_ui.notificationSound->setEnabled(enabled);
		m_ui.unlockSoundPath->setEnabled(unlock);
		m_ui.unlockSoundBrowse->setEnabled(unlock);
		m_ui.unlockSoundOpen->setEnabled(unlock);
		m_ui.unlockSoundReset->setEnabled(unlock);
		m_ui.unlockSound->setEnabled(enabled);
		m_ui.lbSoundPath->setEnabled(lbsound);
		m_ui.lbSoundOpen->setEnabled(lbsound);
		m_ui.lbSoundBrowse->setEnabled(lbsound);
		m_ui.lbSoundReset->setEnabled(lbsound);
		m_ui.lbSound->setEnabled(enabled);
	}

	m_ui.soundEffects->setEnabled(enabled);
	m_ui.overlays->setEnabled(enabled);
	m_ui.leaderboardOverlays->setEnabled(enabled);
	m_ui.encoreMode->setEnabled(enabled);
	m_ui.spectatorMode->setEnabled(enabled);
	m_ui.unofficialAchievements->setEnabled(enabled);
}

void AchievementSettingsWidget::onHardcoreModeStateChanged()
{
	if (!QtHost::IsVMValid())
		return;

	const bool enabled = m_dialog->getEffectiveBoolValue("Achievements", "Enabled", false);
	const bool challenge = m_dialog->getEffectiveBoolValue("Achievements", "ChallengeMode", false);
	if (!enabled || !challenge)
		return;

	// don't bother prompting if the game doesn't have achievements
	auto lock = Achievements::GetLock();
	if (!Achievements::HasActiveGame() || !Achievements::HasAchievementsOrLeaderboards())
		return;

	if (QMessageBox::question(
			QtUtils::GetRootWidget(this), tr("Reset System"),
			tr("Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?")) !=
		QMessageBox::Yes)
	{
		return;
	}

	g_emu_thread->resetVM();
}

void AchievementSettingsWidget::onAchievementsNotificationDurationSliderChanged()
{
	const float duration = m_dialog->getEffectiveFloatValue("Achievements", "NotificationsDuration",
		Pcsx2Config::AchievementsOptions::DEFAULT_NOTIFICATION_DURATION);
	m_ui.achievementNotificationsDurationLabel->setText(tr("%n seconds", nullptr, static_cast<int>(duration)));
}

void AchievementSettingsWidget::onLeaderboardsNotificationDurationSliderChanged()
{
	const float duration = m_dialog->getEffectiveFloatValue("Achievements", "LeaderboardsDuration",
		Pcsx2Config::AchievementsOptions::DEFAULT_LEADERBOARD_DURATION);
	m_ui.leaderboardNotificationsDurationLabel->setText(tr("%n seconds", nullptr, static_cast<int>(duration)));
}

void AchievementSettingsWidget::updateLoginState()
{
	const std::string username(Host::GetBaseStringSettingValue("Achievements", "Username"));
	const bool logged_in = !username.empty();

	if (logged_in)
	{
		const u64 login_unix_timestamp =
			StringUtil::FromChars<u64>(Host::GetBaseStringSettingValue("Achievements", "LoginTimestamp", "0")).value_or(0);
		const QDateTime login_timestamp(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(login_unix_timestamp)));
		m_ui.loginStatus->setText(tr("Username: %1\nLogin token generated on %2.")
									  .arg(QString::fromStdString(username))
									  .arg(login_timestamp.toString(Qt::TextDate)));
		m_ui.loginButton->setText(tr("Logout"));
	}
	else
	{
		m_ui.loginStatus->setText(tr("Not Logged In."));
		m_ui.loginButton->setText(tr("Login..."));
	}

	m_ui.viewProfile->setEnabled(logged_in);
}

void AchievementSettingsWidget::onLoginLogoutPressed()
{
	if (!Host::GetBaseStringSettingValue("Achievements", "Username").empty())
	{
		Host::RunOnCPUThread([]() { Achievements::Logout(); }, true);
		updateLoginState();
		return;
	}

	AchievementLoginDialog login(this, Achievements::LoginRequestReason::UserInitiated);
	int res = login.exec();
	if (res != 0)
		return;

	updateLoginState();

	// Login can enable achievements/hardcore.
	if (!m_ui.enable->isChecked() && Host::GetBaseBoolSettingValue("Achievements", "Enabled", false))
	{
		QSignalBlocker sb(m_ui.enable);
		m_ui.enable->setChecked(true);
		updateEnableState();
	}
	if (!m_ui.hardcoreMode->isChecked() && Host::GetBaseBoolSettingValue("Achievements", "ChallengeMode", false))
	{
		QSignalBlocker sb(m_ui.hardcoreMode);
		m_ui.hardcoreMode->setChecked(true);
	}
}

void AchievementSettingsWidget::onViewProfilePressed()
{
	const std::string username(Host::GetBaseStringSettingValue("Achievements", "Username"));
	if (username.empty())
		return;

	const QByteArray encoded_username(QUrl::toPercentEncoding(QString::fromStdString(username)));
	QtUtils::OpenURL(
		QtUtils::GetRootWidget(this),
		QUrl(QStringLiteral("https://retroachievements.org/user/%1").arg(QString::fromUtf8(encoded_username))));
}

void AchievementSettingsWidget::onAchievementsRefreshed(quint32 id, const QString& game_info_string)
{
	m_ui.gameInfo->setText(game_info_string);
}
