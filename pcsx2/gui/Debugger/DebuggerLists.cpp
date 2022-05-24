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
#include "DebuggerLists.h"
#include "BreakpointWindow.h"
#include "DebugEvents.h"
#include "pcsx2/gui/StringHelpers.h"

wxBEGIN_EVENT_TABLE(GenericListView, wxWindow)
	EVT_SIZE(GenericListView::sizeEvent)
	EVT_KEY_DOWN(GenericListView::keydownEvent)
	EVT_RIGHT_DOWN(GenericListView::mouseEvent)
	EVT_RIGHT_UP(GenericListView::mouseEvent)
	EVT_LEFT_DCLICK(GenericListView::mouseEvent)
	EVT_LIST_ITEM_RIGHT_CLICK(wxID_ANY, GenericListView::listEvent)
wxEND_EVENT_TABLE()

GenericListView::GenericListView(wxWindow* parent, GenericListViewColumn* columns, int columnCount)
	: wxListView(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_VIRTUAL | wxLC_REPORT | wxLC_SINGLE_SEL | wxNO_BORDER)
{
	m_isInResizeColumn = false;
	dontResizeColumnsInSizeEventHandler = false;

	insertColumns(columns, columnCount);
}

void GenericListView::insertColumns(GenericListViewColumn* columns, int count)
{
	int totalWidth = GetSize().x;

	for (int i = 0; i < count; i++)
	{
		wxListItem column;
		column.SetText(columns[i].name);
		column.SetWidth(totalWidth * columns[i].size);

		InsertColumn(i, column);
	}

	this->columns = columns;
}

void GenericListView::resizeColumn(int col, int width)
{
	if (!m_isInResizeColumn)
	{
		m_isInResizeColumn = true;

		SetColumnWidth(col, width);

		m_isInResizeColumn = false;
	}
}

void GenericListView::resizeColumns(int totalWidth)
{
	for (int i = 0; i < GetColumnCount(); i++)
	{
		resizeColumn(i, totalWidth * columns[i].size);
	}
}

void GenericListView::sizeEvent(wxSizeEvent& evt)
{
	// HACK: On Windows, it seems that if you resize the columns in the size
	// event handler when the scrollbar disappears, the listview contents may
	// decide to disappear as well. So let's avoid the resize for this case.
	if (!dontResizeColumnsInSizeEventHandler)
		resizeColumns(GetClientSize().x);
	dontResizeColumnsInSizeEventHandler = false;
	evt.Skip();
}

void GenericListView::keydownEvent(wxKeyEvent& evt)
{
	int sel = GetFirstSelected();
	switch (evt.GetKeyCode())
	{
		case WXK_UP:
			if (sel > 0)
			{
				Select(sel - 1);
				Focus(sel - 1);
			}
			break;
		case WXK_DOWN:
			if (sel + 1 < GetItemCount())
			{
				Select(sel + 1);
				Focus(sel + 1);
			}
			break;
	}

	onKeyDown(evt.GetKeyCode());
}

void GenericListView::update()
{
	int newRows = getRowCount();
	int oldRows = GetItemCount();

	SetItemCount(newRows);

	if (newRows != oldRows)
	{
		resizeColumns(GetClientSize().x);

		// wx adds the horizontal scrollbar based on the old column width,
		// which changes the client width. Simply resizing the columns won't
		// make the scrollbar go away, so let's make it recalculate if it needs it
		SetItemCount(newRows);
	}
	dontResizeColumnsInSizeEventHandler = true;
	Refresh();
}

wxString GenericListView::OnGetItemText(long item, long col) const
{
	return getColumnText(item, col);
}

void GenericListView::postEvent(wxEventType type, int value)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetInt(value);
	wxPostEvent(this, event);
}

void GenericListView::mouseEvent(wxMouseEvent& evt)
{
	wxEventType type = evt.GetEventType();

	if (type == wxEVT_RIGHT_DOWN)
	{
		clickPos = evt.GetPosition();
		evt.Skip();
	}
	else if (type == wxEVT_RIGHT_UP)
	{
		onRightClick(GetFirstSelected(), evt.GetPosition());
	}
	else if (type == wxEVT_LEFT_DCLICK)
	{
		onDoubleClick(GetFirstSelected(), evt.GetPosition());
	}
}

void GenericListView::listEvent(wxListEvent& evt)
{
	onRightClick(GetFirstSelected(), clickPos);
}

//
// BreakpointList
//

enum
{
	BPL_TYPE,
	BPL_OFFSET,
	BPL_SIZELABEL,
	BPL_OPCODE,
	BPL_CONDITION,
	BPL_HITS,
	BPL_ENABLED,
	BPL_COLUMNCOUNT
};

enum BreakpointListMenuIdentifiers
{
	ID_BREAKPOINTLIST_ENABLE = 1,
	ID_BREAKPOINTLIST_EDIT,
	ID_BREAKPOINTLIST_DELETE,
	ID_BREAKPOINTLIST_ADDNEW,
};

GenericListViewColumn breakpointColumns[BPL_COLUMNCOUNT] = {
	{L"Type", 0.12f},
	{L"Offset", 0.12f},
	{L"Size/Label", 0.18f},
	{L"Opcode", 0.28f},
	{L"Condition", 0.17f},
	{L"Hits", 0.05f},
	{L"Enabled", 0.08f}};

BreakpointList::BreakpointList(wxWindow* parent, DebugInterface* _cpu, CtrlDisassemblyView* _disassembly)
	: GenericListView(parent, breakpointColumns, BPL_COLUMNCOUNT)
	, cpu(_cpu)
	, disasm(_disassembly)
{
}

int BreakpointList::getRowCount()
{
	int count = 0;
	for (size_t i = 0; i < CBreakPoints::GetMemChecks().size(); i++)
	{
		if (displayedMemChecks_[i].cpu == cpu->getCpuType())
			count++;
	}
	for (size_t i = 0; i < CBreakPoints::GetBreakpoints().size(); i++)
	{
		if (!displayedBreakPoints_[i].temporary && displayedBreakPoints_[i].cpu == cpu->getCpuType())
			count++;
	}

	return count;
}

wxString BreakpointList::getColumnText(int item, int col) const
{
	FastFormatUnicode dest;
	bool isMemory;
	int index = getBreakpointIndex(item, isMemory);
	if (index == -1)
		return L"Invalid";

	switch (col)
	{
		case BPL_TYPE:
		{
			if (isMemory)
			{
				switch ((int)displayedMemChecks_[index].cond)
				{
					case MEMCHECK_READ:
						dest.Write("Read");
						break;
					case MEMCHECK_WRITE:
						dest.Write("Write");
						break;
					case MEMCHECK_READWRITE:
						dest.Write("Read/Write");
						break;
					case MEMCHECK_WRITE | MEMCHECK_WRITE_ONCHANGE:
						dest.Write("Write Change");
						break;
					case MEMCHECK_READWRITE | MEMCHECK_WRITE_ONCHANGE:
						dest.Write("Read/Write Change");
						break;
				}
			}
			else
			{
				dest.Write(L"Execute");
			}
		}
		break;
		case BPL_OFFSET:
		{
			if (isMemory)
			{
				dest.Write("0x%08X", displayedMemChecks_[index].start);
			}
			else
			{
				dest.Write("0x%08X", displayedBreakPoints_[index].addr);
			}
		}
		break;
		case BPL_SIZELABEL:
		{
			if (isMemory)
			{
				auto mc = displayedMemChecks_[index];
				if (mc.end == 0)
					dest.Write("0x%08X", 1);
				else
					dest.Write("0x%08X", mc.end - mc.start);
			}
			else
			{
				const std::string sym = cpu->GetSymbolMap().GetLabelString(displayedBreakPoints_[index].addr);
				if (!sym.empty())
				{
					dest.Write("%s", sym.c_str());
				}
				else
				{
					dest.Write("-");
				}
			}
		}
		break;
		case BPL_OPCODE:
		{
			if (isMemory)
			{
				dest.Write(L"-");
			}
			else
			{
				if (!cpu->isAlive())
					break;
				char temp[256];
				disasm->getOpcodeText(displayedBreakPoints_[index].addr, temp);
				dest.Write("%s", temp);
			}
		}
		break;
		case BPL_CONDITION:
		{
			if (isMemory || !displayedBreakPoints_[index].hasCond)
			{
				dest.Write("-");
			}
			else
			{
				dest.Write("%s", displayedBreakPoints_[index].cond.expressionString);
			}
		}
		break;
		case BPL_HITS:
		{
			if (isMemory)
			{
				dest.Write("%d", displayedMemChecks_[index].numHits);
			}
			else
			{
				dest.Write("-");
			}
		}
		break;
		case BPL_ENABLED:
		{
			if (isMemory)
			{
				dest.Write("%s", displayedMemChecks_[index].result & MEMCHECK_BREAK ? "true" : "false");
			}
			else
			{
				dest.Write("%s", displayedBreakPoints_[index].enabled ? "true" : "false");
			}
		}
		break;
		default:
			return L"Invalid";
	}

	return dest;
}

void BreakpointList::onKeyDown(int key)
{
	int sel = GetFirstSelected();
	switch (key)
	{
		case WXK_DELETE:
			removeBreakpoint(sel);
			break;
		case WXK_RETURN:
			editBreakpoint(sel);
			break;
		case WXK_SPACE:
			toggleEnabled(sel);
			break;
	}
}

int BreakpointList::getBreakpointIndex(int itemIndex, bool& isMemory) const
{
	// memory breakpoints first
	for (size_t i = 0; i < displayedMemChecks_.size(); i++)
	{
		if (displayedMemChecks_[i].cpu != cpu->getCpuType())
			continue;
		if (itemIndex == 0)
		{
			isMemory = true;
			return (int)i;
		}
		itemIndex--;
	}

	size_t i = 0;
	while (i < displayedBreakPoints_.size())
	{
		if (displayedBreakPoints_[i].temporary || displayedBreakPoints_[i].cpu != cpu->getCpuType())
		{
			i++;
			continue;
		}

		// the index is 0 when there are no more breakpoints to skip
		if (itemIndex == 0)
		{
			isMemory = false;
			return (int)i;
		}

		i++;
		itemIndex--;
	}

	return -1;
}

void BreakpointList::reloadBreakpoints()
{
	// Update the items we're displaying from the debugger.
	displayedBreakPoints_ = CBreakPoints::GetBreakpoints();
	displayedMemChecks_ = CBreakPoints::GetMemChecks();
	update();
}

void BreakpointList::editBreakpoint(int itemIndex)
{
	bool isMemory;
	int index = getBreakpointIndex(itemIndex, isMemory);
	if (index == -1)
		return;

	BreakpointWindow win(this, cpu);
	if (isMemory)
	{
		auto mem = displayedMemChecks_[index];
		win.loadFromMemcheck(mem);
		if (win.ShowModal() == wxID_OK)
		{
			CBreakPoints::RemoveMemCheck(cpu->getCpuType(), mem.start, mem.end);
			win.addBreakpoint();
		}
	}
	else
	{
		auto bp = displayedBreakPoints_[index];
		win.loadFromBreakpoint(bp);
		if (win.ShowModal() == wxID_OK)
		{
			CBreakPoints::RemoveBreakPoint(cpu->getCpuType(), bp.addr);
			win.addBreakpoint();
		}
	}
}

void BreakpointList::toggleEnabled(int itemIndex)
{
	bool isMemory;
	int index = getBreakpointIndex(itemIndex, isMemory);
	if (index == -1)
		return;

	if (isMemory)
	{
		MemCheck mcPrev = displayedMemChecks_[index];
		CBreakPoints::ChangeMemCheck(cpu->getCpuType(), mcPrev.start, mcPrev.end, mcPrev.cond, MemCheckResult(mcPrev.result ^ MEMCHECK_BREAK));
	}
	else
	{
		BreakPoint bpPrev = displayedBreakPoints_[index];
		CBreakPoints::ChangeBreakPoint(cpu->getCpuType(), bpPrev.addr, !bpPrev.enabled);
	}
}

void BreakpointList::gotoBreakpointAddress(int itemIndex)
{
	bool isMemory;
	int index = getBreakpointIndex(itemIndex, isMemory);
	if (index == -1)
		return;

	if (isMemory)
	{
		u32 address = displayedMemChecks_[index].start;
		postEvent(debEVT_GOTOINMEMORYVIEW, address);
	}
	else
	{
		u32 address = displayedBreakPoints_[index].addr;
		postEvent(debEVT_GOTOINDISASM, address);
	}
}

void BreakpointList::removeBreakpoint(int itemIndex)
{
	bool isMemory;
	int index = getBreakpointIndex(itemIndex, isMemory);
	if (index == -1)
		return;

	if (isMemory)
	{
		auto mc = displayedMemChecks_[index];
		CBreakPoints::RemoveMemCheck(cpu->getCpuType(), mc.start, mc.end);
	}
	else
	{
		u32 address = displayedBreakPoints_[index].addr;
		CBreakPoints::RemoveBreakPoint(displayedBreakPoints_[index].cpu, address);
	}
}

void BreakpointList::onPopupClick(wxCommandEvent& evt)
{
	int index = GetFirstSelected();
	switch (evt.GetId())
	{
		case ID_BREAKPOINTLIST_ENABLE:
			toggleEnabled(index);
			break;
		case ID_BREAKPOINTLIST_EDIT:
			editBreakpoint(index);
			break;
		case ID_BREAKPOINTLIST_DELETE:
			removeBreakpoint(index);
			break;
		case ID_BREAKPOINTLIST_ADDNEW:
			postEvent(debEVT_BREAKPOINTWINDOW, 0);
			break;
		default:
			wxMessageBox(L"Unimplemented.", L"Unimplemented.", wxICON_INFORMATION);
			break;
	}
}

void BreakpointList::showMenu(const wxPoint& pos)
{
	bool isMemory;
	int index = getBreakpointIndex(GetFirstSelected(), isMemory);

	wxMenu menu;
	if (index != -1)
	{
		menu.AppendCheckItem(ID_BREAKPOINTLIST_ENABLE, L"Enable");
		menu.Append(ID_BREAKPOINTLIST_EDIT, L"Edit");
		menu.Append(ID_BREAKPOINTLIST_DELETE, L"Delete");
		menu.AppendSeparator();

		// check if the breakpoint is enabled
		bool enabled;
		if (isMemory)
			enabled = (displayedMemChecks_[index].result & MEMCHECK_BREAK) != 0;
		else
			enabled = displayedBreakPoints_[index].enabled;

		menu.Check(ID_BREAKPOINTLIST_ENABLE, enabled);
	}

	menu.Append(ID_BREAKPOINTLIST_ADDNEW, L"Add new");

	menu.Bind(wxEVT_MENU, &BreakpointList::onPopupClick, this);
	PopupMenu(&menu, pos);
}

void BreakpointList::onRightClick(int itemIndex, const wxPoint& point)
{
	showMenu(point);
}

void BreakpointList::onDoubleClick(int itemIndex, const wxPoint& point)
{
	gotoBreakpointAddress(itemIndex);
}


//
// ThreadList
//

enum
{
	TL_TID,
	TL_PROGRAMCOUNTER,
	TL_ENTRYPOINT,
	TL_PRIORITY,
	TL_STATE,
	TL_WAITTYPE,
	TL_COLUMNCOUNT
};

GenericListViewColumn threadColumns[TL_COLUMNCOUNT] = {
	{L"TID", 0.08f},
	{L"PC", 0.21f},
	{L"Entry Point", 0.21f},
	{L"Priority", 0.08f},
	{L"State", 0.21f},
	{L"Wait type", 0.21f},
};

ThreadList::ThreadList(wxWindow* parent, DebugInterface* _cpu)
	: GenericListView(parent, threadColumns, TL_COLUMNCOUNT)
	, cpu(_cpu)
{
}

void ThreadList::reloadThreads()
{
	threads = getEEThreads();
	update();
}


int ThreadList::getRowCount()
{
	return threads.size();
}

wxString ThreadList::getColumnText(int item, int col) const
{
	if (item < 0 || item >= (int)threads.size())
		return L"";

	FastFormatUnicode dest;
	const EEThread& thread = threads[item];

	switch (col)
	{
		case TL_TID:
			dest.Write("%d", thread.tid);
			break;
		case TL_PROGRAMCOUNTER:
			if (thread.data.status == THS_RUN)
				dest.Write("0x%08X", cpu->getPC());
			else
				dest.Write("0x%08X", thread.data.entry);
			break;
		case TL_ENTRYPOINT:
			dest.Write("0x%08X", thread.data.entry_init);
			break;
		case TL_PRIORITY:
			dest.Write("0x%02X", thread.data.currentPriority);
			break;
		case TL_STATE:
			switch (thread.data.status)
			{
				case THS_BAD:
					dest.Write("Bad");
					break;
				case THS_RUN:
					dest.Write("Running");
					break;
				case THS_READY:
					dest.Write("Ready");
					break;
				case THS_WAIT:
					dest.Write("Waiting");
					break;
				case THS_SUSPEND:
					dest.Write("Suspended");
					break;
				case THS_WAIT_SUSPEND:
					dest.Write("Waiting/Suspended");
					break;
				case THS_DORMANT:
					dest.Write("Dormant");
					break;
			}
			break;
		case TL_WAITTYPE:
			switch (thread.data.waitType)
			{
				case WAIT_NONE:
					dest.Write("None");
					break;
				case WAIT_WAKEUP_REQ:
					dest.Write("Wakeup request");
					break;
				case WAIT_SEMA:
					dest.Write("Semaphore");
					break;
			}
			break;
		default:
			return L"Invalid";
	}

	return dest;
}

void ThreadList::onDoubleClick(int itemIndex, const wxPoint& point)
{
	if (itemIndex < 0 || itemIndex >= (int)threads.size())
		return;

	const EEThread& thread = threads[itemIndex];

	switch (thread.data.status)
	{
		case THS_DORMANT:
		case THS_BAD:
			postEvent(debEVT_GOTOINDISASM, thread.data.entry_init);
			break;
		case THS_RUN:
			postEvent(debEVT_GOTOINDISASM, cpu->getPC());
			break;
		default:
			postEvent(debEVT_GOTOINDISASM, thread.data.entry);
			break;
	}
}

EEThread ThreadList::getRunningThread()
{
	for (size_t i = 0; i < threads.size(); i++)
	{
		if (threads[i].data.status == THS_RUN)
			return threads[i];
	}

	EEThread thread;
	memset(&thread, 0, sizeof(thread));
	thread.tid = -1;
	return thread;
}

//
// StackFramesList
//

enum
{
	SF_ENTRY,
	SF_ENTRYNAME,
	SF_CURPC,
	SF_CUROPCODE,
	SF_CURSP,
	SF_FRAMESIZE,
	SF_COLUMNCOUNT
};

GenericListViewColumn stackFrameolumns[SF_COLUMNCOUNT] = {
	{L"Entry", 0.12f},
	{L"Name", 0.24f},
	{L"PC", 0.12f},
	{L"Opcode", 0.28f},
	{L"SP", 0.12f},
	{L"Frame Size", 0.12f}};

StackFramesList::StackFramesList(wxWindow* parent, DebugInterface* _cpu, CtrlDisassemblyView* _disassembly)
	: GenericListView(parent, stackFrameolumns, SF_COLUMNCOUNT)
	, cpu(_cpu)
	, disassembly(_disassembly)
{
}

void StackFramesList::loadStackFrames(EEThread& currentThread)
{
	frames = MipsStackWalk::Walk(cpu, cpu->getPC(), cpu->getRegister(0, 31), cpu->getRegister(0, 29),
		currentThread.data.entry_init, currentThread.data.stack);
	update();
}

int StackFramesList::getRowCount()
{
	return frames.size();
}

wxString StackFramesList::getColumnText(int item, int col) const
{
	if (item < 0 || item >= (int)frames.size())
		return L"";

	FastFormatUnicode dest;
	const MipsStackWalk::StackFrame& frame = frames[item];

	switch (col)
	{
		case SF_ENTRY:
			dest.Write("0x%08X", frame.entry);
			break;
		case SF_ENTRYNAME:
		{
			const std::string sym = cpu->GetSymbolMap().GetLabelString(frame.entry);
			if (!sym.empty())
			{
				dest.Write("%s", sym.c_str());
			}
			else
			{
				dest.Write("-");
			}
		}
		break;
		case SF_CURPC:
			dest.Write("0x%08X", frame.pc);
			break;
		case SF_CUROPCODE:
		{
			if (!cpu->isAlive())
				break;
			char temp[512];
			disassembly->getOpcodeText(frame.pc, temp);
			dest.Write("%s", temp);
		}
		break;
		case SF_CURSP:
			dest.Write("0x%08X", frame.sp);
			break;
		case SF_FRAMESIZE:
			dest.Write("0x%08X", frame.stackSize);
			break;
		default:
			return L"Invalid";
	}

	return dest;
}

void StackFramesList::onDoubleClick(int itemIndex, const wxPoint& point)
{
	if (itemIndex < 0 || itemIndex >= (int)frames.size())
		return;

	postEvent(debEVT_GOTOINDISASM, frames[itemIndex].pc);
}
