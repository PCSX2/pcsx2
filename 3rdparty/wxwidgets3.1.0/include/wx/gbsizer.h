/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gbsizer.h
// Purpose:     wxGridBagSizer:  A sizer that can lay out items in a grid,
//              with items at specified cells, and with the option of row
//              and/or column spanning
//
// Author:      Robin Dunn
// Created:     03-Nov-2003
// Copyright:   (c) Robin Dunn
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WXGBSIZER_H__
#define __WXGBSIZER_H__

#include "wx/sizer.h"


//---------------------------------------------------------------------------
// Classes to represent a position in the grid and a size of an item in the
// grid, IOW, the number of rows and columns it occupies.  I chose to use these
// instead of wxPoint and wxSize because they are (x,y) and usually pixel
// oriented while grids and tables are usually thought of as (row,col) so some
// confusion would definitely result in using wxPoint...
//
// NOTE: This should probably be refactored to a common RowCol data type which
// is used for this and also for wxGridCellCoords.
//---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGBPosition
{
public:
    wxGBPosition() : m_row(0), m_col(0) {}
    wxGBPosition(int row, int col) : m_row(row), m_col(col) {}

    // default copy ctor and assignment operator are okay.

    int GetRow() const { return m_row; }
    int GetCol() const { return m_col; }
    void SetRow(int row) { m_row = row; }
    void SetCol(int col) { m_col = col; }

    bool operator==(const wxGBPosition& p) const { return m_row == p.m_row && m_col == p.m_col; }
    bool operator!=(const wxGBPosition& p) const { return !(*this == p); }

private:
    int m_row;
    int m_col;
};


class WXDLLIMPEXP_CORE wxGBSpan
{
public:
    wxGBSpan() { Init(); }
    wxGBSpan(int rowspan, int colspan)
    {
        // Initialize the members to valid values as not doing it may result in
        // infinite loop in wxGBSizer code if the user passed 0 for any of
        // them, see #12934.
        Init();

        SetRowspan(rowspan);
        SetColspan(colspan);
    }

    // default copy ctor and assignment operator are okay.

    // Factor constructor creating an invalid wxGBSpan: this is mostly supposed
    // to be used as return value for functions returning wxGBSpan in case of
    // errors.
    static wxGBSpan Invalid()
    {
        return wxGBSpan(NULL);
    }

    int GetRowspan() const { return m_rowspan; }
    int GetColspan() const { return m_colspan; }
    void SetRowspan(int rowspan)
    {
        wxCHECK_RET( rowspan > 0, "Row span should be strictly positive" );

        m_rowspan = rowspan;
    }

    void SetColspan(int colspan)
    {
        wxCHECK_RET( colspan > 0, "Column span should be strictly positive" );

        m_colspan = colspan;
    }

    bool operator==(const wxGBSpan& o) const { return m_rowspan == o.m_rowspan && m_colspan == o.m_colspan; }
    bool operator!=(const wxGBSpan& o) const { return !(*this == o); }

private:
    // This private ctor is used by Invalid() only.
    wxGBSpan(struct InvalidCtorTag*)
    {
        m_rowspan =
        m_colspan = -1;
    }

    void Init()
    {
        m_rowspan =
        m_colspan = 1;
    }

    int m_rowspan;
    int m_colspan;
};


extern WXDLLIMPEXP_DATA_CORE(const wxGBSpan) wxDefaultSpan;


//---------------------------------------------------------------------------
// wxGBSizerItem
//---------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxGridBagSizer;


class WXDLLIMPEXP_CORE wxGBSizerItem : public wxSizerItem
{
public:
    // spacer
    wxGBSizerItem( int width,
                   int height,
                   const wxGBPosition& pos,
                   const wxGBSpan& span=wxDefaultSpan,
                   int flag=0,
                   int border=0,
                   wxObject* userData=NULL);

    // window
    wxGBSizerItem( wxWindow *window,
                   const wxGBPosition& pos,
                   const wxGBSpan& span=wxDefaultSpan,
                   int flag=0,
                   int border=0,
                   wxObject* userData=NULL );

    // subsizer
    wxGBSizerItem( wxSizer *sizer,
                   const wxGBPosition& pos,
                   const wxGBSpan& span=wxDefaultSpan,
                   int flag=0,
                   int border=0,
                   wxObject* userData=NULL );

    // default ctor
    wxGBSizerItem();


    // Get the grid position of the item
    wxGBPosition GetPos() const { return m_pos; }
    void GetPos(int& row, int& col) const;

    // Get the row and column spanning of the item
    wxGBSpan GetSpan() const { return m_span; }
    void GetSpan(int& rowspan, int& colspan) const;

    // If the item is already a member of a sizer then first ensure that there
    // is no other item that would intersect with this one at the new
    // position, then set the new position.  Returns true if the change is
    // successful and after the next Layout the item will be moved.
    bool SetPos( const wxGBPosition& pos );

    // If the item is already a member of a sizer then first ensure that there
    // is no other item that would intersect with this one with its new
    // spanning size, then set the new spanning.  Returns true if the change
    // is successful and after the next Layout the item will be resized.
    bool SetSpan( const wxGBSpan& span );

    // Returns true if this item and the other item intersect
    bool Intersects(const wxGBSizerItem& other);

    // Returns true if the given pos/span would intersect with this item.
    bool Intersects(const wxGBPosition& pos, const wxGBSpan& span);

    // Get the row and column of the endpoint of this item
    void GetEndPos(int& row, int& col);


    wxGridBagSizer* GetGBSizer() const { return m_gbsizer; }
    void SetGBSizer(wxGridBagSizer* sizer) { m_gbsizer = sizer; }


protected:
    wxGBPosition    m_pos;
    wxGBSpan        m_span;
    wxGridBagSizer* m_gbsizer;  // so SetPos/SetSpan can check for intersects


private:
    wxDECLARE_DYNAMIC_CLASS(wxGBSizerItem);
    wxDECLARE_NO_COPY_CLASS(wxGBSizerItem);
};


//---------------------------------------------------------------------------
// wxGridBagSizer
//---------------------------------------------------------------------------


class WXDLLIMPEXP_CORE wxGridBagSizer : public wxFlexGridSizer
{
public:
    wxGridBagSizer(int vgap = 0, int hgap = 0 );

    // The Add methods return true if the item was successfully placed at the
    // given position, false if something was already there.
    wxSizerItem* Add( wxWindow *window,
                      const wxGBPosition& pos,
                      const wxGBSpan& span = wxDefaultSpan,
                      int flag = 0,
                      int border = 0,
                      wxObject* userData = NULL );
    wxSizerItem* Add( wxSizer *sizer,
                      const wxGBPosition& pos,
                      const wxGBSpan& span = wxDefaultSpan,
                      int flag = 0,
                      int border = 0,
                      wxObject* userData = NULL );
    wxSizerItem* Add( int width,
                      int height,
                      const wxGBPosition& pos,
                      const wxGBSpan& span = wxDefaultSpan,
                      int flag = 0,
                      int border = 0,
                      wxObject* userData = NULL );
    wxSizerItem* Add( wxGBSizerItem *item );


    // Get/Set the size used for cells in the grid with no item.
    wxSize GetEmptyCellSize() const          { return m_emptyCellSize; }
    void SetEmptyCellSize(const wxSize& sz)  { m_emptyCellSize = sz; }

    // Get the size of the specified cell, including hgap and vgap.  Only
    // valid after a Layout.
    wxSize GetCellSize(int row, int col) const;

    // Get the grid position of the specified item (non-recursive)
    wxGBPosition GetItemPosition(wxWindow *window);
    wxGBPosition GetItemPosition(wxSizer *sizer);
    wxGBPosition GetItemPosition(size_t index);

    // Set the grid position of the specified item.  Returns true on success.
    // If the move is not allowed (because an item is already there) then
    // false is returned.   (non-recursive)
    bool SetItemPosition(wxWindow *window, const wxGBPosition& pos);
    bool SetItemPosition(wxSizer *sizer, const wxGBPosition& pos);
    bool SetItemPosition(size_t index, const wxGBPosition& pos);

    // Get the row/col spanning of the specified item (non-recursive)
    wxGBSpan GetItemSpan(wxWindow *window);
    wxGBSpan GetItemSpan(wxSizer *sizer);
    wxGBSpan GetItemSpan(size_t index);

    // Set the row/col spanning of the specified item. Returns true on
    // success.  If the move is not allowed (because an item is already there)
    // then false is returned. (non-recursive)
    bool SetItemSpan(wxWindow *window, const wxGBSpan& span);
    bool SetItemSpan(wxSizer *sizer, const wxGBSpan& span);
    bool SetItemSpan(size_t index, const wxGBSpan& span);


    // Find the sizer item for the given window or subsizer, returns NULL if
    // not found. (non-recursive)
    wxGBSizerItem* FindItem(wxWindow* window);
    wxGBSizerItem* FindItem(wxSizer* sizer);


    // Return the sizer item for the given grid cell, or NULL if there is no
    // item at that position. (non-recursive)
    wxGBSizerItem* FindItemAtPosition(const wxGBPosition& pos);


    // Return the sizer item located at the point given in pt, or NULL if
    // there is no item at that point. The (x,y) coordinates in pt correspond
    // to the client coordinates of the window using the sizer for
    // layout. (non-recursive)
    wxGBSizerItem* FindItemAtPoint(const wxPoint& pt);


    // Return the sizer item that has a matching user data (it only compares
    // pointer values) or NULL if not found. (non-recursive)
    wxGBSizerItem* FindItemWithData(const wxObject* userData);


    // These are what make the sizer do size calculations and layout
    virtual void RecalcSizes() wxOVERRIDE;
    virtual wxSize CalcMin() wxOVERRIDE;


    // Look at all items and see if any intersect (or would overlap) the given
    // item.  Returns true if so, false if there would be no overlap.  If an
    // excludeItem is given then it will not be checked for intersection, for
    // example it may be the item we are checking the position of.
    bool CheckForIntersection(wxGBSizerItem* item, wxGBSizerItem* excludeItem = NULL);
    bool CheckForIntersection(const wxGBPosition& pos, const wxGBSpan& span, wxGBSizerItem* excludeItem = NULL);


    // The Add base class virtuals should not be used with this class, but
    // we'll try to make them automatically select a location for the item
    // anyway.
    virtual wxSizerItem* Add( wxWindow *window, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Add( wxSizer *sizer, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Add( int width, int height, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );

    // The Insert and Prepend base class virtuals that are not appropriate for
    // this class and should not be used.  Their implementation in this class
    // simply fails.
    virtual wxSizerItem* Add( wxSizerItem *item );
    virtual wxSizerItem* Insert( size_t index, wxWindow *window, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Insert( size_t index, wxSizer *sizer, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Insert( size_t index, int width, int height, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Insert( size_t index, wxSizerItem *item ) wxOVERRIDE;
    virtual wxSizerItem* Prepend( wxWindow *window, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Prepend( wxSizer *sizer, int proportion = 0, int flag = 0, int border = 0, wxObject* userData = NULL );
    virtual wxSizerItem* Prepend( int width,  int height,  int proportion = 0,  int flag = 0,  int border = 0,  wxObject* userData = NULL );
    virtual wxSizerItem* Prepend( wxSizerItem *item );


protected:
    wxGBPosition FindEmptyCell();
    void AdjustForOverflow();

    wxSize m_emptyCellSize;


private:

    wxDECLARE_CLASS(wxGridBagSizer);
    wxDECLARE_NO_COPY_CLASS(wxGridBagSizer);
};

//---------------------------------------------------------------------------
#endif
