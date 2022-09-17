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
#include "common/ProgressCallback.h"
#include "common/Timer.h"
#include <QtCore/QThread>
#include <QtCore/QSemaphore>
#include <QtWidgets/QProgressDialog>
#include <atomic>

class QtModalProgressCallback final : public QObject, public BaseProgressCallback
{
	Q_OBJECT

public:
	QtModalProgressCallback(QWidget* parent_widget, float show_delay = 0.0f);
	~QtModalProgressCallback();

	bool IsCancelled() const override;

	void SetCancellable(bool cancellable) override;
	void SetTitle(const char* title) override;
	void SetStatusText(const char* text) override;
	void SetProgressRange(u32 range) override;
	void SetProgressValue(u32 value) override;

	void DisplayError(const char* message) override;
	void DisplayWarning(const char* message) override;
	void DisplayInformation(const char* message) override;
	void DisplayDebugMessage(const char* message) override;

	void ModalError(const char* message) override;
	bool ModalConfirmation(const char* message) override;
	void ModalInformation(const char* message) override;

private:
	void checkForDelayedShow();

	QProgressDialog m_dialog;
	Common::Timer m_show_timer;
	float m_show_delay;
};

class QtAsyncProgressThread : public QThread, public BaseProgressCallback
{
	Q_OBJECT

public:
	QtAsyncProgressThread(QWidget* parent);
	~QtAsyncProgressThread();

	bool IsCancelled() const override;

	void SetCancellable(bool cancellable) override;
	void SetTitle(const char* title) override;
	void SetStatusText(const char* text) override;
	void SetProgressRange(u32 range) override;
	void SetProgressValue(u32 value) override;

	void DisplayError(const char* message) override;
	void DisplayWarning(const char* message) override;
	void DisplayInformation(const char* message) override;
	void DisplayDebugMessage(const char* message) override;

	void ModalError(const char* message) override;
	bool ModalConfirmation(const char* message) override;
	void ModalInformation(const char* message) override;

Q_SIGNALS:
	void titleUpdated(const QString& title);
	void statusUpdated(const QString& status);
	void progressUpdated(int value, int range);
	void threadStarting();
	void threadFinished();

public Q_SLOTS:
	void start();
	void join();

protected:
	virtual void runAsync() = 0;
	void run() final;

private:
	QWidget* parentWidget() const;

	QSemaphore m_start_semaphore;
	QThread* m_starting_thread = nullptr;
};
