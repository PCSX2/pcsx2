// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_BreakpointWidget.h"

#include "BreakpointModel.h"

#include "Debugger/DebuggerWidget.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

#include <QtWidgets/QMenu>
#include <QtWidgets/QTabBar>
#include <QtGui/QPainter>

class BreakpointWidget : public DebuggerWidget
{
	Q_OBJECT

public:
	BreakpointWidget(const DebuggerWidgetParameters& parameters);

	void onDoubleClicked(const QModelIndex& index);
	void openContextMenu(QPoint pos);

	void contextCopy();
	void contextDelete();
	void contextNew();
	void contextEdit();
	void contextPasteCSV();

	void saveBreakpointsToDebuggerSettings();

private:
	Ui::BreakpointWidget m_ui;

	BreakpointModel* m_model;
};
