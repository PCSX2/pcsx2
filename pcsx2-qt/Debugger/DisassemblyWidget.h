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

#include "ui_DisassemblyWidget.h"

#include "pcsx2/DebugTools/DebugInterface.h"
#include "pcsx2/DebugTools/DisassemblyManager.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtGui/QPainter>

class DisassemblyWidget final : public QWidget
{
	Q_OBJECT

public:
	DisassemblyWidget(QWidget* parent);
	~DisassemblyWidget();

	// Required because our constructor needs to take no extra arguments.
	void SetCpu(DebugInterface* cpu);

	// Required for the breakpoint list (ugh wtf)
	QString GetLineDisasm(u32 address);

protected:
	void paintEvent(QPaintEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseDoubleClickEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent* event);
	void keyPressEvent(QKeyEvent* event);

public slots:
	void customMenuRequested(QPoint pos);

	// Context menu actions
	// When called, m_selectedAddressStart will be the 'selected' instruction
	// Of course, m_selectedAddressEnd will be the end of the selection when required
	void contextCopyAddress();
	void contextCopyInstructionHex();
	void contextCopyInstructionText();
	void contextAssembleInstruction();
	void contextRunToCursor();
	void contextJumpToCursor();
	void contextToggleBreakpoint();
	void contextFollowBranch();
	void contextGoToAddress();
	void contextAddFunction();
	void contextRenameFunction();
	void contextRemoveFunction();
	void gotoAddress(u32 address);

signals:
	void gotoInMemory(u32 address);
	void breakpointsChanged();
	void VMUpdate();

private:
	Ui::DisassemblyWidget ui;

	QMenu* m_contextMenu = 0x0;
	void CreateCustomContextMenu();

	DebugInterface* m_cpu;
	u32 m_visibleStart = 0x00336318; // The address of the first opcode shown(row 0)
	u32 m_visibleRows;
	u32 m_selectedAddressStart = 0;
	u32 m_selectedAddressEnd = 0;
	u32 m_rowHeight = 0;

	DisassemblyManager m_disassemblyManager;

	inline QString DisassemblyStringFromAddress(u32 address, QFont font, u32 pc);
	QColor GetAddressFunctionColor(u32 address);
	enum class SelectionInfo
	{
		ADDRESS,
		INSTRUCTIONHEX,
		INSTRUCTIONTEXT,
	};
	QString FetchSelectionInfo(SelectionInfo selInfo);
};
