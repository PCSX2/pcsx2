// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_BreakpointView.h"

#include "BreakpointModel.h"

#include "Debugger/DebuggerView.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

#include <QtWidgets/QMenu>
#include <QtWidgets/QTabBar>
#include <QtGui/QPainter>

class BreakpointView : public DebuggerView
{
	Q_OBJECT

public:
	BreakpointView(const DebuggerViewParameters& parameters);

	void onDoubleClicked(const QModelIndex& index);
	void openContextMenu(QPoint pos);

	void contextCopy();
	void contextDelete();
	void contextNew();
	void contextEdit();
	void contextPasteCSV();

	void resizeColumns();

	void saveBreakpointsToDebuggerSettings();

private:
	Ui::BreakpointView m_ui;

	BreakpointModel* m_model;
};
