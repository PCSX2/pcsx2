/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2014  PCSX2 Dev Team
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
#pragma once
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/grid.h>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

class CtrlRegisterList : public wxWindow
{
public:
    CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu);

    // Event handlers
    void paintEvent(wxPaintEvent & evt);
    void onPopupClick(wxCommandEvent& evt);
    void gridEvent(wxGridEvent& evt);
    void categoryChangedEvent(wxBookCtrlEvent& evt);
    void keydownEvent(wxEvent& evt);
private:
    enum RegisterChangeMode { LOWER64, UPPER64, CHANGE32 };

    void refreshChangedRegs();

    void changeValue(RegisterChangeMode mode, int cat, int reg);
    void updateHandler();
    void updateValues(int cat);
    void updateSize(int cat);
    int getCurrentCategory() const;
    void postEvent(wxEventType type, wxString text);
    void postEvent(wxEventType type, int value);

    struct ChangedReg
    {
        u128 oldValue;
        bool changed[4];
        ChangedReg() { memset(this, 0, sizeof(*this)); }
    };


    std::vector<std::vector<ChangedReg>> changedCategories;

    DebugInterface* cpu;                        // Used to get register values and other info from the emu
    u32 lastPc, lastCycles;
    int maxBits;                                // maximum number of bits beings displayed
    bool needsSizeUpdating, needsValueUpdating; // flags set in events to signal that values/sizes should be updated on the next display
    wxNotebook* registerCategories;             // Occupies this entire window. Is the tabbed window for selecting register categories.
    std::vector<wxGrid*> registerGrids;         // Grids displaying register values for each of the tabs.
};