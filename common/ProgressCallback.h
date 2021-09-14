/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "Pcsx2Defs.h"
#include <string>

/**
 * Progress callbacks, abstracts a blocking operation and allows it to report progress
 * without having any dependency on the UI.
 */

class ProgressCallback
{
public:
	virtual ~ProgressCallback();

	virtual void PushState() = 0;
	virtual void PopState() = 0;

	virtual bool IsCancelled() const = 0;
	virtual bool IsCancellable() const = 0;

	virtual void SetCancellable(bool cancellable) = 0;

	virtual void SetTitle(const char* title) = 0;
	virtual void SetStatusText(const char* text) = 0;
	virtual void SetProgressRange(u32 range) = 0;
	virtual void SetProgressValue(u32 value) = 0;
	virtual void IncrementProgressValue() = 0;

	void SetFormattedStatusText(const char* Format, ...);

	virtual void DisplayError(const char* message) = 0;
	virtual void DisplayWarning(const char* message) = 0;
	virtual void DisplayInformation(const char* message) = 0;
	virtual void DisplayDebugMessage(const char* message) = 0;

	virtual void ModalError(const char* message) = 0;
	virtual bool ModalConfirmation(const char* message) = 0;
	virtual void ModalInformation(const char* message) = 0;

	void DisplayFormattedError(const char* format, ...);
	void DisplayFormattedWarning(const char* format, ...);
	void DisplayFormattedInformation(const char* format, ...);
	void DisplayFormattedDebugMessage(const char* format, ...);
	void DisplayFormattedModalError(const char* format, ...);
	bool DisplayFormattedModalConfirmation(const char* format, ...);
	void DisplayFormattedModalInformation(const char* format, ...);

public:
	static ProgressCallback* NullProgressCallback;
};

class BaseProgressCallback : public ProgressCallback
{
public:
	BaseProgressCallback();
	virtual ~BaseProgressCallback();

	virtual void PushState() override;
	virtual void PopState() override;

	virtual bool IsCancelled() const override;
	virtual bool IsCancellable() const override;

	virtual void SetCancellable(bool cancellable) override;
	virtual void SetStatusText(const char* text) override;
	virtual void SetProgressRange(u32 range) override;
	virtual void SetProgressValue(u32 value) override;
	virtual void IncrementProgressValue() override;

protected:
	struct State
	{
		State* next_saved_state;
		std::string status_text;
		u32 progress_range;
		u32 progress_value;
		u32 base_progress_value;
		bool cancellable;
	};

	bool m_cancellable;
	bool m_cancelled;
	std::string m_status_text;
	u32 m_progress_range;
	u32 m_progress_value;

	u32 m_base_progress_value;

	State* m_saved_state;
};
