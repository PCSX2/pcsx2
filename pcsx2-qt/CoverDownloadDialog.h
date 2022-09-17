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
#include "common/Timer.h"
#include "common/Pcsx2Defs.h"
#include "QtProgressCallback.h"
#include "ui_CoverDownloadDialog.h"
#include <QtWidgets/QDialog>
#include <array>
#include <memory>
#include <string>

class CoverDownloadDialog final : public QDialog
{
	Q_OBJECT

public:
	CoverDownloadDialog(QWidget* parent = nullptr);
	~CoverDownloadDialog();

Q_SIGNALS:
	void coverRefreshRequested();

protected:
	void closeEvent(QCloseEvent* ev);

private Q_SLOTS:
	void onDownloadStatus(const QString& text);
	void onDownloadProgress(int value, int range);
	void onDownloadComplete();
	void onStartClicked();
	void onCloseClicked();
	void updateEnabled();

private:
	class CoverDownloadThread : public QtAsyncProgressThread
	{
	public:
		CoverDownloadThread(QWidget* parent, const QString& urls, bool use_serials);
		~CoverDownloadThread();

	protected:
		void runAsync() override;

	private:
		std::vector<std::string> m_urls;
		bool m_use_serials;
	};

	void startThread();
	void cancelThread();

	Ui::CoverDownloadDialog m_ui;
	std::unique_ptr<CoverDownloadThread> m_thread;
	Common::Timer m_last_refresh_time;
};
