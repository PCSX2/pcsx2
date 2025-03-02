// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "NoLayoutsWidget.h"

NoLayoutsWidget::NoLayoutsWidget(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
}

QPushButton* NoLayoutsWidget::createDefaultLayoutsButton()
{
	return m_ui.createDefaultLayoutsButton;
}
