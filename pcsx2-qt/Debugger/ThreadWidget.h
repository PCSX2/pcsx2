// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_ThreadWidget.h"

#include "DebuggerWidget.h"
#include "ThreadModel.h"

#include <QtCore/QSortFilterProxyModel>

class ThreadWidget final : public DebuggerWidget
{
	Q_OBJECT

public:
	ThreadWidget(const DebuggerWidgetParameters& parameters);

	void openContextMenu(QPoint pos);
	void onDoubleClick(const QModelIndex& index);

private:
	Ui::ThreadWidget m_ui;

	ThreadModel* m_model;
	QSortFilterProxyModel* m_proxy_model;
};
