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

#pragma once
#include "ui_AutoUpdaterDialog.h"
#include <string>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtWidgets/QDialog>

class QNetworkAccessManager;
class QNetworkReply;
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

Q_SIGNALS:
	void updateCheckCompleted();

public Q_SLOTS:
	void queueUpdateCheck(bool display_message);

private Q_SLOTS:
	void getLatestReleaseComplete(QNetworkReply* reply);

	void queueGetChanges();
	void getChangesComplete(QNetworkReply* reply);

	void downloadUpdateClicked();
	void skipThisUpdateClicked();
	void remindMeLaterClicked();

private:
	void reportError(const char* msg, ...);
	void checkIfUpdateNeeded();
	QString getCurrentUpdateTag() const;

	bool processUpdate(const QByteArray& update_data, QProgressDialog& progress);
#if defined(_WIN32)
	bool doUpdate(const QString& zip_path, const QString& updater_path, const QString& destination_path);
#endif

	Ui::AutoUpdaterDialog m_ui;

	QNetworkAccessManager* m_network_access_mgr = nullptr;
	QString m_latest_version;
	QDateTime m_latest_version_timestamp;
	QString m_download_url;
	int m_download_size = 0;

	bool m_display_messages = false;
	bool m_update_will_break_save_states = false;
};
