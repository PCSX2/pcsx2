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
	void mouseDoubleClickEvent(QMouseEvent* event);
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
	void gotoInDisasm(u32 address, bool should_set_focus = true);
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
