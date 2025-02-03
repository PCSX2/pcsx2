// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_LayoutEditorDialog.h"

#include "Debugger/Docking/DockManager.h"

#include "DebugTools/DebugInterface.h"

#include <QDialog>

class LayoutEditorDialog : public QDialog
{
	Q_OBJECT

public:
	// Create a "New Layout" dialog.
	LayoutEditorDialog(QWidget* parent = nullptr);

	// Create a "Edit Layout" dialog.
	LayoutEditorDialog(std::string& name, BreakPointCpu cpu, QWidget* parent = nullptr);

	std::string name();
	BreakPointCpu cpu();
	DockManager::LayoutCreationMode initial_state();

private:
	void setupComboBoxes(BreakPointCpu cpu, DockManager::LayoutCreationMode initial_state);

	Ui::LayoutEditorDialog m_ui;
};
