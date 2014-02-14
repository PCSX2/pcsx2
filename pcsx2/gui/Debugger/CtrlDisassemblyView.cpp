#include "PrecompiledHeader.h"
#include "CtrlDisassemblyView.h"

BEGIN_EVENT_TABLE(CtrlDisassemblyView, wxWindow)
	EVT_PAINT(CtrlDisassemblyView::paintEvent)
	EVT_MOUSEWHEEL(CtrlDisassemblyView::mouseEvent)
	EVT_LEFT_DOWN(CtrlDisassemblyView::mouseEvent)
	EVT_MOTION(CtrlDisassemblyView::mouseEvent)
	EVT_KEY_DOWN(CtrlDisassemblyView::keydownEvent)
	EVT_SCROLLWIN_LINEUP(CtrlDisassemblyView::scrollbarEvent)
	EVT_SCROLLWIN_LINEDOWN(CtrlDisassemblyView::scrollbarEvent)
	EVT_SCROLLWIN_PAGEUP(CtrlDisassemblyView::scrollbarEvent)
	EVT_SCROLLWIN_PAGEDOWN(CtrlDisassemblyView::scrollbarEvent)
END_EVENT_TABLE()

CtrlDisassemblyView::CtrlDisassemblyView(wxWindow* parent, DebugInterface* _cpu)
	: wxWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxWANTS_CHARS), cpu(_cpu)
{
	manager.setCpu(cpu);
	windowStart = 0x20100000;
	curAddress = windowStart;
	rowHeight = 16;
	charWidth = 10;
	displaySymbols = true;

	SetScrollbar(wxVERTICAL,100,1,201,true);
	SetDoubleBuffered(true);
	calculatePixelPositions();
}

void CtrlDisassemblyView::paintEvent(wxPaintEvent & evt)
{
	wxPaintDC dc(this);
	render(dc);
}

void CtrlDisassemblyView::redraw()
{
	wxClientDC dc(this);
	render(dc);
}

bool CtrlDisassemblyView::getDisasmAddressText(u32 address, char* dest, bool abbreviateLabels, bool showData)
{
	if (displaySymbols)
	{
		const std::string addressSymbol = symbolMap.GetLabelString(address);
		if (!addressSymbol.empty())
		{
			for (int k = 0; addressSymbol[k] != 0; k++)
			{
				// abbreviate long names
				if (abbreviateLabels && k == 16 && addressSymbol[k+1] != 0)
				{
					*dest++ = '+';
					break;
				}
				*dest++ = addressSymbol[k];
			}
			*dest++ = ':';
			*dest = 0;
			return true;
		} else {
			sprintf(dest,"    %08X",address);
			return false;
		}
	} else {
		if (showData)
			sprintf(dest,"%08X %08X",address,debug.read32(address));
		else
			sprintf(dest,"%08X",address);
		return false;
	}
}

wxColor scaleColor(wxColor color, float factor)
{
	unsigned char r = color.Red();
	unsigned char g = color.Green();
	unsigned char b = color.Blue();
	unsigned char a = color.Alpha();

	r = min(255,max((int)(r*factor),0));
	g = min(255,max((int)(g*factor),0));
	b = min(255,max((int)(b*factor),0));

	return wxColor(r,g,b,a);
}

void CtrlDisassemblyView::render(wxDC& dc)
{
	// init stuff
	int totalWidth, totalHeight;
	GetSize(&totalWidth,&totalHeight);
	visibleRows = totalHeight / rowHeight;

	// clear background
	wxColor white = wxColor(0xFFFFFFFF);

	dc.SetBrush(wxBrush(white));
	dc.SetPen(wxPen(white));

	int width,height;
	dc.GetSize(&width,&height);
	dc.DrawRectangle(0,0,width,height);

	if (!cpu->isRunning())
		return;

	wxFont font = wxFont(wxSize(charWidth,rowHeight-2),wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL,false,L"Lucida Console");
	wxFont boldFont = wxFont(wxSize(charWidth,rowHeight-2),wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD,false,L"Lucida Console");

	bool hasFocus = true;//wxWindow::FindFocus() == this;

	unsigned int address = windowStart;
	DisassemblyLineInfo line;
	for (int i = 0; i < visibleRows; i++)
	{
		manager.getLine(address,false,line);
		
		int rowY1 = rowHeight*i;
		int rowY2 = rowHeight*(i+1);
		
		wxColor backgroundColor = wxColor(0xFFFFFFFF);
		wxColor textColor = wxColor(0xFF000000);
		
		if (isInInterval(address,line.totalSize,cpu->getPC()))
		{
			backgroundColor = scaleColor(backgroundColor,1.05f);
		}

		if (address >= selectRangeStart && address < selectRangeEnd)
		{
			if (hasFocus)
			{
				backgroundColor = address == curAddress ? 0xFFFF8822 : 0xFFFF9933;
				textColor = 0xFFFFFFFF;
			} else {
				backgroundColor = 0xFFC0C0C0;
			}
		}

		// draw background
		dc.SetBrush(wxBrush(backgroundColor));
		dc.SetPen(wxPen(backgroundColor));
		dc.DrawRectangle(0,rowY1,totalWidth,rowHeight);
		
		dc.SetTextForeground(textColor);

		char addressText[64];
		getDisasmAddressText(address,addressText,true,line.type == DISTYPE_OPCODE);

		dc.SetFont(font);
		dc.DrawText(wxString(addressText,wxConvUTF8),pixelPositions.addressStart,rowY1+1);
		dc.DrawText(wxString(line.params.c_str(),wxConvUTF8),pixelPositions.argumentsStart,rowY1+1);
		
		if (isInInterval(address,line.totalSize,cpu->getPC()))
			dc.DrawText(L"■",pixelPositions.opcodeStart-8,rowY1);

		dc.SetFont(boldFont);
		dc.DrawText(wxString(line.name.c_str(),wxConvUTF8),pixelPositions.opcodeStart,rowY1+1);
		
		address += line.totalSize;
	}
}

void CtrlDisassemblyView::gotoAddress(u32 addr)
{
	u32 windowEnd = manager.getNthNextAddress(windowStart,visibleRows);
	u32 newAddress = manager.getStartAddress(addr);

	if (newAddress < windowStart || newAddress >= windowEnd)
	{
		windowStart = manager.getNthPreviousAddress(newAddress,visibleRows/2);
	}

	setCurAddress(addr);
	redraw();
}

void CtrlDisassemblyView::scrollAddressIntoView()
{
	u32 windowEnd = manager.getNthNextAddress(windowStart,visibleRows);

	if (curAddress < windowStart)
		windowStart = curAddress;
	else if (curAddress >= windowEnd)
		windowStart =  manager.getNthPreviousAddress(curAddress,visibleRows-1);
}

void CtrlDisassemblyView::calculatePixelPositions()
{
	pixelPositions.addressStart = 16;
	pixelPositions.opcodeStart = pixelPositions.addressStart + 18*charWidth;
	pixelPositions.argumentsStart = pixelPositions.opcodeStart + 9*charWidth;
	pixelPositions.arrowsStart = pixelPositions.argumentsStart + 30*charWidth;
}

void CtrlDisassemblyView::keydownEvent(wxKeyEvent& evt)
{
	u32 windowEnd = manager.getNthNextAddress(windowStart,visibleRows);

	switch (evt.GetKeyCode())
	{
	case WXK_LEFT:
		gotoAddress(cpu->getPC());
		break;
	case WXK_UP:
		setCurAddress(manager.getNthPreviousAddress(curAddress,1), wxGetKeyState(WXK_SHIFT));
		scrollAddressIntoView();
		break;
	case WXK_DOWN:
		setCurAddress(manager.getNthNextAddress(curAddress,1), wxGetKeyState(WXK_SHIFT));
		scrollAddressIntoView();
		break;
	case WXK_TAB:
		displaySymbols = !displaySymbols;
		break;
	case WXK_PAGEUP:
		if (curAddress != windowStart && curAddressIsVisible()) {
			setCurAddress(windowStart, wxGetKeyState(WXK_SHIFT));
			scrollAddressIntoView();
		} else {
			setCurAddress(manager.getNthPreviousAddress(windowStart,visibleRows), wxGetKeyState(WXK_SHIFT));
			scrollAddressIntoView();
		}
		break;
	case WXK_PAGEDOWN:
		if (manager.getNthNextAddress(curAddress,1) != windowEnd && curAddressIsVisible()) {
			setCurAddress(manager.getNthPreviousAddress(windowEnd,1), wxGetKeyState(WXK_SHIFT));
			scrollAddressIntoView();
		} else {
			setCurAddress(manager.getNthNextAddress(windowEnd,visibleRows-1), wxGetKeyState(WXK_SHIFT));
			scrollAddressIntoView();
		}
		break;
	}

	redraw();
}

void CtrlDisassemblyView::scrollbarEvent(wxScrollWinEvent& evt)
{
	int type = evt.GetEventType();
	if (type == wxEVT_SCROLLWIN_LINEUP)
	{
		windowStart = manager.getNthPreviousAddress(windowStart,1);
	} else if (type == wxEVT_SCROLLWIN_LINEDOWN)
	{
		windowStart = manager.getNthNextAddress(windowStart,1);
	} else if (type == wxEVT_SCROLLWIN_PAGEUP)
	{
		windowStart = manager.getNthPreviousAddress(windowStart,visibleRows);
	} else if (type == wxEVT_SCROLLWIN_PAGEDOWN)
	{
		windowStart = manager.getNthNextAddress(windowStart,visibleRows);
	}

	redraw();
}

void CtrlDisassemblyView::mouseEvent(wxMouseEvent& evt)
{
	// left button
	if (evt.ButtonDown(wxMOUSE_BTN_LEFT))
	{
		setCurAddress(yToAddress(evt.GetY()),wxGetKeyState(WXK_SHIFT));
		SetFocus();
		SetFocusFromKbd();
	}

	// handle wheel
	if (evt.GetWheelRotation() > 0)
	{
		windowStart = manager.getNthPreviousAddress(windowStart,3);
	} else if (evt.GetWheelRotation() < 0) {
		windowStart = manager.getNthNextAddress(windowStart,3);
	}

	redraw();
}

u32 CtrlDisassemblyView::yToAddress(int y)
{
	int line = y/rowHeight;
	return manager.getNthNextAddress(windowStart,line);
}

bool CtrlDisassemblyView::curAddressIsVisible()
{
	u32 windowEnd = manager.getNthNextAddress(windowStart,visibleRows);
	return curAddress >= windowStart && curAddress < windowEnd;
}
