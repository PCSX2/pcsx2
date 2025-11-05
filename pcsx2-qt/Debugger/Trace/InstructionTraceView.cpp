// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "InstructionTraceView.h"

#include "DebugTools/DebugInterface.h"

#include "QtUtils.h"
#include "QtHost.h"
#include "Host.h"

#include "common/Console.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHeaderView>

using namespace QtUtils;

// NOTE: InstructionTracer API not yet implemented. This is a stub implementation.
// When InstructionTracer is available, it should provide:
// - InstructionTracer::Enable(cpu, buffer_size) -> enable tracing
// - InstructionTracer::Disable(cpu) -> disable tracing
// - InstructionTracer::Drain(cpu) -> std::vector<TraceEntry> -> get buffered entries
// - InstructionTracer::DumpToFile(filename) -> dump trace to file
// - InstructionTracer::Clear(cpu) -> clear trace buffer

InstructionTraceView::InstructionTraceView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
{
	m_ui.setupUi(this);
	this->repaint();

	// Set up table
	m_ui.tableTrace->setContextMenuPolicy(Qt::CustomContextMenu);
	m_ui.tableTrace->horizontalHeader()->setStretchLastSection(true);
	m_ui.tableTrace->setSelectionMode(QAbstractItemView::SingleSelection);
	m_ui.tableTrace->setSelectionBehavior(QAbstractItemView::SelectRows);

	// Connect controls
	connect(m_ui.btnStartStop, &QPushButton::clicked, this, &InstructionTraceView::onStartStopClicked);
	connect(m_ui.btnClear, &QPushButton::clicked, this, &InstructionTraceView::onClearClicked);
	connect(m_ui.btnDumpToFile, &QPushButton::clicked, this, &InstructionTraceView::onDumpToFileClicked);
	connect(m_ui.cmbCpuFilter, &QComboBox::currentIndexChanged, this, &InstructionTraceView::onCpuFilterChanged);
	connect(m_ui.spinBufferSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &InstructionTraceView::onBufferSizeChanged);
	connect(m_ui.txtAddressStart, &QLineEdit::textChanged, this, &InstructionTraceView::onAddressFilterChanged);
	connect(m_ui.txtAddressEnd, &QLineEdit::textChanged, this, &InstructionTraceView::onAddressFilterChanged);
	connect(m_ui.txtSymbolFilter, &QLineEdit::textChanged, this, &InstructionTraceView::onSymbolFilterChanged);
	connect(m_ui.txtOpcodeFilter, &QLineEdit::textChanged, this, &InstructionTraceView::onOpcodeFilterChanged);
	connect(m_ui.tableTrace, &QTableWidget::customContextMenuRequested, this, &InstructionTraceView::onTableContextMenu);

	// Set up polling timer for trace data
	m_pollTimer.setInterval(100); // Poll every 100ms
	m_pollTimer.setSingleShot(false);
	connect(&m_pollTimer, &QTimer::timeout, this, &InstructionTraceView::onPollTraceData);

	// Handle debugger refresh events
	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		update();
		return true;
	});
}

void InstructionTraceView::onStartStopClicked()
{
	if (m_isTracing)
	{
		stopTracing();
	}
	else
	{
		startTracing();
	}
}

void InstructionTraceView::startTracing()
{
	m_isTracing = true;
	m_ui.btnStartStop->setText(tr("Stop Tracing"));
	m_ui.statusLabel->setText(tr("Status: Tracing..."));

	// Get selected CPU
	const int cpuIndex = m_ui.cmbCpu->currentIndex();

	// Start tracing on CPU thread
	Host::RunOnCPUThread([this, cpuIndex]() {
		// TODO: Call InstructionTracer::Enable(cpuIndex, m_maxBufferSize)
		Console.WriteLn("InstructionTracer: Would enable tracing for CPU %d with buffer size %d", cpuIndex, m_maxBufferSize);
	});

	// Start polling timer
	m_pollTimer.start();
}

void InstructionTraceView::stopTracing()
{
	m_isTracing = false;
	m_pollTimer.stop();
	m_ui.btnStartStop->setText(tr("Start Tracing"));
	m_ui.statusLabel->setText(tr("Status: Stopped"));

	// Get selected CPU
	const int cpuIndex = m_ui.cmbCpu->currentIndex();

	// Stop tracing on CPU thread
	Host::RunOnCPUThread([cpuIndex]() {
		// TODO: Call InstructionTracer::Disable(cpuIndex)
		Console.WriteLn("InstructionTracer: Would disable tracing for CPU %d", cpuIndex);
	});
}

void InstructionTraceView::onClearClicked()
{
	clearBuffer();
}

void InstructionTraceView::clearBuffer()
{
	m_traceBuffer.clear();
	updateTable();
	m_ui.statusLabel->setText(tr("Status: Buffer cleared (%1 entries)").arg(0));

	// Get selected CPU
	const int cpuIndex = m_ui.cmbCpu->currentIndex();

	// Clear on CPU thread
	Host::RunOnCPUThread([cpuIndex]() {
		// TODO: Call InstructionTracer::Clear(cpuIndex)
		Console.WriteLn("InstructionTracer: Would clear buffer for CPU %d", cpuIndex);
	});
}

void InstructionTraceView::onDumpToFileClicked()
{
	QString filename = QFileDialog::getSaveFileName(
		this,
		tr("Save Trace to File"),
		QString(),
		tr("Trace Files (*.trace);;Text Files (*.txt);;All Files (*)"));

	if (filename.isEmpty())
		return;

	// Dump to file
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to open file for writing: %1").arg(filename));
		return;
	}

	QTextStream out(&file);
	out << "# PCSX2 Instruction Trace\n";
	out << "# Timestamp\tCPU\tPC\tDisassembly\tCycles\tMemory Access\n";

	for (const auto& entry : m_traceBuffer)
	{
		out << entry.timestamp << "\t"
			<< cpuName(entry.cpu) << "\t"
			<< QString("0x%1").arg(entry.pc, 8, 16, QChar('0')) << "\t"
			<< entry.disasm << "\t"
			<< entry.cycles << "\t"
			<< entry.memAccess << "\n";
	}

	file.close();
	QMessageBox::information(this, tr("Success"), tr("Trace dumped to file: %1\n%2 entries written.").arg(filename).arg(m_traceBuffer.size()));
}

void InstructionTraceView::onCpuFilterChanged(int index)
{
	m_cpuFilter = index - 1; // 0 = All, 1 = EE, 2 = IOP -> -1 = All, 0 = EE, 1 = IOP
	updateTable();
}

void InstructionTraceView::onBufferSizeChanged()
{
	m_maxBufferSize = m_ui.spinBufferSize->value();
}

void InstructionTraceView::onAddressFilterChanged()
{
	bool ok;
	QString startText = m_ui.txtAddressStart->text().trimmed();
	QString endText = m_ui.txtAddressEnd->text().trimmed();

	if (!startText.isEmpty())
	{
		m_addressRangeStart = startText.toUInt(&ok, 16);
		if (!ok)
			m_addressRangeStart = 0;
	}
	else
	{
		m_addressRangeStart = 0;
	}

	if (!endText.isEmpty())
	{
		m_addressRangeEnd = endText.toUInt(&ok, 16);
		if (!ok)
			m_addressRangeEnd = 0xFFFFFFFF;
	}
	else
	{
		m_addressRangeEnd = 0xFFFFFFFF;
	}

	updateTable();
}

void InstructionTraceView::onSymbolFilterChanged()
{
	m_symbolFilter = m_ui.txtSymbolFilter->text().trimmed();
	updateTable();
}

void InstructionTraceView::onOpcodeFilterChanged()
{
	m_opcodeFilter = m_ui.txtOpcodeFilter->text().trimmed();
	updateTable();
}

void InstructionTraceView::onPollTraceData()
{
	if (!m_isTracing)
		return;

	// Get selected CPU
	const int cpuIndex = m_ui.cmbCpu->currentIndex();

	// Poll for new trace data on CPU thread, then update UI
	Host::RunOnCPUThread([this, cpuIndex]() {
		// TODO: Get trace entries from InstructionTracer::Drain(cpuIndex)
		// For now, generate stub data for testing
		std::vector<TraceEntry> newEntries;

		// Stub: Generate some fake trace entries
		static u64 stubTimestamp = 0;
		if (m_traceBuffer.size() < 100) // Limit stub data
		{
			for (int i = 0; i < 5; i++)
			{
				TraceEntry entry;
				entry.timestamp = stubTimestamp++;
				entry.cpu = cpuIndex;
				entry.pc = 0x00100000 + (m_traceBuffer.size() * 4);
				entry.disasm = QString("nop ; stub instruction %1").arg(m_traceBuffer.size());
				entry.cycles = 1;
				entry.memAccess = "";
				newEntries.push_back(entry);
			}
		}

		// Update UI on UI thread
		if (!newEntries.empty())
		{
			QtHost::RunOnUIThread([this, newEntries = std::move(newEntries)]() {
				// Add to buffer
				for (const auto& entry : newEntries)
				{
					m_traceBuffer.push_back(entry);
					if (m_traceBuffer.size() > m_maxBufferSize)
					{
						// Remove oldest entries if buffer exceeds max size
						m_traceBuffer.erase(m_traceBuffer.begin(), m_traceBuffer.begin() + (m_traceBuffer.size() - m_maxBufferSize));
					}
				}
				updateTable();
				m_ui.statusLabel->setText(tr("Status: Tracing... (%1 entries)").arg(m_traceBuffer.size()));
			});
		}
	});
}

void InstructionTraceView::onTableContextMenu(QPoint pos)
{
	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	if (m_ui.tableTrace->currentRow() >= 0)
	{
		connect(menu->addAction(tr("Copy Address")), &QAction::triggered, this, [this]() {
			int row = m_ui.tableTrace->currentRow();
			if (row >= 0 && static_cast<size_t>(row) < m_traceBuffer.size())
			{
				QApplication::clipboard()->setText(FilledQStringFromValue(m_traceBuffer[row].pc, 16));
			}
		});

		connect(menu->addAction(tr("Copy Disassembly")), &QAction::triggered, this, [this]() {
			int row = m_ui.tableTrace->currentRow();
			if (row >= 0 && static_cast<size_t>(row) < m_traceBuffer.size())
			{
				QApplication::clipboard()->setText(m_traceBuffer[row].disasm);
			}
		});

		createEventActions<DebuggerEvents::GoToAddress>(menu, [this]() {
			int row = m_ui.tableTrace->currentRow();
			if (row >= 0 && static_cast<size_t>(row) < m_traceBuffer.size())
			{
				DebuggerEvents::GoToAddress event;
				event.address = m_traceBuffer[row].pc;
				return std::optional(event);
			}
			return std::optional<DebuggerEvents::GoToAddress>();
		});
	}

	menu->popup(m_ui.tableTrace->viewport()->mapToGlobal(pos));
}

void InstructionTraceView::updateTable()
{
	// Preserve scroll position
	int scrollPos = m_ui.tableTrace->verticalScrollBar()->value();
	bool atBottom = (scrollPos == m_ui.tableTrace->verticalScrollBar()->maximum());

	m_ui.tableTrace->setRowCount(0);

	// Apply filters and populate table
	for (const auto& entry : m_traceBuffer)
	{
		// CPU filter
		if (m_cpuFilter >= 0 && entry.cpu != static_cast<u8>(m_cpuFilter))
			continue;

		// Address range filter
		if (entry.pc < m_addressRangeStart || entry.pc > m_addressRangeEnd)
			continue;

		// Symbol filter (would need symbol lookup - stub for now)
		if (!m_symbolFilter.isEmpty())
		{
			// TODO: Look up symbol for PC and filter by name
		}

		// Opcode filter
		if (!m_opcodeFilter.isEmpty())
		{
			if (!entry.disasm.contains(m_opcodeFilter, Qt::CaseInsensitive))
				continue;
		}

		// Add row
		int row = m_ui.tableTrace->rowCount();
		m_ui.tableTrace->insertRow(row);

		m_ui.tableTrace->setItem(row, 0, new QTableWidgetItem(QString::number(entry.timestamp)));
		m_ui.tableTrace->setItem(row, 1, new QTableWidgetItem(cpuName(entry.cpu)));
		m_ui.tableTrace->setItem(row, 2, new QTableWidgetItem(FilledQStringFromValue(entry.pc, 16)));
		m_ui.tableTrace->setItem(row, 3, new QTableWidgetItem(entry.disasm));
		m_ui.tableTrace->setItem(row, 4, new QTableWidgetItem(QString::number(entry.cycles)));
		m_ui.tableTrace->setItem(row, 5, new QTableWidgetItem(entry.memAccess));
	}

	// Restore scroll position or scroll to bottom
	if (atBottom)
	{
		m_ui.tableTrace->scrollToBottom();
	}
	else
	{
		m_ui.tableTrace->verticalScrollBar()->setValue(scrollPos);
	}
}

QString InstructionTraceView::cpuName(u8 cpu) const
{
	switch (cpu)
	{
		case 0:
			return tr("EE");
		case 1:
			return tr("IOP");
		default:
			return tr("Unknown");
	}
}
