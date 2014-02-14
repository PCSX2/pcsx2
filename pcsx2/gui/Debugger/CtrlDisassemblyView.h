#pragma once
#include <wx/wx.h>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

class CtrlDisassemblyView: public wxWindow
{
public:
	CtrlDisassemblyView(wxWindow* parent, DebugInterface* _cpu);

	void mouseEvent(wxMouseEvent& evt);
	void paintEvent(wxPaintEvent & evt);
	void keydownEvent(wxKeyEvent& evt);
	void paintNow();
	
	void gotoAddress(u32 addr);
	DECLARE_EVENT_TABLE()
private:
	void render(wxDC& dc);
	void calculatePixelPositions();
	bool getDisasmAddressText(u32 address, char* dest, bool abbreviateLabels, bool showData);
	u32 yToAddress(int y);
	bool curAddressIsVisible();

	void setCurAddress(u32 newAddress, bool extend = false)
	{
		newAddress = manager.getStartAddress(newAddress);
		u32 after = manager.getNthNextAddress(newAddress,1);
		curAddress = newAddress;
		selectRangeStart = extend ? std::min(selectRangeStart, newAddress) : newAddress;
		selectRangeEnd = extend ? std::max(selectRangeEnd, after) : after;
	}

	void scrollAddressIntoView();
	struct {
		int addressStart;
		int opcodeStart;
		int argumentsStart;
		int arrowsStart;
	} pixelPositions;

	DebugInterface* cpu;
	DisassemblyManager manager;
	u32 windowStart;
	u32 curAddress;
	u32 selectRangeStart;
	u32 selectRangeEnd;
	int visibleRows;
	int rowHeight;
	int charWidth;
	bool displaySymbols;
};
