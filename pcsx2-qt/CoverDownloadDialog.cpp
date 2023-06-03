/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "CoverDownloadDialog.h"

#include "pcsx2/GameList.h"

#include "common/Assertions.h"

CoverDownloadDialog::CoverDownloadDialog(QWidget* parent /*= nullptr*/)
	: QDialog(parent)
{
	m_ui.setupUi(this);
	m_ui.coverIcon->setPixmap(QIcon::fromTheme("artboard-2-line").pixmap(32));
	updateEnabled();

	connect(m_ui.start, &QPushButton::clicked, this, &CoverDownloadDialog::onStartClicked);
	connect(m_ui.close, &QPushButton::clicked, this, &CoverDownloadDialog::onCloseClicked);
	connect(m_ui.urls, &QTextEdit::textChanged, this, &CoverDownloadDialog::updateEnabled);
}

CoverDownloadDialog::~CoverDownloadDialog()
{
	pxAssert(!m_thread);
}

void CoverDownloadDialog::closeEvent(QCloseEvent* ev)
{
	cancelThread();
}

void CoverDownloadDialog::onDownloadStatus(const QString& text)
{
	m_ui.status->setText(text);
}

void CoverDownloadDialog::onDownloadProgress(int value, int range)
{
	// Limit to once every five seconds, otherwise it's way too flickery.
	// Ideally in the future we'd have some way to invalidate only a single cover.
	if (m_last_refresh_time.GetTimeSeconds() >= 5.0f)
	{
		emit coverRefreshRequested();
		m_last_refresh_time.Reset();
	}

	if (range != m_ui.progress->maximum())
		m_ui.progress->setMaximum(range);
	m_ui.progress->setValue(value);
}

void CoverDownloadDialog::onDownloadComplete()
{
	emit coverRefreshRequested();

	if (m_thread)
	{
		m_thread->join();
		m_thread.reset();
	}

	updateEnabled();

	m_ui.status->setText(tr("Download complete."));
}

void CoverDownloadDialog::onStartClicked()
{
	if (m_thread)
		cancelThread();
	else
		startThread();
}

void CoverDownloadDialog::onCloseClicked()
{
	if (m_thread)
		cancelThread();

	done(0);
}

void CoverDownloadDialog::updateEnabled()
{
	const bool running = static_cast<bool>(m_thread);
	m_ui.start->setText(running ? tr("Stop") : tr("Start"));
	m_ui.start->setEnabled(running || !m_ui.urls->toPlainText().isEmpty());
	m_ui.close->setEnabled(!running);
	m_ui.urls->setEnabled(!running);
}

void CoverDownloadDialog::startThread()
{
	m_thread = std::make_unique<CoverDownloadThread>(this, m_ui.urls->toPlainText(), m_ui.useSerialFileNames->isChecked());
	m_last_refresh_time.Reset();
	connect(m_thread.get(), &CoverDownloadThread::statusUpdated, this, &CoverDownloadDialog::onDownloadStatus);
	connect(m_thread.get(), &CoverDownloadThread::progressUpdated, this, &CoverDownloadDialog::onDownloadProgress);
	connect(m_thread.get(), &CoverDownloadThread::threadFinished, this, &CoverDownloadDialog::onDownloadComplete);
	m_thread->start();
	updateEnabled();
}

void CoverDownloadDialog::cancelThread()
{
	if (!m_thread)
		return;

	m_thread->requestInterruption();
	m_thread->join();
	m_thread.reset();
}

CoverDownloadDialog::CoverDownloadThread::CoverDownloadThread(QWidget* parent, const QString& urls, bool use_serials)
	: QtAsyncProgressThread(parent), m_use_serials(use_serials)
{
	for (const QString& str : urls.split(QChar('\n')))
		m_urls.push_back(str.toStdString());
}

CoverDownloadDialog::CoverDownloadThread::~CoverDownloadThread() = default;

void CoverDownloadDialog::CoverDownloadThread::runAsync()
{
	GameList::DownloadCovers(m_urls, m_use_serials, this);
}
