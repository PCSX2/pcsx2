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

#include "common/ProgressCallback.h"
#include "common/Timer.h"
#include "pcsx2/Frontend/GameList.h"

#include "GameListRefreshThread.h"

#include <QtWidgets/QMessageBox>

AsyncRefreshProgressCallback::AsyncRefreshProgressCallback(GameListRefreshThread* parent)
	: m_parent(parent)
{
}

void AsyncRefreshProgressCallback::Cancel()
{
	// Not atomic, but we don't need to cancel immediately.
	m_cancelled = true;
}

void AsyncRefreshProgressCallback::SetStatusText(const char* text)
{
	QString new_text(QString::fromUtf8(text));
	if (new_text == m_status_text)
		return;

	m_status_text = new_text;
	fireUpdate();
}

void AsyncRefreshProgressCallback::SetProgressRange(u32 range)
{
	BaseProgressCallback::SetProgressRange(range);
	if (static_cast<int>(m_progress_range) == m_last_range)
		return;

	m_last_range = static_cast<int>(m_progress_range);
	fireUpdate();
}

void AsyncRefreshProgressCallback::SetProgressValue(u32 value)
{
	BaseProgressCallback::SetProgressValue(value);
	if (static_cast<int>(m_progress_value) == m_last_value)
		return;

	m_last_value = static_cast<int>(m_progress_value);
	fireUpdate();
}

void AsyncRefreshProgressCallback::SetTitle(const char* title) {}

void AsyncRefreshProgressCallback::DisplayError(const char* message)
{
	QMessageBox::critical(nullptr, QStringLiteral("Error"), QString::fromUtf8(message));
}

void AsyncRefreshProgressCallback::DisplayWarning(const char* message)
{
	QMessageBox::warning(nullptr, QStringLiteral("Warning"), QString::fromUtf8(message));
}

void AsyncRefreshProgressCallback::DisplayInformation(const char* message)
{
	QMessageBox::information(nullptr, QStringLiteral("Information"), QString::fromUtf8(message));
}

void AsyncRefreshProgressCallback::DisplayDebugMessage(const char* message)
{
	qDebug() << message;
}

void AsyncRefreshProgressCallback::ModalError(const char* message)
{
	QMessageBox::critical(nullptr, QStringLiteral("Error"), QString::fromUtf8(message));
}

bool AsyncRefreshProgressCallback::ModalConfirmation(const char* message)
{
	return QMessageBox::question(nullptr, QStringLiteral("Question"), QString::fromUtf8(message)) == QMessageBox::Yes;
}

void AsyncRefreshProgressCallback::ModalInformation(const char* message)
{
	QMessageBox::information(nullptr, QStringLiteral("Information"), QString::fromUtf8(message));
}

void AsyncRefreshProgressCallback::fireUpdate()
{
	m_parent->refreshProgress(m_status_text, m_last_value, m_last_range);
}

GameListRefreshThread::GameListRefreshThread(bool invalidate_cache)
	: QThread()
	, m_progress(this)
	, m_invalidate_cache(invalidate_cache)
{
}

GameListRefreshThread::~GameListRefreshThread() = default;

void GameListRefreshThread::cancel()
{
	m_progress.Cancel();
}

void GameListRefreshThread::run()
{
	GameList::Refresh(m_invalidate_cache, &m_progress);
	emit refreshComplete();
}
