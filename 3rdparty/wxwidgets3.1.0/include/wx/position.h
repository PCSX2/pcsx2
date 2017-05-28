/////////////////////////////////////////////////////////////////////////////
// Name:        wx/position.h
// Purpose:     Common structure and methods for positional information.
// Author:      Vadim Zeitlin, Robin Dunn, Brad Anderson, Bryan Petty
// Created:     2007-03-13
// Copyright:   (c) 2007 The wxWidgets Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_POSITION_H_
#define _WX_POSITION_H_

#include "wx/gdicmn.h"

class WXDLLIMPEXP_CORE wxPosition
{
public:
    wxPosition() : m_row(0), m_column(0) {}
    wxPosition(int row, int col) : m_row(row), m_column(col) {}

    // default copy ctor and assignment operator are okay.

    int GetRow() const          { return m_row; }
    int GetColumn() const       { return m_column; }
    int GetCol() const          { return GetColumn(); }
    void SetRow(int row)        { m_row = row; }
    void SetColumn(int column)  { m_column = column; }
    void SetCol(int column)     { SetColumn(column); }

    bool operator==(const wxPosition& p) const
        { return m_row == p.m_row && m_column == p.m_column; }
    bool operator!=(const wxPosition& p) const
        { return !(*this == p); }

    wxPosition& operator+=(const wxPosition& p)
        { m_row += p.m_row; m_column += p.m_column; return *this; }
    wxPosition& operator-=(const wxPosition& p)
        { m_row -= p.m_row; m_column -= p.m_column; return *this; }
    wxPosition& operator+=(const wxSize& s)
        { m_row += s.y; m_column += s.x; return *this; }
    wxPosition& operator-=(const wxSize& s)
        { m_row -= s.y; m_column -= s.x; return *this; }

    wxPosition operator+(const wxPosition& p) const
        { return wxPosition(m_row + p.m_row, m_column + p.m_column); }
    wxPosition operator-(const wxPosition& p) const
        { return wxPosition(m_row - p.m_row, m_column - p.m_column); }
    wxPosition operator+(const wxSize& s) const
        { return wxPosition(m_row + s.y, m_column + s.x); }
    wxPosition operator-(const wxSize& s) const
        { return wxPosition(m_row - s.y, m_column - s.x); }

private:
    int m_row;
    int m_column;
};

#endif // _WX_POSITION_H_

