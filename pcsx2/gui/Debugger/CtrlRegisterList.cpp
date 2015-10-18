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

#include "PrecompiledHeader.h"
#include "CtrlRegisterList.h"
#include "DebugTools/Debug.h"
#include "DebugEvents.h"
#include "AppConfig.h"

BEGIN_EVENT_TABLE(CtrlRegisterList, wxWindow)
    EVT_PAINT(CtrlRegisterList::paintEvent)
    EVT_GRID_LABEL_LEFT_CLICK(CtrlRegisterList::gridEvent)
    EVT_GRID_LABEL_RIGHT_CLICK(CtrlRegisterList::gridEvent)
    EVT_GRID_CELL_LEFT_CLICK(CtrlRegisterList::gridEvent)
    EVT_GRID_CELL_RIGHT_CLICK(CtrlRegisterList::gridEvent)
    EVT_KEY_DOWN(CtrlRegisterList::keydownEvent)
    EVT_BOOKCTRL_PAGE_CHANGED(-1, CtrlRegisterList::categoryChangedEvent)
END_EVENT_TABLE()

enum DisassemblyMenuIdentifiers
{
    ID_REGISTERLIST_DISPLAY32 = 1,
    ID_REGISTERLIST_DISPLAY64,
    ID_REGISTERLIST_DISPLAY128,
    ID_REGISTERLIST_CHANGELOWER,
    ID_REGISTERLIST_CHANGEUPPER,
    ID_REGISTERLIST_CHANGEVALUE
};

CtrlRegisterList::CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu) :
    wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_NONE),
    cpu(_cpu),
    lastPc(0),
    lastCycles(0),
    maxBits(128),
    needsSizeUpdating(true),
    needsValueUpdating(true)
{
    int rowHeight = g_Conf->EmuOptions.Debugger.FontHeight;
    int charWidth = g_Conf->EmuOptions.Debugger.FontWidth;


#ifdef _WIN32
    wxFont font = wxFont(wxSize(charWidth, rowHeight), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
        false, L"Lucida Console");
    wxFont labelFont = font.Bold();
#else
    wxFont font = wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, L"Lucida Console");
    font.SetPixelSize(wxSize(charWidth, rowHeight));
    wxFont labelFont = font;
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
#endif
    registerCategories = new wxNotebook(this, wxID_ANY);
// 'c' and 'C', much time wasted.
#if wxMAJOR_VERSION >= 3
    registerCategories->Connect(wxEVT_BOOKCTRL_PAGE_CHANGED, wxBookCtrlEventHandler(CtrlRegisterList::categoryChangedEvent), nullptr, this);
#else
    registerCategories->Connect(wxEVT_COMMAND_BOOKCTRL_PAGE_CHANGED, wxBookctrlEventHandler(CtrlRegisterList::categoryChangedEvent), nullptr, this);
#endif
    for (int cat = 0; cat < cpu->getRegisterCategoryCount(); cat++)
    {
        int numRegs = cpu->getRegisterCount(cat);

        changedCategories.push_back(std::vector<ChangedReg>(numRegs));

        wxGrid* regGrid = new wxGrid(registerCategories, -1);

        registerGrids.push_back(regGrid);
        registerCategories->AddPage(regGrid, wxString(cpu->getRegisterCategoryName(cat), wxConvUTF8));

        DebugInterface::RegisterType type = cpu->getRegisterType(cat);
        int registerBits = cpu->getRegisterSize(cat);

        int numCols;
        switch (type)
        {
        case DebugInterface::NORMAL:	// display them in 32 bit parts
            numCols = registerBits / 32;
            regGrid->CreateGrid(numRegs, numCols);
            for (int row = 0; row < numRegs; row++)
                regGrid->SetRowLabelValue(row, wxString(cpu->getRegisterName(cat, row), wxConvUTF8));
            for (int col = 0; col < numCols; col++)
                regGrid->SetColLabelValue(col, wxsFormat(L"%d-%d", 32 * (numCols - col) - 1, 32 * (numCols - col - 1)));
            break;
        case DebugInterface::SPECIAL:
            regGrid->CreateGrid(numRegs, 1);
            for (int row = 0; row < numRegs; row++)
                regGrid->SetRowLabelValue(row, wxString(cpu->getRegisterName(cat, row), wxConvUTF8));
            break;
        }

        regGrid->EnableEditing(false);
        regGrid->SetDefaultCellFont(font);
        regGrid->SetLabelFont(labelFont);
        regGrid->DisableDragGridSize();
        regGrid->DisableDragRowSize();
        regGrid->DisableDragColSize();
        regGrid->Connect(wxEVT_PAINT, wxPaintEventHandler(CtrlRegisterList::paintEvent), nullptr, this);
        regGrid->Connect(wxEVT_GRID_LABEL_LEFT_CLICK, wxGridEventHandler(CtrlRegisterList::gridEvent), nullptr, this);
        regGrid->Connect(wxEVT_GRID_LABEL_RIGHT_CLICK, wxGridEventHandler(CtrlRegisterList::gridEvent), nullptr, this);
        regGrid->Connect(wxEVT_GRID_CELL_RIGHT_CLICK, wxGridEventHandler(CtrlRegisterList::gridEvent), nullptr, this);
        regGrid->Connect(wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler(CtrlRegisterList::gridEvent), nullptr, this);
        regGrid->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(CtrlRegisterList::keydownEvent), nullptr, this);
    }

    for (int cat = 0; cat < cpu->getRegisterCategoryCount(); cat++)
        updateValues(cat);

    updateSize(getCurrentCategory()); // getCurrentCategory() = 0

    SetDoubleBuffered(true);
}

// Called when this window needs to update the strings in the cells.
// This has to be called every repaint of the window since the registers
// can change unpredictably and there seems to be currently is no mechanism
// for the rest of pcsx2 alerting the debugger when values change.
void CtrlRegisterList::updateValues(int cat)
{
    wxGrid* regGrid = registerGrids[cat];
    int numRows = regGrid->GetNumberRows();
    int numCols = regGrid->GetNumberCols();

    std::vector<ChangedReg>& changedRegs = changedCategories[cat];
    DebugInterface::RegisterType type = cpu->getRegisterType(cat);
    for (int row = 0; row < numRows; row++)
    {
        wxColor bgColor = regGrid->GetGridCursorRow() == row ? wxColor(0xFFFFDFE0) :
                                                               wxColor(0xFFFFEFFF);
        u128 value = cpu->getRegister(cat, row);
        ChangedReg& changed = changedRegs[row];

        for (int col = 0; col < numCols; col++)
        {
            const wxColor colorChanged = wxColor(0xFF0000FF);
            const wxColor colorUnchanged = wxColor(0xFF004000);
            wxColor textColor;
            wxString cellValue;

            switch (type)
            {
            case DebugInterface::NORMAL:
                cellValue = wxsFormat(L"%08X", value._u32[numCols - col - 1]);
                textColor = changed.changed[numCols - col - 1] ? colorChanged : colorUnchanged;
                break;
            case DebugInterface::SPECIAL:
                cellValue = cpu->getRegisterString(cat, row);
                textColor = (changed.changed[0] || changed.changed[1] || changed.changed[2] || changed.changed[3]) ?
                    colorChanged : colorUnchanged;
                break;
            default: pxAssert(0 && "Unreachable switch case");
            }
            if (regGrid->GetCellTextColour(row, col) != textColor)
                regGrid->SetCellTextColour(row, col, textColor);
            if (regGrid->GetCellValue(row, col) != cellValue)
                regGrid->SetCellValue(row, col, cellValue);
            if (regGrid->GetCellBackgroundColour(row, col) != bgColor)
                regGrid->SetCellBackgroundColour(row, col, bgColor);
        }
    }
}

void CtrlRegisterList::updateSize(int cat)
{
    wxGrid* regGrid = registerGrids[cat];

    int regBits = cpu->getRegisterSize(cat);
    int numCols = regGrid->GetNumberCols();

#if wxMAJOR_VERSION >= 3
    int shownCols = 0;
    while (shownCols < numCols && regGrid->IsColShown(shownCols)) shownCols++;
    if (shownCols > maxBits / 32)
        shownCols = (maxBits / 32);
    else if (shownCols < regBits / 32)
        shownCols = std::min(maxBits / 32, regBits / 32);

    for (int col = 0; col < numCols; col++)
        if (col < shownCols)
            regGrid->ShowCol(numCols - col - 1); // Big-endian representation so flip order
        else
            regGrid->HideCol(numCols - col - 1); // Big-endian representation so flip order
#endif

    regGrid->AutoSize();
    wxSize pageSize = regGrid->GetSize();

    // Hack: Sometimes the vertical scroll bar covers some of the text so add some room
    pageSize.x += 20;

    // Hack: AFAIK wxNotebook does not provide a way to get the necessary size
    // for the tabs so we use a rough approximation and hope that the tabs
    // will have enough room to all be shown. 50 pixels per tab should hopefully work.
    int minX = registerCategories->GetPageCount() * 50;
    pageSize.x = std::max(pageSize.x, minX);

    // Hack: Sometimes showing all the rows on the screen take up too much
    // vertical room and squeezes other components (breakpoint window, etc.)
    // into nothing so we limit the vertical size with this heuristic.
    // If necessary, this will automatically create a vertical scroll bar so
    // all rows can be accessed.
    int screenSize = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);
    pageSize.y = std::min(std::max(screenSize - 400, screenSize / 2), pageSize.y);

    regGrid->SetSize(pageSize);

    wxSize size = registerCategories->CalcSizeFromPage(pageSize);
    if (registerCategories->GetSize() != size)
        registerCategories->SetSize(size);
    SetMinSize(size); // registerCategories is the whole window so same min-size

    // Need to update the whole debugger window since other components
    // may be resized due to this component being resized
    postEvent(debEVT_UPDATELAYOUT, 0);
}

void CtrlRegisterList::postEvent(wxEventType type, wxString text)
{
    wxCommandEvent event(type, GetId());
    event.SetEventObject(this);
    event.SetString(text);
    wxPostEvent(this, event);
}

void CtrlRegisterList::postEvent(wxEventType type, int value)
{
    wxCommandEvent event(type, GetId());
    event.SetEventObject(this);
    event.SetInt(value);
    wxPostEvent(this, event);
}


// Sets the "changed" flag for values that have changed from the last cycle.
// These values are colored differently when they are displayed.
void CtrlRegisterList::refreshChangedRegs()
{
    if (cpu->getPC() == lastPc && cpu->getCycles() == lastCycles)
        return;

    for (int cat = 0; cat < cpu->getRegisterCategoryCount(); cat++)
    {
        std::vector<ChangedReg>& regs = changedCategories[cat];
        int size = cpu->getRegisterSize(cat);

        for (int i = 0; i < cpu->getRegisterCount(cat); i++)
        {
            ChangedReg& reg = regs[i];
            memset(&reg.changed, 0, sizeof(reg.changed));

            u128 newValue = cpu->getRegister(cat, i);

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
    lastCycles = cpu->getCycles();
}

void CtrlRegisterList::paintEvent(wxPaintEvent & evt)
{
    updateHandler();
    evt.Skip();
}

void CtrlRegisterList::updateHandler()
{
    if (cpu->isCpuPaused() || needsValueUpdating)
    {
        refreshChangedRegs();
        updateValues(getCurrentCategory());
        needsValueUpdating = false;
    }

    if (needsSizeUpdating)
    {
        updateSize(getCurrentCategory());
        needsSizeUpdating = false;
    }

    // The wxGrid allows selecting boxes with a bold outline
    // but we don't want this, and there is no setting to turn off this feature
    wxGrid* regGrid = registerGrids[getCurrentCategory()];
    regGrid->ClearSelection();
}

void CtrlRegisterList::changeValue(RegisterChangeMode mode, int cat, int reg)
{
    wxString oldStr;
    u128 oldValue = cpu->getRegister(cat, reg);

    switch (mode)
    {
    case LOWER64:
        oldStr = wxsFormat(L"0x%016llX", oldValue._u64[0]);
        break;
    case UPPER64:
        oldStr = wxsFormat(L"0x%016llX", oldValue._u64[1]);
        break;
    case CHANGE32:
        oldStr = wxsFormat(L"0x%08X", oldValue._u64[0]);
        break;
    }

    u64 newValue;
    if (executeExpressionWindow(this, cpu, newValue, oldStr))
    {
        switch (mode)
        {
        case LOWER64:
            oldValue._u64[0] = newValue;
            break;
        case UPPER64:
            oldValue._u64[1] = newValue;
            break;
        case CHANGE32:
            oldValue._u32[0] = newValue;
            break;
        }
        cpu->setRegister(cat, reg, oldValue);
        oldValue = cpu->getRegister(cat, reg);
    }
    needsValueUpdating = true;
    needsSizeUpdating = true;
}


void CtrlRegisterList::onPopupClick(wxCommandEvent& evt)
{
    int cat = getCurrentCategory();
    wxGrid* regGrid = registerGrids[cat];
    int reg = regGrid->GetGridCursorRow();
    switch (evt.GetId())
    {
    case ID_REGISTERLIST_DISPLAY32:
        maxBits = 32;
        postEvent(debEVT_UPDATELAYOUT, 0);
        Refresh();
        break;
    case ID_REGISTERLIST_DISPLAY64:
        maxBits = 64;
        postEvent(debEVT_UPDATELAYOUT, 0);
        Refresh();
        break;
    case ID_REGISTERLIST_DISPLAY128:
        maxBits = 128;
        postEvent(debEVT_UPDATELAYOUT, 0);
        Refresh();
        break;
    case ID_REGISTERLIST_CHANGELOWER:
        changeValue(LOWER64, cat, reg);
        Refresh();
        break;
    case ID_REGISTERLIST_CHANGEUPPER:
        changeValue(UPPER64, cat, reg);
        Refresh();
        break;
    case ID_REGISTERLIST_CHANGEVALUE:
        if (cpu->getRegisterSize(cat) == 32)
            changeValue(CHANGE32, cat, reg);
        else
            changeValue(LOWER64, cat, reg);
        Refresh();
        break;
    default:
        wxMessageBox(L"Unimplemented.", L"Unimplemented.", wxICON_INFORMATION);
        break;
    }
    needsValueUpdating = true;
    needsSizeUpdating = true;
}

int CtrlRegisterList::getCurrentCategory() const
{
    return registerCategories->GetSelection();
}


void CtrlRegisterList::gridEvent(wxGridEvent& evt)
{
    // Mouse events
    if (evt.GetEventType() == wxEVT_GRID_CELL_RIGHT_CLICK ||
        evt.GetEventType() == wxEVT_GRID_LABEL_RIGHT_CLICK)
    {
        wxMenu menu;
        int bits = cpu->getRegisterSize(getCurrentCategory());

        menu.AppendRadioItem(ID_REGISTERLIST_DISPLAY32, L"Display 32 bit");
        menu.AppendRadioItem(ID_REGISTERLIST_DISPLAY64, L"Display 64 bit");
        menu.AppendRadioItem(ID_REGISTERLIST_DISPLAY128, L"Display 128 bit");
        menu.AppendSeparator();

        if (bits >= 64)
        {
            menu.Append(ID_REGISTERLIST_CHANGELOWER, L"Change lower 64 bit");
            menu.Append(ID_REGISTERLIST_CHANGEUPPER, L"Change upper 64 bit");
        }
        else {
            menu.Append(ID_REGISTERLIST_CHANGEVALUE, L"Change value");
        }

        switch (maxBits)
        {
        case 128:
            menu.Check(ID_REGISTERLIST_DISPLAY128, true);
            break;
        case 64:
            menu.Check(ID_REGISTERLIST_DISPLAY64, true);
            break;
        case 32:
            menu.Check(ID_REGISTERLIST_DISPLAY32, true);
            break;
        }

        menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CtrlRegisterList::onPopupClick), nullptr, this);
        PopupMenu(&menu, evt.GetPosition());
        needsValueUpdating = true;
    }
    else
    {
        evt.Skip();
    }
}

void CtrlRegisterList::categoryChangedEvent(wxBookCtrlEvent& evt)
{
    needsSizeUpdating = true;
    needsValueUpdating = true;
    evt.Skip();
}

void CtrlRegisterList::keydownEvent(wxKeyEvent& evt)
{
    needsValueUpdating = true;
    evt.Skip();
}
