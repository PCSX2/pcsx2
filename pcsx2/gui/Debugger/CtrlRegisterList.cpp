#include "PrecompiledHeader.h"
#include "CtrlRegisterList.h"
#include "DebugTools/Debug.h"

#include "DebugEvents.h"

BEGIN_EVENT_TABLE(CtrlRegisterList, wxWindow)
	EVT_PAINT(CtrlRegisterList::paintEvent)
END_EVENT_TABLE()

CtrlRegisterList::CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu)
	: wxWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxWANTS_CHARS|wxBORDER), cpu(_cpu)
{
	rowHeight = 14;
	charWidth = 8;
	currentRow = 0;
	category = 0;

	for (int i = 0; i < cpu->getRegisterCategoryCount(); i++)
	{
		int count = cpu->getRegisterCount(i);

		ChangedReg* regs = new ChangedReg[count];
		memset(regs,0,sizeof(ChangedReg)*count);
		changedCategories.push_back(regs);
	}

	SetDoubleBuffered(true);
	SetInitialBestSize(ClientToWindowSize(GetMinClientSize()));
}

void CtrlRegisterList::refreshChangedRegs()
{
	if (cpu->getPC() == lastPc)
		return;

	for (size_t cat = 0; cat < changedCategories.size(); cat++)
	{
		ChangedReg* regs = changedCategories[cat];
		int size = cpu->getRegisterSize(category);

		for (int i = 0; i < cpu->getRegisterCount(cat); i++)
		{
			ChangedReg& reg = regs[i];
			memset(&reg.changed,0,sizeof(reg.changed));

			u128 newValue = cpu->getRegister(cat,i);

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

void CtrlRegisterList::paintEvent(wxPaintEvent & evt)
{
	wxPaintDC dc(this);
	render(dc);
}

void CtrlRegisterList::redraw()
{
	wxClientDC dc(this);
	render(dc);
}

void drawU32Text(wxDC& dc, u32 value, int x, int y)
{
	char str[32];
	sprintf(str,"%08X",value);
	dc.DrawText(wxString(str,wxConvUTF8),x,y);
}

void CtrlRegisterList::render(wxDC& dc)
{
	wxFont font = wxFont(wxSize(charWidth,rowHeight-2),wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL,false,L"Lucida Console");
	dc.SetFont(font);
	
	// clear background
	wxColor white = wxColor(0xFFFFFFFF);

	dc.SetBrush(wxBrush(white));
	dc.SetPen(wxPen(white));

	wxSize size = GetSize();
	dc.DrawRectangle(0,0,size.x,size.y);

	refreshChangedRegs();

	wxColor colorChanged = wxColor(0xFF0000FF);
	wxColor colorUnchanged = wxColor(0xFF004000);
	wxColor colorNormal = wxColor(0xFF600000);

	int nameStart = 17;
	int valueStart = 77;

	ChangedReg* changedRegs = changedCategories[category];
	int displayBits = cpu->getRegisterSize(category);
	DebugInterface::RegisterType type = cpu->getRegisterType(category);

	for (int i = 0; i < cpu->getRegisterCount(category); i++)
	{
		int x = valueStart;
		int y = 2+i*rowHeight;

		const char* name = cpu->getRegisterName(category,i);
		dc.SetTextForeground(colorNormal);
		dc.DrawText(wxString(name,wxConvUTF8),nameStart,y);

		u128 value = cpu->getRegister(category,i);
		ChangedReg& changed = changedRegs[i];

		switch (type)
		{
		case DebugInterface::NORMAL:	// display them in 32 bit parts
			switch (displayBits)
			{
			case 128:
				{
					for (int i = 3; i >= 0; i--)
					{
						if (changed.changed[i])
							dc.SetTextForeground(colorChanged);
						else
							dc.SetTextForeground(colorUnchanged);

						drawU32Text(dc,value._u32[i],x,y);
						x += charWidth*8+2;
					}
					break;
				}
			case 64:
				{
					for (int i = 1; i >= 0; i--)
					{
						if (changed.changed[i])
							dc.SetTextForeground(colorChanged);
						else
							dc.SetTextForeground(colorUnchanged);

						drawU32Text(dc,value._u32[i],x,y);
						x += charWidth*8+2;
					}
					break;
				}
			case 32:
				{
					if (changed.changed[0])
						dc.SetTextForeground(colorChanged);
					else
						dc.SetTextForeground(colorUnchanged);

					switch (type)
					{
					case DebugInterface::NORMAL:
						drawU32Text(dc,value._u32[0],x,y);
						break;
					}
					break;
				}
			}
			break;
		case DebugInterface::SPECIAL:		// let debug interface format them and just display them
			{
				if (changed.changed[0] || changed.changed[1] || changed.changed[2] || changed.changed[3])
					dc.SetTextForeground(colorChanged);
				else
					dc.SetTextForeground(colorUnchanged);

				dc.DrawText(cpu->getRegisterString(category,i),x,y);
				break;
			}
		}
	}
}