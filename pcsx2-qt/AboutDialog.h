// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_AboutDialog.h"
#include <QtWidgets/QDialog>

class AboutDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit AboutDialog(QWidget* parent = nullptr);
	~AboutDialog();

	static QString getWebsiteUrl();
	static QString getSupportForumsUrl();
	static QString getGitHubRepositoryUrl();
	static QString getLicenseUrl();
	static QString getThirdPartyLicensesUrl();
	static QString getWikiUrl();
	static QString getDocumentationUrl();
	static QString getDiscordServerUrl();

	static void showHTMLDialog(QWidget* parent, const QString& title, const QString& url);

private Q_SLOTS:
	void linksLinkActivated(const QString& link);

private:
	Ui::AboutDialog m_ui;
};
