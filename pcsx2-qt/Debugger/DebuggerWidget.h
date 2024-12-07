// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>

inline void not_yet_implemented()
{
	abort();
}

class DebuggerWidget : public QWidget
{
	Q_OBJECT

protected:
	DebuggerWidget(DebugInterface* cpu, QWidget* parent = nullptr);

	DebugInterface& cpu() const;

	void applyMonospaceFont();

private:
	DebugInterface* m_cpu;
};
