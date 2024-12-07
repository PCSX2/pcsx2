// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWidget.h"

#include "common/Assertions.h"

DebuggerWidget::DebuggerWidget(DebugInterface* cpu, QWidget* parent)
	: QWidget(parent)
	, m_cpu(cpu)
{
}

DebugInterface& DebuggerWidget::cpu() const
{
	pxAssertRel(m_cpu, "DebuggerWidget::cpu() called on object that doesn't have a CPU type set.");
	return *m_cpu;
}

void DebuggerWidget::applyMonospaceFont()
{
	// Easiest way to handle cross platform monospace fonts
	// There are issues related to TabWidget -> Children font inheritance otherwise
#if defined(WIN32)
	setStyleSheet(QStringLiteral("font: 10pt 'Lucida Console'"));
#elif defined(__APPLE__)
	setStyleSheet(QStringLiteral("font: 10pt 'Monaco'"));
#else
	setStyleSheet(QStringLiteral("font: 10pt 'Monospace'"));
#endif
}
