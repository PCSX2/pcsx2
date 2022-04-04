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

#include "NewInputRecordingDlg.h"

#include "QtUtils.h"
#include <QtCore/QString>
#include <QtWidgets/QDialog>
#include <QtWidgets/qfiledialog.h>

NewInputRecordingDlg::NewInputRecordingDlg(QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedSize(geometry().width(), geometry().height());

	// Default State
	m_ui.m_recTypeWarning->hide();
	m_ui.m_dlgBtns->button(QDialogButtonBox::Ok)->setEnabled(false);
	m_ui.m_filePathInput->setDisabled(true);

	connect(m_ui.m_recTypePowerOn, &QRadioButton::clicked, this, &NewInputRecordingDlg::onRecordingTypePowerOnChecked);
	connect(m_ui.m_recTypeSaveState, &QRadioButton::clicked, this, &NewInputRecordingDlg::onRecordingTypeSaveStateChecked);

	connect(m_ui.m_filePathBrowseBtn, &QPushButton::clicked, this, &NewInputRecordingDlg::onBrowseForPathClicked);
	connect(m_ui.m_authorInput, &QLineEdit::textEdited, this, &NewInputRecordingDlg::onAuthorNameChanged);
}

NewInputRecordingDlg::~NewInputRecordingDlg() = default;

InputRecording::Type NewInputRecordingDlg::getInputRecType()
{
	return m_recType;
}

std::string NewInputRecordingDlg::getFilePath()
{
	return m_filePath.toStdString();
}

std::string NewInputRecordingDlg::getAuthorName()
{
	return m_authorName.toStdString();
}

void NewInputRecordingDlg::onRecordingTypePowerOnChecked(bool checked)
{
	if (checked)
	{
		m_recType = InputRecording::Type::POWER_ON;
		m_ui.m_recTypeWarning->hide();
	}
}

void NewInputRecordingDlg::onRecordingTypeSaveStateChecked(bool checked)
{
	if (checked)
	{
		m_recType = InputRecording::Type::FROM_SAVESTATE;
		m_ui.m_recTypeWarning->show();
	}
}

void NewInputRecordingDlg::onBrowseForPathClicked()
{
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setWindowTitle("Select a File");
	dialog.setNameFilter(tr("Input Recording Files (*.p2m2)"));
	dialog.setDefaultSuffix("p2m2");
	QStringList fileNames;
	if (dialog.exec())
	{
		fileNames = dialog.selectedFiles();
	}
	if (fileNames.length() > 0)
	{
		m_filePath = fileNames.first();
		m_ui.m_filePathInput->setText(m_filePath);
		updateFormStatus();
	}
}

void NewInputRecordingDlg::onAuthorNameChanged(const QString& text)
{
	m_authorName = text;
	updateFormStatus();
}

bool NewInputRecordingDlg::isFormValid()
{
	return !m_filePath.isEmpty() && !m_authorName.isEmpty();
}

void NewInputRecordingDlg::updateFormStatus()
{
	m_ui.m_dlgBtns->button(QDialogButtonBox::Ok)->setEnabled(isFormValid());
}
