// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
