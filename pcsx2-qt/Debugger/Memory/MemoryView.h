// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
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
	BYTE = 1,
	BYTEHW = 2,
	WORD = 4,
	DWORD = 8,
};

class MemoryViewTable
{
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

	u32 nextAddress(u32 addr);
	u32 prevAddress(u32 addr);

public:
	MemoryViewTable(QWidget* parent)
		: parent(parent)
	{
	}

	u32 startAddress;
	u32 selectedAddress;

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
	void contextCopyByte();
	void contextCopySegment();
	void contextCopyCharacter();
	void contextPaste();
	void gotoAddress(u32 address);

private:
	Ui::MemoryView ui;

	MemoryViewTable m_table;
};
