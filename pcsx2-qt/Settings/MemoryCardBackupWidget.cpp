// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryCardBackupWidget.h"

#include "MemoryCardBackupModel.h"

MemoryCardBackupWidget::MemoryCardBackupWidget(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	m_model = new MemoryCardBackupModel(m_ui.backupsTable);
	m_ui.backupsTable->setModel(m_model);
}
#include "moc_MemoryCardBackupWidget.cpp"
