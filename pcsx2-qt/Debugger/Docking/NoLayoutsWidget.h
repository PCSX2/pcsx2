// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_NoLayoutsWidget.h"

#include <QtWidgets/QPushButton>

class NoLayoutsWidget : public QWidget
{
	Q_OBJECT

public:
	NoLayoutsWidget(QWidget* parent = nullptr);

	QPushButton* createDefaultLayoutsButton();

private:
	Ui::NoLayoutsWidget m_ui;
};
