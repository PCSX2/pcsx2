// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QDialog>

#include "ui_MemoryCardCreateDialog.h"

#include "pcsx2/Config.h"

class MemoryCardCreateDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit MemoryCardCreateDialog(QWidget* parent = nullptr);
	~MemoryCardCreateDialog();

private Q_SLOTS:
	void nameTextChanged();
	void createCard();

private:
	void setType(MemoryCardType type, MemoryCardFileType fileType);
	void restoreDefaults();
	void updateState();

	Ui::MemoryCardCreateDialog m_ui;

	MemoryCardType m_type = MemoryCardType::File;
	MemoryCardFileType m_fileType = MemoryCardFileType::PS2_8MB;
};