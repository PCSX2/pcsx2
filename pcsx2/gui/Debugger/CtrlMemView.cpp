/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "CtrlMemView.h"
#include "DebugTools/Debug.h"
#include "gui/AppConfig.h"

#include "BreakpointWindow.h"
#include "DebugEvents.h"
#include "DisassemblyDialog.h"
#include <wchar.h>
#include <wx/clipbrd.h>


wxBEGIN_EVENT_TABLE(CtrlMemView, wxWindow)
	EVT_PAINT(CtrlMemView::paintEvent)
	EVT_MOUSEWHEEL(CtrlMemView::mouseEvent)
	EVT_LEFT_DOWN(CtrlMemView::mouseEvent)
	EVT_LEFT_DCLICK(CtrlMemView::mouseEvent)
	EVT_RIGHT_DOWN(CtrlMemView::mouseEvent)
	EVT_RIGHT_UP(CtrlMemView::mouseEvent)
	EVT_KEY_DOWN(CtrlMemView::keydownEvent)
	EVT_CHAR(CtrlMemView::charEvent)
	EVT_SET_FOCUS(CtrlMemView::focusEvent)
	EVT_KILL_FOCUS(CtrlMemView::focusEvent)
	EVT_SCROLLWIN_LINEUP(CtrlMemView::scrollbarEvent)
	EVT_SCROLLWIN_LINEDOWN(CtrlMemView::scrollbarEvent)
	EVT_SCROLLWIN_PAGEUP(CtrlMemView::scrollbarEvent)
	EVT_SCROLLWIN_PAGEDOWN(CtrlMemView::scrollbarEvent)
wxEND_EVENT_TABLE()

enum MemoryViewMenuIdentifiers
{
	ID_MEMVIEW_GOTOINDISASM = 1,
	ID_MEMVIEW_GOTOADDRESS,
	ID_MEMVIEW_COPYADDRESS,
	ID_MEMVIEW_FOLLOWADDRESS,
	ID_MEMVIEW_DISPLAYVALUE_8,
	ID_MEMVIEW_DISPLAYVALUE_16,
	ID_MEMVIEW_DISPLAYVALUE_32,
	ID_MEMVIEW_DISPLAYVALUE_64,
	ID_MEMVIEW_DISPLAYVALUE_128,
	ID_MEMVIEW_COPYVALUE_8,
	ID_MEMVIEW_COPYVALUE_16,
	ID_MEMVIEW_COPYVALUE_32,
	ID_MEMVIEW_COPYVALUE_64,
	ID_MEMVIEW_COPYVALUE_128,
	ID_MEMVIEW_ALIGNWINDOW,
};

CtrlMemView::CtrlMemView(wxWindow* parent, DebugInterface* _cpu)
	: wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxVSCROLL)
	, cpu(_cpu)
{
	rowHeight = getDebugFontHeight();
	charWidth = getDebugFontWidth();
	windowStart = 0x480000;
	curAddress = windowStart;
	byteGroupSize = 1;

	asciiSelected = false;
	selectedNibble = 0;
	addressStart = charWidth;
	hexStart = addressStart + 9 * charWidth;

	setRowSize(g_Conf->EmuOptions.Debugger.MemoryViewBytesPerRow);

	font = pxGetFixedFont(8);
	underlineFont = pxGetFixedFont(8, wxFONTWEIGHT_NORMAL, true);
	font.SetPixelSize(wxSize(charWidth, rowHeight));
	underlineFont.SetPixelSize(wxSize(charWidth, rowHeight));

	menu.Append(ID_MEMVIEW_GOTOINDISASM, L"Go to in Disasm");
	menu.Append(ID_MEMVIEW_GOTOADDRESS, L"Go to address");
	menu.Append(ID_MEMVIEW_COPYADDRESS, L"Copy address");
	menu.Append(ID_MEMVIEW_FOLLOWADDRESS, L"Follow address");
	menu.AppendSeparator();
	menu.AppendRadioItem(ID_MEMVIEW_DISPLAYVALUE_8, L"Display as 1 byte");
	menu.AppendRadioItem(ID_MEMVIEW_DISPLAYVALUE_16, L"Display as 2 byte");
	menu.AppendRadioItem(ID_MEMVIEW_DISPLAYVALUE_32, L"Display as 4 byte");
	menu.AppendRadioItem(ID_MEMVIEW_DISPLAYVALUE_64, L"Display as 8 byte");
	menu.AppendRadioItem(ID_MEMVIEW_DISPLAYVALUE_128, L"Display as 16 byte");
	menu.Check(ID_MEMVIEW_DISPLAYVALUE_8, true);
	menu.AppendSeparator();
	menu.Append(ID_MEMVIEW_COPYVALUE_8, L"Copy Value (8 bit)");
	menu.Append(ID_MEMVIEW_COPYVALUE_16, L"Copy Value (16 bit)");
	menu.Append(ID_MEMVIEW_COPYVALUE_32, L"Copy Value (32 bit)");
	menu.Append(ID_MEMVIEW_COPYVALUE_64, L"Copy Value (64 bit)");
	menu.Append(ID_MEMVIEW_COPYVALUE_128, L"Copy Value (128 bit)");
	menu.AppendSeparator();
	menu.AppendCheckItem(ID_MEMVIEW_ALIGNWINDOW, L"Align window to row size");
	menu.Check(ID_MEMVIEW_ALIGNWINDOW, g_Conf->EmuOptions.Debugger.AlignMemoryWindowStart);
	menu.Bind(wxEVT_MENU, &CtrlMemView::onPopupClick, this);

	SetScrollbar(wxVERTICAL, 100, 1, 201, true);
	SetDoubleBuffered(true);
}

void CtrlMemView::setRowSize(int bytesInRow)
{
	rowSize = (std::max(16, std::min(256, bytesInRow)) / 16) * 16;
	asciiStart = hexStart + (rowSize * 3 + 1) * charWidth;
}

void CtrlMemView::postEvent(wxEventType type, wxString text)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetClientData(cpu);
	event.SetString(text);
	wxPostEvent(this, event);
}

void CtrlMemView::postEvent(wxEventType type, int value)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetClientData(cpu);
	event.SetInt(value);
	wxPostEvent(this, event);
}

void CtrlMemView::paintEvent(wxPaintEvent& evt)
{
	wxPaintDC dc(this);
	render(dc);
}

void CtrlMemView::redraw()
{
	wxClientDC dc(this);
	render(dc);
}

int CtrlMemView::hexGroupPositionFromIndex(int idx)
{
	int groupPos = idx * charWidth * 2;

	int space = (charWidth / 4);

	// spaces after every byte
	groupPos += idx * space;

	// spaces after every 2 bytes
	groupPos += (idx / 2) * space;

	// spaces after every 4 bytes
	return groupPos + (idx / 4) * space;
}

void CtrlMemView::render(wxDC& dc)
{
	bool hasFocus = wxWindow::FindFocus() == this;
	int visibleRows = GetClientSize().y / rowHeight;

	const wxColor COLOR_WHITE = wxColor(0xFFFFFFFF);
	const wxColor COLOR_BLACK = wxColor(0xFF000000);
	const wxColor COLOR_SELECTED_BG = wxColor(0xFFFF9933);
	const wxColor COLOR_SELECTED_INACTIVE_BG = wxColor(0xFFC0C0C0);
	const wxColor COLOR_REFRENCE_BG = wxColor(0xFFFFCFC8);
	const wxColor COLOR_ADDRESS = wxColor(0xFF600000);
	const wxColor COLOR_DELIMETER = wxColor(0xFFC0C0C0);

	dc.SetBrush(wxBrush(COLOR_WHITE));
	dc.SetPen(wxPen(COLOR_WHITE));

	int width, height;
	dc.GetSize(&width, &height);
	dc.DrawRectangle(0, 0, width, height);

	const int TEMP_SIZE = 64;
	wchar_t temp[TEMP_SIZE];

	bool validCpu = cpu && cpu->isAlive();

	// not hexGroupPositionFromIndex(byteGroupSize), because we dont need space after last symbol;
	int groupWidth = hexGroupPositionFromIndex(byteGroupSize - 1) + charWidth * 2;

	for (int i = 0; i < visibleRows + 1; i++)
	{
		u32 rowAddress = windowStart + i * rowSize;
		int rowY = rowHeight * i;

		swprintf(temp, TEMP_SIZE, L"%08X", rowAddress);
		dc.SetFont(font);
		dc.SetTextForeground(COLOR_ADDRESS);
		dc.DrawText(temp, addressStart, rowY);

		for (int j = 0; j < rowSize; j++)
		{
			u32 byteAddress = rowAddress + j;
			u8 byteCurrent = 0;
			bool byteValid = false;

			try
			{
				byteValid = validCpu && cpu->isValidAddress(byteAddress);

				if (byteValid)
					byteCurrent = cpu->read8(byteAddress);
			}
			catch (Exception::Ps2Generic&)
			{
				byteValid = false;
			}

			// not optimized way, but more flexible than previous

			// calculate group position
			int groupNum = j / byteGroupSize;
			int groupPosX = hexStart + groupNum * byteGroupSize * 3 * charWidth;

			// calculate symbol position in group
			int groupIndex = j % byteGroupSize;

			int symbolPosX = groupPosX + hexGroupPositionFromIndex(byteGroupSize - groupIndex - 1);

			u32 groupAddress = byteAddress - groupIndex;

			bool backgroundIsDark = false;
			if (curAddress >= groupAddress && curAddress < groupAddress + byteGroupSize)
			{
				// if group selected, draw rectangle behind
				if (groupIndex == 0)
				{
					if (hasFocus && !asciiSelected)
					{
						dc.SetPen(COLOR_SELECTED_BG);
						dc.SetBrush(COLOR_SELECTED_BG);
					}
					else
					{
						dc.SetPen(COLOR_SELECTED_INACTIVE_BG);
						dc.SetBrush(COLOR_SELECTED_INACTIVE_BG);
					}

					dc.DrawRectangle(groupPosX, rowY, groupWidth, rowHeight);
				}

				backgroundIsDark = hasFocus && !asciiSelected;
			}
			if (groupAddress + groupIndex == referencedAddress)
			{
				dc.SetPen(COLOR_REFRENCE_BG);
				dc.SetBrush(COLOR_REFRENCE_BG);
				dc.DrawRectangle(symbolPosX, rowY, charWidth * 2, rowHeight);
				backgroundIsDark = false;
			}

			dc.SetTextForeground(backgroundIsDark ? COLOR_WHITE : COLOR_BLACK);

			swprintf(temp, TEMP_SIZE, byteValid ? L"%02X" : L"??", byteCurrent);
			// if selected byte, need hint current nibble
			if (byteAddress == curAddress)
			{
				if (selectedNibble == 1)
					dc.SetFont(underlineFont);

				dc.DrawText(temp + 1, symbolPosX + charWidth, rowY);

				if (selectedNibble == 1)
					dc.SetFont(font);
				else
					dc.SetFont(underlineFont);

				temp[1] = 0;
				dc.DrawText(temp, symbolPosX, rowY);

				if (selectedNibble == 0)
					dc.SetFont(font);
			}
			else
			{
				dc.DrawText(temp, symbolPosX, rowY);
			}

			// draw in ansii text representation table
			temp[1] = 0;
			temp[0] = (!byteValid || byteCurrent < 32 || byteCurrent > 128) ? '.' : byteCurrent;

			if (byteAddress == curAddress)
			{
				if (hasFocus && asciiSelected)
				{
					dc.SetPen(COLOR_SELECTED_BG);
					dc.SetBrush(COLOR_SELECTED_BG);
					dc.SetTextForeground(COLOR_WHITE);
				}
				else
				{
					dc.SetPen(COLOR_SELECTED_INACTIVE_BG);
					dc.SetBrush(COLOR_SELECTED_INACTIVE_BG);
					dc.SetTextForeground(COLOR_BLACK);
				}
				dc.DrawRectangle(asciiStart + j * (charWidth + 2), rowY, charWidth, rowHeight);
			}
			else
			{
				dc.SetTextForeground(COLOR_BLACK);
			}

			dc.DrawText(temp, asciiStart + j * (charWidth + 2), rowY);
		}
	}

	dc.SetPen(COLOR_DELIMETER);
	dc.SetBrush(COLOR_DELIMETER);
	int linestep = std::max((u32)4, byteGroupSize);
	for (int i = linestep; i < rowSize; i += linestep)
	{
		int x = hexStart + i * 3 * charWidth - charWidth / 2;
		int y = (visibleRows + 1) * rowHeight;
		dc.DrawLine(x, 0, x, y);
	}
}

void CtrlMemView::onPopupClick(wxCommandEvent& evt)
{
	wchar_t str[64];

	switch (evt.GetId())
	{
		case ID_MEMVIEW_COPYADDRESS:
			if (wxTheClipboard->Open())
			{
				swprintf(str, 64, L"%08X", curAddress);
				wxTheClipboard->SetData(new wxTextDataObject(str));
				wxTheClipboard->Close();
			}
			break;
		case ID_MEMVIEW_GOTOINDISASM:
			postEvent(debEVT_GOTOINDISASM, curAddress);
			break;
		case ID_MEMVIEW_GOTOADDRESS:
			postEvent(debEVT_GOTOADDRESS, curAddress);
			break;
		case ID_MEMVIEW_FOLLOWADDRESS:
			gotoAddress(cpu->read32(curAddress), true);
			break;
		case ID_MEMVIEW_DISPLAYVALUE_8:
			byteGroupSize = 1;
			Refresh();
			break;
		case ID_MEMVIEW_DISPLAYVALUE_16:
			byteGroupSize = 2;
			Refresh();
			break;
		case ID_MEMVIEW_DISPLAYVALUE_32:
			byteGroupSize = 4;
			Refresh();
			break;
		case ID_MEMVIEW_DISPLAYVALUE_64:
			byteGroupSize = 8;
			Refresh();
			break;
		case ID_MEMVIEW_DISPLAYVALUE_128:
			byteGroupSize = 16;
			Refresh();
			break;
		case ID_MEMVIEW_COPYVALUE_8:
			if (wxTheClipboard->Open())
			{
				swprintf(str, 64, L"%02X", cpu->read8(curAddress));
				wxTheClipboard->SetData(new wxTextDataObject(str));
				wxTheClipboard->Close();
			}
			break;
		case ID_MEMVIEW_COPYVALUE_16:
			if (wxTheClipboard->Open())
			{
				swprintf(str, 64, L"%04X", cpu->read16(curAddress));
				wxTheClipboard->SetData(new wxTextDataObject(str));
				wxTheClipboard->Close();
			}
			break;
		case ID_MEMVIEW_COPYVALUE_32:
			if (wxTheClipboard->Open())
			{
				swprintf(str, 64, L"%08X", cpu->read32(curAddress));
				wxTheClipboard->SetData(new wxTextDataObject(str));
				wxTheClipboard->Close();
			}
			break;
		case ID_MEMVIEW_COPYVALUE_64:
			if (wxTheClipboard->Open())
			{
				swprintf(str, 64, L"%016llX", cpu->read64(curAddress));
				wxTheClipboard->SetData(new wxTextDataObject(str));
				wxTheClipboard->Close();
			}
			break;
		case ID_MEMVIEW_COPYVALUE_128:
			if (wxTheClipboard->Open())
			{
				u128 value = cpu->read128(curAddress);
				swprintf(str, 64, L"%016llX%016llX", value._u64[1], value._u64[0]);
				wxTheClipboard->SetData(new wxTextDataObject(str));
				wxTheClipboard->Close();
			}
			break;
		case ID_MEMVIEW_ALIGNWINDOW:
			g_Conf->EmuOptions.Debugger.AlignMemoryWindowStart = evt.IsChecked();
			if (g_Conf->EmuOptions.Debugger.AlignMemoryWindowStart)
			{
				windowStart -= windowStart % rowSize;
				redraw();
			}
			break;
	}
}

void CtrlMemView::mouseEvent(wxMouseEvent& evt)
{
	// left button
	if (evt.GetEventType() == wxEVT_LEFT_DOWN || evt.GetEventType() == wxEVT_LEFT_DCLICK || evt.GetEventType() == wxEVT_RIGHT_DOWN || evt.GetEventType() == wxEVT_RIGHT_DCLICK)
	{
		gotoPoint(evt.GetPosition().x, evt.GetPosition().y);
		SetFocus();
		SetFocusFromKbd();
	}
	else if (evt.GetEventType() == wxEVT_RIGHT_UP)
	{
		curAddress -= (curAddress - windowStart) % byteGroupSize;

		menu.Enable(ID_MEMVIEW_FOLLOWADDRESS, (curAddress & 3) == 0);

		menu.Check(ID_MEMVIEW_DISPLAYVALUE_8, byteGroupSize == 1);
		menu.Check(ID_MEMVIEW_DISPLAYVALUE_16, byteGroupSize == 2);
		menu.Check(ID_MEMVIEW_DISPLAYVALUE_32, byteGroupSize == 4);
		menu.Check(ID_MEMVIEW_DISPLAYVALUE_64, byteGroupSize == 8);
		menu.Check(ID_MEMVIEW_DISPLAYVALUE_128, byteGroupSize == 16);

		menu.Enable(ID_MEMVIEW_COPYVALUE_128, (curAddress & 15) == 0);
		menu.Enable(ID_MEMVIEW_COPYVALUE_64, (curAddress & 7) == 0);
		menu.Enable(ID_MEMVIEW_COPYVALUE_32, (curAddress & 3) == 0);
		menu.Enable(ID_MEMVIEW_COPYVALUE_16, (curAddress & 1) == 0);

		menu.Check(ID_MEMVIEW_ALIGNWINDOW, g_Conf->EmuOptions.Debugger.AlignMemoryWindowStart);

		PopupMenu(&menu);
		return;
	}
	else if (evt.GetEventType() == wxEVT_MOUSEWHEEL)
	{
		if (evt.ControlDown())
		{
			if (evt.GetWheelRotation() > 0)
			{
				setRowSize(rowSize + 16);
			}
			else
			{
				setRowSize(rowSize - 16);
			}
		}
		else
		{
			if (evt.GetWheelRotation() > 0)
			{
				scrollWindow(-3);
			}
			else if (evt.GetWheelRotation() < 0)
			{
				scrollWindow(3);
			}
		}
	}
	else
	{
		evt.Skip();
		return;
	}

	redraw();
}

void CtrlMemView::keydownEvent(wxKeyEvent& evt)
{
	if (evt.ControlDown())
	{
		switch (evt.GetKeyCode())
		{
			case 'g':
			case 'G':
			{
				u64 addr;
				if (!executeExpressionWindow(this, cpu, addr))
					return;

				gotoAddress(addr, true);
			}
			break;
			case 'b':
			case 'B':
			{
				BreakpointWindow bpw(this, cpu);
				if (bpw.ShowModal() == wxID_OK)
				{
					bpw.addBreakpoint();
					postEvent(debEVT_UPDATE, 0);
				}
			}
			break;
			case 'v':
			case 'V':
				pasteHex();
				break;
			default:
				evt.Skip();
				break;
		}
		return;
	}

	switch (evt.GetKeyCode())
	{
		case 'g':
		case 'G':
		{
			u64 addr;
			if (!executeExpressionWindow(this, cpu, addr))
				return;
			gotoAddress(addr, true);
		}
		break;
		case WXK_LEFT:
			scrollCursor(-1);
			break;
		case WXK_RIGHT:
			scrollCursor(1);
			break;
		case WXK_UP:
			scrollCursor(-rowSize);
			break;
		case WXK_DOWN:
			scrollCursor(rowSize);
			break;
		case WXK_PAGEUP:
			scrollWindow(-GetClientSize().y / rowHeight);
			break;
		case WXK_PAGEDOWN:
			scrollWindow(GetClientSize().y / rowHeight);
			break;
		case WXK_ESCAPE:
			if (!history.empty())
			{
				gotoAddress(history.top());
				history.pop();
			}
			break;
		default:
			evt.Skip();
			break;
	}
}

void CtrlMemView::charEvent(wxKeyEvent& evt)
{
	if (evt.GetKeyCode() < 32)
		return;

	if (!cpu->isValidAddress(curAddress))
	{
		scrollCursor(1);
		return;
	}

	bool active = !cpu->isCpuPaused();
	if (active)
		cpu->pauseCpu();

	if (asciiSelected)
	{
		u8 newValue = evt.GetKeyCode();
		cpu->write8(curAddress, newValue);
		scrollCursor(1);
	}
	else
	{
		u8 key = tolower(evt.GetKeyCode());
		int inputValue = -1;

		if (key >= '0' && key <= '9')
			inputValue = key - '0';
		if (key >= 'a' && key <= 'f')
			inputValue = key - 'a' + 10;

		if (inputValue >= 0)
		{
			int shiftAmount = (1 - selectedNibble) * 4;

			u8 oldValue = cpu->read8(curAddress);
			oldValue &= ~(0xF << shiftAmount);
			u8 newValue = oldValue | (inputValue << shiftAmount);
			cpu->write8(curAddress, newValue);
			scrollCursor(1);
		}
	}

	if (active)
		cpu->resumeCpu();
	redraw();
}

void CtrlMemView::scrollbarEvent(wxScrollWinEvent& evt)
{
	int type = evt.GetEventType();
	if (type == wxEVT_SCROLLWIN_LINEUP)
	{
		scrollCursor(-rowSize);
	}
	else if (type == wxEVT_SCROLLWIN_LINEDOWN)
	{
		scrollCursor(rowSize);
	}
	else if (type == wxEVT_SCROLLWIN_PAGEUP)
	{
		scrollWindow(-GetClientSize().y / rowHeight);
	}
	else if (type == wxEVT_SCROLLWIN_PAGEDOWN)
	{
		scrollWindow(GetClientSize().y / rowHeight);
	}

	redraw();
}

void CtrlMemView::scrollWindow(int lines)
{
	windowStart += lines * rowSize;
	curAddress += lines * rowSize;
	redraw();
}

void CtrlMemView::scrollCursor(int bytes)
{
	if (!asciiSelected && bytes == 1)
	{
		if (selectedNibble == 0)
		{
			selectedNibble = 1;
			bytes = 0;
		}
		else
		{
			selectedNibble = 0;
		}
	}
	else if (!asciiSelected && bytes == -1)
	{
		if (selectedNibble == 0)
		{
			selectedNibble = 1;
		}
		else
		{
			selectedNibble = 0;
			bytes = 0;
		}
	}

	curAddress += bytes;

	int visibleRows = GetClientSize().y / rowHeight;
	u32 windowEnd = windowStart + visibleRows * rowSize;

	if (curAddress < windowStart)
	{
		windowStart = (curAddress / rowSize) * rowSize;
	}
	else if (curAddress >= windowEnd)
	{
		windowStart = curAddress - (visibleRows - 1) * rowSize;
		windowStart = (windowStart / rowSize) * rowSize;
	}

	updateStatusBarText();
	redraw();
}

void CtrlMemView::updateStatusBarText()
{
	wchar_t text[64];

	int needpad = (curAddress - windowStart) % byteGroupSize;
	u32 addr = curAddress - needpad;

	swprintf(text, 64, L"%08X %08X", curAddress, addr);

	postEvent(debEVT_SETSTATUSBARTEXT, text);
}

void CtrlMemView::gotoAddress(u32 addr, bool pushInHistory)
{
	if (pushInHistory)
		history.push(windowStart);

	curAddress = addr;
	selectedNibble = 0;

	if (g_Conf->EmuOptions.Debugger.AlignMemoryWindowStart)
	{
		int visibleRows = GetClientSize().y / rowHeight;
		u32 windowEnd = windowStart + visibleRows * rowSize;

		if (curAddress < windowStart || curAddress >= windowEnd)
			windowStart = (curAddress / rowSize) * rowSize;
	}
	else
	{
		windowStart = curAddress;
	}

	updateStatusBarText();
	redraw();
}

void CtrlMemView::gotoPoint(int x, int y)
{
	int line = y / rowHeight;
	int lineAddress = windowStart + line * rowSize;

	if (x >= asciiStart)
	{
		int col = (x - asciiStart) / (charWidth + 2);
		if (col >= rowSize)
			return;

		asciiSelected = true;
		curAddress = lineAddress + col;
		selectedNibble = 0;
		updateStatusBarText();
		redraw();
	}
	else if (x >= hexStart)
	{
		int col = (x - hexStart);

		int groupWidth = byteGroupSize * charWidth * 3;
		int group = col / groupWidth;

		int posInGroup = col % groupWidth;

		int indexInGroup = -1;

		for (int i = 0; i < int(byteGroupSize); i++)
		{
			int start = hexGroupPositionFromIndex(i);
			int end = start + 2 * charWidth - 1;
			if (posInGroup < start)
			{
				return;
			}
			else if (posInGroup <= end)
			{
				selectedNibble = ((posInGroup - start) / charWidth) % 2;
				indexInGroup = i;
				break;
			}
		}

		if (indexInGroup == -1)
			return;

		curAddress = lineAddress + group * byteGroupSize + (byteGroupSize - indexInGroup - 1);

		asciiSelected = false;
		updateStatusBarText();
		redraw();
	}
}

void CtrlMemView::updateReference(u32 address)
{
	referencedAddress = address;
	redraw();
}

void CtrlMemView::pasteHex()
{
	if (wxTheClipboard->Open())
	{
		if (wxTheClipboard->IsSupported(wxDF_TEXT))
		{
			wxTextDataObject data;
			wxTheClipboard->GetData(data);
			wxString str = data.GetText();
			str.Replace(" ", "");
			str.Replace("\n", "");
			str.Replace("\r", "");
			str.Replace("\t", "");

			bool active = !cpu->isCpuPaused();
			if (active)
				cpu->pauseCpu();

			std::size_t i;
			for (i = 0; i < str.size() / 2; i++)
			{
				long byte;
				if (str.Mid(i * 2, 2).ToLong(&byte, 16))
				{
					cpu->write8(curAddress + i, static_cast<u8>(byte));
				}
				else
				{
					break;
				}
			}
			scrollCursor(i);

			if (active)
				cpu->resumeCpu();
		}
		wxTheClipboard->Close();
	}
}
