// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_StackView.h"

#include "StackModel.h"

#include "DebuggerView.h"

class StackView final : public DebuggerView
{
	Q_OBJECT

public:
	StackView(const DebuggerViewParameters& parameters);

	void openContextMenu(QPoint pos);
	void onDoubleClick(const QModelIndex& index);

private:
	Ui::StackView m_ui;

	StackModel* m_model;
};
