// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "ui_AchievementLoginDialog.h"
#include <QtWidgets/QDialog>
#include <QtWidgets/QPushButton>

namespace Achievements
{
	enum class LoginRequestReason;
}

class AchievementLoginDialog : public QDialog
{
	Q_OBJECT

public:
	AchievementLoginDialog(QWidget* parent, Achievements::LoginRequestReason reason);
	~AchievementLoginDialog();

private Q_SLOTS:
	void loginClicked();
	void cancelClicked();
	void processLoginResult(bool result, const QString& message);

private:
	void connectUi();
	void enableUI(bool enabled);
	bool canEnableLoginButton() const;

	Ui::AchievementLoginDialog m_ui;
	QPushButton* m_login;
	Achievements::LoginRequestReason m_reason;
};
