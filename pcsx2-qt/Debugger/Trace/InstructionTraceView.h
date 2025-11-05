// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_InstructionTraceView.h"

#include "Debugger/DebuggerView.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>

// Instruction trace entry structure
struct TraceEntry
{
	u64 timestamp;      // Cycle timestamp
	u8 cpu;            // 0 = EE, 1 = IOP
	u32 pc;            // Program counter
	QString disasm;    // Disassembled instruction
	u32 cycles;        // Cycles for this instruction
	QString memAccess; // Memory access info (if any)

	TraceEntry() : timestamp(0), cpu(0), pc(0), cycles(0) {}
	TraceEntry(u64 ts, u8 c, u32 p, const QString& d, u32 cy, const QString& m)
		: timestamp(ts), cpu(c), pc(p), disasm(d), cycles(cy), memAccess(m) {}
};

class InstructionTraceView final : public DebuggerView
{
	Q_OBJECT

public:
	InstructionTraceView(const DebuggerViewParameters& parameters);
	~InstructionTraceView() = default;

public slots:
	void onStartStopClicked();
	void onClearClicked();
	void onDumpToFileClicked();
	void onCpuFilterChanged(int index);
	void onBufferSizeChanged();
	void onAddressFilterChanged();
	void onSymbolFilterChanged();
	void onOpcodeFilterChanged();
	void onPollTraceData();
	void onTableContextMenu(QPoint pos);

private:
	Ui::InstructionTraceView m_ui;
	QTimer m_pollTimer;
	std::vector<TraceEntry> m_traceBuffer;
	bool m_isTracing = false;
	u32 m_maxBufferSize = 10000;

	void updateTable();
	void startTracing();
	void stopTracing();
	void clearBuffer();
	QString cpuName(u8 cpu) const;

	// Filter settings
	int m_cpuFilter = -1; // -1 = all, 0 = EE, 1 = IOP
	u32 m_addressRangeStart = 0;
	u32 m_addressRangeEnd = 0xFFFFFFFF;
	QString m_symbolFilter;
	QString m_opcodeFilter;
};
