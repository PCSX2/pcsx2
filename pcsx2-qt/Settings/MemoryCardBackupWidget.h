// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_MemoryCardBackupWidget.h"

class MemoryCardBackupModel;

class MemoryCardBackupWidget : public QWidget
{
	Q_OBJECT

public:
	MemoryCardBackupWidget(QWidget* parent = nullptr);

private:
	Ui::MemoryCardBackupWidget m_ui;

	MemoryCardBackupModel* m_model;
};
