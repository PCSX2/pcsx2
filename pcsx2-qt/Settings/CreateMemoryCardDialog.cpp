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

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "Settings/CreateMemoryCardDialog.h"

#include "pcsx2/MemoryCardFile.h"
#include "pcsx2/System.h"

CreateMemoryCardDialog::CreateMemoryCardDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_ui.setupUi(this);
	m_ui.icon->setPixmap(QIcon::fromTheme("memcard-line").pixmap(m_ui.icon->width()));

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	connect(m_ui.name, &QLineEdit::textChanged, this, &CreateMemoryCardDialog::nameTextChanged);

	connect(m_ui.size8MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_8MB); });
	connect(m_ui.size16MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_16MB); });
	connect(m_ui.size32MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_32MB); });
	connect(m_ui.size64MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_64MB); });
	connect(m_ui.size128KB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS1); });
	connect(m_ui.sizeFolder, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::Folder, MemoryCardFileType::Unknown); });

	disconnect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, nullptr);

	connect(m_ui.buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &CreateMemoryCardDialog::createCard);
	connect(m_ui.buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &CreateMemoryCardDialog::close);
	connect(m_ui.buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &CreateMemoryCardDialog::restoreDefaults);

#ifndef _WIN32
	m_ui.ntfsCompression->setEnabled(false);
#endif

	updateState();
}

CreateMemoryCardDialog::~CreateMemoryCardDialog() = default;

void CreateMemoryCardDialog::nameTextChanged()
{
	QString controlName(m_ui.name->text());
	const int cursorPos = m_ui.name->cursorPosition();

	controlName.replace(".", "");

	QSignalBlocker sb(m_ui.name);
	if (controlName.isEmpty())
		m_ui.name->setText(QString());
	else
		m_ui.name->setText(controlName);

	m_ui.name->setCursorPosition(cursorPos);
	updateState();
}

void CreateMemoryCardDialog::setType(MemoryCardType type, MemoryCardFileType fileType)
{
	m_type = type;
	m_fileType = fileType;
	updateState();
}

void CreateMemoryCardDialog::restoreDefaults()
{
	setType(MemoryCardType::File, MemoryCardFileType::PS2_8MB);
	m_ui.size8MB->setChecked(true);
	m_ui.size16MB->setChecked(false);
	m_ui.size32MB->setChecked(false);
	m_ui.size64MB->setChecked(false);
	m_ui.size128KB->setChecked(false);
	m_ui.sizeFolder->setChecked(false);
}

void CreateMemoryCardDialog::updateState()
{
	const bool okay = (m_ui.name->text().length() > 0);

	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(okay);
#ifdef _WIN32
	m_ui.ntfsCompression->setEnabled(m_type == MemoryCardType::File);
#endif
}

void CreateMemoryCardDialog::createCard()
{
	QString name(m_ui.name->text());
	std::string nameStr;

	if (m_fileType == MemoryCardFileType::PS1)
	{
		name += QStringLiteral(".mcr");
	}
	else
	{
		name += QStringLiteral(".ps2");
	}

	nameStr = name.toStdString();

	if (FileMcd_GetCardInfo(nameStr).has_value())
	{
		QMessageBox::critical(this, tr("Create Memory Card"),
			tr("Failed to create the Memory Card, because another card with the name '%1' already exists.").arg(name));
		return;
	}

	if (!FileMcd_CreateNewCard(nameStr, m_type, m_fileType))
	{
		QMessageBox::critical(this, tr("Create Memory Card"),
			tr("Failed to create the Memory Card, the log may contain more information."));
		return;
	}

#ifdef  _WIN32
	if (m_ui.ntfsCompression->isChecked() && m_type == MemoryCardType::File)
	{
		const std::string fullPath(Path::Combine(EmuFolders::MemoryCards, nameStr));
		FileSystem::SetPathCompression(fullPath.c_str(), true);
	}
#endif

	QMessageBox::information(this, tr("Create Memory Card"), tr("Memory Card '%1' created.").arg(name));
	accept();
}
