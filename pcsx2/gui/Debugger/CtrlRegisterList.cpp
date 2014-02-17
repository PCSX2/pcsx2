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
	displayBits = 128;

	SetDoubleBuffered(true);
	SetInitialBestSize(ClientToWindowSize(GetMinClientSize()));
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

	int nameStart = 17;
	int valueStart = 77;
	for (int i = 0; i < 32; i++)
	{
		int x = valueStart;
		int y = 2+i*rowHeight;

		const char* name = cpu->getRegName(i);
		dc.DrawText(wxString(name,wxConvUTF8),nameStart,y);

		u128 value = cpu->getGPR(i);
		if (displayBits >= 128)
		{
			drawU32Text(dc,value._u32[3],x,y);
			x += charWidth*8+2;
		}
		if (displayBits >= 96)
		{
			drawU32Text(dc,value._u32[2],x,y);
			x += charWidth*8+2;
		}
		if (displayBits >= 64)
		{
			drawU32Text(dc,value._u32[1],x,y);
			x += charWidth*8+2;
		}

		drawU32Text(dc,value._u32[0],x,y);
		x += charWidth*8+2;
	}
}