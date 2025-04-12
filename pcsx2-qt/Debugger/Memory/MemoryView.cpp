// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryView.h"

#include "Debugger/JsonValueWrapper.h"

#include "QtHost.h"
#include "QtUtils.h"
#include <QtCore/QObject>
#include <QtGui/QActionGroup>
#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

using namespace QtUtils;

/*
	MemoryViewTable
*/
void MemoryViewTable::UpdateStartAddress(u32 start)
{
	startAddress = start & ~0xF;
}

void MemoryViewTable::UpdateSelectedAddress(u32 selected, bool page)
{
	selectedAddress = selected;
	if (startAddress > selectedAddress)
	{
		if (page)
			startAddress -= 0x10 * rowVisible;
		else
			startAddress -= 0x10;
	}
	else if (startAddress + ((rowVisible - 1) * 0x10) < selectedAddress)
	{
		if (page)
			startAddress += 0x10 * rowVisible;
		else
			startAddress += 0x10;
	}
}

void MemoryViewTable::DrawTable(QPainter& painter, const QPalette& palette, s32 height, DebugInterface& cpu)
{
	rowHeight = painter.fontMetrics().height() + 2;
	const s32 charWidth = painter.fontMetrics().averageCharWidth();
	const s32 x = charWidth; // Left padding
	const s32 y = rowHeight;
	rowVisible = (height / rowHeight);
	rowCount = rowVisible + 1;

	row1YAxis = 0;

	// Draw the row addresses
	painter.setPen(palette.text().color());
	for (u32 i = 0; i < rowCount; i++)
	{
		painter.drawText(x, y + (rowHeight * i), FilledQStringFromValue(startAddress + (i * 0x10), 16));
	}
	valuexAxis = x + (charWidth * 8);

	// Draw the row values
	for (u32 i = 0; i < rowCount; i++)
	{
		const u32 currentRowAddress = startAddress + (i * 0x10);
		s32 valX = valuexAxis;
		segmentXAxis[0] = valX;
		for (int j = 0; j < 16 / static_cast<s32>(displayType); j++)
		{
			valX += charWidth;
			const u32 thisSegmentsStart = currentRowAddress + (j * static_cast<s32>(displayType));

			segmentXAxis[j] = valX;

			bool penDefault = false;
			if ((selectedAddress & ~0xF) == currentRowAddress)
			{
				if (selectedAddress >= thisSegmentsStart && selectedAddress < (thisSegmentsStart + static_cast<s32>(displayType)))
				{ // If the current byte and row we are drawing is selected
					if (!selectedText)
					{
						s32 charsIntoSegment = ((selectedAddress - thisSegmentsStart) * 2) + ((selectedNibbleHI ? 0 : 1) ^ littleEndian);
						if (littleEndian)
							charsIntoSegment = (static_cast<s32>(displayType) * 2) - charsIntoSegment - 1;
						painter.setPen(QColor::fromRgb(205, 165, 0)); // SELECTED NIBBLE LINE COLOUR
						const QPoint lineStart(valX + (charsIntoSegment * charWidth) + 1, y + (rowHeight * i));
						painter.drawLine(lineStart, lineStart + QPoint(charWidth - 3, 0));
					}
					painter.setPen(QColor::fromRgb(0xaa, 0x22, 0x22)); // SELECTED BYTE COLOUR
				}
				else
				{
					penDefault = true;
					painter.setPen(palette.text().color()); // Default colour
				}
			}
			else
			{
				penDefault = true;
				painter.setPen(palette.text().color()); // Default colour
			}

			bool valid;
			switch (displayType)
			{
				case MemoryViewType::BYTE:
				{
					const u8 val = static_cast<u8>(cpu.read8(thisSegmentsStart, valid));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "??");
					break;
				}
				case MemoryViewType::BYTEHW:
				{
					const u16 val = convertEndian<u16>(static_cast<u16>(cpu.read16(thisSegmentsStart, valid)));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "????");
					break;
				}
				case MemoryViewType::WORD:
				{
					const u32 val = convertEndian<u32>(cpu.read32(thisSegmentsStart, valid));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "????????");
					break;
				}
				case MemoryViewType::DWORD:
				{
					const u64 val = convertEndian<u64>(cpu.read64(thisSegmentsStart, valid));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "????????????????");
					break;
				}
			}
			valX += charWidth * 2 * static_cast<s32>(displayType);
		}

		// valX is our new X position after the hex values
		valX = valX + 6;
		textXAxis = valX;

		// Print the string representation
		for (s32 j = 0; j < 16; j++)
		{
			if (selectedAddress == j + currentRowAddress)
				painter.setPen(palette.highlight().color());
			else
				painter.setPen(palette.text().color());

			bool valid;
			const u8 value = cpu.read8(currentRowAddress + j, valid);
			if (valid)
			{
				QChar curChar = QChar::fromLatin1(value);
				if (!curChar.isPrint() && curChar != ' ') // Default to '.' for unprintable characters
					curChar = '.';

				painter.drawText(valX, y + (rowHeight * i), curChar);
			}
			else
			{
				painter.drawText(valX, y + (rowHeight * i), "?");
			}
			valX += charWidth + 1;
		}
	}
}

void MemoryViewTable::SelectAt(QPoint pos)
{
	// Check if SelectAt was called before DrawTable.
	if (rowHeight == 0)
		return;

	const u32 selectedRow = (pos.y() - 2) / (rowHeight);
	const s32 x = pos.x();
	const s32 avgSegmentWidth = segmentXAxis[1] - segmentXAxis[0];
	const u32 nibbleWidth = (avgSegmentWidth / (2 * (s32)displayType));
	selectedAddress = (selectedRow * 0x10) + startAddress;

	if (x <= segmentXAxis[0])
	{
		// The user clicked before the first segment
		selectedText = false;
		if (littleEndian)
			selectedAddress += static_cast<s32>(displayType) - 1;
		selectedNibbleHI = true;
	}
	else if (x > valuexAxis && x < textXAxis)
	{
		selectedText = false;
		// The user clicked inside of the hexadecimal area
		for (s32 i = 0; i < 16; i++)
		{
			if (i == ((16 / static_cast<s32>(displayType)) - 1) || (x >= segmentXAxis[i] && x < (segmentXAxis[i + 1])))
			{
				u32 indexInSegment = (x - segmentXAxis[i]) / nibbleWidth;
				if (littleEndian)
					indexInSegment = (static_cast<s32>(displayType) * 2) - indexInSegment - 1;
				selectedAddress = selectedAddress + i * static_cast<s32>(displayType) + (indexInSegment / 2);
				selectedNibbleHI = littleEndian ? indexInSegment & 1 : !(indexInSegment & 1);
				break;
			}
		}
	}
	else if (x >= textXAxis)
	{
		selectedText = true;
		// The user clicked the text area
		selectedAddress += std::min((x - textXAxis) / 8, 15);
	}
}

u128 MemoryViewTable::GetSelectedSegment(DebugInterface& cpu)
{
	u128 val;
	switch (displayType)
	{
		case MemoryViewType::BYTE:
			val.lo = cpu.read8(selectedAddress);
			break;
		case MemoryViewType::BYTEHW:
			val.lo = convertEndian(static_cast<u16>(cpu.read16(selectedAddress & ~1)));
			break;
		case MemoryViewType::WORD:
			val.lo = convertEndian(cpu.read32(selectedAddress & ~3));
			break;
		case MemoryViewType::DWORD:
			val._u64[0] = convertEndian(cpu.read64(selectedAddress & ~7));
			break;
	}
	return val;
}

void MemoryViewTable::InsertIntoSelectedHexView(u8 value, DebugInterface& cpu)
{
	const u8 mask = selectedNibbleHI ? 0x0f : 0xf0;
	u8 curVal = cpu.read8(selectedAddress) & mask;
	u8 newVal = value << (selectedNibbleHI ? 4 : 0);
	curVal |= newVal;

	Host::RunOnCPUThread([this, address = selectedAddress, &cpu, val = curVal] {
		cpu.write8(address, val);
		QtHost::RunOnUIThread([this] { parent->update(); });
	});
}

void MemoryViewTable::InsertAtCurrentSelection(const QString& text, DebugInterface& cpu)
{
	if (!cpu.isValidAddress(selectedAddress))
		return;

	// If pasting into the hex view, also decode the input as hex bytes.
	// This approach prevents one from pasting on a nibble boundary, but that is almost always
	// user error, and we don't have an undo function in this view, so best to stay conservative.
	QByteArray input = selectedText ? text.toUtf8() : QByteArray::fromHex(text.toUtf8());
	Host::RunOnCPUThread([this, address = selectedAddress, &cpu, inBytes = input] {
		u32 currAddr = address;
		for (int i = 0; i < inBytes.size(); i++)
		{
			cpu.write8(currAddr, inBytes[i]);
			currAddr = nextAddress(currAddr);
			QtHost::RunOnUIThread([this] { parent->update(); });
		}
		QtHost::RunOnUIThread([this, inBytes] { UpdateSelectedAddress(selectedAddress + inBytes.size()); parent->update(); });
	});
}

u32 MemoryViewTable::nextAddress(u32 addr)
{
	if (!littleEndian)
	{
		return addr + 1;
	}
	else
	{
		if (selectedAddress % static_cast<s32>(displayType) == 0)
			return addr + (static_cast<s32>(displayType) * 2 - 1);
		else
			return addr - 1;
	}
}

u32 MemoryViewTable::prevAddress(u32 addr)
{
	if (!littleEndian)
	{
		return addr - 1;
	}
	else
	{
		// It works
		if ((addr & (static_cast<u32>(displayType) - 1)) == (static_cast<u32>(displayType) - 1))
			return addr - (static_cast<s32>(displayType) * 2 - 1);
		else
			return selectedAddress + 1;
	}
}

void MemoryViewTable::ForwardSelection()
{
	if (!littleEndian)
	{
		if ((selectedNibbleHI = !selectedNibbleHI))
			UpdateSelectedAddress(selectedAddress + 1);
	}
	else
	{
		if ((selectedNibbleHI = !selectedNibbleHI))
		{
			if (selectedAddress % static_cast<s32>(displayType) == 0)
				UpdateSelectedAddress(selectedAddress + (static_cast<s32>(displayType) * 2 - 1));
			else
				UpdateSelectedAddress(selectedAddress - 1);
		}
	}
}

void MemoryViewTable::BackwardSelection()
{
	if (!littleEndian)
	{
		if (!(selectedNibbleHI = !selectedNibbleHI))
			UpdateSelectedAddress(selectedAddress - 1);
	}
	else
	{
		if (!(selectedNibbleHI = !selectedNibbleHI))
		{
			// It works
			if ((selectedAddress & (static_cast<u32>(displayType) - 1)) == (static_cast<u32>(displayType) - 1))
				UpdateSelectedAddress(selectedAddress - (static_cast<s32>(displayType) * 2 - 1));
			else
				UpdateSelectedAddress(selectedAddress + 1);
		}
	}
}


// We need both key and keychar because `key` is easy to use, but is case insensitive
bool MemoryViewTable::KeyPress(int key, QChar keychar, DebugInterface& cpu)
{
	if (!cpu.isValidAddress(selectedAddress))
		return false;

	bool pressHandled = false;

	const bool keyCharIsText = keychar.isLetterOrNumber() || keychar.isSpace();

	if (selectedText)
	{
		if (keyCharIsText || (!keychar.isNonCharacter() && keychar.category() != QChar::Other_Control))
		{
			Host::RunOnCPUThread([this, address = selectedAddress, &cpu, val = keychar.toLatin1()] {
				cpu.write8(address, val);
				QtHost::RunOnUIThread([this] { UpdateSelectedAddress(selectedAddress + 1); parent->update(); });
			});
			pressHandled = true;
		}

		switch (key)
		{
			case Qt::Key::Key_Backspace:
			case Qt::Key::Key_Escape:
				Host::RunOnCPUThread([this, address = selectedAddress, &cpu] {
					cpu.write8(address, 0);
					QtHost::RunOnUIThread([this] {BackwardSelection(); parent->update(); });
				});
				pressHandled = true;
				break;
			case Qt::Key::Key_Right:
				ForwardSelection();
				pressHandled = true;
				break;
			case Qt::Key::Key_Left:
				BackwardSelection();
				pressHandled = true;
				break;
			default:
				break;
		}
	}
	else
	{
		// Hex view is selected

		if (keyCharIsText)
		{
			// Check if key pressed is hex before insertion (QString conversion fails otherwise)
			const u8 keyPressed = static_cast<u8>(QString(QChar(key)).toInt(&pressHandled, 16));
			if (pressHandled)
			{
				InsertIntoSelectedHexView(keyPressed, cpu);
				ForwardSelection();
			}
		}

		switch (key)
		{
			case Qt::Key::Key_Backspace:
			case Qt::Key::Key_Escape:
				InsertIntoSelectedHexView(0, cpu);
				BackwardSelection();
				pressHandled = true;
				break;
			case Qt::Key::Key_Right:
				ForwardSelection();
				pressHandled = true;
				break;
			case Qt::Key::Key_Left:
				BackwardSelection();
				pressHandled = true;
				break;
			default:
				break;
		}
	}

	// Keybinds that are the same for the text and hex view

	switch (key)
	{
		case Qt::Key::Key_Up:
			UpdateSelectedAddress(selectedAddress - 0x10);
			pressHandled = true;
			break;
		case Qt::Key::Key_PageUp:
			UpdateSelectedAddress(selectedAddress - (0x10 * rowVisible), true);
			pressHandled = true;
			break;
		case Qt::Key::Key_Down:
			UpdateSelectedAddress(selectedAddress + 0x10);
			pressHandled = true;
			break;
		case Qt::Key::Key_PageDown:
			UpdateSelectedAddress(selectedAddress + (0x10 * rowVisible), true);
			pressHandled = true;
			break;
		default:
			break;
	}

	return pressHandled;
}

/*
	MemoryView
*/
MemoryView::MemoryView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
	, m_table(this)
{
	ui.setupUi(this);

	setFocusPolicy(Qt::FocusPolicy::ClickFocus);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &MemoryView::customContextMenuRequested, this, &MemoryView::openContextMenu);

	m_table.UpdateStartAddress(0x100000);

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		update();
		return true;
	});

	receiveEvent<DebuggerEvents::GoToAddress>([this](const DebuggerEvents::GoToAddress& event) -> bool {
		if (event.filter != DebuggerEvents::GoToAddress::NONE &&
			event.filter != DebuggerEvents::GoToAddress::MEMORY_VIEW)
			return false;

		gotoAddress(event.address);

		if (event.switch_to_tab)
			switchToThisTab();

		return true;
	});
}

MemoryView::~MemoryView() = default;

void MemoryView::toJson(JsonValueWrapper& json)
{
	DebuggerView::toJson(json);

	json.value().AddMember("startAddress", m_table.startAddress, json.allocator());
	json.value().AddMember("viewType", static_cast<int>(m_table.GetViewType()), json.allocator());
	json.value().AddMember("littleEndian", m_table.GetLittleEndian(), json.allocator());
}

bool MemoryView::fromJson(const JsonValueWrapper& json)
{
	if (!DebuggerView::fromJson(json))
		return false;

	auto start_address = json.value().FindMember("startAddress");
	if (start_address != json.value().MemberEnd() && start_address->value.IsUint())
		m_table.UpdateStartAddress(start_address->value.GetUint());

	auto view_type = json.value().FindMember("viewType");
	if (view_type != json.value().MemberEnd() && view_type->value.IsInt())
	{
		MemoryViewType type = static_cast<MemoryViewType>(view_type->value.GetInt());
		if (type == MemoryViewType::BYTE ||
			type == MemoryViewType::BYTEHW ||
			type == MemoryViewType::WORD ||
			type == MemoryViewType::DWORD)
			m_table.SetViewType(type);
	}

	auto little_endian = json.value().FindMember("littleEndian");
	if (little_endian != json.value().MemberEnd() && little_endian->value.IsBool())
		m_table.SetLittleEndian(little_endian->value.GetBool());

	repaint();

	return true;
}

void MemoryView::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	painter.fillRect(rect(), palette().window());

	if (!cpu().isAlive())
		return;

	m_table.DrawTable(painter, this->palette(), this->height(), cpu());
}

void MemoryView::mousePressEvent(QMouseEvent* event)
{
	if (!cpu().isAlive())
		return;

	m_table.SelectAt(event->pos());
	repaint();
}

void MemoryView::openContextMenu(QPoint pos)
{
	if (!cpu().isAlive())
		return;

	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy_action = menu->addAction(tr("Copy Address"));
	connect(copy_action, &QAction::triggered, this, [this]() {
		QApplication::clipboard()->setText(QString::number(m_table.selectedAddress, 16).toUpper());
	});

	createEventActions<DebuggerEvents::GoToAddress>(menu, [this]() {
		DebuggerEvents::GoToAddress event;
		event.address = m_table.selectedAddress;
		return std::optional(event);
	});

	QAction* go_to_address_action = menu->addAction(tr("Go to address"));
	connect(go_to_address_action, &QAction::triggered, this, [this]() { contextGoToAddress(); });

	menu->addSeparator();

	QAction* endian_action = menu->addAction(tr("Show as Little Endian"));
	endian_action->setCheckable(true);
	endian_action->setChecked(m_table.GetLittleEndian());
	connect(endian_action, &QAction::triggered, this, [this, endian_action]() {
		m_table.SetLittleEndian(endian_action->isChecked());
	});

	const MemoryViewType current_view_type = m_table.GetViewType();

	// View Types
	QActionGroup* view_type_group = new QActionGroup(menu);
	view_type_group->setExclusive(true);

	QAction* byte_action = menu->addAction(tr("Show as 1 byte"));
	byte_action->setCheckable(true);
	byte_action->setChecked(current_view_type == MemoryViewType::BYTE);
	connect(byte_action, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::BYTE); });
	view_type_group->addAction(byte_action);

	QAction* bytehw_action = menu->addAction(tr("Show as 2 bytes"));
	bytehw_action->setCheckable(true);
	bytehw_action->setChecked(current_view_type == MemoryViewType::BYTEHW);
	connect(bytehw_action, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::BYTEHW); });
	view_type_group->addAction(bytehw_action);

	QAction* word_action = menu->addAction(tr("Show as 4 bytes"));
	word_action->setCheckable(true);
	word_action->setChecked(current_view_type == MemoryViewType::WORD);
	connect(word_action, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::WORD); });
	view_type_group->addAction(word_action);

	QAction* dword_action = menu->addAction(tr("Show as 8 bytes"));
	dword_action->setCheckable(true);
	dword_action->setChecked(current_view_type == MemoryViewType::DWORD);
	connect(dword_action, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::DWORD); });
	view_type_group->addAction(dword_action);

	menu->addSeparator();

	createEventActions<DebuggerEvents::AddToSavedAddresses>(menu, [this]() {
		DebuggerEvents::AddToSavedAddresses event;
		event.address = m_table.selectedAddress;
		return std::optional(event);
	});

	connect(menu->addAction(tr("Copy Byte")), &QAction::triggered, this, &MemoryView::contextCopyByte);
	connect(menu->addAction(tr("Copy Segment")), &QAction::triggered, this, &MemoryView::contextCopySegment);
	connect(menu->addAction(tr("Copy Character")), &QAction::triggered, this, &MemoryView::contextCopyCharacter);
	connect(menu->addAction(tr("Paste")), &QAction::triggered, this, &MemoryView::contextPaste);

	menu->popup(this->mapToGlobal(pos));

	this->repaint();
	return;
}

void MemoryView::contextCopyByte()
{
	QApplication::clipboard()->setText(QString::number(cpu().read8(m_table.selectedAddress), 16).toUpper());
}

void MemoryView::contextCopySegment()
{
	QApplication::clipboard()->setText(QString::number(m_table.GetSelectedSegment(cpu()).lo, 16).toUpper());
}

void MemoryView::contextCopyCharacter()
{
	QApplication::clipboard()->setText(QChar::fromLatin1(cpu().read8(m_table.selectedAddress)).toUpper());
}

void MemoryView::contextPaste()
{
	m_table.InsertAtCurrentSelection(QApplication::clipboard()->text(), cpu());
}

void MemoryView::contextGoToAddress()
{
	bool ok;
	QString targetString = QInputDialog::getText(this, tr("Go To In Memory View"), "",
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

	gotoAddress(static_cast<u32>(address));
}

void MemoryView::mouseDoubleClickEvent(QMouseEvent* event)
{
}

void MemoryView::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().y() < 0)
	{
		m_table.UpdateStartAddress(m_table.startAddress + 0x10);
	}
	else if (event->angleDelta().y() > 0)
	{
		m_table.UpdateStartAddress(m_table.startAddress - 0x10);
	}
	this->repaint();
}

void MemoryView::keyPressEvent(QKeyEvent* event)
{
	if (!m_table.KeyPress(event->key(), event->text().size() ? event->text()[0] : '\0', cpu()))
	{
		switch (event->key())
		{
			case Qt::Key_G:
				contextGoToAddress();
				break;
			case Qt::Key_C:
				if (event->modifiers() & Qt::ControlModifier)
					contextCopySegment();
				break;
			default:
				break;
		}
	}
	this->repaint();
	DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
}

void MemoryView::gotoAddress(u32 address)
{
	m_table.UpdateStartAddress(address & ~0xF);
	m_table.selectedAddress = address;
	this->repaint();
	this->setFocus();
}
