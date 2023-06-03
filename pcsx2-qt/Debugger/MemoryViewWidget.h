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
	void KeyPress(int key, QChar keychar);

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
	void gotoInDisasm(u32 address);
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
