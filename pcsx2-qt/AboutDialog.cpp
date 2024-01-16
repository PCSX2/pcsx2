// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "pcsx2/SupportURLs.h"

#include "AboutDialog.h"
#include "QtHost.h"
#include "QtUtils.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SmallString.h"

#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>

static QString GetDocFileUrl(std::string_view name)
{
#ifdef _WIN32
	// Windows uses the docs directory in bin.
	const std::string path = Path::Combine(EmuFolders::AppRoot,
		TinyString::from_fmt("docs" FS_OSPATH_SEPARATOR_STR "{}", name));
#else
	// Linux/Mac has this in the Resources directory.
	const std::string path = Path::Combine(EmuFolders::Resources,
		TinyString::from_fmt("docs" FS_OSPATH_SEPARATOR_STR "{}", name));
#endif
	return QUrl::fromLocalFile(QString::fromStdString(path)).toString();
}

AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedSize(geometry().width(), geometry().height());

	m_ui.scmversion->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_ui.scmversion->setText(QtHost::GetAppNameAndVersion());

	m_ui.links->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_ui.links->setText(QStringLiteral(
		R"(<a href="%1">%2</a> | <a href="%3">%4</a> | <a href="%5">%6</a> | <a href="%7">%8</a> | <a href="%9">%10</a>)")
							.arg(getWebsiteUrl())
							.arg(tr("Website"))
							.arg(getSupportForumsUrl())
							.arg(tr("Support Forums"))
							.arg(getGitHubRepositoryUrl())
							.arg(tr("GitHub Repository"))
							.arg(getLicenseUrl())
							.arg(tr("License"))
							.arg(getThirdPartyLicensesUrl())
							.arg(tr("Third-Party Licenses")));

	connect(m_ui.links, &QLabel::linkActivated, this, &AboutDialog::linksLinkActivated);
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
	return GetDocFileUrl("GPL.html");
}

QString AboutDialog::getThirdPartyLicensesUrl()
{
	return GetDocFileUrl("ThirdPartyLicenses.html");
}

QString AboutDialog::getDiscordServerUrl()
{
	return QString::fromUtf8(PCSX2_DISCORD_URL);
}

void AboutDialog::linksLinkActivated(const QString& link)
{
	const QUrl url(link);
	if (!url.isValid())
		return;

	if (!url.isLocalFile())
	{
		QDesktopServices::openUrl(url);
		return;
	}

	showHTMLDialog(this, tr("View Document"), url.toLocalFile());
}

void AboutDialog::showHTMLDialog(QWidget* parent, const QString& title, const QString& path)
{
	QDialog dialog(parent);
	dialog.setMinimumSize(700, 400);
	dialog.setWindowTitle(title);
	dialog.setWindowIcon(QtHost::GetAppIcon());

	QVBoxLayout* layout = new QVBoxLayout(&dialog);

	QTextBrowser* tb = new QTextBrowser(&dialog);
	tb->setAcceptRichText(true);
	tb->setReadOnly(true);
	tb->setOpenExternalLinks(true);

	QFile file(path);
	file.open(QIODevice::ReadOnly);
	if (const QByteArray data = file.readAll(); !data.isEmpty())
		tb->setText(QString::fromUtf8(data));
	else
		tb->setText(tr("File not found: %1").arg(path));

	layout->addWidget(tb, 1);

	QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	connect(bb->button(QDialogButtonBox::Close), &QPushButton::clicked, &dialog, &QDialog::done);
	layout->addWidget(bb, 0);

	dialog.exec();
}
