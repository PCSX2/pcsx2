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
		int maxBits = 0;
		for (int i = 0; i < cpu->getRegisterCategoryCount(); i++)
		{
			maxBits = std::max<int>(maxBits,cpu->getRegisterSize(i));
		}

		int w = 77+(maxBits/4) * charWidth;
		if (maxBits > 32)
			w += (maxBits/32)*2-2;
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

	std::vector<ChangedReg*> changedCategories;

	DebugInterface* cpu;
	int rowHeight,charWidth;
	int currentRow;
	u32 lastPc;
	int category;
};