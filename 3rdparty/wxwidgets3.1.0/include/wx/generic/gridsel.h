/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/gridsel.h
// Purpose:     wxGridSelection
// Author:      Stefan Neis
// Modified by:
// Created:     20/02/2000
// Copyright:   (c) Stefan Neis
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_GRIDSEL_H_
#define _WX_GENERIC_GRIDSEL_H_

#include "wx/defs.h"

#if wxUSE_GRID

#include "wx/grid.h"

class WXDLLIMPEXP_ADV wxGridSelection
{
public:
    wxGridSelection(wxGrid *grid,
                    wxGrid::wxGridSelectionModes sel = wxGrid::wxGridSelectCells);

    bool IsSelection();
    bool IsInSelection(int row, int col);
    bool IsInSelection(const wxGridCellCoords& coords)
    {
        return IsInSelection(coords.GetRow(), coords.GetCol());
    }

    void SetSelectionMode(wxGrid::wxGridSelectionModes selmode);
    wxGrid::wxGridSelectionModes GetSelectionMode() { return m_selectionMode; }
    void SelectRow(int row, const wxKeyboardState& kbd = wxKeyboardState());
    void SelectCol(int col, const wxKeyboardState& kbd = wxKeyboardState());
    void SelectBlock(int topRow, int leftCol,
                     int bottomRow, int rightCol,
                     const wxKeyboardState& kbd = wxKeyboardState(),
                     bool sendEvent = true );
    void SelectBlock(const wxGridCellCoords& topLeft,
                     const wxGridCellCoords& bottomRight,
                     const wxKeyboardState& kbd = wxKeyboardState(),
                     bool sendEvent = true )
    {
        SelectBlock(topLeft.GetRow(), topLeft.GetCol(),
                    bottomRight.GetRow(), bottomRight.GetCol(),
                    kbd, sendEvent);
    }

    void SelectCell(int row, int col,
                    const wxKeyboardState& kbd = wxKeyboardState(),
                    bool sendEvent = true);
    void SelectCell(const wxGridCellCoords& coords,
                    const wxKeyboardState& kbd = wxKeyboardState(),
                    bool sendEvent = true)
    {
        SelectCell(coords.GetRow(), coords.GetCol(), kbd, sendEvent);
    }

    void ToggleCellSelection(int row, int col,
                             const wxKeyboardState& kbd = wxKeyboardState());
    void ToggleCellSelection(const wxGridCellCoords& coords,
                             const wxKeyboardState& kbd = wxKeyboardState())
    {
        ToggleCellSelection(coords.GetRow(), coords.GetCol(), kbd);
    }

    void ClearSelection();

    void UpdateRows( size_t pos, int numRows );
    void UpdateCols( size_t pos, int numCols );

private:
    int BlockContain( int topRow1, int leftCol1,
                       int bottomRow1, int rightCol1,
                       int topRow2, int leftCol2,
                       int bottomRow2, int rightCol2 );
      // returns 1, if Block1 contains Block2,
      //        -1, if Block2 contains Block1,
      //         0, otherwise

    int BlockContainsCell( int topRow, int leftCol,
                           int bottomRow, int rightCol,
                           int row, int col )
      // returns 1, if Block contains Cell,
      //         0, otherwise
    {
        return ( topRow <= row && row <= bottomRow &&
                 leftCol <= col && col <= rightCol );
    }

    void SelectBlockNoEvent(int topRow, int leftCol,
                            int bottomRow, int rightCol)
    {
        SelectBlock(topRow, leftCol, bottomRow, rightCol,
                    wxKeyboardState(), false);
    }

    wxGridCellCoordsArray               m_cellSelection;
    wxGridCellCoordsArray               m_blockSelectionTopLeft;
    wxGridCellCoordsArray               m_blockSelectionBottomRight;
    wxArrayInt                          m_rowSelection;
    wxArrayInt                          m_colSelection;

    wxGrid                              *m_grid;
    wxGrid::wxGridSelectionModes        m_selectionMode;

    friend class WXDLLIMPEXP_FWD_ADV wxGrid;

    wxDECLARE_NO_COPY_CLASS(wxGridSelection);
};

#endif  // wxUSE_GRID
#endif  // _WX_GENERIC_GRIDSEL_H_
