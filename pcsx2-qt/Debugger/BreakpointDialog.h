/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
