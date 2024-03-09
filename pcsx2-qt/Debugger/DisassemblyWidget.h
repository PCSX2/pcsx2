// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	void contextCopyFunctionName();
	void contextAssembleInstruction();
	void contextNoopInstruction();
	void contextRestoreInstruction();
	void contextRunToCursor();
	void contextJumpToCursor();
	void contextToggleBreakpoint();
	void contextFollowBranch();
	void contextGoToAddress();
	void contextAddFunction();
	void contextRenameFunction();
	void contextRemoveFunction();
	void contextStubFunction();
	void contextRestoreFunction();
	void contextShowOpcode();

	void gotoAddress(u32 address, bool should_set_focus = true);

	void setDemangle(bool demangle) { m_demangleFunctions = demangle; };
signals:
	void gotoInMemory(u32 address);
	void breakpointsChanged();
	void VMUpdate();

private:
	Ui::DisassemblyWidget ui;

	DebugInterface* m_cpu;
	u32 m_visibleStart = 0x00336318; // The address of the first opcode shown(row 0)
	u32 m_visibleRows;
	u32 m_selectedAddressStart = 0;
	u32 m_selectedAddressEnd = 0;
	u32 m_rowHeight = 0;

	std::map<u32, u32> m_nopedInstructions;
	std::map<u32, std::tuple<u32, u32>> m_stubbedFunctions;

	bool m_demangleFunctions = true;
	bool m_showInstructionOpcode = true;
	DisassemblyManager m_disassemblyManager;

	inline QString DisassemblyStringFromAddress(u32 address, QFont font, u32 pc, bool selected);
	QColor GetAddressFunctionColor(u32 address);
	enum class SelectionInfo
	{
		ADDRESS,
		INSTRUCTIONHEX,
		INSTRUCTIONTEXT,
	};
	QString FetchSelectionInfo(SelectionInfo selInfo);

	bool AddressCanRestore(u32 start, u32 end);
	bool FunctionCanRestore(u32 address);
};
