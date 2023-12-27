// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_BreakpointDialog.h"

#include "DebugTools/Breakpoints.h"

#include "Models/BreakpointModel.h"

#include <QtWidgets/QDialog>

class BreakpointDialog final : public QDialog
{
	Q_OBJECT

public:
	BreakpointDialog(QWidget* parent, DebugInterface* cpu, BreakpointModel& model);
	BreakpointDialog(QWidget* parent, DebugInterface* cpu, BreakpointModel& model, BreakpointMemcheck bpmc, int rowIndex);
	~BreakpointDialog();

public slots:
	void onRdoButtonToggled();
	void accept() override;

private:
	enum class PURPOSE
	{
		CREATE,
		EDIT
	};

	Ui::BreakpointDialog m_ui;
	DebugInterface* m_cpu;

	const PURPOSE m_purpose;
	BreakpointModel& m_bpModel;
	BreakpointMemcheck m_bp_mc;
	int m_rowIndex;
};
