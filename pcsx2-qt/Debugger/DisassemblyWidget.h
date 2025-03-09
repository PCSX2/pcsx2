// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DisassemblyWidget.h"

#include "DebuggerWidget.h"

#include "pcsx2/DebugTools/DisassemblyManager.h"

#include <QtWidgets/QMenu>
#include <QtGui/QPainter>

class DisassemblyWidget final : public DebuggerWidget
{
	Q_OBJECT

public:
	DisassemblyWidget(const DebuggerWidgetParameters& parameters);
	~DisassemblyWidget();

	void toJson(JsonValueWrapper& json) override;
	bool fromJson(const JsonValueWrapper& json) override;

	// Required for the breakpoint list (ugh wtf)
	QString GetLineDisasm(u32 address);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

public slots:
	void openContextMenu(QPoint pos);

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
	void contextShowInstructionBytes();

	void gotoAddressAndSetFocus(u32 address);
	void gotoProgramCounterOnPause();
	void gotoAddress(u32 address, bool should_set_focus);

	void toggleBreakpoint(u32 address);

private:
	Ui::DisassemblyWidget m_ui;

	u32 m_visibleStart = 0x100000; // The address of the first instruction shown.
	u32 m_visibleRows;
	u32 m_selectedAddressStart = 0;
	u32 m_selectedAddressEnd = 0;
	u32 m_rowHeight = 0;

	std::map<u32, u32> m_nopedInstructions;
	std::map<u32, std::tuple<u32, u32>> m_stubbedFunctions;

	bool m_showInstructionBytes = true;
	bool m_goToProgramCounterOnPause = true;
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
