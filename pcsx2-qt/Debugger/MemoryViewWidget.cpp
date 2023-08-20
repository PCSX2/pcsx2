/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "MemoryViewWidget.h"

#include "QtHost.h"
#include "QtUtils.h"
#include <QtGui/QMouseEvent>
#include <QtCore/QObject>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QClipboard>
#include <QtCore/QtEndian>

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
		u32 currentSegmentAddress = currentRowAddress;
		for (int j = 0; j < 16; j++)
		{
			const u32 currentByteAddress = currentRowAddress + j;

			if (!(j % (s32)displayType))
			{
				valX += charWidth;
				currentSegmentAddress = currentByteAddress;
			}
			segmentXAxis[j] = valX;

			if ((selectedAddress & ~0xF) == currentRowAddress)
			{
				if (selectedAddress == currentByteAddress)
				{ // If the current byte and row we are drawing is selected
					if (!selectedText)
					{
						painter.setPen(QColor::fromRgb(205, 165, 0)); // SELECTED NIBBLE LINE COLOUR
						const QPoint lineStart(valX + (selectedNibbleHI ? 0 : charWidth) + 1, y + (rowHeight * i));
						painter.drawLine(lineStart, lineStart + QPoint(charWidth - 3, 0));
					}
					painter.setPen(QColor::fromRgb(0xaa, 0x22, 0x22)); // SELECTED BYTE COLOUR
				}
				// If the current selected byte is in our current segment, highlight the entire segment
				else if (displayType != MemoryViewType::BYTE &&
						 currentSegmentAddress <= selectedAddress && (selectedAddress <= (currentSegmentAddress + (s32)displayType - 1)))
				{
					painter.setPen(palette.highlight().color()); // SELECTED SEGMENT COLOUR
				}
				else
					painter.setPen(palette.text().color()); // Default colour
			}
			else
				painter.setPen(palette.text().color()); // Default colour

			bool valid;
			const u8 val = static_cast<u8>(m_cpu->read8(currentByteAddress, valid));

			painter.drawText(valX, y + (rowHeight * i), valid ? FilledQStringFromValue(val, 16) : "??");

			valX += charWidth * 2;
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
	selectedAddress = (selectedRow * 0x10) + startAddress;

	if (x <= segmentXAxis[0])
	{
		selectedText = false;
		// The user clicked before the first segment
		selectedNibbleHI = true;
	}
	else if (x > valuexAxis && x < textXAxis)
	{
		selectedText = false;
		// The user clicked inside of the hexadecimal area
		for (s32 i = 0; i < 16; i++)
		{
			if (i == 15 || (x >= segmentXAxis[i] && x < (segmentXAxis[i + 1])))
			{
				selectedAddress = selectedAddress + i;
				selectedNibbleHI = ((x - segmentXAxis[i]) < ((avgSegmentWidth / 2) - 2)); // Subtract 2 units, makes selecting nibbles feel more natural
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
			val.lo = qToBigEndian((u16)m_cpu->read16(selectedAddress & ~1));
			break;
		case MemoryViewType::WORD:
			val.lo = qToBigEndian(m_cpu->read32(selectedAddress & ~3));
			break;
		case MemoryViewType::DWORD:
			val._u64[0] = qToBigEndian(m_cpu->read64(selectedAddress & ~7));
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

void MemoryViewTable::InsertAtCurrentSelection(const QString& text) {
	if (!m_cpu->isValidAddress(selectedAddress))
		return;

	// If pasting into the hex view, also decode the input as hex bytes.
	// This approach prevents one from pasting on a nibble boundary, but that is almost always
	// user error, and we don't have an undo function in this view, so best to stay conservative.
	QByteArray input = selectedText ? text.toUtf8() : QByteArray::fromHex(text.toUtf8());

	Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu, inBytes = input] {
		for (int i = 0; i < inBytes.size(); i++)
		{
			cpu->write8(address + i, inBytes[i]);
		}
		QtHost::RunOnUIThread([this, inBytes] { UpdateSelectedAddress(selectedAddress + inBytes.size()); parent->update(); });
	});
}

// We need both key and keychar because `key` is easy to use, but is case insensitive
void MemoryViewTable::KeyPress(int key, QChar keychar)
{
	if (!m_cpu->isValidAddress(selectedAddress))
		return;

	const bool keyCharIsText = keychar.isLetterOrNumber() || keychar.isSpace();

	if (selectedText)
	{
		if (keyCharIsText || (!keychar.isNonCharacter() && keychar.category() != QChar::Other_Control))
		{
			Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu, val = keychar.toLatin1()] {
				cpu->write8(address, val);
				QtHost::RunOnUIThread([this] { UpdateSelectedAddress(selectedAddress + 1); parent->update(); });
			});
		}

		switch (key)
		{
			case Qt::Key::Key_Backspace:
			case Qt::Key::Key_Escape:
				Host::RunOnCPUThread([this, address = selectedAddress, cpu = m_cpu] {
					cpu->write8(address, 0);
					QtHost::RunOnUIThread([this] { UpdateSelectedAddress(selectedAddress - 1); parent->update(); });
				});
				break;
			case Qt::Key::Key_Right:
				UpdateSelectedAddress(selectedAddress + 1);
				break;
			case Qt::Key::Key_Left:
				UpdateSelectedAddress(selectedAddress - 1);
				break;
		}
	}
	else
	{
		// Hex view is selected

		if (keyCharIsText)
		{
			InsertIntoSelectedHexView(((u8)QString(QChar(key)).toInt(nullptr, 16)));
			// Increment to the next nibble or byte
			if ((selectedNibbleHI = !selectedNibbleHI))
				UpdateSelectedAddress(selectedAddress + 1);
		}

		switch (key)
		{
			case Qt::Key::Key_Backspace:
			case Qt::Key::Key_Escape:
				InsertIntoSelectedHexView(0);
				// Move back a byte or nibble if it's backspace being pressed
				if (!(selectedNibbleHI = !selectedNibbleHI))
					UpdateSelectedAddress(selectedAddress - 1);
				break;
			case Qt::Key::Key_Right:
				if ((selectedNibbleHI = !selectedNibbleHI))
					UpdateSelectedAddress(selectedAddress + 1);
				break;
			case Qt::Key::Key_Left:
				if (!(selectedNibbleHI = !selectedNibbleHI))
					UpdateSelectedAddress(selectedAddress - 1);
				break;
		}
	}

	// Keybinds that are the same for the text and hex view

	switch (key)
	{
		case Qt::Key::Key_Up:
			UpdateSelectedAddress(selectedAddress - 0x10);
			break;
		case Qt::Key::Key_PageUp:
			UpdateSelectedAddress(selectedAddress - (0x10 * rowVisible), true);
			break;
		case Qt::Key::Key_Down:
			UpdateSelectedAddress(selectedAddress + 0x10);
			break;
		case Qt::Key::Key_PageDown:
			UpdateSelectedAddress(selectedAddress + (0x10 * rowVisible), true);
			break;
	}
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

		action = new QAction(tr("Go to in disassembly"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { emit gotoInDisasm(m_table.selectedAddress); });

		action = new QAction(tr("Go to address"));
		m_contextMenu->addAction(action);
		connect(action, &QAction::triggered, this, [this]() { contextGoToAddress(); });

		m_contextMenu->addSeparator();

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
	QString targetString = QInputDialog::getText(this, tr("Go to address"), "",
		QLineEdit::Normal, "", &ok);

	if (!ok)
		return;

	const u32 targetAddress = targetString.toUInt(&ok, 16);

	if (!ok)
	{
		QMessageBox::warning(this, "Go to address error", "Invalid address");
		return;
	}

	gotoAddress(targetAddress);
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
	bool handledByWidget = true;
	switch (event->key())
	{
		case Qt::Key_G:
			contextGoToAddress();
			break;
		case Qt::Key_C:
			if (event->modifiers() & Qt::ControlModifier)
				contextCopySegment();
			else
				handledByWidget = false;
			break;
		default:
			handledByWidget = false;
			break;
	}

	if (!handledByWidget)
		m_table.KeyPress(event->key(), event->text().size() ? event->text()[0] : '\0');

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
