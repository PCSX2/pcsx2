// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryViewWidget.h"
#include "common/Console.h"

#include "QtHost.h"
#include "QtUtils.h"
#include <QtGui/QMouseEvent>
#include <QtCore/QObject>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QClipboard>

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

void MemoryViewTable::DrawTable(QPainter& painter, const QPalette& palette, s32 height)
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
					const u8 val = static_cast<u8>(m_cpu->read8(thisSegmentsStart, valid));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "??");
					break;
				}
				case MemoryViewType::BYTEHW:
				{
					const u16 val = convertEndian<u16>(static_cast<u16>(m_cpu->read16(thisSegmentsStart, valid)));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "????");
					break;
				}
				case MemoryViewType::WORD:
				{
					const u32 val = convertEndian<u32>(m_cpu->read32(thisSegmentsStart, valid));
					if (penDefault && val == 0)
						painter.setPen(QColor::fromRgb(145, 145, 155)); // ZERO BYTE COLOUR
					painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "????????");
					break;
				}
				case MemoryViewType::DWORD:
				{
					const u64 val = convertEndian<u64>(m_cpu->read64(thisSegmentsStart, valid));
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
			const u8 value = m_cpu->read8(currentRowAddress + j, valid);
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

u128 MemoryViewTable::GetSelectedSegment()
{
	u128 val;
	switch (displayType)
	{
		case MemoryViewType::BYTE:
			val.lo = m_cpu->read8(selectedAddress);
			break;
		case MemoryViewType::BYTEHW:
			val.lo = convertEndian(static_cast<u16>(m_cpu->read16(selectedAddress & ~1)));
			break;
		case MemoryViewType::WORD:
			val.lo = convertEndian(m_cpu->read32(selectedAddress & ~3));
			break;
		case MemoryViewType::DWORD:
			val._u64[0] = convertEndian(m_cpu->read64(selectedAddress & ~7));
			break;
	}
	return val;
}

void MemoryViewTable::InsertIntoSelectedHexView(u8 value)
{
	const u8 mask = selectedNibbleHI ? 0x0f : 0xf0;
	u8 curVal = m_cpu->read8(selectedAddress) & mask;
	u8 newVal = value << (selectedNibbleHI ? 4 : 0);
	curVal |= newVal;

	Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu, val = curVal] {
		cpu->write8(address, val);
		QtHost::RunOnUIThread([this] { parent->update(); });
	});
}

void MemoryViewTable::InsertAtCurrentSelection(const QString& text)
{
	if (!m_cpu->isValidAddress(selectedAddress))
		return;

	// If pasting into the hex view, also decode the input as hex bytes.
	// This approach prevents one from pasting on a nibble boundary, but that is almost always
	// user error, and we don't have an undo function in this view, so best to stay conservative.
	QByteArray input = selectedText ? text.toUtf8() : QByteArray::fromHex(text.toUtf8());
	Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu, inBytes = input] {
		u32 currAddr = address;
		for (int i = 0; i < inBytes.size(); i++)
		{
			cpu->write8(currAddr, inBytes[i]);
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
bool MemoryViewTable::KeyPress(int key, QChar keychar)
{
	if (!m_cpu->isValidAddress(selectedAddress))
		return false;

	bool pressHandled = false;

	const bool keyCharIsText = keychar.isLetterOrNumber() || keychar.isSpace();

	if (selectedText)
	{
		if (keyCharIsText || (!keychar.isNonCharacter() && keychar.category() != QChar::Other_Control))
		{
			Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu, val = keychar.toLatin1()] {
				cpu->write8(address, val);
				QtHost::RunOnUIThread([this] { UpdateSelectedAddress(selectedAddress + 1); parent->update(); });
			});
			pressHandled = true;
		}

		switch (key)
		{
			case Qt::Key::Key_Backspace:
			case Qt::Key::Key_Escape:
				Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu] {
					cpu->write8(address, 0);
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
				InsertIntoSelectedHexView(keyPressed);
				ForwardSelection();
			}
		}

		switch (key)
		{
			case Qt::Key::Key_Backspace:
			case Qt::Key::Key_Escape:
				InsertIntoSelectedHexView(0);
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
	MemoryViewWidget
*/
MemoryViewWidget::MemoryViewWidget(QWidget* parent)
	: QWidget(parent)
	, m_table(this)
{
	ui.setupUi(this);
	this->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
	connect(this, &MemoryViewWidget::customContextMenuRequested, this, &MemoryViewWidget::customMenuRequested);
}

MemoryViewWidget::~MemoryViewWidget() = default;

void MemoryViewWidget::SetCpu(DebugInterface* cpu)
{
	m_cpu = cpu;
	m_table.SetCpu(cpu);
	m_table.UpdateStartAddress(0x480000);
}

void MemoryViewWidget::paintEvent(QPaintEvent* event)
{
	if (!m_cpu->isAlive())
		return;

	QPainter painter(this);

	m_table.DrawTable(painter, this->palette(), this->height());
}

void MemoryViewWidget::mousePressEvent(QMouseEvent* event)
{
	if (!m_cpu->isAlive())
		return;

	m_table.SelectAt(event->pos());
	repaint();
}

void MemoryViewWidget::customMenuRequested(QPoint pos)
{
	if (!m_cpu->isAlive())
		return;

	if (!m_contextMenu)
	{
		m_contextMenu = new QMenu(this);

		QAction* action = new QAction(tr("Copy Address"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { QApplication::clipboard()->setText(QString::number(m_table.selectedAddress, 16).toUpper()); });

		action = new QAction(tr("Go to in Disassembly"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { emit gotoInDisasm(m_table.selectedAddress); });

		action = new QAction(tr("Go to address"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { contextGoToAddress(); });

		m_contextMenu->addSeparator();

		m_actionLittleEndian = new QAction(tr("Show as Little Endian"));
		m_actionLittleEndian->setCheckable(true);
		m_contextMenu->addAction(m_actionLittleEndian);
		connect(m_actionLittleEndian, &QAction::triggered, this, [this]() { m_table.SetLittleEndian(m_actionLittleEndian->isChecked()); });

		// View Types
		m_actionBYTE = new QAction(tr("Show as 1 byte"));
		m_actionBYTE->setCheckable(true);
		m_contextMenu->addAction(m_actionBYTE);
		connect(m_actionBYTE, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::BYTE); });

		m_actionBYTEHW = new QAction(tr("Show as 2 bytes"));
		m_actionBYTEHW->setCheckable(true);
		m_contextMenu->addAction(m_actionBYTEHW);
		connect(m_actionBYTEHW, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::BYTEHW); });

		m_actionWORD = new QAction(tr("Show as 4 bytes"));
		m_actionWORD->setCheckable(true);
		m_contextMenu->addAction(m_actionWORD);
		connect(m_actionWORD, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::WORD); });

		m_actionDWORD = new QAction(tr("Show as 8 bytes"));
		m_actionDWORD->setCheckable(true);
		m_contextMenu->addAction(m_actionDWORD);
		connect(m_actionDWORD, &QAction::triggered, this, [this]() { m_table.SetViewType(MemoryViewType::DWORD); });

		m_contextMenu->addSeparator();

		action = new QAction((tr("Add to Saved Memory Addresses")));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { emit addToSavedAddresses(m_table.selectedAddress); });

		action = new QAction(tr("Copy Byte"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { contextCopyByte(); });

		action = new QAction(tr("Copy Segment"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { contextCopySegment(); });

		action = new QAction(tr("Copy Character"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { contextCopyCharacter(); });

		action = new QAction(tr("Paste"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { contextPaste(); });
	}
	m_actionLittleEndian->setChecked(m_table.GetLittleEndian());

	const MemoryViewType currentViewType = m_table.GetViewType();

	m_actionBYTE->setChecked(currentViewType == MemoryViewType::BYTE);
	m_actionBYTEHW->setChecked(currentViewType == MemoryViewType::BYTEHW);
	m_actionWORD->setChecked(currentViewType == MemoryViewType::WORD);
	m_actionDWORD->setChecked(currentViewType == MemoryViewType::DWORD);
	m_contextMenu->popup(this->mapToGlobal(pos));

	this->repaint();
	return;
}

void MemoryViewWidget::contextCopyByte()
{
	QApplication::clipboard()->setText(QString::number(m_cpu->read8(m_table.selectedAddress), 16).toUpper());
}

void MemoryViewWidget::contextCopySegment()
{
	QApplication::clipboard()->setText(QString::number(m_table.GetSelectedSegment().lo, 16).toUpper());
}

void MemoryViewWidget::contextCopyCharacter()
{
	QApplication::clipboard()->setText(QChar::fromLatin1(m_cpu->read8(m_table.selectedAddress)).toUpper());
}

void MemoryViewWidget::contextPaste()
{
	m_table.InsertAtCurrentSelection(QApplication::clipboard()->text());
}

void MemoryViewWidget::contextGoToAddress()
{
	bool ok;
	QString targetString = QInputDialog::getText(this, tr("Go To In Memory View"), "",
		QLineEdit::Normal, "", &ok);

	if (!ok)
		return;

	u64 address = 0;
	if (!m_cpu->evaluateExpression(targetString.toStdString().c_str(), address))
	{
		QMessageBox::warning(this, tr("Cannot Go To"), getExpressionError());
		return;
	}

	gotoAddress(static_cast<u32>(address));
}

void MemoryViewWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
}

void MemoryViewWidget::wheelEvent(QWheelEvent* event)
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

void MemoryViewWidget::keyPressEvent(QKeyEvent* event)
{
	if (!m_table.KeyPress(event->key(), event->text().size() ? event->text()[0] : '\0'))
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
	VMUpdate();
}

void MemoryViewWidget::gotoAddress(u32 address)
{
	m_table.UpdateStartAddress(address & ~0xF);
	m_table.selectedAddress = address;
	this->repaint();
	this->setFocus();
}
