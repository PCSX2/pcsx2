// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_NewInputRecordingDlg.h"

#include "pcsx2/Recording/InputRecording.h"

#include <QtWidgets/QDialog>

class NewInputRecordingDlg final : public QDialog
{
	Q_OBJECT

public:
	explicit NewInputRecordingDlg(QWidget* parent = nullptr);
	~NewInputRecordingDlg();

	InputRecording::Type getInputRecType();
	std::string getFilePath();
	std::string getAuthorName();

private Q_SLOTS:
	void onRecordingTypePowerOnChecked(bool checked);
	void onRecordingTypeSaveStateChecked(bool checked);

	void onBrowseForPathClicked();
	void onAuthorNameChanged(const QString& text);

private:
	Ui::NewInputRecordingDlg m_ui;

	InputRecording::Type m_recType = InputRecording::Type::POWER_ON;
	QString m_filePath = "";
	QString m_authorName = "";

	bool isFormValid();
	void updateFormStatus();
};
