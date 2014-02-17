#pragma once
#include <wx/wx.h>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

class CtrlRegisterList: public wxWindow
{
public:
	CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu);
	
	void paintEvent(wxPaintEvent & evt);
	void redraw();
	DECLARE_EVENT_TABLE()

	virtual wxSize GetMinClientSize() const
	{
		int w = 77+(displayBits/4) * charWidth;
		if (displayBits > 32)
			w += (displayBits/32)*2-2;
		return wxSize(w+2,2+rowHeight);
	}
	virtual wxSize DoGetBestClientSize() const
	{
		return GetMinClientSize();
	}
private:
	void render(wxDC& dc);
	void refreshChangedRegs();

	struct ChangedReg
	{
		u128 oldValue;
		bool changed[4];
	};

	ChangedReg changedRegs[32];

	DebugInterface* cpu;
	int rowHeight,charWidth;
	int currentRow;
	int displayBits;
	u32 lastPc;
};