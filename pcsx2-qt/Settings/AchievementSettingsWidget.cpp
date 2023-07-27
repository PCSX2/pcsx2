/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "AchievementSettingsWidget.h"
#include "AchievementLoginDialog.h"
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "SettingWidgetBinder.h"
#include "QtUtils.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/Host.h"

#include "common/StringUtil.h"

#include <QtCore/QDateTime>
#include <QtWidgets/QMessageBox>

AchievementSettingsWidget::AchievementSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	m_ui.setupUi(this);

	SettingsInterface* sif = dialog->getSettingsInterface();

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enable, "Achievements", "Enabled", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.richPresence, "Achievements", "RichPresence", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.challengeMode, "Achievements", "ChallengeMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.leaderboards, "Achievements", "Leaderboards", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.testMode, "Achievements", "TestMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.unofficialTestMode, "Achievements", "UnofficialTestMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.notifications, "Achievements", "Notifications", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.soundEffects, "Achievements", "SoundEffects", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.primedIndicators, "Achievements", "PrimedIndicators", true);

	dialog->registerWidgetHelp(m_ui.enable, tr("Enable Achievements"), tr("Unchecked"),
		tr("When enabled and logged in, PCSX2 will scan for achievements on game load."));
	dialog->registerWidgetHelp(m_ui.testMode, tr("Enable Test Mode"), tr("Unchecked"),
		tr("When enabled, PCSX2 will assume all achievements are locked and not send any "
		   "unlock notifications to the server."));
	dialog->registerWidgetHelp(m_ui.unofficialTestMode, tr("Test Unofficial Achievements"), tr("Unchecked"),
		tr("When enabled, PCSX2 will list achievements from unofficial sets. Please note that these achievements are "
		   "not tracked by RetroAchievements, so they unlock every time."));
	dialog->registerWidgetHelp(m_ui.richPresence, tr("Enable RA's Rich Presence"), tr("Unchecked"),
		tr("When enabled, rich presence information will be collected and sent to the RetroAchievements servers where supported."));
	dialog->registerWidgetHelp(m_ui.challengeMode, tr("Enable Hardcore Mode"), tr("Unchecked"),
		tr("\"Challenge\" mode for achievements, including leaderboard tracking. Disables save state, cheats, and slowdown functions."));
	dialog->registerWidgetHelp(m_ui.leaderboards, tr("Enable Leaderboards"), tr("Checked"),
		tr("Enables tracking and submission of leaderboards in supported games. If leaderboards are disabled, you will still "
		   "be able to view the leaderboard and scores, but no scores will be uploaded."));
	dialog->registerWidgetHelp(m_ui.notifications, tr("Show Notifications"), tr("Checked"),
		tr("Displays popup messages on events such as achievement unlocks and leaderboard submissions."));
	dialog->registerWidgetHelp(m_ui.soundEffects, tr("Enable Sound Effects"), tr("Checked"),
		tr("Plays sound effects for events such as achievement unlocks and leaderboard submissions."));
	dialog->registerWidgetHelp(m_ui.primedIndicators, tr("Show Challenge Indicators"), tr("Checked"),
		tr("Shows icons in the lower-right corner of the screen when a challenge/primed achievement is active."));

	connect(m_ui.enable, &QCheckBox::stateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.notifications, &QCheckBox::stateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.challengeMode, &QCheckBox::stateChanged, this, &AchievementSettingsWidget::updateEnableState);
	connect(m_ui.challengeMode, &QCheckBox::stateChanged, this, &AchievementSettingsWidget::onChallengeModeStateChanged);

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
	}

	updateEnableState();
}

AchievementSettingsWidget::~AchievementSettingsWidget() = default;

void AchievementSettingsWidget::updateEnableState()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("Achievements", "Enabled", false);
	const bool challenge = m_dialog->getEffectiveBoolValue("Achievements", "ChallengeMode", false);
	m_ui.testMode->setEnabled(enabled);
	m_ui.unofficialTestMode->setEnabled(enabled);
	m_ui.richPresence->setEnabled(enabled);
	m_ui.challengeMode->setEnabled(enabled);
	m_ui.leaderboards->setEnabled(enabled && challenge);
	m_ui.notifications->setEnabled(enabled);
	m_ui.soundEffects->setEnabled(enabled);
	m_ui.primedIndicators->setEnabled(enabled);
}

void AchievementSettingsWidget::onChallengeModeStateChanged()
{
	if (!QtHost::IsVMValid())
		return;

	const bool enabled = m_dialog->getEffectiveBoolValue("Achievements", "Enabled", false);
	const bool challenge = m_dialog->getEffectiveBoolValue("Achievements", "ChallengeMode", false);
	if (!enabled || !challenge)
		return;

	// don't bother prompting if the game doesn't have achievements
	auto lock = Achievements::GetLock();
	if (!Achievements::HasActiveGame())
		return;

	if (QMessageBox::question(QtUtils::GetRootWidget(this), tr("Reset System"),
			tr("Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?")) != QMessageBox::Yes)
	{
		return;
	}

	g_emu_thread->resetVM();
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
		//: Variable %1 is an username, variable %2 is a timestamp.
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
		Host::RunOnCPUThread(Achievements::Logout, true);
		updateLoginState();
		return;
	}

	AchievementLoginDialog login(this, Achievements::LoginRequestReason::UserInitiated);
	int res = login.exec();
	if (res != 0)
		return;

	updateLoginState();
}

void AchievementSettingsWidget::onViewProfilePressed()
{
	const std::string username(Host::GetBaseStringSettingValue("Achievements", "Username"));
	if (username.empty())
		return;

	const QByteArray encoded_username(QUrl::toPercentEncoding(QString::fromStdString(username)));
	QtUtils::OpenURL(QtUtils::GetRootWidget(this),
		QUrl(QStringLiteral("https://retroachievements.org/user/%1").arg(QString::fromUtf8(encoded_username))));
}

void AchievementSettingsWidget::onAchievementsRefreshed(quint32 id, const QString& game_info_string, quint32 total, quint32 points)
{
	m_ui.gameInfo->setText(game_info_string);
}
