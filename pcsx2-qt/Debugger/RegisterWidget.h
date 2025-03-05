// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_RegisterWidget.h"

#include "DebuggerWidget.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

#include <QtWidgets/QMenu>
#include <QtWidgets/QTabBar>
#include <QtGui/QPainter>

class RegisterWidget final : public DebuggerWidget
{
	Q_OBJECT

public:
	RegisterWidget(const DebuggerWidgetParameters& parameters);
	~RegisterWidget();

	void toJson(JsonValueWrapper& json) override;
	bool fromJson(const JsonValueWrapper& json) override;

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

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

	std::optional<DebuggerEvents::GoToAddress> contextCreateGotoEvent();

	void tabCurrentChanged(int cur);

private:
	Ui::RegisterWidget ui;

	// Returns true on success
	bool contextFetchNewValue(u64& out, u64 currentValue, bool segment = false);

	// Used for the height offset the tab bar creates
	// because we share a widget
	QPoint m_renderStart;

	s32 m_rowStart = 0; // Index, 0 -> VF00, 1 -> VF01 etc
	s32 m_rowEnd; // Index, what register is the last one drawn
	s32 m_rowHeight; // The height of each register row
	// Used for mouse clicks
	s32 m_fieldStartX[4]; // Where the register segments start
	s32 m_fieldWidth; // How wide the register segments are

	s32 m_selectedRow = 0; // Index
	s32 m_selected128Field = 0; // Values are from 0 to 3

	bool m_showVU0FFloat = false;
	bool m_showFPRFloat = false;
};
