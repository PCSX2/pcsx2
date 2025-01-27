// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>

inline void not_yet_implemented()
{
	abort();
}

class JsonValueWrapper;

// The base class for the contents of the dock widgets in the debugger.
class DebuggerWidget : public QWidget
{
	Q_OBJECT

public:
	// Get the effective debug interface associated with this particular widget
	// if it's set, otherwise return the one associated with the layout that
	// contains this widget.
	DebugInterface& cpu() const;

	// Set the debug interface associated with the layout. If false is returned,
	// we have to recreate the object.
	bool setCpu(DebugInterface& new_cpu);

	// Get the CPU associated with this particular widget.
	std::optional<BreakPointCpu> cpuOverride() const;

	// Set the CPU associated with the individual dock widget. If false is
	// returned, we have to recreate the object.
	bool setCpuOverride(std::optional<BreakPointCpu> new_cpu);

	virtual void toJson(JsonValueWrapper& json);
	virtual bool fromJson(JsonValueWrapper& json);

	void applyMonospaceFont();

protected:
	DebuggerWidget(DebugInterface* cpu, QWidget* parent = nullptr);

private:
	DebugInterface* m_cpu;
	std::optional<BreakPointCpu> m_cpu_override;
};
