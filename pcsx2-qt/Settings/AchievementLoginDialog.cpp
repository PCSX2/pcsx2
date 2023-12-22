// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AchievementLoginDialog.h"
#include "QtHost.h"

#include "pcsx2/Achievements.h"

#include "common/Error.h"

#include <QtWidgets/QMessageBox>

AchievementLoginDialog::AchievementLoginDialog(QWidget* parent, Achievements::LoginRequestReason reason)
	: QDialog(parent)
	, m_reason(reason)
{
	m_ui.setupUi(this);
	m_ui.loginIcon->setPixmap(QIcon::fromTheme("login-box-line").pixmap(32));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// Adjust text if needed based on reason.
	if (reason == Achievements::LoginRequestReason::TokenInvalid)
	{
		m_ui.instructionText->setText(
			tr("<strong>Your RetroAchievements login token is no longer valid.</strong> You must re-enter your "
			   "credentials for achievements to be tracked. Your password will not be saved in PCSX2, an access token "
			   "will be generated and used instead."));
	}

	m_login = m_ui.buttonBox->addButton(tr("&Login"), QDialogButtonBox::AcceptRole);
	m_login->setEnabled(false);
	connectUi();
}

AchievementLoginDialog::~AchievementLoginDialog() = default;

void AchievementLoginDialog::loginClicked()
{
	std::string username(m_ui.userName->text().toStdString());
	std::string password(m_ui.password->text().toStdString());

	// TODO: Make cancellable.
	m_ui.status->setText(tr("Logging in..."));
	enableUI(false);

	Host::RunOnCPUThread([this, username = std::move(username), password = std::move(password)]() {
		Error error;
		const bool result = Achievements::Login(username.c_str(), password.c_str(), &error);
		const QString message = QString::fromStdString(error.GetDescription());
		QMetaObject::invokeMethod(this, "processLoginResult", Qt::QueuedConnection, Q_ARG(bool, result), Q_ARG(const QString&, message));
	});
}

void AchievementLoginDialog::cancelClicked()
{
	// Disable hardcore mode if we cancelled reauthentication.
	if (m_reason == Achievements::LoginRequestReason::TokenInvalid && QtHost::IsVMValid())
	{
		Host::RunOnCPUThread([]() {
			if (VMManager::HasValidVM() && !Achievements::HasActiveGame())
				Achievements::DisableHardcoreMode();
		});
	}

	done(1);
}

void AchievementLoginDialog::processLoginResult(bool result, const QString& message)
{
	if (!result)
	{
		QMessageBox::critical(
			this, tr("Login Error"),
			tr("Login failed.\nError: %1\n\nPlease check your username and password, and try again.").arg(message));
		m_ui.status->setText(tr("Login failed."));
		enableUI(true);
		return;
	}

	if (m_reason == Achievements::LoginRequestReason::UserInitiated)
	{
		if (!Host::GetBaseBoolSettingValue("Achievements", "Enabled", false) &&
			QMessageBox::question(this, tr("Enable Achievements"),
				tr("Achievement tracking is not currently enabled. Your login will have no effect until "
				   "after tracking is enabled.\n\nDo you want to enable tracking now?"),
				QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
		{
			Host::SetBaseBoolSettingValue("Achievements", "Enabled", true);
			Host::CommitBaseSettingChanges();
			g_emu_thread->applySettings();
		}

		if (!Host::GetBaseBoolSettingValue("Achievements", "ChallengeMode", false) &&
			QMessageBox::question(
				this, tr("Enable Hardcore Mode"),
				tr("Hardcore mode is not currently enabled. Enabling hardcore mode allows you to set times, scores, and "
				   "participate in game-specific leaderboards.\n\nHowever, hardcore mode also prevents the usage of save "
				   "states, cheats and slowdown functionality.\n\nDo you want to enable hardcore mode?"),
				QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
		{
			Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", true);
			Host::CommitBaseSettingChanges();
			g_emu_thread->applySettings();

			bool has_active_game;
			{
				auto lock = Achievements::GetLock();
				has_active_game = Achievements::HasActiveGame();
			}

			if (has_active_game &&
				QMessageBox::question(this, tr("Reset System"),
					tr("Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?")) ==
					QMessageBox::Yes)
			{
				g_emu_thread->resetVM();
			}
		}

	}

	done(0);
}

void AchievementLoginDialog::connectUi()
{
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &AchievementLoginDialog::loginClicked);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &AchievementLoginDialog::cancelClicked);

	auto enableLoginButton = [this](const QString&) { m_login->setEnabled(canEnableLoginButton()); };
	connect(m_ui.userName, &QLineEdit::textChanged, enableLoginButton);
	connect(m_ui.password, &QLineEdit::textChanged, enableLoginButton);
}

void AchievementLoginDialog::enableUI(bool enabled)
{
	m_ui.userName->setEnabled(enabled);
	m_ui.password->setEnabled(enabled);
	m_ui.buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(enabled);
	m_login->setEnabled(enabled && canEnableLoginButton());
}

bool AchievementLoginDialog::canEnableLoginButton() const
{
	return !m_ui.userName->text().isEmpty() && !m_ui.password->text().isEmpty();
}
