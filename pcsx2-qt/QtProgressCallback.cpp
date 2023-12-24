// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Assertions.h"

#include "QtProgressCallback.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtWidgets/QMessageBox>
#include <array>

QtModalProgressCallback::QtModalProgressCallback(QWidget* parent_widget, float show_delay)
	: QObject(parent_widget)
	, m_dialog(QString(), QString(), 0, 1, parent_widget)
	, m_show_delay(show_delay)
{
	m_dialog.setWindowTitle(tr("PCSX2"));
	m_dialog.setMinimumSize(QSize(500, 0));
	m_dialog.setModal(parent_widget != nullptr);
	m_dialog.setAutoClose(false);
	m_dialog.setAutoReset(false);
	connect(&m_dialog, &QProgressDialog::canceled, this, &QtModalProgressCallback::dialogCancelled);
	checkForDelayedShow();
}

QtModalProgressCallback::~QtModalProgressCallback() = default;

void QtModalProgressCallback::SetCancellable(bool cancellable)
{
	if (m_cancellable == cancellable)
		return;

	BaseProgressCallback::SetCancellable(cancellable);
	m_dialog.setCancelButtonText(cancellable ? tr("Cancel") : QString());
}

void QtModalProgressCallback::SetTitle(const char* title)
{
	m_dialog.setWindowTitle(QString::fromUtf8(title));
}

void QtModalProgressCallback::SetStatusText(const char* text)
{
	BaseProgressCallback::SetStatusText(text);
	checkForDelayedShow();

	if (m_dialog.isVisible())
		m_dialog.setLabelText(QString::fromUtf8(text));
}

void QtModalProgressCallback::SetProgressRange(u32 range)
{
	BaseProgressCallback::SetProgressRange(range);
	checkForDelayedShow();

	if (m_dialog.isVisible())
		m_dialog.setRange(0, m_progress_range);
}

void QtModalProgressCallback::SetProgressValue(u32 value)
{
	BaseProgressCallback::SetProgressValue(value);
	checkForDelayedShow();

	if (m_dialog.isVisible() && static_cast<u32>(m_dialog.value()) != m_progress_range)
		m_dialog.setValue(m_progress_value);

	QCoreApplication::processEvents();
}

void QtModalProgressCallback::DisplayError(const char* message)
{
	qWarning() << message;
}

void QtModalProgressCallback::DisplayWarning(const char* message)
{
	qWarning() << message;
}

void QtModalProgressCallback::DisplayInformation(const char* message)
{
	qWarning() << message;
}

void QtModalProgressCallback::DisplayDebugMessage(const char* message)
{
	qWarning() << message;
}

void QtModalProgressCallback::ModalError(const char* message)
{
	QMessageBox::critical(&m_dialog, tr("Error"), QString::fromUtf8(message));
}

bool QtModalProgressCallback::ModalConfirmation(const char* message)
{
	return (QMessageBox::question(&m_dialog, tr("Question"), QString::fromUtf8(message), QMessageBox::Yes,
				QMessageBox::No) == QMessageBox::Yes);
}

void QtModalProgressCallback::ModalInformation(const char* message)
{
	QMessageBox::information(&m_dialog, tr("Information"), QString::fromUtf8(message));
}

void QtModalProgressCallback::dialogCancelled()
{
	m_cancelled = true;
}

void QtModalProgressCallback::checkForDelayedShow()
{
	if (m_dialog.isVisible())
		return;

	if (m_show_timer.GetTimeSeconds() >= m_show_delay)
	{
		m_dialog.setRange(0, m_progress_range);
		m_dialog.setValue(m_progress_value);
		m_dialog.show();
	}
}

QtAsyncProgressThread::QtAsyncProgressThread(QWidget* parent)
	: QThread()
{
	// NOTE: We deliberately don't set the thread parent, because otherwise we can't move it.
}

QtAsyncProgressThread::~QtAsyncProgressThread() = default;

bool QtAsyncProgressThread::IsCancelled() const
{
	return isInterruptionRequested();
}

void QtAsyncProgressThread::SetCancellable(bool cancellable)
{
	if (m_cancellable == cancellable)
		return;

	BaseProgressCallback::SetCancellable(cancellable);
}

void QtAsyncProgressThread::SetTitle(const char* title)
{
	emit titleUpdated(QString::fromUtf8(title));
}

void QtAsyncProgressThread::SetStatusText(const char* text)
{
	BaseProgressCallback::SetStatusText(text);
	emit statusUpdated(QString::fromUtf8(text));
}

void QtAsyncProgressThread::SetProgressRange(u32 range)
{
	BaseProgressCallback::SetProgressRange(range);
	emit progressUpdated(static_cast<int>(m_progress_value), static_cast<int>(m_progress_range));
}

void QtAsyncProgressThread::SetProgressValue(u32 value)
{
	BaseProgressCallback::SetProgressValue(value);
	emit progressUpdated(static_cast<int>(m_progress_value), static_cast<int>(m_progress_range));
}

void QtAsyncProgressThread::DisplayError(const char* message)
{
	qWarning() << message;
}

void QtAsyncProgressThread::DisplayWarning(const char* message)
{
	qWarning() << message;
}

void QtAsyncProgressThread::DisplayInformation(const char* message)
{
	qWarning() << message;
}

void QtAsyncProgressThread::DisplayDebugMessage(const char* message)
{
	qWarning() << message;
}

void QtAsyncProgressThread::ModalError(const char* message)
{
	QMessageBox::critical(parentWidget(), tr("Error"), QString::fromUtf8(message));
}

bool QtAsyncProgressThread::ModalConfirmation(const char* message)
{
	return (QMessageBox::question(parentWidget(), tr("Question"), QString::fromUtf8(message), QMessageBox::Yes,
				QMessageBox::No) == QMessageBox::Yes);
}

void QtAsyncProgressThread::ModalInformation(const char* message)
{
	QMessageBox::information(parentWidget(), tr("Information"), QString::fromUtf8(message));
}

void QtAsyncProgressThread::start()
{
	pxAssertRel(!isRunning(), "Async progress thread is not already running");

	QThread::start();
	moveToThread(this);
	m_starting_thread = QThread::currentThread();
	m_start_semaphore.release();
}

void QtAsyncProgressThread::join()
{
	if (isRunning())
		QThread::wait();
}

void QtAsyncProgressThread::run()
{
	m_start_semaphore.acquire();
	emit threadStarting();
	runAsync();
	emit threadFinished();
	moveToThread(m_starting_thread);
}

QWidget* QtAsyncProgressThread::parentWidget() const
{
	return qobject_cast<QWidget*>(parent());
}
