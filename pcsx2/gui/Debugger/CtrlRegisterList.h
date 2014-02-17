#pragma once
#include <wx/wx.h>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

class CtrlRegisterList: public wxWindow
{
public:
	CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu);
	
	void paintEvent(wxPaintEvent & evt);
	void mouseEvent(wxMouseEvent& evt);
	void redraw();
	DECLARE_EVENT_TABLE()

	virtual wxSize GetMinClientSize() const
	{
		int maxWidth = 0;
		for (int i = 0; i < cpu->getRegisterCategoryCount(); i++)
		{
			int bits = cpu->getRegisterSize(i);
			int start = startPositions[i];
			
			int w = start+(bits/4) * charWidth;
			if (bits > 32)
				w += (bits/32)*2-2;

			maxWidth = std::max<int>(maxWidth,w);
		}

		return wxSize(maxWidth+4,2+rowHeight);
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
	std::vector<int> startPositions;

	DebugInterface* cpu;
	int rowHeight,charWidth;
	int currentRow;
	u32 lastPc;
	int category;
};