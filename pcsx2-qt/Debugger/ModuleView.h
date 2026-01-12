// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_ModuleView.h"

#include "DebuggerView.h"
#include "ModuleModel.h"

#include <QtCore/QSortFilterProxyModel>

class ModuleView final : public DebuggerView
{
	Q_OBJECT

public:
	ModuleView(const DebuggerViewParameters& parameters);

	void openContextMenu(QPoint pos);
	void onDoubleClick(const QModelIndex& index);

private:
	Ui::ModuleView m_ui;

	ModuleModel* m_model;
};
