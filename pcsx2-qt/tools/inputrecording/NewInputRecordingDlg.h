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
