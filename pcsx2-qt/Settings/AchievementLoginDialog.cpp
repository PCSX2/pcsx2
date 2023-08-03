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

#include "AchievementLoginDialog.h"
#include "QtHost.h"

#include "pcsx2/Achievements.h"

#include <QtWidgets/QMessageBox>

AchievementLoginDialog::AchievementLoginDialog(QWidget* parent, Achievements::LoginRequestReason reason)
	: QDialog(parent)
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
		const bool result = Achievements::Login(username.c_str(), password.c_str());
		QMetaObject::invokeMethod(this, "processLoginResult", Qt::QueuedConnection, Q_ARG(bool, result));
	});
}

void AchievementLoginDialog::cancelClicked()
{
	done(1);
}

void AchievementLoginDialog::processLoginResult(bool result)
{
	if (!result)
	{
		QMessageBox::critical(this, tr("Login Error"),
			tr("Login failed. Please check your username and password, and try again."));
		m_ui.status->setText(tr("Login failed."));
		enableUI(true);
		return;
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
