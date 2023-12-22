// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "pcsx2/SysForwardDefs.h"

#include "AboutDialog.h"
#include "QtHost.h"
#include "QtUtils.h"
#include <QtCore/QString>
#include <QtWidgets/QDialog>

AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedSize(geometry().width(), geometry().height());

	m_ui.scmversion->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_ui.scmversion->setText(QtHost::GetAppNameAndVersion());

	m_ui.links->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_ui.links->setOpenExternalLinks(true);
	m_ui.links->setText(QStringLiteral(
		R"(<a href="%1">%2</a> | <a href="%3">%4</a> | <a href="%5">%6</a> | <a href="%7">%8</a>)")
							.arg(getWebsiteUrl())
							.arg(tr("Website"))
							.arg(getSupportForumsUrl())
							.arg(tr("Support Forums"))
							.arg(getGitHubRepositoryUrl())
							.arg(tr("GitHub Repository"))
							.arg(getLicenseUrl())
							.arg(tr("License")));

	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);
}

AboutDialog::~AboutDialog() = default;

QString AboutDialog::getWebsiteUrl()
{
	return QString::fromUtf8(PCSX2_WEBSITE_URL);
}

QString AboutDialog::getSupportForumsUrl()
{
	return QString::fromUtf8(PCSX2_FORUMS_URL);
}

QString AboutDialog::getGitHubRepositoryUrl()
{
	return QString::fromUtf8(PCSX2_GITHUB_URL);
}

QString AboutDialog::getLicenseUrl()
{
	return QString::fromUtf8(PCSX2_LICENSE_URL);
}

QString AboutDialog::getDiscordServerUrl()
{
	return QString::fromUtf8(PCSX2_DISCORD_URL);
}
