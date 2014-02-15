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
	EVT_SIZE(CtrlDisassemblyView::sizeEvent)
END_EVENT_TABLE()

CtrlDisassemblyView::CtrlDisassemblyView(wxWindow* parent, DebugInterface* _cpu)
	: wxWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxWANTS_CHARS|wxBORDER_SUNKEN), cpu(_cpu)
{
	manager.setCpu(cpu);
	windowStart = 0x20100000;
	rowHeight = 14;
	charWidth = 8;
	displaySymbols = true;
	visibleRows = 1;

	SetScrollbar(wxVERTICAL,100,1,201,true);
	SetDoubleBuffered(true);
	calculatePixelPositions();
	setCurAddress(windowStart);
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

void CtrlDisassemblyView::drawBranchLine(wxDC& dc, std::map<u32,int>& addressPositions, BranchLine& line)
{
	u32 windowEnd = manager.getNthNextAddress(windowStart,visibleRows);
	
	int winBottom = GetSize().GetHeight();

	int topY;
	int bottomY;
	if (line.first < windowStart)
	{
		topY = -1;
	} else if (line.first >= windowEnd)
	{
		topY = GetSize().GetHeight()+1;
	} else {
		topY = addressPositions[line.first] + rowHeight/2;
	}
			
	if (line.second < windowStart)
	{
		bottomY = -1;
	} else if (line.second >= windowEnd)
	{
		bottomY = GetSize().GetHeight()+1;
	} else {
		bottomY = addressPositions[line.second] + rowHeight/2;
	}

	if ((topY < 0 && bottomY < 0) || (topY > winBottom && bottomY > winBottom))
	{
		return;
	}

	// highlight line in a different color if it affects the currently selected opcode
	wxColor color;
	if (line.first == curAddress || line.second == curAddress)
	{
		color = wxColor(0xFF257AFA);
	} else {
		color = wxColor(0xFFFF3020);
	}
	
	wxPen pen = wxPen(color);
	dc.SetBrush(wxBrush(color));
	dc.SetPen(wxPen(color));

	int x = pixelPositions.arrowsStart+line.laneIndex*8;

	if (topY < 0)	// first is not visible, but second is
	{
		dc.DrawLine(x-2,bottomY,x+2,bottomY);
		dc.DrawLine(x+2,bottomY,x+2,0);

		if (line.type == LINE_DOWN)
		{
			dc.DrawLine(x,bottomY-4,x-4,bottomY);
			dc.DrawLine(x-4,bottomY,x+1,bottomY+5);
		}
	} else if (bottomY > winBottom) // second is not visible, but first is
	{
		dc.DrawLine(x-2,topY,x+2,topY);
		dc.DrawLine(x+2,topY,x+2,winBottom);
				
		if (line.type == LINE_UP)
		{
			dc.DrawLine(x,topY-4,x-4,topY);
			dc.DrawLine(x-4,topY,x+1,topY+5);
		}
	} else { // both are visible
		if (line.type == LINE_UP)
		{
			dc.DrawLine(x-2,bottomY,x+2,bottomY);
			dc.DrawLine(x+2,bottomY,x+2,topY);
			dc.DrawLine(x+2,topY,x-4,topY);
			
			dc.DrawLine(x,topY-4,x-4,topY);
			dc.DrawLine(x-4,topY,x+1,topY+5);
		} else {
			dc.DrawLine(x-2,topY,x+2,topY);
			dc.DrawLine(x+2,topY,x+2,bottomY);
			dc.DrawLine(x+2,bottomY,x-4,bottomY);
			
			dc.DrawLine(x,bottomY-4,x-4,bottomY);
			dc.DrawLine(x-4,bottomY,x+1,bottomY+5);
		}
	}
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
	
	std::map<u32,int> addressPositions;

	unsigned int address = windowStart;
	DisassemblyLineInfo line;
	for (int i = 0; i < visibleRows; i++)
	{
		manager.getLine(address,false,line);
		
		int rowY1 = rowHeight*i;
		int rowY2 = rowHeight*(i+1);
		
		addressPositions[address] = rowY1;

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

	std::vector<BranchLine> branchLines = manager.getBranchLines(windowStart,address-windowStart);
	for (size_t i = 0; i < branchLines.size(); i++)
	{
		drawBranchLine(dc,addressPositions,branchLines[i]);
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


void CtrlDisassemblyView::followBranch()
{
	DisassemblyLineInfo line;
	manager.getLine(curAddress,true,line);

	if (line.type == DISTYPE_OPCODE || line.type == DISTYPE_MACRO)
	{
		if (line.info.isBranch)
		{
			jumpStack.push_back(curAddress);
			gotoAddress(line.info.branchTarget);
		} else if (line.info.hasRelevantAddress)
		{
			// well, not  exactly a branch, but we can do something anyway
			// todo: position memory viewer to line.info.releventAddress
		}
	} else if (line.type == DISTYPE_DATA)
	{
		// jump to the start of the current line
			// todo: position memory viewer to curAddress
	}
}

void CtrlDisassemblyView::keydownEvent(wxKeyEvent& evt)
{
	u32 windowEnd = manager.getNthNextAddress(windowStart,visibleRows);

	switch (evt.GetKeyCode())
	{
	case WXK_LEFT:
		if (jumpStack.empty())
		{
			gotoAddress(cpu->getPC());
		} else {
			u32 addr = jumpStack[jumpStack.size()-1];
			jumpStack.pop_back();
			gotoAddress(addr);
		}
		break;
	case WXK_RIGHT:
		followBranch();
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

void CtrlDisassemblyView::sizeEvent(wxSizeEvent& evt)
{
	wxSize s = evt.GetSize();
	visibleRows = s.GetWidth()/rowHeight;
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
