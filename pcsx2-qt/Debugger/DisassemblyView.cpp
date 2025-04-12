// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DisassemblyView.h"

#include "Debugger/DebuggerWindow.h"
#include "Debugger/JsonValueWrapper.h"
#include "Debugger/Breakpoints/BreakpointModel.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MipsAssembler.h"

#include "QtUtils.h"
#include "QtHost.h"
#include <QtCore/QPointer>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QMenu>
#include <QtGui/QClipboard>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include "SymbolTree/NewSymbolDialogs.h"

using namespace QtUtils;

DisassemblyView::DisassemblyView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
{
	m_ui.setupUi(this);

	m_disassemblyManager.setCpu(&cpu());

	setFocusPolicy(Qt::FocusPolicy::ClickFocus);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &DisassemblyView::customContextMenuRequested, this, &DisassemblyView::openContextMenu);

	connect(g_debugger_window, &DebuggerWindow::onVMActuallyPaused,
		this, &DisassemblyView::gotoProgramCounterOnPause);

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		update();
		return true;
	});

	receiveEvent<DebuggerEvents::GoToAddress>([this](const DebuggerEvents::GoToAddress& event) -> bool {
		if (event.filter != DebuggerEvents::GoToAddress::NONE &&
			event.filter != DebuggerEvents::GoToAddress::DISASSEMBLER)
			return false;

		gotoAddress(event.address, event.switch_to_tab);

		if (event.switch_to_tab)
			switchToThisTab();

		return true;
	});
}

DisassemblyView::~DisassemblyView() = default;

void DisassemblyView::toJson(JsonValueWrapper& json)
{
	DebuggerView::toJson(json);

	json.value().AddMember("startAddress", m_visibleStart, json.allocator());
	json.value().AddMember("goToPCOnPause", m_goToProgramCounterOnPause, json.allocator());
	json.value().AddMember("showInstructionBytes", m_showInstructionBytes, json.allocator());
}

bool DisassemblyView::fromJson(const JsonValueWrapper& json)
{
	if (!DebuggerView::fromJson(json))
		return false;

	auto start_address = json.value().FindMember("startAddress");
	if (start_address != json.value().MemberEnd() && start_address->value.IsUint())
		m_visibleStart = start_address->value.GetUint() & ~3;

	auto go_to_pc_on_pause = json.value().FindMember("goToPCOnPause");
	if (go_to_pc_on_pause != json.value().MemberEnd() && go_to_pc_on_pause->value.IsBool())
		m_goToProgramCounterOnPause = go_to_pc_on_pause->value.GetBool();

	auto show_instruction_bytes = json.value().FindMember("showInstructionBytes");
	if (show_instruction_bytes != json.value().MemberEnd() && show_instruction_bytes->value.IsBool())
		m_showInstructionBytes = show_instruction_bytes->value.GetBool();

	repaint();

	return true;
}

void DisassemblyView::contextCopyAddress()
{
	QGuiApplication::clipboard()->setText(FetchSelectionInfo(SelectionInfo::ADDRESS));
}

void DisassemblyView::contextCopyInstructionHex()
{
	QGuiApplication::clipboard()->setText(FetchSelectionInfo(SelectionInfo::INSTRUCTIONHEX));
}

void DisassemblyView::contextCopyInstructionText()
{
	QGuiApplication::clipboard()->setText(FetchSelectionInfo(SelectionInfo::INSTRUCTIONTEXT));
}

void DisassemblyView::contextAssembleInstruction()
{
	if (!cpu().isCpuPaused())
	{
		QMessageBox::warning(this, tr("Assemble Error"), tr("Unable to change assembly while core is running"));
		return;
	}

	DisassemblyLineInfo line;
	bool ok;
	m_disassemblyManager.getLine(m_selectedAddressStart, false, line);
	QString instruction = QInputDialog::getText(this, tr("Assemble Instruction"), "",
		QLineEdit::Normal, QString("%1 %2").arg(line.name.c_str()).arg(line.params.c_str()), &ok);

	if (!ok)
		return;

	u32 encodedInstruction;
	std::string errorText;
	bool valid = MipsAssembleOpcode(instruction.toLocal8Bit().constData(), &cpu(), m_selectedAddressStart, encodedInstruction, errorText);

	if (!valid)
	{
		QMessageBox::warning(this, tr("Assemble Error"), QString::fromStdString(errorText));
		return;
	}
	else
	{
		Host::RunOnCPUThread([this, start = m_selectedAddressStart, end = m_selectedAddressEnd, cpu = &cpu(), val = encodedInstruction] {
			for (u32 i = start; i <= end; i += 4)
			{
				this->m_nopedInstructions.insert({i, cpu->read32(i)});
				cpu->write32(i, val);
			}
			DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
		});
	}
}

void DisassemblyView::contextNoopInstruction()
{
	Host::RunOnCPUThread([this, start = m_selectedAddressStart, end = m_selectedAddressEnd, cpu = &cpu()] {
		for (u32 i = start; i <= end; i += 4)
		{
			this->m_nopedInstructions.insert({i, cpu->read32(i)});
			cpu->write32(i, 0x00);
		}
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

void DisassemblyView::contextRestoreInstruction()
{
	Host::RunOnCPUThread([this, start = m_selectedAddressStart, end = m_selectedAddressEnd, cpu = &cpu()] {
		for (u32 i = start; i <= end; i += 4)
		{
			if (this->m_nopedInstructions.find(i) != this->m_nopedInstructions.end())
			{
				cpu->write32(i, this->m_nopedInstructions[i]);
				this->m_nopedInstructions.erase(i);
			}
		}
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

void DisassemblyView::contextRunToCursor()
{
	const u32 selectedAddressStart = m_selectedAddressStart;
	Host::RunOnCPUThread([cpu = &cpu(), selectedAddressStart] {
		CBreakPoints::AddBreakPoint(cpu->getCpuType(), selectedAddressStart, true);
		cpu->resumeCpu();
	});
}

void DisassemblyView::contextJumpToCursor()
{
	cpu().setPc(m_selectedAddressStart);
	this->repaint();
}

void DisassemblyView::contextToggleBreakpoint()
{
	toggleBreakpoint(m_selectedAddressStart);
}

void DisassemblyView::contextFollowBranch()
{
	DisassemblyLineInfo line;

	m_disassemblyManager.getLine(m_selectedAddressStart, true, line);

	if (line.type == DISTYPE_OPCODE || line.type == DISTYPE_MACRO)
	{
		if (line.info.isBranch)
			gotoAddressAndSetFocus(line.info.branchTarget);
		else if (line.info.hasRelevantAddress)
			gotoAddressAndSetFocus(line.info.releventAddress);
	}
}

void DisassemblyView::contextGoToAddress()
{
	bool ok;
	const QString targetString = QInputDialog::getText(this, tr("Go To In Disassembly"), "",
		QLineEdit::Normal, "", &ok);

	if (!ok)
		return;

	u64 address = 0;
	std::string error;
	if (!cpu().evaluateExpression(targetString.toStdString().c_str(), address, error))
	{
		QMessageBox::warning(this, tr("Cannot Go To"), QString::fromStdString(error));
		return;
	}

	gotoAddressAndSetFocus(static_cast<u32>(address) & ~3);
}

void DisassemblyView::contextAddFunction()
{
	NewFunctionDialog* dialog = new NewFunctionDialog(cpu(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setName(QString("func_%1").arg(m_selectedAddressStart, 8, 16, QChar('0')));
	dialog->setAddress(m_selectedAddressStart);
	if (m_selectedAddressEnd != m_selectedAddressStart)
		dialog->setCustomSize(m_selectedAddressEnd - m_selectedAddressStart + 4);
	if (dialog->exec() == QDialog::Accepted)
		update();
}

void DisassemblyView::contextCopyFunctionName()
{
	std::string name = cpu().GetSymbolGuardian().FunctionStartingAtAddress(m_selectedAddressStart).name;
	QGuiApplication::clipboard()->setText(QString::fromStdString(name));
}

void DisassemblyView::contextRemoveFunction()
{
	cpu().GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Function* curFunc = database.functions.symbol_overlapping_address(m_selectedAddressStart);
		if (!curFunc)
			return;

		ccc::Function* previousFunc = database.functions.symbol_overlapping_address(curFunc->address().value - 4);
		if (previousFunc)
			previousFunc->set_size(curFunc->size() + previousFunc->size());

		database.functions.mark_symbol_for_destruction(curFunc->handle(), &database);
		database.destroy_marked_symbols();
	});
}

void DisassemblyView::contextRenameFunction()
{
	const FunctionInfo curFunc = cpu().GetSymbolGuardian().FunctionOverlappingAddress(m_selectedAddressStart);

	if (!curFunc.address.valid())
	{
		QMessageBox::warning(this, tr("Rename Function Error"), tr("No function / symbol is currently selected."));
		return;
	}

	QString oldName = QString::fromStdString(curFunc.name);

	bool ok;
	QString newName = QInputDialog::getText(this, tr("Rename Function"), tr("Function name"), QLineEdit::Normal, oldName, &ok);
	if (!ok)
		return;

	if (newName.isEmpty())
	{
		QMessageBox::warning(this, tr("Rename Function Error"), tr("Function name cannot be nothing."));
		return;
	}

	cpu().GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		database.functions.rename_symbol(curFunc.handle, newName.toStdString());
	});
}

void DisassemblyView::contextStubFunction()
{
	FunctionInfo function = cpu().GetSymbolGuardian().FunctionOverlappingAddress(m_selectedAddressStart);
	u32 address = function.address.valid() ? function.address.value : m_selectedAddressStart;

	Host::RunOnCPUThread([this, address, cpu = &cpu()] {
		this->m_stubbedFunctions.insert({address, {cpu->read32(address), cpu->read32(address + 4)}});
		cpu->write32(address, 0x03E00008); // jr ra
		cpu->write32(address + 4, 0x00000000); // nop
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

void DisassemblyView::contextRestoreFunction()
{
	u32 address = m_selectedAddressStart;
	cpu().GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_overlapping_address(m_selectedAddressStart);
		if (function)
			address = function->address().value;
	});

	auto stub = m_stubbedFunctions.find(address);
	if (stub != m_stubbedFunctions.end())
	{
		Host::RunOnCPUThread([this, address, cpu = &cpu(), stub] {
			auto [first_instruction, second_instruction] = stub->second;
			cpu->write32(address, first_instruction);
			cpu->write32(address + 4, second_instruction);
			this->m_stubbedFunctions.erase(address);
			DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
		});
	}
	else
	{
		QMessageBox::warning(this, tr("Restore Function Error"), tr("Unable to stub selected address."));
	}
}

void DisassemblyView::contextShowInstructionBytes()
{
	m_showInstructionBytes = !m_showInstructionBytes;
	this->repaint();
}

QString DisassemblyView::GetLineDisasm(u32 address)
{
	DisassemblyLineInfo lineInfo;
	m_disassemblyManager.getLine(address, true, lineInfo);
	return QString("%1 %2").arg(lineInfo.name.c_str()).arg(lineInfo.params.c_str());
};

// Here we go!
void DisassemblyView::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	const u32 w = painter.device()->width() - 1;
	const u32 h = painter.device()->height() - 1;

	// Get the current font size
	const QFontMetrics fm = painter.fontMetrics();

	// Get the row height
	m_rowHeight = fm.height() + 2;

	// Find the amount of visible disassembly rows. Minus 1 to not count column title row.
	m_visibleRows = h / m_rowHeight - 1;

	m_disassemblyManager.analyze(m_visibleStart, m_disassemblyManager.getNthNextAddress(m_visibleStart, m_visibleRows) - m_visibleStart);

	const u32 curPC = cpu().getPC(); // Get the PC here, because it'll change when we are drawing and make it seem like there are two PCs

	// Format and draw title line on first row
	const QString titleLineString = GetDisassemblyTitleLine();
	const QColor titleLineColor = GetDisassemblyTitleLineColor();
	painter.fillRect(0, 0 * m_rowHeight, w, m_rowHeight, titleLineColor);
	painter.drawText(2, 0 * m_rowHeight, w, m_rowHeight, Qt::AlignLeft, titleLineString);

	// Prepare to draw the disassembly rows
	bool inSelectionBlock = false;
	bool alternate = m_visibleStart % 8;

	// Draw visible disassembly rows
	for (u32 i = 0; i <= m_visibleRows; i++)
	{
		// Address of instruction being displayed on row
		const u32 rowAddress = (i * 4) + m_visibleStart;

		// Row will be drawn at row index+1 to offset past title row
		const u32 rowIndex = (i + 1) * m_rowHeight;

		// Row backgrounds
		if (inSelectionBlock || (m_selectedAddressStart <= rowAddress && rowAddress <= m_selectedAddressEnd))
		{
			painter.fillRect(0, rowIndex, w, m_rowHeight, this->palette().highlight());
			inSelectionBlock = m_selectedAddressEnd != rowAddress;
		}
		else
		{
			painter.fillRect(0, rowIndex, w, m_rowHeight, alternate ? this->palette().base() : this->palette().alternateBase());
		}

		// Row text
		painter.setPen(GetAddressFunctionColor(rowAddress));
		QString lineString = DisassemblyStringFromAddress(rowAddress, painter.font(), curPC, rowAddress == m_selectedAddressStart);

		painter.drawText(2, rowIndex, w, m_rowHeight, Qt::AlignLeft, lineString);

		// Breakpoint marker
		bool enabled;
		if (CBreakPoints::IsAddressBreakPoint(cpu().getCpuType(), rowAddress, &enabled) && !CBreakPoints::IsTempBreakPoint(cpu().getCpuType(), rowAddress))
		{
			if (enabled)
			{
				painter.setPen(Qt::green);
				painter.drawText(2, rowIndex, w, m_rowHeight, Qt::AlignLeft, "\u25A0");
			}
			else
			{
				painter.drawText(2, rowIndex, w, m_rowHeight, Qt::AlignLeft, "\u2612");
			}
		}
		alternate = !alternate;
	}
	// Draw the branch lines
	// This is where it gets a little scary
	// It's been mostly copied from the wx implementation

	u32 visibleEnd = m_disassemblyManager.getNthNextAddress(m_visibleStart, m_visibleRows);
	std::vector<BranchLine> branchLines = m_disassemblyManager.getBranchLines(m_visibleStart, visibleEnd - m_visibleStart);

	s32 branchCount = 0;
	for (const auto& branchLine : branchLines)
	{
		if (branchCount == (m_showInstructionBytes ? 3 : 5))
			break;
		const int winBottom = this->height();

		const int x = this->width() - 10 - (branchCount * 10);

		int top, bottom;
		// If the start is technically 'above' our address view
		if (branchLine.first < m_visibleStart)
		{
			top = -1;
		}
		// If the start is technically 'below' our address view
		else if (branchLine.first >= visibleEnd)
		{
			top = winBottom + 1;
		}
		else
		{
			// Explaination
			// ((branchLine.first - m_visibleStart) -> Find the amount of bytes from the top of the view
			// / 4 -> Convert that into rowss in instructions
			// + 1 -> Offset 1 to account for column title row
			// * m_rowHeight -> convert that into rows in pixels
			// + (m_rowHeight / 2) -> Add half a row in pixels to center the arrow
			top = (((branchLine.first - m_visibleStart) / 4 + 1) * m_rowHeight) + (m_rowHeight / 2);
		}

		if (branchLine.second < m_visibleStart)
		{
			bottom = -1;
		}
		else if (branchLine.second >= visibleEnd)
		{
			bottom = winBottom + 1;
		}
		else
		{
			bottom = (((branchLine.second - m_visibleStart) / 4 + 1) * m_rowHeight) + (m_rowHeight / 2);
		}

		branchCount++;

		if (branchLine.first == m_selectedAddressStart || branchLine.second == m_selectedAddressStart)
		{
			painter.setPen(QColor(0xFF257AFA));
		}
		else
		{
			painter.setPen(QColor(0xFFFF3020));
		}

		if (top < 0) // first is not visible, but second is
		{
			painter.drawLine(x - 2, bottom, x + 2, bottom);
			// Draw to first visible disassembly row so branch line is not drawn on title line
			painter.drawLine(x + 2, bottom, x + 2, m_rowHeight);

			if (branchLine.type == LINE_DOWN)
			{
				painter.drawLine(x, bottom - 4, x - 4, bottom);
				painter.drawLine(x - 4, bottom, x + 1, bottom + 5);
			}
		}
		else if (bottom > winBottom) // second is not visible, but first is
		{
			painter.drawLine(x - 2, top, x + 2, top);
			painter.drawLine(x + 2, top, x + 2, winBottom);

			if (branchLine.type == LINE_UP)
			{
				painter.drawLine(x, top - 4, x - 4, top);
				painter.drawLine(x - 4, top, x + 1, top + 5);
			}
		}
		else
		{ // both are visible
			if (branchLine.type == LINE_UP)
			{
				painter.drawLine(x - 2, bottom, x + 2, bottom);
				painter.drawLine(x + 2, bottom, x + 2, top);
				painter.drawLine(x + 2, top, x - 4, top);

				painter.drawLine(x, top - 4, x - 4, top);
				painter.drawLine(x - 4, top, x + 1, top + 5);
			}
			else
			{
				painter.drawLine(x - 2, top, x + 2, top);
				painter.drawLine(x + 2, top, x + 2, bottom);
				painter.drawLine(x + 2, bottom, x - 4, bottom);

				painter.drawLine(x, bottom - 4, x - 4, bottom);
				painter.drawLine(x - 4, bottom, x + 1, bottom + 5);
			}
		}
	}
	// Draw a border
	painter.setPen(this->palette().shadow().color());
	painter.drawRect(0, 0, w, h);
}

void DisassemblyView::mousePressEvent(QMouseEvent* event)
{
	// Calculate index of row that was clicked
	const u32 selectedRowIndex = static_cast<int>(event->position().y()) / m_rowHeight;

	// Only process if a row other than the column title row was clicked
	if (selectedRowIndex > 0)
	{
		// Calculate address of selected row. Index minus one for title row.
		const u32 selectedAddress = ((selectedRowIndex - 1) * 4) + m_visibleStart;
		if (event->buttons() & Qt::LeftButton)
		{
			if (event->modifiers() & Qt::ShiftModifier)
			{
				if (selectedAddress < m_selectedAddressStart)
				{
					m_selectedAddressStart = selectedAddress;
				}
				else if (selectedAddress > m_visibleStart)
				{
					m_selectedAddressEnd = selectedAddress;
				}
			}
			else
			{
				m_selectedAddressStart = selectedAddress;
				m_selectedAddressEnd = selectedAddress;
			}
		}
		else if (event->buttons() & Qt::RightButton)
		{
			if (m_selectedAddressStart == m_selectedAddressEnd)
			{
				m_selectedAddressStart = selectedAddress;
				m_selectedAddressEnd = selectedAddress;
			}
		}
		this->repaint();
	}
}

void DisassemblyView::mouseDoubleClickEvent(QMouseEvent* event)
{
	// Calculate index of row that was double clicked
	const u32 selectedRowIndex = static_cast<int>(event->position().y()) / m_rowHeight;

	// Only process if a row other than the column title row was double clicked
	if (selectedRowIndex > 0)
	{
		// Calculate address of selected row. Index minus one for title row.
		toggleBreakpoint(((selectedRowIndex - 1) * 4) + m_visibleStart);
	}
}

void DisassemblyView::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().y() < 0) // todo: max address bounds check?
	{
		m_visibleStart += 4;
	}
	else if (event->angleDelta().y() && m_visibleStart > 0)
	{
		m_visibleStart -= 4;
	}
	this->repaint();
}

void DisassemblyView::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
		case Qt::Key_Up:
		{
			m_selectedAddressStart -= 4;
			if (!(event->modifiers() & Qt::ShiftModifier))
				m_selectedAddressEnd = m_selectedAddressStart;

			// Auto scroll
			if (m_visibleStart > m_selectedAddressStart)
				m_visibleStart -= 4;
		}
		break;
		case Qt::Key_PageUp:
		{
			m_selectedAddressStart -= m_visibleRows * 4;
			m_selectedAddressEnd = m_selectedAddressStart;
			m_visibleStart -= m_visibleRows * 4;
		}
		break;
		case Qt::Key_Down:
		{
			m_selectedAddressEnd += 4;
			if (!(event->modifiers() & Qt::ShiftModifier))
				m_selectedAddressStart = m_selectedAddressEnd;

			// Purposely scroll on the second to last row. It's possible to
			// size the window so part of a row is visible and we don't want to have half a row selected and cut off!
			if (m_visibleStart + ((m_visibleRows - 1) * 4) < m_selectedAddressEnd)
				m_visibleStart += 4;

			break;
		}
		case Qt::Key_PageDown:
		{
			m_selectedAddressStart += m_visibleRows * 4;
			m_selectedAddressEnd = m_selectedAddressStart;
			m_visibleStart += m_visibleRows * 4;
			break;
		}
		case Qt::Key_G:
			contextGoToAddress();
			break;
		case Qt::Key_J:
			contextJumpToCursor();
			break;
		case Qt::Key_C:
			contextCopyInstructionText();
			break;
		case Qt::Key_B:
		case Qt::Key_Space:
			contextToggleBreakpoint();
			break;
		case Qt::Key_M:
			contextAssembleInstruction();
			break;
		case Qt::Key_Right:
			contextFollowBranch();
			break;
		case Qt::Key_Left:
			gotoAddressAndSetFocus(cpu().getPC());
			break;
		case Qt::Key_I:
			m_showInstructionBytes = !m_showInstructionBytes;
			break;
	}

	this->repaint();
}

void DisassemblyView::openContextMenu(QPoint pos)
{
	if (!cpu().isAlive())
		return;

	// Dont open context menu when used on column title row
	if (pos.y() / m_rowHeight == 0)
		return;

	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy_address_action = menu->addAction(tr("Copy Address"));
	connect(copy_address_action, &QAction::triggered, this, &DisassemblyView::contextCopyAddress);

	QAction* copy_instruction_hex_action = menu->addAction(tr("Copy Instruction Hex"));
	connect(copy_instruction_hex_action, &QAction::triggered, this, &DisassemblyView::contextCopyInstructionHex);

	QAction* copy_instruction_text_action = menu->addAction(tr("&Copy Instruction Text"));
	copy_instruction_text_action->setShortcut(QKeySequence(Qt::Key_C));
	connect(copy_instruction_text_action, &QAction::triggered, this, &DisassemblyView::contextCopyInstructionText);

	if (cpu().GetSymbolGuardian().FunctionExistsWithStartingAddress(m_selectedAddressStart))
	{
		QAction* copy_function_name_action = menu->addAction(tr("Copy Function Name"));
		connect(copy_function_name_action, &QAction::triggered, this, &DisassemblyView::contextCopyFunctionName);
	}

	menu->addSeparator();

	if (AddressCanRestore(m_selectedAddressStart, m_selectedAddressEnd))
	{
		QAction* restore_instruction_action = menu->addAction(tr("Restore Instruction(s)"));
		connect(restore_instruction_action, &QAction::triggered, this, &DisassemblyView::contextRestoreInstruction);
	}

	QAction* assemble_new_instruction = menu->addAction(tr("Asse&mble new Instruction(s)"));
	assemble_new_instruction->setShortcut(QKeySequence(Qt::Key_M));
	connect(assemble_new_instruction, &QAction::triggered, this, &DisassemblyView::contextAssembleInstruction);

	QAction* nop_instruction_action = menu->addAction(tr("NOP Instruction(s)"));
	connect(nop_instruction_action, &QAction::triggered, this, &DisassemblyView::contextNoopInstruction);

	menu->addSeparator();

	QAction* run_to_cursor_action = menu->addAction(tr("Run to Cursor"));
	connect(run_to_cursor_action, &QAction::triggered, this, &DisassemblyView::contextRunToCursor);

	QAction* jump_to_cursor_action = menu->addAction(tr("&Jump to Cursor"));
	jump_to_cursor_action->setShortcut(QKeySequence(Qt::Key_J));
	connect(jump_to_cursor_action, &QAction::triggered, this, &DisassemblyView::contextJumpToCursor);

	QAction* toggle_breakpoint_action = menu->addAction(tr("Toggle &Breakpoint"));
	toggle_breakpoint_action->setShortcut(QKeySequence(Qt::Key_B));
	connect(toggle_breakpoint_action, &QAction::triggered, this, &DisassemblyView::contextToggleBreakpoint);

	QAction* follow_branch_action = menu->addAction(tr("Follow Branch"));
	connect(follow_branch_action, &QAction::triggered, this, &DisassemblyView::contextFollowBranch);

	menu->addSeparator();

	QAction* go_to_address_action = menu->addAction(tr("&Go to Address"));
	go_to_address_action->setShortcut(QKeySequence(Qt::Key_G));
	connect(go_to_address_action, &QAction::triggered, this, &DisassemblyView::contextGoToAddress);

	createEventActions<DebuggerEvents::GoToAddress>(menu, [this]() {
		DebuggerEvents::GoToAddress event;
		event.address = m_selectedAddressStart;
		return std::optional(event);
	});

	QAction* go_to_pc_on_pause = menu->addAction(tr("Go to PC on Pause"));
	go_to_pc_on_pause->setCheckable(true);
	go_to_pc_on_pause->setChecked(m_goToProgramCounterOnPause);
	connect(go_to_pc_on_pause, &QAction::triggered, this,
		[this](bool value) { m_goToProgramCounterOnPause = value; });

	menu->addSeparator();

	QAction* add_function_action = menu->addAction(tr("Add Function"));
	connect(add_function_action, &QAction::triggered, this, &DisassemblyView::contextAddFunction);

	QAction* rename_function_action = menu->addAction(tr("Rename Function"));
	connect(rename_function_action, &QAction::triggered, this, &DisassemblyView::contextRenameFunction);

	QAction* remove_function_action = menu->addAction(tr("Remove Function"));
	menu->addAction(remove_function_action);
	connect(remove_function_action, &QAction::triggered, this, &DisassemblyView::contextRemoveFunction);

	if (FunctionCanRestore(m_selectedAddressStart))
	{
		QAction* restore_action = menu->addAction(tr("Restore Function"));
		connect(restore_action, &QAction::triggered, this, &DisassemblyView::contextRestoreFunction);
	}
	else
	{
		QAction* stub_action = menu->addAction(tr("Stub (NOP) Function"));
		connect(stub_action, &QAction::triggered, this, &DisassemblyView::contextStubFunction);
	}

	menu->addSeparator();

	QAction* show_instruction_bytes_action = menu->addAction(tr("Show &Instruction Bytes"));
	show_instruction_bytes_action->setShortcut(QKeySequence(Qt::Key_I));
	show_instruction_bytes_action->setCheckable(true);
	show_instruction_bytes_action->setChecked(m_showInstructionBytes);
	connect(show_instruction_bytes_action, &QAction::triggered, this, &DisassemblyView::contextShowInstructionBytes);

	menu->popup(this->mapToGlobal(pos));
}

QString DisassemblyView::GetDisassemblyTitleLine()
{
	// Disassembly column title line based on format created by DisassemblyStringFromAddress()
	QString title_line_string;

	// Determine layout of disassembly row. Layout depends on user setting "Show Instruction Bytes".
	const bool show_instruction_bytes = m_showInstructionBytes && cpu().isAlive();
	if (show_instruction_bytes)
	{
		title_line_string = QCoreApplication::translate("DisassemblyViewColumnTitle", " %1 %2 %3  %4");
	}
	else
	{
		title_line_string = QCoreApplication::translate("DisassemblyViewColumnTitle", " %1 %2  %3");
	}

	// First 2 chars in disassembly row is always for non-returning functions (NR)
	// Do not display column title for this field.
	title_line_string = title_line_string.arg("  ");

	// Second column title is always address of instruction
	title_line_string = title_line_string.arg(QCoreApplication::translate("DisassemblyViewColumnTitle", "Location"));

	// If user specified to "Show Instruction Bytes", third column is opcode + args
	if (show_instruction_bytes)
	{
		title_line_string = title_line_string.arg(QCoreApplication::translate("DisassemblyViewColumnTitle", "Bytes   "));
	}

	// Last column title is always disassembled instruction
	title_line_string = title_line_string.arg(QCoreApplication::translate("DisassemblyViewColumnTitle", "Instruction"));

	return title_line_string;
}

QColor DisassemblyView::GetDisassemblyTitleLineColor()
{
	// Determine color of column title line. Based on QFusionStyle.
	QColor title_line_color = this->palette().button().color();
	const int title_line_color_val = qGray(title_line_color.rgb());
	title_line_color = title_line_color.lighter(100 + qMax(1, (180 - title_line_color_val) / 6));
	title_line_color.setHsv(title_line_color.hue(), title_line_color.saturation() * 0.75, title_line_color.value());
	return title_line_color.lighter(104);
}

inline QString DisassemblyView::DisassemblyStringFromAddress(u32 address, QFont font, u32 pc, bool selected)
{
	DisassemblyLineInfo line;

	if (!cpu().isValidAddress(address))
		return tr("%1 NOT VALID ADDRESS").arg(address, 8, 16, QChar('0')).toUpper();
	// Todo? support non symbol view?
	m_disassemblyManager.getLine(address, true, line);

	const bool isConditional = line.info.isConditional && cpu().getPC() == address;
	const bool isConditionalMet = line.info.conditionMet;
	const bool isCurrentPC = cpu().getPC() == address;

	FunctionInfo function = cpu().GetSymbolGuardian().FunctionStartingAtAddress(address);
	SymbolInfo symbol = cpu().GetSymbolGuardian().SymbolStartingAtAddress(address);
	const bool showOpcode = m_showInstructionBytes && cpu().isAlive();

	QString lineString;
	if (showOpcode)
	{
		lineString = QString(" %1 %2 %3  %4 %5  %6 %7");
	}
	else
	{
		lineString = QString(" %1 %2  %3 %4  %5 %6");
	}

	if (function.is_no_return)
	{
		lineString = lineString.arg("NR");
	}
	else
	{
		lineString = lineString.arg("  ");
	}

	if (symbol.name.empty())
		lineString = lineString.arg(address, 8, 16, QChar('0')).toUpper();
	else
	{
		QFontMetrics metric(font);
		QString symbolString = QString::fromStdString(symbol.name);

		lineString = lineString.arg(metric.elidedText(symbolString, Qt::ElideRight, (selected ? 32 : 7) * font.pointSize()));
	}

	if (showOpcode)
	{
		const u32 opcode = cpu().read32(address);
		lineString = lineString.arg(QtUtils::FilledQStringFromValue(opcode, 16));
	}

	lineString = lineString.leftJustified(4, ' ') // Address / symbol
					 .arg(line.name.c_str())
					 .arg(line.params.c_str()) // opcode + arguments
					 .arg(isConditional ? (isConditionalMet ? "# true" : "# false") : "")
					 .arg(isCurrentPC ? "<--" : "");

	return lineString;
}

QColor DisassemblyView::GetAddressFunctionColor(u32 address)
{
	std::array<QColor, 6> colors;
	if (QtUtils::IsLightTheme(palette()))
	{
		colors = {
			QColor::fromRgba(0xFFFA3434),
			QColor::fromRgba(0xFF206b6b),
			QColor::fromRgba(0xFF858534),
			QColor::fromRgba(0xFF378c37),
			QColor::fromRgba(0xFF783278),
			QColor::fromRgba(0xFF21214a),
		};
	}
	else
	{
		colors = {
			QColor::fromRgba(0xFFe05555),
			QColor::fromRgba(0xFF55e0e0),
			QColor::fromRgba(0xFFe8e855),
			QColor::fromRgba(0xFF55e055),
			QColor::fromRgba(0xFFe055e0),
			QColor::fromRgba(0xFFC2C2F5),
		};
	}

	// Use the address to pick the colour since the value of the handle may
	// change from run to run.
	ccc::Address function_address =
		cpu().GetSymbolGuardian().FunctionOverlappingAddress(address).address;
	if (!function_address.valid())
		return palette().text().color();

	// Chop off the first few bits of the address since functions will be
	// aligned in memory.
	return colors[(function_address.value >> 4) % colors.size()];
}

QString DisassemblyView::FetchSelectionInfo(SelectionInfo selInfo)
{
	QString infoBlock;
	for (u32 i = m_selectedAddressStart; i <= m_selectedAddressEnd; i += 4)
	{
		if (i != m_selectedAddressStart)
			infoBlock += '\n';
		if (selInfo == SelectionInfo::ADDRESS)
		{
			infoBlock += FilledQStringFromValue(i, 16);
		}
		else if (selInfo == SelectionInfo::INSTRUCTIONTEXT)
		{
			DisassemblyLineInfo line;
			m_disassemblyManager.getLine(i, true, line);
			infoBlock += QString("%1 %2").arg(line.name.c_str()).arg(line.params.c_str());
		}
		else // INSTRUCTIONHEX
		{
			infoBlock += FilledQStringFromValue(cpu().read32(i), 16);
		}
	}
	return infoBlock;
}

void DisassemblyView::gotoAddressAndSetFocus(u32 address)
{
	gotoAddress(address, true);
}

void DisassemblyView::gotoProgramCounterOnPause()
{
	if (m_goToProgramCounterOnPause)
		gotoAddress(cpu().getPC(), false);
}

void DisassemblyView::gotoAddress(u32 address, bool should_set_focus)
{
	const u32 destAddress = address & ~3;
	// Center the address
	m_visibleStart = (destAddress - (m_visibleRows * 4 / 2)) & ~3;
	m_selectedAddressStart = destAddress;
	m_selectedAddressEnd = destAddress;

	this->repaint();
	if (should_set_focus)
		this->setFocus();
}

void DisassemblyView::toggleBreakpoint(u32 address)
{
	if (!cpu().isAlive())
		return;

	QPointer<DisassemblyView> disassembly_widget(this);

	Host::RunOnCPUThread([cpu = &cpu(), address, disassembly_widget] {
		if (!CBreakPoints::IsAddressBreakPoint(cpu->getCpuType(), address))
			CBreakPoints::AddBreakPoint(cpu->getCpuType(), address);
		else
			CBreakPoints::RemoveBreakPoint(cpu->getCpuType(), address);

		QtHost::RunOnUIThread([cpu, disassembly_widget]() {
			BreakpointModel::getInstance(*cpu)->refreshData();
			if (disassembly_widget)
				disassembly_widget->repaint();
		});
	});
}

bool DisassemblyView::AddressCanRestore(u32 start, u32 end)
{
	for (u32 i = start; i <= end; i += 4)
	{
		if (this->m_nopedInstructions.find(i) != this->m_nopedInstructions.end())
		{
			return true;
		}
	}
	return false;
}

bool DisassemblyView::FunctionCanRestore(u32 address)
{
	FunctionInfo function = cpu().GetSymbolGuardian().FunctionOverlappingAddress(address);
	if (function.address.valid())
		address = function.address.value;

	return m_stubbedFunctions.find(address) != m_stubbedFunctions.end();
}
