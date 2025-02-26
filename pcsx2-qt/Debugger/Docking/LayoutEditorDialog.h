// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_LayoutEditorDialog.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QDialog>

class LayoutEditorDialog : public QDialog
{
	Q_OBJECT

public:
	using NameValidator = std::function<bool(const QString&)>;

	enum CreationMode
	{
		DEFAULT_LAYOUT,
		BLANK_LAYOUT,
		CLONE_LAYOUT,
	};

	// Bundles together a creation mode and inital state.
	using InitialState = std::pair<CreationMode, size_t>;

	// Create a "New Layout" dialog.
	LayoutEditorDialog(NameValidator name_validator, bool can_clone_current_layout, QWidget* parent = nullptr);

	// Create a "Edit Layout" dialog.
	LayoutEditorDialog(const QString& name, BreakPointCpu cpu, NameValidator name_validator, QWidget* parent = nullptr);

	QString name();
	BreakPointCpu cpu();
	InitialState initialState();

private:
	void setupInputWidgets(BreakPointCpu cpu, bool can_clone_current_layout);
	void onNameChanged();

	Ui::LayoutEditorDialog m_ui;
	NameValidator m_name_validator;
};
