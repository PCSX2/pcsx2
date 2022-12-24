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

#include <QtWidgets/QDialog>

class BreakpointDialog final : public QDialog
{
	Q_OBJECT

public:
	BreakpointDialog(QWidget* parent, DebugInterface* cpu);
	BreakpointDialog(QWidget* parent, DebugInterface* cpu, BreakPoint* bp);
	BreakpointDialog(QWidget* parent, DebugInterface* cpu, MemCheck* mc);
	~BreakpointDialog();


public slots:
	void onRdoButtonToggled();
	void accept() override;

private:
	enum class BPDIALOG_PURPOSE
	{
		CREATE,
		EDIT_BP,
		EDIT_MC
	};

	Ui::BreakpointDialog m_ui;
	DebugInterface* m_cpu;

	const BPDIALOG_PURPOSE m_purpose;
	BreakPoint* m_bp = nullptr;
	MemCheck* m_mc = nullptr;
};