// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_ThreadView.h"

#include "DebuggerView.h"
#include "ThreadModel.h"

#include <QtCore/QSortFilterProxyModel>

class ThreadView final : public DebuggerView
{
	Q_OBJECT

public:
	ThreadView(const DebuggerViewParameters& parameters);

	void openContextMenu(QPoint pos);
	void onDoubleClick(const QModelIndex& index);

private:
	Ui::ThreadView m_ui;

	ThreadModel* m_model;
	QSortFilterProxyModel* m_proxy_model;
};
