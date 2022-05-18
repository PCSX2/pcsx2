/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include "CtrlRegisterList.h"
#include "DebugTools/Debug.h"
#include "common/BitCast.h"
#include "common/StringUtil.h"

#include "DebugEvents.h"
#include "gui/AppConfig.h"
#include "DisassemblyDialog.h"

wxBEGIN_EVENT_TABLE(CtrlRegisterList, wxWindow)
	EVT_SIZE(CtrlRegisterList::sizeEvent)
	EVT_LEFT_DOWN(CtrlRegisterList::mouseEvent)
	EVT_RIGHT_DOWN(CtrlRegisterList::mouseEvent)
	EVT_RIGHT_UP(CtrlRegisterList::mouseEvent)
	EVT_MOTION(CtrlRegisterList::mouseEvent)
	EVT_KEY_DOWN(CtrlRegisterList::keydownEvent)
wxEND_EVENT_TABLE()

enum DisassemblyMenuIdentifiers {
	ID_REGISTERLIST_RESOLVESTRINGS,
	ID_REGISTERLIST_DISPLAYVU0FFLOATS,
	ID_REGISTERLIST_CHANGELOWER,
	ID_REGISTERLIST_CHANGEUPPER,
	ID_REGISTERLIST_CHANGEVALUE,
	ID_REGISTERLIST_GOTOINMEMORYVIEW,
	ID_REGISTERLIST_GOTOINDISASM
};


CtrlRegisterList::CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_NONE | wxVSCROLL)
	, cpu(_cpu)
{
	rowHeight = getDebugFontHeight() + 2;
	charWidth = getDebugFontWidth();
	category = 0;
	lastPc = 0xFFFFFFFF;
	resolvePointerStrings = false;
	displayVU0FAsFloat = false;

	for (int i = 0; i < cpu->getRegisterCategoryCount(); i++)
	{
		const int count = cpu->getRegisterCount(i);

		ChangedReg* regs = new ChangedReg[count];
		memset(regs, 0, sizeof(ChangedReg) * count);
		changedCategories.push_back(regs);

		int maxLen = 0;
		for (int k = 0; k < cpu->getRegisterCount(i); k++)
		{
			maxLen = std::max<int>(maxLen, strlen(cpu->getRegisterName(i, k))); /* Flawfinder: ignore */
		}

		const int x = 17 + (maxLen + 2) * charWidth;
		startPositions.push_back(x);
		currentRows.push_back(0);
	}

	SetDoubleBuffered(true);

	const wxSize optSize = getOptimalSize();
	SetVirtualSize(optSize.x,optSize.y);
	SetScrollbars(1, rowHeight, optSize.x, optSize.y * MSW_GetDPIScale() / rowHeight, 0, 0);
}

CtrlRegisterList::~CtrlRegisterList()
{
	for (auto& regs : changedCategories)
		delete[] regs;
}

wxSize CtrlRegisterList::getOptimalSize() const
{
	wxSize optRegListSize;
	if (cpu->getCpuType() == BreakPointCpu::BREAKPOINT_EE)
	{
		optRegListSize = wxSize(331, 504);
	}
	else
	{
		// We are constructing the IOP register view
		optRegListSize = wxSize(133, 504);
	}

	return wxSize(optRegListSize.x, optRegListSize.y);
}

void CtrlRegisterList::postEvent(wxEventType type, wxString text)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetString(text);
	wxPostEvent(this, event);
}

void CtrlRegisterList::postEvent(wxEventType type, int value)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetInt(value);
	wxPostEvent(this, event);
}

void CtrlRegisterList::refreshChangedRegs()
{
	if (cpu->getPC() == lastPc)
		return;

	for (size_t cat = 0; cat < changedCategories.size(); cat++)
	{
		ChangedReg* regs = changedCategories[cat];
		const int size = cpu->getRegisterSize(category);

		for (int i = 0; i < cpu->getRegisterCount(cat); i++)
		{
			ChangedReg& reg = regs[i];
			memset(&reg.changed, 0, sizeof(reg.changed));

			const u128 newValue = cpu->getRegister(cat, i);

			if (reg.oldValue != newValue)
			{
				bool changed = false;

				if (size >= 128 && (reg.oldValue._u32[3] != newValue._u32[3] || reg.oldValue._u32[2] != newValue._u32[2]))
				{
					changed = true;
					reg.changed[3] = true;
					reg.changed[2] = true;
				}

				if (size >= 64 && (reg.oldValue._u32[1] != newValue._u32[1] || changed))
				{
					changed = true;
					reg.changed[1] = true;
				}

				if (reg.oldValue._u32[0] != newValue._u32[0] || changed)
				{
					reg.changed[0] = true;
				}

				reg.oldValue = newValue;
			}
		}
	}

	lastPc = cpu->getPC();
}

void CtrlRegisterList::redraw()
{
	Update();
}

void CtrlRegisterList::sizeEvent(wxSizeEvent& evt)
{
	Refresh();
	evt.Skip();
}

void drawU32Text(wxDC& dc, u32 value, int x, int y)
{
	char str[32];
	sprintf(str, "%08X", value);
	dc.DrawText(wxString(str, wxConvUTF8), x, y);
}

void CtrlRegisterList::OnDraw(wxDC& dc)
{
	wxFont font = pxGetFixedFont(8);
	font.SetPixelSize(wxSize(charWidth, rowHeight - 2));
	dc.SetFont(font);

	refreshChangedRegs();

	wxColor colorChanged = wxColor(0xFF0000FF);
	wxColor colorUnchanged = wxColor(0xFF004000);
	wxColor colorNormal = wxColor(0xFF600000);

	int startRow;
	GetViewStart(nullptr, &startRow);
	int endRow = startRow + ceil(float(GetClientSize().y) / rowHeight);

	// draw categories
	const int width = GetClientSize().x;
	if (startRow == 0)
	{
		int piece = width / cpu->getRegisterCategoryCount();
		for (int i = 0; i < cpu->getRegisterCategoryCount(); i++)
		{
			const char* name = cpu->getRegisterCategoryName(i);

			int x = i * piece;

			if (i == category)
			{
				dc.SetBrush(wxBrush(wxColor(0xFF70FF70)));
				dc.SetPen(wxPen(wxColor(0xFF000000)));
			}
			else
			{
				dc.SetBrush(wxBrush(wxColor(0xFFFFEFE8)));
				dc.SetPen(wxPen(wxColor(0xFF000000)));
			}

			if (i == cpu->getRegisterCategoryCount() - 1)
				piece += width - piece * cpu->getRegisterCategoryCount() - 1;

			dc.DrawRectangle(x, 0, piece + 1, rowHeight);

			// center text
			x += (piece - strlen(name) * charWidth) / 2; /* Flawfinder: ignore */
			dc.DrawText(wxString(name, wxConvUTF8), x, 2);
		}
	}

	// skip the tab row
	startRow = std::max<int>(0, startRow - 1);
	endRow = std::min<int>(cpu->getRegisterCount(category) - 1, endRow - 1);

	const wxString vu0fTitles[5] = {"W", "Z", "Y", "X"};
	int vu0fTitleCur = 0;
	const int nameStart = 17;
	const int valueStart = startPositions[category];

	ChangedReg* changedRegs = changedCategories[category];
	const int registerBits = cpu->getRegisterSize(category);
	DebugInterface::RegisterType type = cpu->getRegisterType(category);

	for (int i = startRow; i <= endRow; i++)
	{
		int x = valueStart;
		// Skip a row if we are showing our VU0f registers
		// This makes room for the WZYX title
		const int y = rowHeight * (i + (category == EECAT_VU0F ? 2 : 1));

		wxColor backgroundColor;
		if (currentRows[category] == i)
			backgroundColor = wxColor(0xFFFFCFC8);
		else if (i % 2)
			backgroundColor = wxColor(237, 242, 255, 255);
		else
			backgroundColor = wxColor(0xFFFFFFFF);

		dc.SetBrush(backgroundColor);
		dc.SetPen(backgroundColor);
		dc.DrawRectangle(0, y, width, rowHeight);

		const char* name = cpu->getRegisterName(category, i);
		dc.SetTextForeground(colorNormal);
		dc.DrawText(wxString(name, wxConvUTF8), nameStart, y + 2);

		const u128 value = cpu->getRegister(category, i);
		const ChangedReg& changed = changedRegs[i];

		switch (type)
		{
			case DebugInterface::SPECIAL: // let debug interface format them and just display them
			{
				if (changed.changed[0] || changed.changed[1] || changed.changed[2] || changed.changed[3])
					dc.SetTextForeground(colorChanged);
				else
					dc.SetTextForeground(colorUnchanged);

				if (category == EECAT_VU0F)
				{
					if (displayVU0FAsFloat)
					{
						char str[256];
						const u128 val = cpu->getRegister(category, i);
						// Print each register part individually so we know exactly where each part starts
						// and use this information for printing the VU0F titles
						for (int j = 3; j >= 0; j--)
						{
							sprintf(str, "%7.2f", bit_cast<float,u32>(val._u32[j]));
							dc.DrawText(wxString(str), x, y + 2);
							x += charWidth * 8 + 2;
						}

						// Only draw the VU0f titles when we are drawing the first row
						if (i == startRow)
						{
							x = valueStart; // Reset our X back to the starting position

							for (int j = 0; j < 4; j++)
							{
								dc.SetTextForeground(colorNormal);
								dc.DrawText(vu0fTitles[j], (x + charWidth * 4) - 4, rowHeight + 2);
								x += charWidth * 8 + 2;
							}
						}

						break;
					}
				}
				else
				{
					dc.DrawText(StringUtil::UTF8StringToWxString(cpu->getRegisterString(category, i)), x, y + 2);
					break;
				}
			}
				[[fallthrough]];
			case DebugInterface::NORMAL: // display them in 32 bit parts
			{
				switch (registerBits)
				{
					case 128:
					{
						int startIndex = 3;

						if (resolvePointerStrings && cpu->isAlive())
						{
							const char* strval = cpu->stringFromPointer(value._u32[0]);
							if (strval)
							{
								static wxColor clr = wxColor(0xFF228822);
								dc.SetTextForeground(clr);
								dc.DrawText(wxString(strval), width - (32 * charWidth + 12), y + 2);
								startIndex = 0;
							}
						}

						const int actualX = width - 4 - (startIndex + 1) * (8 * charWidth + 2);
						x = std::max<int>(actualX, x);

						if (startIndex != 3)
						{
							bool c = false;
							for (int i = 3; i > startIndex; i--)
								c = c || changed.changed[i];

							if (c)
							{
								dc.SetTextForeground(colorChanged);
								dc.DrawText(L"+", x - charWidth, y + 2);
							}
						}

						for (int j = startIndex; j >= 0; j--)
						{
							if (changed.changed[j])
								dc.SetTextForeground(colorChanged);
							else
								dc.SetTextForeground(colorUnchanged);

							drawU32Text(dc, value._u32[j], x, y + 2);

							// Only draw the VU0f titles when we are drawing the first row and are on the VU0f category
							if (category == EECAT_VU0F && i == startRow)
							{
								dc.SetTextForeground(colorNormal);
								dc.DrawText(vu0fTitles[vu0fTitleCur++], x + (charWidth * 2.5), rowHeight + 2);
							}
							x += charWidth * 8 + 2;
						}

						break;
					}
					case 64:
					{
						for (int j = 1; j >= 0; j--)
						{
							if (changed.changed[j])
								dc.SetTextForeground(colorChanged);
							else
								dc.SetTextForeground(colorUnchanged);

							drawU32Text(dc, value._u32[j], x, y + 2);
							x += charWidth * 8 + 2;
						}

						break;
					}
					case 32:
					{
						if (changed.changed[0])
							dc.SetTextForeground(colorChanged);
						else
							dc.SetTextForeground(colorUnchanged);

						drawU32Text(dc, value._u32[0], x, y + 2);
						break;
					}
				}
				break;
			}
		}
	}
}

void CtrlRegisterList::changeValue(RegisterChangeMode mode)
{
	wchar_t str[64];

	u128 oldValue = cpu->getRegister(category, currentRows[category]);

	switch (mode)
	{
		case LOWER64:
			swprintf(str, 64, L"0x%016llX", oldValue._u64[0]);
			break;
		case UPPER64:
			swprintf(str, 64, L"0x%016llX", oldValue._u64[1]);
			break;
		case CHANGE32:
			swprintf(str, 64, L"0x%08X", oldValue._u32[0]);
			break;
	}

	u64 newValue;
	if (executeExpressionWindow(this, cpu, newValue, str))
	{
		switch (mode)
		{
			case LOWER64:
				oldValue._u64[0] = newValue;
				break;
			case UPPER64:
				oldValue._u64[1] = newValue;
				break;
			case CHANGE32:
				oldValue._u32[0] = newValue;
				break;
		}

		cpu->setRegister(category, currentRows[category], oldValue);
	}
}


void CtrlRegisterList::onPopupClick(wxCommandEvent& evt)
{
	switch (evt.GetId())
	{
		case ID_REGISTERLIST_RESOLVESTRINGS:
			resolvePointerStrings = !resolvePointerStrings;
			postEvent(debEVT_UPDATELAYOUT, 0);
			Refresh();
			break;
		case ID_REGISTERLIST_DISPLAYVU0FFLOATS:
			displayVU0FAsFloat = !displayVU0FAsFloat;
			postEvent(debEVT_UPDATELAYOUT, 0);
			Refresh();
			break;
		case ID_REGISTERLIST_CHANGELOWER:
			changeValue(LOWER64);
			Refresh();
			break;
		case ID_REGISTERLIST_CHANGEUPPER:
			changeValue(UPPER64);
			Refresh();
			break;
		case ID_REGISTERLIST_CHANGEVALUE:
			if (cpu->getRegisterSize(category) == 32)
				changeValue(CHANGE32);
			else
				changeValue(LOWER64);
			Refresh();
			break;
		case ID_REGISTERLIST_GOTOINMEMORYVIEW:
			postEvent(debEVT_GOTOINMEMORYVIEW, cpu->getRegister(category, currentRows[category])._u32[0]);
			break;
		case ID_REGISTERLIST_GOTOINDISASM:
			postEvent(debEVT_GOTOINDISASM, cpu->getRegister(category, currentRows[category])._u32[0]);
			break;
		default:
			wxMessageBox(L"Unimplemented.", L"Unimplemented.", wxICON_INFORMATION);
			break;
	}
}

void CtrlRegisterList::setCurrentRow(int row)
{
	char str[256];
	u128 value;
	std::string text;

	const char* name = cpu->getRegisterName(category, row);

	switch (cpu->getRegisterType(category))
	{
		case DebugInterface::NORMAL:
			value = cpu->getRegister(category, row);
			postEvent(debEVT_REFERENCEMEMORYVIEW, value._u32[0]);

			switch (cpu->getRegisterSize(category))
			{
				case 128:
					sprintf(str, "%s = 0x%08X%08X%08X%08X", name, value._u32[3], value._u32[2], value._u32[1], value._u32[0]);
					break;
				case 64:
					sprintf(str, "%s = 0x%08X%08X", name, value._u32[1], value._u32[0]);
					break;
				case 32:
					sprintf(str, "%s = 0x%08X", name, value._u32[0]);
					break;
			}
			text = str;
			break;
		case DebugInterface::SPECIAL:
			text = std::string(name) + " = " + cpu->getRegisterString(category, row);
			break;
	}

	currentRows[category] = row;
	postEvent(debEVT_SETSTATUSBARTEXT, StringUtil::UTF8StringToWxString(text));
	ensureVisible(currentRows[category] + 1); //offset due to header at scroll position 0
	Refresh();
}

void CtrlRegisterList::mouseEvent(wxMouseEvent& evt)
{
	int xOffset, yOffset;
	((wxScrolledWindow*)this)->GetViewStart(&xOffset, &yOffset);

	wxClientDC dc(this);
	wxPoint pos = evt.GetPosition();
	const int x = dc.DeviceToLogicalX(pos.x) + xOffset;
	const int y = dc.DeviceToLogicalY(pos.y) + yOffset * rowHeight;

	if (evt.GetEventType() == wxEVT_RIGHT_UP)
	{
		if (y >= rowHeight)
		{
			// The ternary here is due to the fact that VU0f has an extra row for the w z y x title
			const int row = (y - rowHeight * (category == EECAT_VU0F ? 2 : 1)) / rowHeight;
			if (row != currentRows[category] && row < cpu->getRegisterCount(category))
				setCurrentRow(row);
		}

		wxMenu menu;


		if (category == EECAT_VU0F)
		{
			menu.AppendCheckItem(ID_REGISTERLIST_DISPLAYVU0FFLOATS, L"Display VU0f registers as floats")->Check(displayVU0FAsFloat);
		}
		else
		{
			menu.AppendCheckItem(ID_REGISTERLIST_RESOLVESTRINGS, L"Resolve string pointers")->Check(resolvePointerStrings);
		}

		menu.AppendSeparator();

		const int bits = cpu->getRegisterSize(category);
		if (bits >= 64)
		{
			menu.Append(ID_REGISTERLIST_CHANGELOWER, L"Change lower 64 bit");
			menu.Append(ID_REGISTERLIST_CHANGEUPPER, L"Change upper 64 bit");
		}
		else
		{
			menu.Append(ID_REGISTERLIST_CHANGEVALUE, L"Change value");
		}

		menu.AppendSeparator();
		menu.Append(ID_REGISTERLIST_GOTOINMEMORYVIEW, L"Follow in Memory view");
		menu.Append(ID_REGISTERLIST_GOTOINDISASM, L"Follow in Disasm");

		menu.Bind(wxEVT_MENU, &CtrlRegisterList::onPopupClick, this);
		PopupMenu(&menu, evt.GetPosition());
		return;
	}

	if (evt.ButtonIsDown(wxMOUSE_BTN_LEFT) || evt.ButtonIsDown(wxMOUSE_BTN_RIGHT))
	{
		if (y < rowHeight)
		{
			const int piece = GetSize().x / cpu->getRegisterCategoryCount();
			const int cat = std::min<int>(x / piece, cpu->getRegisterCategoryCount() - 1);

			if (cat != category)
			{
				category = cat;
				Refresh();
			}
		}
		else
		{
			// The ternary here is due to the fact that VU0f has an extra row for the w z y x title
			const int row = (y - rowHeight * (category == EECAT_VU0F ? 2 : 1)) / rowHeight;
			if (row != currentRows[category] && row < cpu->getRegisterCount(category))
				setCurrentRow(row);
		}

		SetFocus();
		SetFocusFromKbd();
	}
}

void CtrlRegisterList::ensureVisible(int index)
{
	//scroll vertically to keep a logical position visible
	int x, y;
	GetViewStart(&x, &y);
	const int visibleOffset = floor(float(GetClientSize().y) / rowHeight) - 1;
	if (index < y)
		Scroll(x, index);
	else if (index > y + visibleOffset)
		Scroll(x, index - visibleOffset);
}

void CtrlRegisterList::keydownEvent(wxKeyEvent& evt)
{
	switch (evt.GetKeyCode())
	{
		case WXK_UP:
			int x, y;
			GetViewStart(&x, &y);
			//If at top of rows allow scrolling an extra time to show tab header
			if (currentRows[category] == 0 && y == 1)
				Scroll(x, 0);
			else
				setCurrentRow(std::max<int>(currentRows[category] - 1, 0));
			break;
		case WXK_DOWN:
			setCurrentRow(std::min<int>(currentRows[category] + 1, cpu->getRegisterCount(category) - 1));
			break;
		case WXK_TAB:
			category = (category + 1) % cpu->getRegisterCategoryCount();
			Refresh();
			break;
	}
}
