// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include "ui_AutoUpdaterDialog.h"

#include <memory>
#include <string>

#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtWidgets/QDialog>

class HTTPDownloader;
class QProgressDialog;

class AutoUpdaterDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit AutoUpdaterDialog(QWidget* parent = nullptr);
	~AutoUpdaterDialog();

	static bool isSupported();
	static QStringList getTagList();
	static std::string getDefaultTag();
	static QString getCurrentVersion();
	static QString getCurrentVersionDate();
	static void cleanupAfterUpdate();

Q_SIGNALS:
	void updateCheckCompleted();

public Q_SLOTS:
	void queueUpdateCheck(bool display_message);

private Q_SLOTS:
	void httpPollTimerPoll();
	void downloadUpdateClicked();
	void skipThisUpdateClicked();
	void remindMeLaterClicked();

private:
	void reportError(const char* msg, ...);

	bool ensureHttpReady();

	void checkIfUpdateNeeded();
	QString getCurrentUpdateTag() const;

	void getLatestReleaseComplete(s32 status_code, std::vector<u8> data);

	void queueGetChanges();
	void getChangesComplete(s32 status_code, std::vector<u8> data);

	bool processUpdate(const std::vector<u8>& data, QProgressDialog& progress);
#if defined(_WIN32)
	bool doesUpdaterNeedElevation(const std::string& application_dir) const;
	bool doUpdate(const std::string& application_dir, const std::string& zip_path, const std::string& updater_path);
#endif

	Ui::AutoUpdaterDialog m_ui;

	std::unique_ptr<HTTPDownloader> m_http;
	QTimer* m_http_poll_timer = nullptr;
	QString m_latest_version;
	QDateTime m_latest_version_timestamp;
	QString m_download_url;
	int m_download_size = 0;

	bool m_display_messages = false;
	bool m_update_will_break_save_states = false;
};
