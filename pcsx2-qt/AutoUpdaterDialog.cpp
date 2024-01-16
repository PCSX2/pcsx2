// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AutoUpdaterDialog.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtProgressCallback.h"
#include "QtUtils.h"

#include "pcsx2/Host.h"
#include "svnrev.h"

#include "updater/UpdaterExtractor.h"

#include "common/Assertions.h"
#include "common/CocoaTools.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/HTTPDownloader.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "cpuinfo.h"

#include <functional>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QProcess>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <shellapi.h>
#endif

// Interval at which HTTP requests are polled.
static constexpr u32 HTTP_POLL_INTERVAL = 10;

// Logic to detect whether we can use the auto updater.
// We use tagged commit, because this gets set on nightly builds.
#if (defined(_WIN32) || defined(__linux__) || defined(__APPLE__)) && GIT_TAGGED_COMMIT

#define AUTO_UPDATER_SUPPORTED 1

#if defined(_WIN32)
#define UPDATE_PLATFORM_STR "Windows"
#elif defined(__linux__)
#define UPDATE_PLATFORM_STR "Linux"
#elif defined(__APPLE__)
#define UPDATE_PLATFORM_STR "MacOS"
#endif

#ifdef MULTI_ISA_SHARED_COMPILATION
// #undef UPDATE_ADDITIONAL_TAGS
#elif _M_SSE >= 0x501
#define UPDATE_ADDITIONAL_TAGS "AVX2"
#else
#define UPDATE_ADDITIONAL_TAGS "SSE4"
#endif

#endif

#ifdef AUTO_UPDATER_SUPPORTED

#define LATEST_RELEASE_URL "https://api.pcsx2.net/v1/%1Releases?pageSize=1"
#define CHANGES_URL "https://api.github.com/repos/PCSX2/pcsx2/compare/%1...%2"

// Available release channels.
static const char* UPDATE_TAGS[] = {"stable", "nightly"};

// TODO: Make manual releases create this file, and make it contain `#define DEFAULT_UPDATER_CHANNEL "stable"`.
#if __has_include("DefaultUpdaterChannel.h")
#include "DefaultUpdaterChannel.h"
#endif
#ifndef DEFAULT_UPDATER_CHANNEL
#define DEFAULT_UPDATER_CHANNEL "nightly"
#endif

#endif

AutoUpdaterDialog::AutoUpdaterDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	connect(m_ui.downloadAndInstall, &QPushButton::clicked, this, &AutoUpdaterDialog::downloadUpdateClicked);
	connect(m_ui.skipThisUpdate, &QPushButton::clicked, this, &AutoUpdaterDialog::skipThisUpdateClicked);
	connect(m_ui.remindMeLater, &QPushButton::clicked, this, &AutoUpdaterDialog::remindMeLaterClicked);

	m_http = HTTPDownloader::Create(Host::GetHTTPUserAgent());
	if (!m_http)
		Console.Error("Failed to create HTTP downloader, auto updater will not be available.");
}

AutoUpdaterDialog::~AutoUpdaterDialog() = default;

bool AutoUpdaterDialog::isSupported()
{
#ifdef AUTO_UPDATER_SUPPORTED
#ifdef __linux__
	// For Linux, we need to check whether we're running from the appimage.
	if (!std::getenv("APPIMAGE"))
	{
		Console.Warning("We're a tagged commit, but not running from an AppImage. Disabling automatic updater.");
		return false;
	}

	return true;
#else
	// Windows, MacOS - always supported.
	return true;
#endif
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
	return DEFAULT_UPDATER_CHANNEL;
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
	return QStringLiteral(GIT_DATE);
}

QString AutoUpdaterDialog::getCurrentUpdateTag() const
{
#ifdef AUTO_UPDATER_SUPPORTED
	return QString::fromStdString(Host::GetBaseStringSettingValue("AutoUpdater", "UpdateTag", DEFAULT_UPDATER_CHANNEL));
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
	Console.Error("Updater Error: %s", full_msg.c_str());
	if (m_display_messages)
		QMessageBox::critical(this, tr("Updater Error"), QString::fromStdString(full_msg));
}

bool AutoUpdaterDialog::ensureHttpReady()
{
	if (!m_http)
		return false;

	if (!m_http_poll_timer)
	{
		m_http_poll_timer = new QTimer(this);
		m_http_poll_timer->connect(m_http_poll_timer, &QTimer::timeout, this, &AutoUpdaterDialog::httpPollTimerPoll);
	}

	if (!m_http_poll_timer->isActive())
	{
		m_http_poll_timer->setSingleShot(false);
		m_http_poll_timer->setInterval(HTTP_POLL_INTERVAL);
		m_http_poll_timer->start();
	}

	return true;
}

void AutoUpdaterDialog::httpPollTimerPoll()
{
	pxAssert(m_http);
	m_http->PollRequests();

	if (!m_http->HasAnyRequests())
	{
		Console.WriteLn("(AutoUpdaterDialog) All HTTP requests done.");
		m_http_poll_timer->stop();
	}
}

void AutoUpdaterDialog::queueUpdateCheck(bool display_message)
{
	m_display_messages = display_message;

#ifdef AUTO_UPDATER_SUPPORTED
	if (!ensureHttpReady())
	{
		emit updateCheckCompleted();
		return;
	}

	m_http->CreateRequest(QStringLiteral(LATEST_RELEASE_URL).arg(getCurrentUpdateTag()).toStdString(),
		std::bind(&AutoUpdaterDialog::getLatestReleaseComplete, this, std::placeholders::_1, std::placeholders::_3));
#else
	emit updateCheckCompleted();
#endif
}

void AutoUpdaterDialog::getLatestReleaseComplete(s32 status_code, std::vector<u8> data)
{
#ifdef _M_X86
	// should already be initialized, but just in case this somehow runs before the CPU thread starts setting up...
	cpuinfo_initialize();
#endif

#ifdef AUTO_UPDATER_SUPPORTED
	bool found_update_info = false;

	if (status_code == HTTPDownloader::HTTP_STATUS_OK)
	{
		QJsonParseError parse_error;
		QJsonDocument doc(QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(data.data()), data.size()), &parse_error));
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
					QJsonObject best_asset;
					int best_asset_score = 0;

					// search for usable files
					for (const QJsonValue& asset_value : platform_array)
					{
						const QJsonObject asset_object(asset_value.toObject());
						const QJsonArray additional_tags_array(asset_object["additionalTags"].toArray());
						bool is_symbols = false;
						bool is_avx2 = false;
						bool is_sse4 = false;
						bool is_perfect_match = false;
						for (const QJsonValue& additional_tag : additional_tags_array)
						{
							const QString additional_tag_str(additional_tag.toString());
							if (additional_tag_str == QStringLiteral("symbols"))
							{
								// we're not interested in symbols downloads
								is_symbols = true;
								break;
							}
							else if (additional_tag_str == QStringLiteral("SSE4"))
							{
								is_sse4 = true;
							}
							else if (additional_tag_str == QStringLiteral("AVX2"))
							{
								is_avx2 = true;
							}
#ifdef UPDATE_ADDITIONAL_TAGS
							if (additional_tag_str == QStringLiteral(UPDATE_ADDITIONAL_TAGS))
							{
								// Found the same variant as what's currently running!  But keep checking in case it's symbols.
								is_perfect_match = true;
							}
#endif
						}

						if (is_symbols)
						{
							// skip this asset
							continue;
						}

#ifdef _M_X86
						if (is_avx2 && cpuinfo_has_x86_avx2())
						{
							// skip this asset
							continue;
						}
#endif

						int score;
						if (is_perfect_match)
							score = 4; // #1 choice is the one matching this binary
						else if (is_avx2)
							score = 3; // Prefer AVX2 over SSE4 (support test was done above)
						else if (is_sse4)
							score = 2; // Prefer SSE4 over one with no tags at all
						else
							score = 1; // Multi-ISA builds will have no tags, they'll only get picked because they're the only available build

						if (score > best_asset_score)
						{
							best_asset = std::move(asset_object);
							best_asset_score = score;
						}
					}

					if (best_asset_score == 0)
					{
						reportError("no matching assets found");
					}
					else
					{
						m_latest_version = data_object["version"].toString();
						m_latest_version_timestamp = QDateTime::fromString(data_object["publishedAt"].toString(), QStringLiteral("yyyy-MM-ddThh:mm:ss.zzzZ"));
						m_download_url = best_asset["url"].toString();
						m_download_size = best_asset["size"].toInt();
						found_update_info = true;
					}
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
		reportError("Failed to download latest release info: %d", status_code);
	}

	if (found_update_info)
		checkIfUpdateNeeded();

	emit updateCheckCompleted();
#endif
}

void AutoUpdaterDialog::queueGetChanges()
{
#ifdef AUTO_UPDATER_SUPPORTED
	if (!ensureHttpReady())
		return;

	m_http->CreateRequest(QStringLiteral(CHANGES_URL).arg(GIT_HASH).arg(m_latest_version).toStdString(),
		std::bind(&AutoUpdaterDialog::getChangesComplete, this, std::placeholders::_1, std::placeholders::_3));
#endif
}

void AutoUpdaterDialog::getChangesComplete(s32 status_code, std::vector<u8> data)
{
#ifdef AUTO_UPDATER_SUPPORTED
	if (status_code == HTTPDownloader::HTTP_STATUS_OK)
	{
		QJsonParseError parse_error;
		QJsonDocument doc(QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(data.data()), data.size()), &parse_error));
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

				if (message.contains(QStringLiteral("[SAVEVERSION+]")))
					update_will_break_save_states = true;

				if (message.contains(QStringLiteral("[SETTINGSVERSION+]")))
					update_increases_settings_version = true;

				const int first_line_terminator = message.indexOf('\n');
				if (first_line_terminator >= 0)
					message.remove(first_line_terminator, message.size() - first_line_terminator);
				if (!message.isEmpty())
				{
					changes_html +=
						QStringLiteral("<li>%1 <i>(%2)</i></li>").arg(message.toHtmlEscaped()).arg(author.toHtmlEscaped());
				}
			}

			changes_html += "</ul>";

			if (update_will_break_save_states)
			{
				changes_html.prepend(tr("<h2>Save State Warning</h2><p>Installing this update will make your save states "
										"<b>incompatible</b>. Please ensure you have saved your games to a Memory Card "
										"before installing this update or you will lose progress.</p>"));

				m_update_will_break_save_states = true;
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
		reportError("Failed to download change list: %d", status_code);
	}
#endif

	m_ui.downloadAndInstall->setEnabled(true);
}

void AutoUpdaterDialog::downloadUpdateClicked()
{
	if (m_update_will_break_save_states)
	{
		QMessageBox msgbox;
		msgbox.setIcon(QMessageBox::Critical);
		msgbox.setWindowTitle(tr("Savestate Warning"));
		msgbox.setText(tr("<h1>WARNING</h1><p style='font-size:12pt;'>Installing this update will make your <b>save states incompatible</b>, <i>be sure to save any progress to your memory cards before proceeding</i>.</p><p>Do you wish to continue?</p>"));
		msgbox.addButton(QMessageBox::Yes);
		msgbox.addButton(QMessageBox::No);
		msgbox.setDefaultButton(QMessageBox::No);
		// This makes the box wider, for some reason sizing boxes in Qt is hard - Source: The internet.
		QSpacerItem* horizontalSpacer = new QSpacerItem(500, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
		QGridLayout* layout = (QGridLayout*)msgbox.layout();
		layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
		if (msgbox.exec() != QMessageBox::Yes)
			return;
	}

	m_display_messages = true;

	std::optional<bool> download_result;
	QtModalProgressCallback progress(this);
	progress.SetTitle(tr("Automatic Updater").toUtf8().constData());
	progress.SetStatusText(tr("Downloading %1...").arg(m_latest_version).toUtf8().constData());
	progress.GetDialog().setWindowIcon(windowIcon());
	progress.SetCancellable(true);

	m_http->CreateRequest(
		m_download_url.toStdString(),
		[this, &download_result, &progress](s32 status_code, const std::string&, std::vector<u8> data) {
			if (status_code == HTTPDownloader::HTTP_STATUS_CANCELLED)
				return;

			if (status_code != HTTPDownloader::HTTP_STATUS_OK)
			{
				reportError("Download failed: %d", status_code);
				download_result = false;
				return;
			}

			if (data.empty())
			{
				reportError("Download failed: Update is empty");
				download_result = false;
				return;
			}

			download_result = processUpdate(data, progress.GetDialog());
		},
		&progress);

	// Block until completion.
	while (m_http->HasAnyRequests())
	{
		QApplication::processEvents(QEventLoop::AllEvents, HTTP_POLL_INTERVAL);
		m_http->PollRequests();
	}

	if (download_result.value_or(false))
	{
		// updater started. since we're a modal on the main window, we have to queue this.
		QMetaObject::invokeMethod(g_main_window, "requestExit", Qt::QueuedConnection, Q_ARG(bool, true));
		done(0);
	}

	// download error or cancelled
}

void AutoUpdaterDialog::checkIfUpdateNeeded()
{
	const QString last_checked_version(
		QString::fromStdString(Host::GetBaseStringSettingValue("AutoUpdater", "LastVersion")));

	Console.WriteLn(Color_StrongGreen, "Current version: %s", GIT_TAG);
	Console.WriteLn(Color_StrongYellow, "Latest version: %s", m_latest_version.toUtf8().constData());
	Console.WriteLn(Color_StrongOrange, "Last checked version: %s", last_checked_version.toUtf8().constData());
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

	// Don't show the dialog if a game started while the update info was downloading. Some people have
	// really slow connections, apparently. If we're a manual triggered update check, then display
	// regardless. This will fall through and signal main to delete us.
	if (!m_display_messages &&
		(QtHost::IsVMValid() || (g_emu_thread->isRunningFullscreenUI() && g_emu_thread->isFullscreen())))
	{
		Console.WriteLn(Color_StrongRed, "Not showing update dialog due to active VM.");
		return;
	}

	m_ui.currentVersion->setText(tr("Current Version: %1 (%2)").arg(getCurrentVersion()).arg(getCurrentVersionDate()));
	m_ui.newVersion->setText(tr("New Version: %1 (%2)").arg(m_latest_version).arg(m_latest_version_timestamp.toString()));
	m_ui.downloadSize->setText(tr("Download Size: %1 MB").arg(static_cast<double>(m_download_size) / 1048576.0, 0, 'f', 2));
	m_ui.updateNotes->setText(tr("Loading..."));
	queueGetChanges();

	// We have to defer this, because it comes back through the timer/HTTP callback...
	QMetaObject::invokeMethod(this, "exec", Qt::QueuedConnection);
}

void AutoUpdaterDialog::skipThisUpdateClicked()
{
	Host::SetBaseStringSettingValue("AutoUpdater", "LastVersion", m_latest_version.toUtf8().constData());
	Host::CommitBaseSettingChanges();
	done(0);
}

void AutoUpdaterDialog::remindMeLaterClicked()
{
	done(0);
}

#if defined(_WIN32)

bool AutoUpdaterDialog::doesUpdaterNeedElevation(const std::string& application_dir) const
{
	// Try to create a dummy text file in the PCSX2 updater directory. If it fails, we probably won't have write permission.
	const std::string dummy_path = Path::Combine(application_dir, "update.txt");
	auto fp = FileSystem::OpenManagedCFile(dummy_path.c_str(), "wb");
	if (!fp)
		return true;

	fp.reset();
	FileSystem::DeleteFilePath(dummy_path.c_str());
	return false;
}

bool AutoUpdaterDialog::processUpdate(const std::vector<u8>& data, QProgressDialog&)
{
	const std::string& application_dir = EmuFolders::AppRoot;
	const std::string update_zip_path = Path::Combine(EmuFolders::DataRoot, UPDATER_ARCHIVE_NAME);
	const std::string updater_path = Path::Combine(EmuFolders::DataRoot, UPDATER_EXECUTABLE);

	if ((FileSystem::FileExists(update_zip_path.c_str()) && !FileSystem::DeleteFilePath(update_zip_path.c_str())))
	{
		reportError("Removing existing update zip failed");
		return false;
	}

	if (!FileSystem::WriteBinaryFile(update_zip_path.c_str(), data.data(), data.size()))
	{
		reportError("Writing update zip to '%s' failed", update_zip_path.c_str());
		return false;
	}

	std::string updater_extract_error;
	if (!ExtractUpdater(update_zip_path.c_str(), updater_path.c_str(), &updater_extract_error))
	{
		reportError("Extracting updater failed: %s", updater_extract_error.c_str());
		return false;
	}

	return doUpdate(application_dir, update_zip_path, updater_path);
}

bool AutoUpdaterDialog::doUpdate(const std::string& application_dir, const std::string& zip_path, const std::string& updater_path)
{
	const std::string program_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdString();
	if (program_path.empty())
	{
		reportError("Failed to get current application path");
		return false;
	}

	const std::wstring wupdater_path = StringUtil::UTF8StringToWideString(updater_path);
	const std::wstring wapplication_dir = StringUtil::UTF8StringToWideString(application_dir);
	const std::wstring arguments = StringUtil::UTF8StringToWideString(fmt::format("{} \"{}\" \"{}\" \"{}\"",
		QCoreApplication::applicationPid(), application_dir, zip_path, program_path));

	const bool needs_elevation = doesUpdaterNeedElevation(application_dir);

	SHELLEXECUTEINFOW sei = {};
	sei.cbSize = sizeof(sei);
	sei.lpVerb = needs_elevation ? L"runas" : nullptr; // needed to trigger elevation
	sei.lpFile = wupdater_path.c_str();
	sei.lpParameters = arguments.c_str();
	sei.lpDirectory = wapplication_dir.c_str();
	sei.nShow = SW_SHOWNORMAL;
	if (!ShellExecuteExW(&sei))
	{
		reportError("Failed to start %s: %s", needs_elevation ? "elevated updater" : "updater",
			Error::CreateWin32(GetLastError()).GetDescription().c_str());
		return false;
	}

	return true;
}

void AutoUpdaterDialog::cleanupAfterUpdate()
{
	// If we weren't portable, then updater executable gets left in the application directory.
	if (EmuFolders::AppRoot == EmuFolders::DataRoot)
		return;

	const std::string updater_path = Path::Combine(EmuFolders::DataRoot, UPDATER_EXECUTABLE);
	if (!FileSystem::FileExists(updater_path.c_str()))
		return;

	if (!FileSystem::DeleteFilePath(updater_path.c_str()))
	{
		QMessageBox::critical(nullptr, tr("Updater Error"), tr("Failed to remove updater exe after update."));
		return;
	}
}

#elif defined(__linux__)

bool AutoUpdaterDialog::processUpdate(const std::vector<u8>& data, QProgressDialog&)
{
	const char* appimage_path = std::getenv("APPIMAGE");
	if (!appimage_path || !FileSystem::FileExists(appimage_path))
	{
		reportError("Missing APPIMAGE.");
		return false;
	}

	const QString qappimage_path(QString::fromUtf8(appimage_path));
	if (!QFile::exists(qappimage_path))
	{
		reportError("Current AppImage does not exist: %s", appimage_path);
		return false;
	}

	const QString new_appimage_path(qappimage_path + QStringLiteral(".new"));
	const QString backup_appimage_path(qappimage_path + QStringLiteral(".backup"));
	Console.WriteLn("APPIMAGE = %s", appimage_path);
	Console.WriteLn("Backup AppImage path = %s", backup_appimage_path.toUtf8().constData());
	Console.WriteLn("New AppImage path = %s", new_appimage_path.toUtf8().constData());

	// Remove old "new" appimage and existing backup appimage.
	if (QFile::exists(new_appimage_path) && !QFile::remove(new_appimage_path))
	{
		reportError("Failed to remove old destination AppImage: %s", new_appimage_path.toUtf8().constData());
		return false;
	}
	if (QFile::exists(backup_appimage_path) && !QFile::remove(backup_appimage_path))
	{
		reportError("Failed to remove old backup AppImage: %s", new_appimage_path.toUtf8().constData());
		return false;
	}

	// Write "new" appimage.
	{
		// We want to copy the permissions from the old appimage to the new one.
		QFile old_file(qappimage_path);
		const QFileDevice::Permissions old_permissions = old_file.permissions();
		QFile new_file(new_appimage_path);
		if (!new_file.open(QIODevice::WriteOnly) ||
			new_file.write(reinterpret_cast<const char*>(data.data()), static_cast<qint64>(data.size())) != static_cast<qint64>(data.size()) ||
			!new_file.setPermissions(old_permissions))
		{
			QFile::remove(new_appimage_path);
			reportError("Failed to write new destination AppImage: %s", new_appimage_path.toUtf8().constData());
			return false;
		}
	}

	// Rename "old" appimage.
	if (!QFile::rename(qappimage_path, backup_appimage_path))
	{
		reportError("Failed to rename old AppImage to %s", backup_appimage_path.toUtf8().constData());
		QFile::remove(new_appimage_path);
		return false;
	}

	// Rename "new" appimage.
	if (!QFile::rename(new_appimage_path, qappimage_path))
	{
		reportError("Failed to rename new AppImage to %s", qappimage_path.toUtf8().constData());
		return false;
	}

	// Execute new appimage.
	QProcess* new_process = new QProcess();
	new_process->setProgram(qappimage_path);
	new_process->setArguments(QStringList{QStringLiteral("-updatecleanup")});
	if (!new_process->startDetached())
	{
		reportError("Failed to execute new AppImage.");
		return false;
	}

	// We exit once we return.
	return true;
}

void AutoUpdaterDialog::cleanupAfterUpdate()
{
	// Remove old/backup AppImage.
	const char* appimage_path = std::getenv("APPIMAGE");
	if (!appimage_path)
		return;

	const QString qappimage_path(QString::fromUtf8(appimage_path));
	const QString backup_appimage_path(qappimage_path + QStringLiteral(".backup"));
	if (!QFile::exists(backup_appimage_path))
		return;

	Console.WriteLn(Color_StrongOrange, QStringLiteral("Removing backup AppImage %1").arg(backup_appimage_path).toStdString());
	if (!QFile::remove(backup_appimage_path))
		Console.Error(QStringLiteral("Failed to remove backup AppImage %1").arg(backup_appimage_path).toStdString());
}

#elif defined(__APPLE__)

static QString UpdateVersionNumberInName(QString name, QStringView new_version)
{
	QString current_version_string = QStringLiteral(GIT_TAG);
	QStringView current_version = current_version_string;
	if (!current_version.empty() && !new_version.empty() && current_version[0] == 'v' && new_version[0] == 'v')
	{
		current_version = current_version.mid(1);
		new_version = new_version.mid(1);
	}
	if (!current_version.empty() && !new_version.empty())
		name.replace(current_version.data(), current_version.size(), new_version.data(), new_version.size());
	return name;
}

bool AutoUpdaterDialog::processUpdate(const std::vector<u8>& data, QProgressDialog& progress)
{
	std::optional<std::string> path = CocoaTools::GetNonTranslocatedBundlePath();
	if (!path.has_value())
	{
		reportError("Couldn't get bundle path");
		return false;
	}

	QFileInfo info(QString::fromStdString(*path));
	if (!info.isBundle())
	{
		reportError("Application %s isn't a bundle", path->c_str());
		return false;
	}
	if (info.suffix() != QStringLiteral("app"))
	{
		reportError("Unexpected application suffix %s on %s", info.suffix().toUtf8().constData(), path->c_str());
		return false;
	}
	QString open_path;
	{
		QTemporaryDir temp_dir(info.path() + QStringLiteral("/PCSX2-UpdateStaging-XXXXXX"));
		if (!temp_dir.isValid())
		{
			reportError("Failed to create update staging directory");
			return false;
		}

		constexpr size_t chunk_size = 65536;
		progress.setLabelText(QStringLiteral("Unpacking update..."));
		progress.reset();
		progress.setRange(0, static_cast<int>((data.size() + chunk_size - 1) / chunk_size));

		QProcess untar;
		untar.setProgram(QStringLiteral("/usr/bin/tar"));
		untar.setArguments({QStringLiteral("xC"), temp_dir.path()});
		untar.start();
		for (size_t i = 0; i < data.size(); i += chunk_size)
		{
			progress.setValue(static_cast<int>(i / chunk_size));
			const size_t amt = std::min(data.size() - i, chunk_size);
			if (progress.wasCanceled() ||
				untar.write(reinterpret_cast<const char*>(data.data() + i), static_cast<qsizetype>(amt)) != static_cast<qsizetype>(amt))
			{
				if (!progress.wasCanceled())
					reportError("Failed to unpack update (write stopped short)");
				untar.closeWriteChannel();
				if (!untar.waitForFinished(1000))
					untar.kill();
				return false;
			}
		}
		untar.closeWriteChannel();
		while (!untar.waitForFinished(1000))
		{
			if (progress.wasCanceled())
			{
				untar.kill();
				return false;
			}
		}
		progress.setValue(progress.maximum());
		if (untar.exitCode() != EXIT_SUCCESS)
		{
			QByteArray msg = untar.readAllStandardError();
			const char* join = msg.isEmpty() ? "" : ": ";
			reportError("Failed to unpack update (tar exited with %u%s%s)", untar.exitCode(), join, msg.toStdString().c_str());
			return false;
		}

		QFileInfoList temp_dir_contents = QDir(temp_dir.path()).entryInfoList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
		auto new_app = std::find_if(temp_dir_contents.begin(), temp_dir_contents.end(), [](const QFileInfo& file) { return file.suffix() == QStringLiteral("app"); });
		if (new_app == temp_dir_contents.end())
		{
			reportError("Couldn't find application in update package");
			return false;
		}
		QString new_name = UpdateVersionNumberInName(info.completeBaseName(), m_latest_version);
		std::optional<std::string> trashed_path = CocoaTools::MoveToTrash(*path);
		if (!trashed_path.has_value())
		{
			reportError("Failed to trash old application");
			return false;
		}
		open_path = info.path() + QStringLiteral("/") + new_name + QStringLiteral(".app");
		if (!QFile::rename(new_app->absoluteFilePath(), open_path))
		{
			QFile::rename(QString::fromStdString(*trashed_path), info.filePath());
			reportError("Failed to move new application into place (couldn't rename '%s' to '%s')",
				new_app->absoluteFilePath().toUtf8().constData(), open_path.toUtf8().constData());
			return false;
		}
		QDir(QString::fromStdString(*trashed_path)).removeRecursively();
	}
	// For some reason if I use QProcess the shell gets killed immediately with SIGKILL, but NSTask is fine...
	if (!CocoaTools::DelayedLaunch(open_path.toStdString()))
	{
		reportError("Failed to start new application");
		return false;
	}
	return true;
}

void AutoUpdaterDialog::cleanupAfterUpdate()
{
}

#else

bool AutoUpdaterDialog::processUpdate(const QByteArray& update_data, QProgressDialog& progress)
{
	return false;
}

void AutoUpdaterDialog::cleanupAfterUpdate()
{
}

#endif
