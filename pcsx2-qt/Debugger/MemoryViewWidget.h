// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_RegisterWidget.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTabBar>
#include <QtGui/QPainter>

#include <vector>

enum class MemoryViewType
{
	BYTE = 1,
	BYTEHW = 2,
	WORD = 4,
	DWORD = 8,
};

class MemoryViewTable
{
	QWidget* parent;
	DebugInterface* m_cpu;
	MemoryViewType displayType = MemoryViewType::BYTE;
	u32 rowCount;
	u32 rowVisible;
	s32 rowHeight;

	// Stuff used for selection handling
	// This gets set every paint and depends on the window size / current display mode (1byte,2byte,etc)
	s32 valuexAxis;            // Where the hexadecimal view begins
	s32 textXAxis;             // Where the text view begins
	s32 row1YAxis;             // Where the first row starts
	s32 segmentXAxis[16];      // Where the segments begin
	bool selectedText = false; // Whether the user has clicked on text or hex
	
	bool selectedNibbleHI = false;

	void InsertIntoSelectedHexView(u8 value);

public:
	MemoryViewTable(QWidget* parent)
		: parent(parent){};
	u32 startAddress;
	u32 selectedAddress;

	void SetCpu(DebugInterface* cpu)
	{
		m_cpu = cpu;
	}
	void UpdateStartAddress(u32 start);
	void UpdateSelectedAddress(u32 selected, bool page = false);
	void DrawTable(QPainter& painter, const QPalette& palette, s32 height);
	void SelectAt(QPoint pos);
	u128 GetSelectedSegment();
	void InsertAtCurrentSelection(const QString& text);
	// Returns true if the keypress was handled
	bool KeyPress(int key, QChar keychar);

	MemoryViewType GetViewType()
	{
		return displayType;
	}

	void SetViewType(MemoryViewType viewType)
	{
		displayType = viewType;
	}
};


class MemoryViewWidget final : public QWidget
{
	Q_OBJECT

public:
	MemoryViewWidget(QWidget* parent);
	~MemoryViewWidget();

	void SetCpu(DebugInterface* cpu);

protected:
	void paintEvent(QPaintEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseDoubleClickEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent* event);
	void keyPressEvent(QKeyEvent* event);

public slots:
	void customMenuRequested(QPoint pos);

	void contextGoToAddress();
	void contextCopyByte();
	void contextCopySegment();
	void contextCopyCharacter();
	void contextPaste();
	void gotoAddress(u32 address);

signals:
	void gotoInDisasm(u32 address, bool should_set_focus = true);
	void addToSavedAddresses(u32 address);
	void VMUpdate();

private:
	Ui::RegisterWidget ui;

	QMenu* m_contextMenu = 0x0;
	QAction* m_actionBYTE;
	QAction* m_actionBYTEHW;
	QAction* m_actionWORD;
	QAction* m_actionDWORD;

	DebugInterface* m_cpu;
	MemoryViewTable m_table;
};
