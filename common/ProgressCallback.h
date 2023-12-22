// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	enum class ProgressState
	{
		Normal,
		Indeterminate,
		Paused,
		Error
	};

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
	virtual void SetProgressState(ProgressState state) = 0;

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
	virtual void SetProgressState(ProgressState state) override;

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

	bool m_cancellable = false;
	bool m_cancelled = false;
	std::string m_status_text;
	u32 m_progress_range = 1;
	u32 m_progress_value = 0;
	ProgressState m_progress_state = ProgressState::Normal;

	u32 m_base_progress_value = 0;

	State* m_saved_state = nullptr;
};
