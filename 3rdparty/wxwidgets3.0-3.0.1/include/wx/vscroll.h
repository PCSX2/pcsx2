/////////////////////////////////////////////////////////////////////////////
// Name:        wx/vscroll.h
// Purpose:     Variable scrolled windows (wx[V/H/HV]ScrolledWindow)
// Author:      Vadim Zeitlin
// Modified by: Brad Anderson, Bryan Petty
// Created:     30.05.03
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_VSCROLL_H_
#define _WX_VSCROLL_H_

#include "wx/panel.h"
#include "wx/position.h"
#include "wx/scrolwin.h"

class WXDLLIMPEXP_FWD_CORE wxVarScrollHelperEvtHandler;


// Using the same techniques as the wxScrolledWindow class      |
// hierarchy, the wx[V/H/HV]ScrolledWindow classes are slightly |
// more complex (compare with the diagram outlined in           |
// scrolwin.h) for the purpose of reducing code duplication     |
// through the use of mix-in classes.                           |
//                                                              |
//                   wxAnyScrollHelperBase                      |
//                            |                                 |
//                            |                                 |
//                            |                                 |
//                            V                                 |
//                  wxVarScrollHelperBase                       |
//                   /                 \                        |
//                  /                   \                       |
//                 V                     V                      |
//  wxVarHScrollHelper                 wxVarVScrollHelper       |
//       |          \                   /           |           |
//       |           \                 /            |           |
//       |            V               V             |           |
//       |           wxVarHVScrollHelper            |           |
//       |                      |                   |           |
//       |                      |                   V           |
//       |         wxPanel      |    wxVarVScrollLegacyAdaptor  |
//       |         /  \  \      |                   |           |
//       |        /    \  `-----|----------.        |           |
//       |       /      \       |           \       |           |
//       |      /        \      |            \      |           |
//       V     V          \     |             V     V           |
//  wxHScrolledWindow      \    |        wxVScrolledWindow      |
//                          V   V                               |
//                    wxHVScrolledWindow                        |
//                                                              |
//                                                              |
//   Border added to suppress GCC multi-line comment warnings ->|


// ===========================================================================
// wxVarScrollHelperBase
// ===========================================================================

// Provides all base common scroll calculations needed for either orientation,
// automatic scrollbar functionality, saved scroll positions, functionality
// for changing the target window to be scrolled, as well as defining all
// required virtual functions that need to be implemented for any orientation
// specific work.

class WXDLLIMPEXP_CORE wxVarScrollHelperBase : public wxAnyScrollHelperBase
{
public:
    // constructors and such
    // ---------------------

    wxVarScrollHelperBase(wxWindow *winToScroll);
    virtual ~wxVarScrollHelperBase();

    // operations
    // ----------

    // with physical scrolling on, the device origin is changed properly when
    // a wxPaintDC is prepared, children are actually moved and laid out
    // properly, and the contents of the window (pixels) are actually moved
    void EnablePhysicalScrolling(bool scrolling = true)
        { m_physicalScrolling = scrolling; }

    // wxNOT_FOUND if none, i.e. if it is below the last item
    int VirtualHitTest(wxCoord coord) const;

    // recalculate all our parameters and redisplay all units
    virtual void RefreshAll();

    // accessors
    // ---------

    // get the first currently visible unit
    size_t GetVisibleBegin() const { return m_unitFirst; }

    // get the last currently visible unit
    size_t GetVisibleEnd() const
        { return m_unitFirst + m_nUnitsVisible; }

    // is this unit currently visible?
    bool IsVisible(size_t unit) const
        { return unit >= m_unitFirst && unit < GetVisibleEnd(); }

    // translate between scrolled and unscrolled coordinates
    int CalcScrolledPosition(int coord) const
        {  return DoCalcScrolledPosition(coord); }
    int CalcUnscrolledPosition(int coord) const
        {  return DoCalcUnscrolledPosition(coord); }

    virtual int DoCalcScrolledPosition(int coord) const;
    virtual int DoCalcUnscrolledPosition(int coord) const;

    // update the thumb size shown by the scrollbar
    virtual void UpdateScrollbar();
    void RemoveScrollbar();

    // Normally the wxScrolledWindow will scroll itself, but in some rare
    // occasions you might want it to scroll [part of] another window (e.g. a
    // child of it in order to scroll only a portion the area between the
    // scrollbars (spreadsheet: only cell area will move).
    virtual void SetTargetWindow(wxWindow *target);

    // change the DC origin according to the scroll position. To properly
    // forward calls to wxWindow::Layout use WX_FORWARD_TO_SCROLL_HELPER()
    // derived class
    virtual void DoPrepareDC(wxDC& dc);

    // the methods to be called from the window event handlers
    void HandleOnScroll(wxScrollWinEvent& event);
    void HandleOnSize(wxSizeEvent& event);
#if wxUSE_MOUSEWHEEL
    void HandleOnMouseWheel(wxMouseEvent& event);
#endif // wxUSE_MOUSEWHEEL

    // these functions must be overidden in the derived class to return
    // orientation specific data (e.g. the width for vertically scrolling
    // derivatives in the case of GetOrientationTargetSize())
    virtual int GetOrientationTargetSize() const = 0;
    virtual int GetNonOrientationTargetSize() const = 0;
    virtual wxOrientation GetOrientation() const = 0;

protected:
    // all *Unit* functions are protected to be exposed by
    // wxVarScrollHelperBase implementations (with appropriate names)

    // get the number of units this window contains (previously set by
    // SetUnitCount())
    size_t GetUnitCount() const { return m_unitMax; }

    // set the number of units the helper contains: the derived class must
    // provide the sizes for all units with indices up to the one given here
    // in its OnGetUnitSize()
    void SetUnitCount(size_t count);

    // redraw the specified unit
    virtual void RefreshUnit(size_t unit);

    // redraw all units in the specified range (inclusive)
    virtual void RefreshUnits(size_t from, size_t to);

    // scroll to the specified unit: it will become the first visible unit in
    // the window
    //
    // return true if we scrolled the window, false if nothing was done
    bool DoScrollToUnit(size_t unit);

    // scroll by the specified number of units/pages
    virtual bool DoScrollUnits(int units);
    virtual bool DoScrollPages(int pages);

    // this function must be overridden in the derived class and it should
    // return the size of the given unit in pixels
    virtual wxCoord OnGetUnitSize(size_t n) const = 0;

    // this function doesn't have to be overridden but it may be useful to do
    // it if calculating the units' sizes is a relatively expensive operation
    // as it gives the user code a possibility to calculate several of them at
    // once
    //
    // OnGetUnitsSizeHint() is normally called just before OnGetUnitSize() but
    // you shouldn't rely on the latter being called for all units in the
    // interval specified here. It is also possible that OnGetUnitHeight() will
    // be called for the units outside of this interval, so this is really just
    // a hint, not a promise.
    //
    // finally note that unitMin is inclusive, while unitMax is exclusive, as
    // usual
    virtual void OnGetUnitsSizeHint(size_t WXUNUSED(unitMin),
                                    size_t WXUNUSED(unitMax)) const
        { }

    // when the number of units changes, we try to estimate the total size
    // of all units which is a rather expensive operation in terms of unit
    // access, so if the user code may estimate the average size
    // better/faster than we do, it should override this function to implement
    // its own logic
    //
    // this function should return the best guess for the total size it may
    // make
    virtual wxCoord EstimateTotalSize() const { return DoEstimateTotalSize(); }

    wxCoord DoEstimateTotalSize() const;

    // find the index of the unit we need to show to fit the specified unit on
    // the opposite side either fully or partially (depending on fullyVisible)
    size_t FindFirstVisibleFromLast(size_t last,
                                    bool fullyVisible = false) const;

    // get the total size of the units between unitMin (inclusive) and
    // unitMax (exclusive)
    wxCoord GetUnitsSize(size_t unitMin, size_t unitMax) const;

    // get the offset of the first visible unit
    wxCoord GetScrollOffset() const
        { return GetUnitsSize(0, GetVisibleBegin()); }

    // get the size of the target window
    wxSize GetTargetSize() const { return m_targetWindow->GetClientSize(); }

    void GetTargetSize(int *w, int *h)
    {
        wxSize size = GetTargetSize();
        if ( w )
            *w = size.x;
        if ( h )
            *h = size.y;
    }

    // calculate the new scroll position based on scroll event type
    size_t GetNewScrollPosition(wxScrollWinEvent& event) const;

    // replacement implementation of wxWindow::Layout virtual method.  To
    // properly forward calls to wxWindow::Layout use
    // WX_FORWARD_TO_SCROLL_HELPER() derived class
    bool ScrollLayout();

#ifdef __WXMAC__
    // queue mac window update after handling scroll event
    virtual void UpdateMacScrollWindow() { }
#endif // __WXMAC__

    // change the target window
    void DoSetTargetWindow(wxWindow *target);

    // delete the event handler we installed
    void DeleteEvtHandler();

    // helper function abstracting the orientation test: with vertical
    // orientation, it assigns the first value to x and the second one to y,
    // with horizontal orientation it reverses them, i.e. the first value is
    // assigned to y and the second one to x
    void AssignOrient(wxCoord& x, wxCoord& y, wxCoord first, wxCoord second);

    // similar to "oriented assignment" above but does "oriented increment":
    // for vertical orientation, y is incremented by the given value and x if
    // left unchanged, for horizontal orientation x is incremented
    void IncOrient(wxCoord& x, wxCoord& y, wxCoord inc);

private:
    // the total number of (logical) units
    size_t m_unitMax;

    // the total (estimated) size
    wxCoord m_sizeTotal;

    // the first currently visible unit
    size_t m_unitFirst;

    // the number of currently visible units (including the last, possibly only
    // partly, visible one)
    size_t m_nUnitsVisible;

    // accumulated mouse wheel rotation
#if wxUSE_MOUSEWHEEL
    int m_sumWheelRotation;
#endif

    // do child scrolling (used in DoPrepareDC())
    bool m_physicalScrolling;

    // handler injected into target window to forward some useful events to us
    wxVarScrollHelperEvtHandler *m_handler;
};



// ===========================================================================
// wxVarVScrollHelper
// ===========================================================================

// Provides public API functions targeted for vertical-specific scrolling,
// wrapping the functionality of wxVarScrollHelperBase.

class WXDLLIMPEXP_CORE wxVarVScrollHelper : public wxVarScrollHelperBase
{
public:
    // constructors and such
    // ---------------------

    // ctor must be given the associated window
    wxVarVScrollHelper(wxWindow *winToScroll)
        : wxVarScrollHelperBase(winToScroll)
    {
    }

    // operators

    void SetRowCount(size_t rowCount) { SetUnitCount(rowCount); }
    bool ScrollToRow(size_t row) { return DoScrollToUnit(row); }

    virtual bool ScrollRows(int rows)
        { return DoScrollUnits(rows); }
    virtual bool ScrollRowPages(int pages)
        { return DoScrollPages(pages); }

    virtual void RefreshRow(size_t row)
        { RefreshUnit(row); }
    virtual void RefreshRows(size_t from, size_t to)
        { RefreshUnits(from, to); }

    // accessors

    size_t GetRowCount() const                  { return GetUnitCount(); }
    size_t GetVisibleRowsBegin() const          { return GetVisibleBegin(); }
    size_t GetVisibleRowsEnd() const            { return GetVisibleEnd(); }
    bool IsRowVisible(size_t row) const         { return IsVisible(row); }

    virtual int GetOrientationTargetSize() const
        { return GetTargetWindow()->GetClientSize().y; }
    virtual int GetNonOrientationTargetSize() const
        { return GetTargetWindow()->GetClientSize().x; }
    virtual wxOrientation GetOrientation() const { return wxVERTICAL; }

protected:
    // this function must be overridden in the derived class and it should
    // return the size of the given row in pixels
    virtual wxCoord OnGetRowHeight(size_t n) const = 0;
    wxCoord OnGetUnitSize(size_t n) const       { return OnGetRowHeight(n); }

    virtual void OnGetRowsHeightHint(size_t WXUNUSED(rowMin),
                                     size_t WXUNUSED(rowMax)) const { }

    // forward calls to OnGetRowsHeightHint()
    virtual void OnGetUnitsSizeHint(size_t unitMin, size_t unitMax) const
        { OnGetRowsHeightHint(unitMin, unitMax); }

    // again, if not overridden, it will fall back on default method
    virtual wxCoord EstimateTotalHeight() const
        { return DoEstimateTotalSize(); }

    // forward calls to EstimateTotalHeight()
    virtual wxCoord EstimateTotalSize() const { return EstimateTotalHeight(); }

    wxCoord GetRowsHeight(size_t rowMin, size_t rowMax) const
        { return GetUnitsSize(rowMin, rowMax); }
};



// ===========================================================================
// wxVarHScrollHelper
// ===========================================================================

// Provides public API functions targeted for horizontal-specific scrolling,
// wrapping the functionality of wxVarScrollHelperBase.

class WXDLLIMPEXP_CORE wxVarHScrollHelper : public wxVarScrollHelperBase
{
public:
    // constructors and such
    // ---------------------

    // ctor must be given the associated window
    wxVarHScrollHelper(wxWindow *winToScroll)
        : wxVarScrollHelperBase(winToScroll)
    {
    }

    // operators

    void SetColumnCount(size_t columnCount)
        { SetUnitCount(columnCount); }

    bool ScrollToColumn(size_t column)
        { return DoScrollToUnit(column); }
    virtual bool ScrollColumns(int columns)
        { return DoScrollUnits(columns); }
    virtual bool ScrollColumnPages(int pages)
        { return DoScrollPages(pages); }

    virtual void RefreshColumn(size_t column)
        { RefreshUnit(column); }
    virtual void RefreshColumns(size_t from, size_t to)
        { RefreshUnits(from, to); }

    // accessors

    size_t GetColumnCount() const
        { return GetUnitCount(); }
    size_t GetVisibleColumnsBegin() const
        { return GetVisibleBegin(); }
    size_t GetVisibleColumnsEnd() const
        { return GetVisibleEnd(); }
    bool IsColumnVisible(size_t column) const
        { return IsVisible(column); }


    virtual int GetOrientationTargetSize() const
        { return GetTargetWindow()->GetClientSize().x; }
    virtual int GetNonOrientationTargetSize() const
        { return GetTargetWindow()->GetClientSize().y; }
    virtual wxOrientation GetOrientation() const   { return wxHORIZONTAL; }

protected:
    // this function must be overridden in the derived class and it should
    // return the size of the given column in pixels
    virtual wxCoord OnGetColumnWidth(size_t n) const = 0;
    wxCoord OnGetUnitSize(size_t n) const { return OnGetColumnWidth(n); }

    virtual void OnGetColumnsWidthHint(size_t WXUNUSED(columnMin),
                                        size_t WXUNUSED(columnMax)) const
        { }

    // forward calls to OnGetColumnsWidthHint()
    virtual void OnGetUnitsSizeHint(size_t unitMin, size_t unitMax) const
        { OnGetColumnsWidthHint(unitMin, unitMax); }

    // again, if not overridden, it will fall back on default method
    virtual wxCoord EstimateTotalWidth() const { return DoEstimateTotalSize(); }

    // forward calls to EstimateTotalWidth()
    virtual wxCoord EstimateTotalSize() const { return EstimateTotalWidth(); }

    wxCoord GetColumnsWidth(size_t columnMin, size_t columnMax) const
        { return GetUnitsSize(columnMin, columnMax); }
};



// ===========================================================================
// wxVarHVScrollHelper
// ===========================================================================

// Provides public API functions targeted at functions with similar names in
// both wxVScrollHelper and wxHScrollHelper so class scope doesn't need to be
// specified (since we are using multiple inheritance). It also provides
// functions to make changing values for both orientations at the same time
// easier.

class WXDLLIMPEXP_CORE wxVarHVScrollHelper : public wxVarVScrollHelper,
                                        public wxVarHScrollHelper
{
public:
    // constructors and such
    // ---------------------

    // ctor must be given the associated window
    wxVarHVScrollHelper(wxWindow *winToScroll)
        : wxVarVScrollHelper(winToScroll), wxVarHScrollHelper(winToScroll) { }

    // operators
    // ---------

    // set the number of units the window contains for each axis: the derived
    // class must provide the widths and heights for all units with indices up
    // to each of the one given here in its OnGetColumnWidth() and
    // OnGetRowHeight()
    void SetRowColumnCount(size_t rowCount, size_t columnCount);


    // with physical scrolling on, the device origin is changed properly when
    // a wxPaintDC is prepared, children are actually moved and laid out
    // properly, and the contents of the window (pixels) are actually moved
    void EnablePhysicalScrolling(bool vscrolling = true, bool hscrolling = true)
    {
        wxVarVScrollHelper::EnablePhysicalScrolling(vscrolling);
        wxVarHScrollHelper::EnablePhysicalScrolling(hscrolling);
    }

    // scroll to the specified row/column: it will become the first visible
    // cell in the window
    //
    // return true if we scrolled the window, false if nothing was done
    bool ScrollToRowColumn(size_t row, size_t column);
    bool ScrollToRowColumn(const wxPosition &pos)
        { return ScrollToRowColumn(pos.GetRow(), pos.GetColumn()); }

    // redraw the specified cell
    virtual void RefreshRowColumn(size_t row, size_t column);
    virtual void RefreshRowColumn(const wxPosition &pos)
        { RefreshRowColumn(pos.GetRow(), pos.GetColumn()); }

    // redraw the specified regions (inclusive).  If the target window for
    // both orientations is the same the rectangle of cells is refreshed; if
    // the target windows differ the entire client size opposite the
    // orientation direction is refreshed between the specified limits
    virtual void RefreshRowsColumns(size_t fromRow, size_t toRow,
                                    size_t fromColumn, size_t toColumn);
    virtual void RefreshRowsColumns(const wxPosition& from,
                                    const wxPosition& to)
    {
        RefreshRowsColumns(from.GetRow(), to.GetRow(),
                          from.GetColumn(), to.GetColumn());
    }

    // locate the virtual position from the given device coordinates
    wxPosition VirtualHitTest(wxCoord x, wxCoord y) const;
    wxPosition VirtualHitTest(const wxPoint &pos) const
        { return VirtualHitTest(pos.x, pos.y); }

    // change the DC origin according to the scroll position. To properly
    // forward calls to wxWindow::Layout use WX_FORWARD_TO_SCROLL_HELPER()
    // derived class. We use this version to call both base classes'
    // DoPrepareDC()
    virtual void DoPrepareDC(wxDC& dc);

    // replacement implementation of wxWindow::Layout virtual method.  To
    // properly forward calls to wxWindow::Layout use
    // WX_FORWARD_TO_SCROLL_HELPER() derived class. We use this version to
    // call both base classes' ScrollLayout()
    bool ScrollLayout();

    // accessors
    // ---------

    // get the number of units this window contains (previously set by
    // Set[Column/Row/RowColumn/Unit]Count())
    wxSize GetRowColumnCount() const;

    // get the first currently visible units
    wxPosition GetVisibleBegin() const;
    wxPosition GetVisibleEnd() const;

    // is this cell currently visible?
    bool IsVisible(size_t row, size_t column) const;
    bool IsVisible(const wxPosition &pos) const
        { return IsVisible(pos.GetRow(), pos.GetColumn()); }
};



#if WXWIN_COMPATIBILITY_2_8

// ===========================================================================
// wxVarVScrollLegacyAdaptor
// ===========================================================================

// Provides backwards compatible API for applications originally built using
// wxVScrolledWindow in 2.6 or 2.8. Originally, wxVScrolledWindow referred
// to scrolling "lines". We use "units" in wxVarScrollHelperBase to avoid
// implying any orientation (since the functions are used for both horizontal
// and vertical scrolling in derived classes). And in the new
// wxVScrolledWindow and wxHScrolledWindow classes, we refer to them as
// "rows" and "columns", respectively. This is to help clear some confusion
// in not only those classes, but also in wxHVScrolledWindow where functions
// are inherited from both.

class WXDLLIMPEXP_CORE wxVarVScrollLegacyAdaptor : public wxVarVScrollHelper
{
public:
    // constructors and such
    // ---------------------
    wxVarVScrollLegacyAdaptor(wxWindow *winToScroll)
        : wxVarVScrollHelper(winToScroll)
    {
    }

    // accessors
    // ---------

    // this is the same as GetVisibleRowsBegin(), exists to match
    // GetLastVisibleLine() and for backwards compatibility only
    wxDEPRECATED( size_t GetFirstVisibleLine() const );

    // get the last currently visible line
    //
    // this function is unsafe as it returns (size_t)-1 (i.e. a huge positive
    // number) if the control is empty, use GetVisibleRowsEnd() instead, this
    // one is kept for backwards compatibility
    wxDEPRECATED( size_t GetLastVisibleLine() const );

    // "line" to "unit" compatibility functions
    // ----------------------------------------

    // get the number of lines this window contains (set by SetLineCount())
    wxDEPRECATED( size_t GetLineCount() const );

    // set the number of lines the helper contains: the derived class must
    // provide the sizes for all lines with indices up to the one given here
    // in its OnGetLineHeight()
    wxDEPRECATED( void SetLineCount(size_t count) );

    // redraw the specified line
    wxDEPRECATED( virtual void RefreshLine(size_t line) );

    // redraw all lines in the specified range (inclusive)
    wxDEPRECATED( virtual void RefreshLines(size_t from, size_t to) );

    // scroll to the specified line: it will become the first visible line in
    // the window
    //
    // return true if we scrolled the window, false if nothing was done
    wxDEPRECATED( bool ScrollToLine(size_t line) );

    // scroll by the specified number of lines/pages
    wxDEPRECATED( virtual bool ScrollLines(int lines) );
    wxDEPRECATED( virtual bool ScrollPages(int pages) );

protected:
    // unless the code has been updated to override OnGetRowHeight() instead,
    // this function must be overridden in the derived class and it should
    // return the height of the given row in pixels
    wxDEPRECATED_BUT_USED_INTERNALLY(
        virtual wxCoord OnGetLineHeight(size_t n) const );

    // forwards the calls from base class pure virtual function to pure virtual
    // OnGetLineHeight instead (backwards compatible name)
    // note that we don't need to forward OnGetUnitSize() as it is already
    // forwarded to OnGetRowHeight() in wxVarVScrollHelper
    virtual wxCoord OnGetRowHeight(size_t n) const;

    // this function doesn't have to be overridden but it may be useful to do
    // it if calculating the lines heights is a relatively expensive operation
    // as it gives the user code a possibility to calculate several of them at
    // once
    //
    // OnGetLinesHint() is normally called just before OnGetLineHeight() but you
    // shouldn't rely on the latter being called for all lines in the interval
    // specified here. It is also possible that OnGetLineHeight() will be
    // called for the lines outside of this interval, so this is really just a
    // hint, not a promise.
    //
    // finally note that lineMin is inclusive, while lineMax is exclusive, as
    // usual
    wxDEPRECATED_BUT_USED_INTERNALLY( virtual void OnGetLinesHint(
        size_t lineMin, size_t lineMax) const );

    // forwards the calls from base class pure virtual function to pure virtual
    // OnGetLinesHint instead (backwards compatible name)
    void OnGetRowsHeightHint(size_t rowMin, size_t rowMax) const;
};

#else // !WXWIN_COMPATIBILITY_2_8

// shortcut to avoid checking compatibility modes later
// remove this and all references to wxVarVScrollLegacyAdaptor once
// wxWidgets 2.6 and 2.8 compatibility is removed
typedef wxVarVScrollHelper wxVarVScrollLegacyAdaptor;

#endif // WXWIN_COMPATIBILITY_2_8/!WXWIN_COMPATIBILITY_2_8


// this macro must be used in declaration of wxVarScrollHelperBase-derived
// classes
#define WX_FORWARD_TO_VAR_SCROLL_HELPER()                                     \
public:                                                                       \
    virtual void PrepareDC(wxDC& dc) { DoPrepareDC(dc); }                     \
    virtual bool Layout() { return ScrollLayout(); }



// ===========================================================================
// wxVScrolledWindow
// ===========================================================================

// In the name of this class, "V" may stand for "variable" because it can be
// used for scrolling rows of variable heights; "virtual", because it is not
// necessary to know the heights of all rows in advance -- only those which
// are shown on the screen need to be measured; or even "vertical", because
// this class only supports scrolling vertically.

// In any case, this is a generalization of the wxScrolledWindow class which
// can be only used when all rows have the same heights. It lacks some other
// wxScrolledWindow features however, notably it can't scroll only a rectangle
// of the window and not its entire client area.

class WXDLLIMPEXP_CORE wxVScrolledWindow : public wxPanel,
                                      public wxVarVScrollLegacyAdaptor
{
public:
    // constructors and such
    // ---------------------

    // default ctor, you must call Create() later
    wxVScrolledWindow() : wxVarVScrollLegacyAdaptor(this) { }

    // normal ctor, no need to call Create() after this one
    //
    // note that wxVSCROLL is always automatically added to our style, there is
    // no need to specify it explicitly
    wxVScrolledWindow(wxWindow *parent,
                      wxWindowID id = wxID_ANY,
                      const wxPoint& pos = wxDefaultPosition,
                      const wxSize& size = wxDefaultSize,
                      long style = 0,
                      const wxString& name = wxPanelNameStr)
    : wxVarVScrollLegacyAdaptor(this)
    {
        (void)Create(parent, id, pos, size, style, name);
    }

    // same as the previous ctor but returns status code: true if ok
    //
    // just as with the ctor above, wxVSCROLL style is always used, there is no
    // need to specify it
    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr)
    {
        return wxPanel::Create(parent, id, pos, size, style | wxVSCROLL, name);
    }

#if WXWIN_COMPATIBILITY_2_8
    // Make sure we prefer our version of HitTest rather than wxWindow's
    // These functions should no longer be masked in favor of VirtualHitTest()
    int HitTest(wxCoord WXUNUSED(x), wxCoord y) const
        { return wxVarVScrollHelper::VirtualHitTest(y); }
    int HitTest(const wxPoint& pt) const
        { return HitTest(pt.x, pt.y); }
#endif // WXWIN_COMPATIBILITY_2_8

    WX_FORWARD_TO_VAR_SCROLL_HELPER()

#ifdef __WXMAC__
protected:
    virtual void UpdateMacScrollWindow() { Update(); }
#endif // __WXMAC__

private:
    wxDECLARE_NO_COPY_CLASS(wxVScrolledWindow);
    DECLARE_ABSTRACT_CLASS(wxVScrolledWindow)
};



// ===========================================================================
// wxHScrolledWindow
// ===========================================================================

// In the name of this class, "H" stands for "horizontal" because it can be
// used for scrolling columns of variable widths. It is not necessary to know
// the widths of all columns in advance -- only those which are shown on the
// screen need to be measured.

// This is a generalization of the wxScrolledWindow class which can be only
// used when all columns have the same width. It lacks some other
// wxScrolledWindow features however, notably it can't scroll only a rectangle
// of the window and not its entire client area.

class WXDLLIMPEXP_CORE wxHScrolledWindow : public wxPanel,
                                      public wxVarHScrollHelper
{
public:
    // constructors and such
    // ---------------------

    // default ctor, you must call Create() later
    wxHScrolledWindow() : wxVarHScrollHelper(this) { }

    // normal ctor, no need to call Create() after this one
    //
    // note that wxHSCROLL is always automatically added to our style, there is
    // no need to specify it explicitly
    wxHScrolledWindow(wxWindow *parent,
                      wxWindowID id = wxID_ANY,
                      const wxPoint& pos = wxDefaultPosition,
                      const wxSize& size = wxDefaultSize,
                      long style = 0,
                      const wxString& name = wxPanelNameStr)
        : wxVarHScrollHelper(this)
    {
        (void)Create(parent, id, pos, size, style, name);
    }

    // same as the previous ctor but returns status code: true if ok
    //
    // just as with the ctor above, wxHSCROLL style is always used, there is no
    // need to specify it
    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr)
    {
        return wxPanel::Create(parent, id, pos, size, style | wxHSCROLL, name);
    }

    WX_FORWARD_TO_VAR_SCROLL_HELPER()

#ifdef __WXMAC__
protected:
    virtual void UpdateMacScrollWindow() { Update(); }
#endif // __WXMAC__

private:
    wxDECLARE_NO_COPY_CLASS(wxHScrolledWindow);
    DECLARE_ABSTRACT_CLASS(wxHScrolledWindow)
};



// ===========================================================================
// wxHVScrolledWindow
// ===========================================================================

// This window inherits all functionality of both vertical and horizontal
// scrolled windows automatically handling everything needed to scroll both
// axis simultaneously.

class WXDLLIMPEXP_CORE wxHVScrolledWindow : public wxPanel,
                                       public wxVarHVScrollHelper
{
public:
    // constructors and such
    // ---------------------

    // default ctor, you must call Create() later
    wxHVScrolledWindow()
        : wxPanel(),
          wxVarHVScrollHelper(this) { }

    // normal ctor, no need to call Create() after this one
    //
    // note that wxVSCROLL and wxHSCROLL are always automatically added to our
    // style, there is no need to specify them explicitly
    wxHVScrolledWindow(wxWindow *parent,
                       wxWindowID id = wxID_ANY,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& size = wxDefaultSize,
                       long style = 0,
                       const wxString& name = wxPanelNameStr)
        : wxPanel(),
          wxVarHVScrollHelper(this)
    {
        (void)Create(parent, id, pos, size, style, name);
    }

    // same as the previous ctor but returns status code: true if ok
    //
    // just as with the ctor above, wxVSCROLL and wxHSCROLL styles are always
    // used, there is no need to specify them
    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr)
    {
        return wxPanel::Create(parent, id, pos, size,
                               style | wxVSCROLL | wxHSCROLL, name);
    }

    WX_FORWARD_TO_VAR_SCROLL_HELPER()

#ifdef __WXMAC__
protected:
    virtual void UpdateMacScrollWindow() { Update(); }
#endif // __WXMAC__

private:
    wxDECLARE_NO_COPY_CLASS(wxHVScrolledWindow);
    DECLARE_ABSTRACT_CLASS(wxHVScrolledWindow)
};

#endif // _WX_VSCROLL_H_

