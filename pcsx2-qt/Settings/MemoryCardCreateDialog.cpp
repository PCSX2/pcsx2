// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "Settings/MemoryCardCreateDialog.h"

#include "pcsx2/SIO/Memcard/MemoryCardFile.h"

MemoryCardCreateDialog::MemoryCardCreateDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_ui.setupUi(this);
	m_ui.icon->setPixmap(QIcon::fromTheme("memcard-line").pixmap(m_ui.icon->width()));

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	connect(m_ui.name, &QLineEdit::textChanged, this, &MemoryCardCreateDialog::nameTextChanged);

	connect(m_ui.size8MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_8MB); });
	connect(m_ui.size16MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_16MB); });
	connect(m_ui.size32MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_32MB); });
	connect(m_ui.size64MB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS2_64MB); });
	connect(m_ui.size128KB, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::File, MemoryCardFileType::PS1); });
	connect(m_ui.sizeFolder, &QRadioButton::clicked, this, [this]() { setType(MemoryCardType::Folder, MemoryCardFileType::Unknown); });

	disconnect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, nullptr);

	connect(m_ui.buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &MemoryCardCreateDialog::createCard);
	connect(m_ui.buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &MemoryCardCreateDialog::close);
	connect(m_ui.buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &MemoryCardCreateDialog::restoreDefaults);

#ifndef _WIN32
	m_ui.ntfsCompression->setEnabled(false);
#endif

	updateState();
}

MemoryCardCreateDialog::~MemoryCardCreateDialog() = default;

void MemoryCardCreateDialog::nameTextChanged()
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

void MemoryCardCreateDialog::setType(MemoryCardType type, MemoryCardFileType fileType)
{
	m_type = type;
	m_fileType = fileType;
	updateState();
}

void MemoryCardCreateDialog::restoreDefaults()
{
	setType(MemoryCardType::File, MemoryCardFileType::PS2_8MB);
	m_ui.size8MB->setChecked(true);
	m_ui.size16MB->setChecked(false);
	m_ui.size32MB->setChecked(false);
	m_ui.size64MB->setChecked(false);
	m_ui.size128KB->setChecked(false);
	m_ui.sizeFolder->setChecked(false);
}

void MemoryCardCreateDialog::updateState()
{
	const bool okay = (m_ui.name->text().length() > 0);

	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(okay);
#ifdef _WIN32
	m_ui.ntfsCompression->setEnabled(m_type == MemoryCardType::File);
#endif
}

void MemoryCardCreateDialog::createCard()
{
	const QString name = m_ui.name->text();
	const std::string name_str = QStringLiteral("%1.%2").arg(name)
		.arg((m_fileType == MemoryCardFileType::PS1) ? QStringLiteral("mcr") : QStringLiteral("ps2"))
							   .toStdString();
	if (!Path::IsValidFileName(name_str, false))
	{
		QMessageBox::critical(this, tr("Create Memory Card"),
			tr("Failed to create the Memory Card, because the name '%1' contains one or more invalid characters.").arg(name));
		return;
	}

	if (FileMcd_GetCardInfo(name_str).has_value())
	{
		QMessageBox::critical(this, tr("Create Memory Card"),
			tr("Failed to create the Memory Card, because another card with the name '%1' already exists.").arg(name));
		return;
	}

	if (!FileMcd_CreateNewCard(name_str, m_type, m_fileType))
	{
		QMessageBox::critical(this, tr("Create Memory Card"),
			tr("Failed to create the Memory Card, the log may contain more information."));
		return;
	}

#ifdef  _WIN32
	if (m_ui.ntfsCompression->isChecked() && m_type == MemoryCardType::File)
	{
		const std::string fullPath(Path::Combine(EmuFolders::MemoryCards, name_str));
		FileSystem::SetPathCompression(fullPath.c_str(), true);
	}
#endif

	QMessageBox::information(this, tr("Create Memory Card"), tr("Memory Card '%1' created.").arg(name));
	accept();
}
