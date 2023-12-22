/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
	bool doUpdate(const QString& zip_path, const QString& updater_path, const QString& destination_path);
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
