/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "AutoUpdaterDialog.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"

#include "pcsx2/HostSettings.h"
#include "pcsx2/SysForwardDefs.h"
#include "svnrev.h"

#include "updater/UpdaterExtractor.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QProcess>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>

// Logic to detect whether we can use the auto updater.
// We use tagged commit, because this gets set on nightly builds.
#if (defined(_WIN32)) && (defined(GIT_TAGGED_COMMIT) && GIT_TAGGED_COMMIT)

#define AUTO_UPDATER_SUPPORTED 1

#if defined(_WIN32)
#define UPDATE_PLATFORM_STR "Windows"
#if _M_SSE >= 0x500
#define UPDATE_ADDITIONAL_TAGS "AVX2"
#else
#define UPDATE_ADDITIONAL_TAGS "SSE4"
#endif
#endif

#endif

#ifdef AUTO_UPDATER_SUPPORTED

#define LATEST_RELEASE_URL "https://api.pcsx2.net/v1/%1Releases?pageSize=1"
#define CHANGES_URL "https://api.github.com/repos/PCSX2/pcsx2/compare/%1...%2"

// Available release channels.
static const char* UPDATE_TAGS[] = {"stable", "nightly"};

// Bit annoying, because PCSX2_isReleaseVersion is a bool, but whatever.
#define THIS_RELEASE_TAG (PCSX2_isReleaseVersion ? "stable" : "nightly")

#endif

AutoUpdaterDialog::AutoUpdaterDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_network_access_mgr = new QNetworkAccessManager(this);

	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	connect(m_ui.downloadAndInstall, &QPushButton::clicked, this, &AutoUpdaterDialog::downloadUpdateClicked);
	connect(m_ui.skipThisUpdate, &QPushButton::clicked, this, &AutoUpdaterDialog::skipThisUpdateClicked);
	connect(m_ui.remindMeLater, &QPushButton::clicked, this, &AutoUpdaterDialog::remindMeLaterClicked);
}

AutoUpdaterDialog::~AutoUpdaterDialog() = default;

bool AutoUpdaterDialog::isSupported()
{
#ifdef AUTO_UPDATER_SUPPORTED
	return true;
#else
	return false;
#endif
}

QStringList AutoUpdaterDialog::getTagList()
{
#ifdef AUTO_UPDATER_SUPPORTED
	return QStringList(std::begin(UPDATE_TAGS), std::end(UPDATE_TAGS));
#else
	return QStringList();
#endif
}

std::string AutoUpdaterDialog::getDefaultTag()
{
#ifdef AUTO_UPDATER_SUPPORTED
	return THIS_RELEASE_TAG;
#else
	return {};
#endif
}

QString AutoUpdaterDialog::getCurrentVersion()
{
	return QStringLiteral(GIT_TAG);
}

QString AutoUpdaterDialog::getCurrentVersionDate()
{
	// 20220403235450ll
	const QDateTime current_build_date(QDateTime::fromString(QStringLiteral("%1").arg(SVN_REV), "yyyyMMddhhmmss"));
	return current_build_date.toString();
}

QString AutoUpdaterDialog::getCurrentUpdateTag() const
{
#ifdef AUTO_UPDATER_SUPPORTED
	return QString::fromStdString(Host::GetBaseStringSettingValue("AutoUpdater", "UpdateTag", THIS_RELEASE_TAG));
#else
	return QString();
#endif
}

void AutoUpdaterDialog::reportError(const char* msg, ...)
{
	std::va_list ap;
	va_start(ap, msg);
	std::string full_msg = StringUtil::StdStringFromFormatV(msg, ap);
	va_end(ap);

	// don't display errors when we're doing an automatic background check, it's just annoying
	if (m_display_messages)
		QMessageBox::critical(this, tr("Updater Error"), QString::fromStdString(full_msg));
}

void AutoUpdaterDialog::queueUpdateCheck(bool display_message)
{
	m_display_messages = display_message;

#ifdef AUTO_UPDATER_SUPPORTED
	connect(m_network_access_mgr, &QNetworkAccessManager::finished, this, &AutoUpdaterDialog::getLatestReleaseComplete);

	QUrl url(QStringLiteral(LATEST_RELEASE_URL).arg(getCurrentUpdateTag()));
	QNetworkRequest request(url);
	m_network_access_mgr->get(request);
#else
	emit updateCheckCompleted();
#endif
}

void AutoUpdaterDialog::getLatestReleaseComplete(QNetworkReply* reply)
{
#ifdef AUTO_UPDATER_SUPPORTED
	// this might fail due to a lack of internet connection - in which case, don't spam the user with messages every time.
	m_network_access_mgr->disconnect(this);
	reply->deleteLater();

	bool found_update_info = false;

	if (reply->error() == QNetworkReply::NoError)
	{
		const QByteArray reply_json(reply->readAll());
		QJsonParseError parse_error;
		QJsonDocument doc(QJsonDocument::fromJson(reply_json, &parse_error));
		if (doc.isObject())
		{
			const QJsonObject doc_object(doc.object());
			const QJsonArray data_array(doc_object["data"].toArray());
			if (!data_array.isEmpty())
			{
				// just take the first one, that's all we requested anyway
				const QJsonObject data_object(data_array.first().toObject());
				const QJsonObject assets_object(data_object["assets"].toObject());
				const QJsonArray platform_array(assets_object[UPDATE_PLATFORM_STR].toArray());
				if (!platform_array.isEmpty())
				{
					// search for the correct file
					for (const QJsonValue& asset_value : platform_array)
					{
						const QJsonObject asset_object(asset_value.toObject());
						const QJsonArray additional_tags_array(asset_object["additionalTags"].toArray());
						bool is_matching_asset = false;
						bool is_qt_asset = false;
						for (const QJsonValue& additional_tag : additional_tags_array)
						{
							const QString additional_tag_str(additional_tag.toString());
							if (additional_tag_str == QStringLiteral("symbols"))
							{
								// we're not interested in symbols downloads
								is_matching_asset = false;
								break;
							}
							else if (additional_tag_str == QStringLiteral("Qt"))
							{
								// found a qt build
								is_qt_asset = true;
							}

							// is this the right variant?
							if (additional_tag_str == QStringLiteral(UPDATE_ADDITIONAL_TAGS))
							{
								// yep! found the right one. but keep checking in case it's symbols.
								is_matching_asset = true;
							}
						}

						if (!is_qt_asset || !is_matching_asset)
						{
							// skip this asset
							continue;
						}

						m_latest_version = data_object["version"].toString();
						m_latest_version_timestamp = QDateTime::fromString(data_object["publishedAt"].toString(), QStringLiteral("yyyy-MM-ddThh:mm:ss.zzzZ"));
						m_download_url = asset_object["url"].toString();
						if (!m_latest_version.isEmpty() && !m_download_url.isEmpty())
						{
							found_update_info = true;
							break;
						}
						else
						{
							reportError("missing version/download info");
						}
					}

					if (!found_update_info)
						reportError("matching asset not found");
				}
				else
				{
					reportError("platform not found in assets array");
				}
			}
			else
			{
				reportError("data is not an array");
			}
		}
		else
		{
			reportError("JSON is not an object");
		}
	}
	else
	{
		reportError("Failed to download latest release info: %d", static_cast<int>(reply->error()));
	}

	if (found_update_info)
		checkIfUpdateNeeded();

	emit updateCheckCompleted();
#endif
}

void AutoUpdaterDialog::queueGetChanges()
{
#ifdef AUTO_UPDATER_SUPPORTED
	connect(m_network_access_mgr, &QNetworkAccessManager::finished, this, &AutoUpdaterDialog::getChangesComplete);

	const QString url_string(QStringLiteral(CHANGES_URL).arg(GIT_HASH).arg(m_latest_version));
	QUrl url(url_string);
	QNetworkRequest request(url);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
	m_network_access_mgr->get(request);
#endif
}

void AutoUpdaterDialog::getChangesComplete(QNetworkReply* reply)
{
#ifdef AUTO_UPDATER_SUPPORTED
	m_network_access_mgr->disconnect(this);
	reply->deleteLater();

	if (reply->error() == QNetworkReply::NoError)
	{
		const QByteArray reply_json(reply->readAll());
		QJsonParseError parse_error;
		QJsonDocument doc(QJsonDocument::fromJson(reply_json, &parse_error));
		if (doc.isObject())
		{
			const QJsonObject doc_object(doc.object());

			QString changes_html = tr("<h2>Changes:</h2>");
			changes_html += QStringLiteral("<ul>");

			const QJsonArray commits(doc_object["commits"].toArray());
			bool update_will_break_save_states = false;
			bool update_increases_settings_version = false;

			for (const QJsonValue& commit : commits)
			{
				const QJsonObject commit_obj(commit["commit"].toObject());

				QString message = commit_obj["message"].toString();
				QString author = commit_obj["author"].toObject()["name"].toString();
				const int first_line_terminator = message.indexOf('\n');
				if (first_line_terminator >= 0)
					message.remove(first_line_terminator, message.size() - first_line_terminator);
				if (!message.isEmpty())
				{
					changes_html +=
						QStringLiteral("<li>%1 <i>(%2)</i></li>").arg(message.toHtmlEscaped()).arg(author.toHtmlEscaped());
				}

				if (message.contains(QStringLiteral("[SAVEVERSION+]")))
					update_will_break_save_states = true;

				if (message.contains(QStringLiteral("[SETTINGSVERSION+]")))
					update_increases_settings_version = true;
			}

			changes_html += "</ul>";

			if (update_will_break_save_states)
			{
				changes_html.prepend(tr("<h2>Save State Warning</h2><p>Installing this update will make your save states "
										"<b>incompatible</b>. Please ensure you have saved your games to memory card "
										"before installing this update or you will lose progress.</p>"));
			}

			if (update_increases_settings_version)
			{
				changes_html.prepend(
					tr("<h2>Settings Warning</h2><p>Installing this update will reset your program configuration. Please note "
					   "that you will have to reconfigure your settings after this update.</p>"));
			}

			m_ui.updateNotes->setText(changes_html);
		}
		else
		{
			reportError("Change list JSON is not an object");
		}
	}
	else
	{
		reportError("Failed to download change list: %d", static_cast<int>(reply->error()));
	}
#endif

	m_ui.downloadAndInstall->setEnabled(true);
}

void AutoUpdaterDialog::downloadUpdateClicked()
{
	QUrl url(m_download_url);
	QNetworkRequest request(url);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
	QNetworkReply* reply = m_network_access_mgr->get(request);

	QProgressDialog progress(tr("Downloading %1...").arg(m_download_url), tr("Cancel"), 0, 1);
	progress.setWindowTitle(tr("Automatic Updater"));
	progress.setWindowIcon(windowIcon());
	progress.setAutoClose(false);

	connect(reply, &QNetworkReply::downloadProgress, [&progress](quint64 received, quint64 total) {
		progress.setRange(0, static_cast<int>(total));
		progress.setValue(static_cast<int>(received));
	});

	connect(m_network_access_mgr, &QNetworkAccessManager::finished, [this, &progress](QNetworkReply* reply) {
		m_network_access_mgr->disconnect();

		if (reply->error() != QNetworkReply::NoError)
		{
			reportError("Download failed: %s", reply->errorString().toUtf8().constData());
			progress.done(-1);
			return;
		}

		const QByteArray data = reply->readAll();
		if (data.isEmpty())
		{
			reportError("Download failed: Update is empty");
			progress.done(-1);
			return;
		}

		if (processUpdate(data))
			progress.done(1);
		else
			progress.done(-1);
	});

	const int result = progress.exec();
	if (result == 0)
	{
		// cancelled
		reply->abort();
	}
	else if (result == 1)
	{
		// updater started. since we're a modal on the main window, we have to queue this.
		QMetaObject::invokeMethod(g_main_window, &MainWindow::requestExit, Qt::QueuedConnection);
		done(0);
	}

	reply->deleteLater();
}

void AutoUpdaterDialog::checkIfUpdateNeeded()
{
	const QString last_checked_version(
		QString::fromStdString(Host::GetBaseStringSettingValue("AutoUpdater", "LastVersion")));

	Console.WriteLn(Color_StrongGreen, "Current version: %s", GIT_TAG);
	Console.WriteLn(Color_StrongYellow, "Latest SHA: %s", m_latest_version.toUtf8().constData());
	Console.WriteLn(Color_StrongOrange, "Last Checked SHA: %s", last_checked_version.toUtf8().constData());
	if (m_latest_version == GIT_TAG || m_latest_version == last_checked_version)
	{
		Console.WriteLn(Color_StrongGreen, "No update needed.");

		if (m_display_messages)
		{
			QMessageBox::information(this, tr("Automatic Updater"),
				tr("No updates are currently available. Please try again later."));
		}

		return;
	}

	Console.WriteLn(Color_StrongRed, "Update needed.");

	m_ui.currentVersion->setText(tr("Current Version: %1 (%2)").arg(getCurrentVersion()).arg(getCurrentVersionDate()));
	m_ui.newVersion->setText(tr("New Version: %1 (%2)").arg(m_latest_version).arg(m_latest_version_timestamp.toString()));
	m_ui.updateNotes->setText(tr("Loading..."));
	queueGetChanges();
	exec();
}

void AutoUpdaterDialog::skipThisUpdateClicked()
{
	QtHost::SetBaseStringSettingValue("AutoUpdater", "LastVersion", m_latest_version.toUtf8().constData());
	done(0);
}

void AutoUpdaterDialog::remindMeLaterClicked()
{
	done(0);
}

#ifdef _WIN32

bool AutoUpdaterDialog::processUpdate(const QByteArray& update_data)
{
	const QString update_directory = QCoreApplication::applicationDirPath();
	const QString update_zip_path = QStringLiteral("%1" FS_OSPATH_SEPARATOR_STR "%2").arg(update_directory).arg(UPDATER_ARCHIVE_NAME);
	const QString updater_path = QStringLiteral("%1" FS_OSPATH_SEPARATOR_STR "%2").arg(update_directory).arg(UPDATER_EXECUTABLE);

	Q_ASSERT(!update_zip_path.isEmpty() && !updater_path.isEmpty() && !update_directory.isEmpty());
	if ((QFile::exists(update_zip_path) && !QFile::remove(update_zip_path)) ||
		(QFile::exists(updater_path) && !QFile::remove(updater_path)))
	{
		reportError("Removing existing update zip/updater failed");
		return false;
	}

	{
		QFile update_zip_file(update_zip_path);
		if (!update_zip_file.open(QIODevice::WriteOnly) || update_zip_file.write(update_data) != update_data.size())
		{
			reportError("Writing update zip to '%s' failed", update_zip_path.toUtf8().constData());
			return false;
		}
		update_zip_file.close();
	}

	std::string updater_extract_error;
	if (!ExtractUpdater(update_zip_path.toUtf8().constData(), updater_path.toUtf8().constData(), &updater_extract_error))
	{
		reportError("Extracting updater failed: %s", updater_extract_error.c_str());
		return false;
	}

	if (!doUpdate(update_zip_path, updater_path, update_directory))
	{
		reportError("Launching updater failed");
		return false;
	}

	return true;
}

bool AutoUpdaterDialog::doUpdate(const QString& zip_path, const QString& updater_path, const QString& destination_path)
{
	const QString program_path = QCoreApplication::applicationFilePath();
	if (program_path.isEmpty())
	{
		reportError("Failed to get current application path");
		return false;
	}

	QStringList arguments;
	arguments << QString::number(QCoreApplication::applicationPid());
	arguments << destination_path;
	arguments << zip_path;
	arguments << program_path;

	// this will leak, but not sure how else to handle it...
	QProcess* updater_process = new QProcess();
	updater_process->setProgram(updater_path);
	updater_process->setArguments(arguments);
	updater_process->start(QIODevice::NotOpen);
	if (!updater_process->waitForStarted())
	{
		reportError("Failed to start updater");
		return false;
	}

	return true;
}

#else

bool AutoUpdaterDialog::processUpdate(const QByteArray& update_data)
{
	return false;
}

#endif
