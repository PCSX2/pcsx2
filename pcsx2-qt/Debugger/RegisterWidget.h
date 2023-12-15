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

class RegisterWidget final : public QWidget
{
	Q_OBJECT

public:
	RegisterWidget(QWidget* parent);
	~RegisterWidget();

	void SetCpu(DebugInterface* cpu);

protected:
	void paintEvent(QPaintEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent* event);

public slots:
	void customMenuRequested(QPoint pos);
	void contextCopyValue();
	void contextCopyTop();
	void contextCopyBottom();
	void contextCopySegment();
	void contextChangeValue();
	void contextChangeTop();
	void contextChangeBottom();
	void contextChangeSegment();

	void contextGotoDisasm();
	void contextGotoMemory();

	void tabCurrentChanged(int cur);

signals:
	void gotoInDisasm(u32 address);
	void gotoInMemory(u32 address);
	void VMUpdate();

private:
	Ui::RegisterWidget ui;

	QMenu* m_contextMenu = 0x0;

	// Returns true on success
	bool contextFetchNewValue(u64& out, u64 currentValue, bool segment = false);

	DebugInterface* m_cpu;

	// Used for the height offset the tab bar creates
	// because we share a widget
	QPoint m_renderStart;

	s32 m_rowStart = 0;         // Index, 0 -> VF00, 1 -> VF01 etc
	s32 m_rowEnd;               // Index, what register is the last one drawn
	s32 m_rowHeight;            // The height of each register row
	// Used for mouse clicks
	s32 m_fieldStartX[4];       // Where the register segments start
	s32 m_fieldWidth;           // How wide the register segments are

	s32 m_selectedRow = 0;      // Index
	s32 m_selected128Field = 0; // Values are from 0 to 3

	// TODO: Save this configuration ??
	bool m_showVU0FFloat = false;
	bool m_showFPRFloat = false;
};
