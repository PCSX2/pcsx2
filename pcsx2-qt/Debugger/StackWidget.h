// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_StackWidget.h"

#include "StackModel.h"

#include "DebuggerWidget.h"

class StackWidget final : public DebuggerWidget
{
	Q_OBJECT

public:
	StackWidget(const DebuggerWidgetParameters& parameters);

	void openContextMenu(QPoint pos);
	void onDoubleClick(const QModelIndex& index);

private:
	Ui::StackWidget m_ui;

	StackModel* m_model;
};
