// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_MemoryView.h"

#include "Debugger/DebuggerView.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTabBar>
#include <QtGui/QPainter>
#include <QtCore/QtEndian>

#include <vector>

enum class MemoryViewType
{
	BYTE = 0,
	BYTEHW = 1,
	WORD = 2,
	DWORD = 3,
	FLOAT = 4,
};

const s32 MemoryViewTypeWidth[] = {
	1, //	BYTE
	2, //	BYTEHW
	4, //	WORD
	8, //	DWORD
	4, //	FLOAT
};

const s32 MemoryViewTypeVisualWidth[] = {
	2, //	BYTE
	4, //	BYTEHW
	8, //	WORD
	16, //	DWORD
	14, //	FLOAT
};

class MemoryViewTable : public QObject
{
	Q_OBJECT
private:
	QWidget* parent;
	MemoryViewType displayType = MemoryViewType::BYTE;
	bool littleEndian = true;
	u32 rowCount;
	u32 rowVisible;
	s32 rowHeight;

	// Stuff used for selection handling
	// This gets set every paint and depends on the window size / current display mode (1byte,2byte,etc)
	s32 valuexAxis; // Where the hexadecimal view begins
	s32 textXAxis; // Where the text view begins
	s32 row1YAxis; // Where the first row starts
	s32 segmentXAxis[16]; // Where the segments begin
	bool selectedText = false; // Whether the user has clicked on text or hex

	bool selectedNibbleHI = false;

	void InsertIntoSelectedHexView(u8 value, DebugInterface& cpu);
	bool InsertFloatIntoSelectedHexView(DebugInterface& cpu);

	template <class T>
	T convertEndian(T in)
	{
		if (littleEndian)
		{
			return in;
		}
		else
		{
			return qToBigEndian(in);
		}
	}

	static u32 nextAddress(u32 addr, u32 selected_address, MemoryViewType display_type, bool little_endian);
	static u32 prevAddress(u32 addr, u32 selected_address, MemoryViewType display_type, bool little_endian);

public:
	MemoryViewTable(QWidget* parent)
		: parent(parent)
	{
	}

	u32 startAddress;
	u32 selectedAddress;
	s32 selectedIndex;

	void UpdateStartAddress(u32 start);
	void UpdateSelectedAddress(u32 selected, bool page = false);
	void DrawTable(QPainter& painter, const QPalette& palette, s32 height, DebugInterface& cpu);
	void SelectAt(QPoint pos);
	u128 GetSelectedSegment(DebugInterface& cpu);
	void InsertAtCurrentSelection(const QString& text, DebugInterface& cpu);
	void ForwardSelection();
	void BackwardSelection();
	// Returns true if the keypress was handled
	bool KeyPress(int key, QChar keychar, DebugInterface& cpu);

	MemoryViewType GetViewType()
	{
		return displayType;
	}

	void SetViewType(MemoryViewType viewType)
	{
		displayType = viewType;
	}

	bool GetLittleEndian()
	{
		return littleEndian;
	}

	void SetLittleEndian(bool le)
	{
		littleEndian = le;
	}
};

class MemoryView final : public DebuggerView
{
	Q_OBJECT

public:
	MemoryView(const DebuggerViewParameters& parameters);
	~MemoryView();

	void toJson(JsonValueWrapper& json) override;
	bool fromJson(const JsonValueWrapper& json) override;

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

public slots:
	void openContextMenu(QPoint pos);

	void contextGoToAddress();
	void contextFollowAddress();
	void contextCopyByte();
	void contextCopySegment();
	void contextCopyCharacter();
	void contextPaste();
	void gotoAddress(u32 address);

private:
	Ui::MemoryView ui;

	MemoryViewTable m_table;
};
