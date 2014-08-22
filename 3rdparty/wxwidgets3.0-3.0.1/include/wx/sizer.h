/////////////////////////////////////////////////////////////////////////////
// Name:        wx/sizer.h
// Purpose:     provide wxSizer class for layout
// Author:      Robert Roebling and Robin Dunn
// Modified by: Ron Lee, Vadim Zeitlin (wxSizerFlags)
// Created:
// Copyright:   (c) Robin Dunn, Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WXSIZER_H__
#define __WXSIZER_H__

#include "wx/defs.h"

#include "wx/window.h"

//---------------------------------------------------------------------------
// classes
//---------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxButton;
class WXDLLIMPEXP_FWD_CORE wxBoxSizer;
class WXDLLIMPEXP_FWD_CORE wxSizerItem;
class WXDLLIMPEXP_FWD_CORE wxSizer;

#ifndef wxUSE_BORDER_BY_DEFAULT
    #ifdef __SMARTPHONE__
        // no borders by default on limited size screen
        #define wxUSE_BORDER_BY_DEFAULT 0
    #else
        #define wxUSE_BORDER_BY_DEFAULT 1
    #endif
#endif

// ----------------------------------------------------------------------------
// wxSizerFlags: flags used for an item in the sizer
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSizerFlags
{
public:
    // construct the flags object initialized with the given proportion (0 by
    // default)
    wxSizerFlags(int proportion = 0) : m_proportion(proportion)
    {
        m_flags = 0;
        m_borderInPixels = 0;
    }

    // setters for all sizer flags, they all return the object itself so that
    // calls to them can be chained

    wxSizerFlags& Proportion(int proportion)
    {
        m_proportion = proportion;
        return *this;
    }

    wxSizerFlags& Expand()
    {
        m_flags |= wxEXPAND;
        return *this;
    }

    // notice that Align() replaces the current alignment flags, use specific
    // methods below such as Top(), Left() &c if you want to set just the
    // vertical or horizontal alignment
    wxSizerFlags& Align(int alignment) // combination of wxAlignment values
    {
        m_flags &= ~wxALIGN_MASK;
        m_flags |= alignment;

        return *this;
    }

    // some shortcuts for Align()
    wxSizerFlags& Centre() { return Align(wxALIGN_CENTRE); }
    wxSizerFlags& Center() { return Centre(); }

    wxSizerFlags& Top()
    {
        m_flags &= ~(wxALIGN_BOTTOM | wxALIGN_CENTRE_VERTICAL);
        return *this;
    }

    wxSizerFlags& Left()
    {
        m_flags &= ~(wxALIGN_RIGHT | wxALIGN_CENTRE_HORIZONTAL);
        return *this;
    }

    wxSizerFlags& Right()
    {
        m_flags = (m_flags & ~wxALIGN_CENTRE_HORIZONTAL) | wxALIGN_RIGHT;
        return *this;
    }

    wxSizerFlags& Bottom()
    {
        m_flags = (m_flags & ~wxALIGN_CENTRE_VERTICAL) | wxALIGN_BOTTOM;
        return *this;
    }


    // default border size used by Border() below
    static int GetDefaultBorder()
    {
#if wxUSE_BORDER_BY_DEFAULT
    #ifdef __WXGTK20__
        // GNOME HIG says to use 6px as the base unit:
        // http://library.gnome.org/devel/hig-book/stable/design-window.html.en
        return 6;
    #else
        // FIXME: default border size shouldn't be hardcoded and at the very
        //        least they should depend on the current font size
        return 5;
    #endif
#else
        return 0;
#endif
    }


    wxSizerFlags& Border(int direction, int borderInPixels)
    {
        wxCHECK_MSG( !(direction & ~wxALL), *this,
                     wxS("direction must be a combination of wxDirection ")
                     wxS("enum values.") );

        m_flags &= ~wxALL;
        m_flags |= direction;

        m_borderInPixels = borderInPixels;

        return *this;
    }

    wxSizerFlags& Border(int direction = wxALL)
    {
#if wxUSE_BORDER_BY_DEFAULT
        return Border(direction, GetDefaultBorder());
#else
        // no borders by default on limited size screen
        wxUnusedVar(direction);

        return *this;
#endif
    }

    wxSizerFlags& DoubleBorder(int direction = wxALL)
    {
#if wxUSE_BORDER_BY_DEFAULT
        return Border(direction, 2*GetDefaultBorder());
#else
        wxUnusedVar(direction);

        return *this;
#endif
    }

    wxSizerFlags& TripleBorder(int direction = wxALL)
    {
#if wxUSE_BORDER_BY_DEFAULT
        return Border(direction, 3*GetDefaultBorder());
#else
        wxUnusedVar(direction);

        return *this;
#endif
    }

    wxSizerFlags& HorzBorder()
    {
#if wxUSE_BORDER_BY_DEFAULT
        return Border(wxLEFT | wxRIGHT, GetDefaultBorder());
#else
        return *this;
#endif
    }

    wxSizerFlags& DoubleHorzBorder()
    {
#if wxUSE_BORDER_BY_DEFAULT
        return Border(wxLEFT | wxRIGHT, 2*GetDefaultBorder());
#else
        return *this;
#endif
    }

    // setters for the others flags
    wxSizerFlags& Shaped()
    {
        m_flags |= wxSHAPED;

        return *this;
    }

    wxSizerFlags& FixedMinSize()
    {
        m_flags |= wxFIXED_MINSIZE;

        return *this;
    }

    // makes the item ignore window's visibility status
    wxSizerFlags& ReserveSpaceEvenIfHidden()
    {
        m_flags |= wxRESERVE_SPACE_EVEN_IF_HIDDEN;
        return *this;
    }

    // accessors for wxSizer only
    int GetProportion() const { return m_proportion; }
    int GetFlags() const { return m_flags; }
    int GetBorderInPixels() const { return m_borderInPixels; }

private:
    int m_proportion;
    int m_flags;
    int m_borderInPixels;
};


// ----------------------------------------------------------------------------
// wxSizerSpacer: used by wxSizerItem to represent a spacer
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSizerSpacer
{
public:
    wxSizerSpacer(const wxSize& size) : m_size(size), m_isShown(true) { }

    void SetSize(const wxSize& size) { m_size = size; }
    const wxSize& GetSize() const { return m_size; }

    void Show(bool show) { m_isShown = show; }
    bool IsShown() const { return m_isShown; }

private:
    // the size, in pixel
    wxSize m_size;

    // is the spacer currently shown?
    bool m_isShown;
};

// ----------------------------------------------------------------------------
// wxSizerItem
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSizerItem : public wxObject
{
public:
    // window
    wxSizerItem( wxWindow *window,
                 int proportion=0,
                 int flag=0,
                 int border=0,
                 wxObject* userData=NULL );

    // window with flags
    wxSizerItem(wxWindow *window, const wxSizerFlags& flags)
    {
        Init(flags);

        DoSetWindow(window);
    }

    // subsizer
    wxSizerItem( wxSizer *sizer,
                 int proportion=0,
                 int flag=0,
                 int border=0,
                 wxObject* userData=NULL );

    // sizer with flags
    wxSizerItem(wxSizer *sizer, const wxSizerFlags& flags)
    {
        Init(flags);

        DoSetSizer(sizer);
    }

    // spacer
    wxSizerItem( int width,
                 int height,
                 int proportion=0,
                 int flag=0,
                 int border=0,
                 wxObject* userData=NULL);

    // spacer with flags
    wxSizerItem(int width, int height, const wxSizerFlags& flags)
    {
        Init(flags);

        DoSetSpacer(wxSize(width, height));
    }

    wxSizerItem();
    virtual ~wxSizerItem();

    virtual void DeleteWindows();

    // Enable deleting the SizerItem without destroying the contained sizer.
    void DetachSizer() { m_sizer = NULL; }

    virtual wxSize GetSize() const;
    virtual wxSize CalcMin();
    virtual void SetDimension( const wxPoint& pos, const wxSize& size );

    wxSize GetMinSize() const
        { return m_minSize; }
    wxSize GetMinSizeWithBorder() const;

    wxSize GetMaxSize() const
        { return IsWindow() ? m_window->GetMaxSize() : wxDefaultSize; }
    wxSize GetMaxSizeWithBorder() const;

    void SetMinSize(const wxSize& size)
    {
        if ( IsWindow() )
            m_window->SetMinSize(size);
        m_minSize = size;
    }
    void SetMinSize( int x, int y )
        { SetMinSize(wxSize(x, y)); }
    void SetInitSize( int x, int y )
        { SetMinSize(wxSize(x, y)); }

    // if either of dimensions is zero, ratio is assumed to be 1
    // to avoid "divide by zero" errors
    void SetRatio(int width, int height)
        { m_ratio = (width && height) ? ((float) width / (float) height) : 1; }
    void SetRatio(const wxSize& size)
        { SetRatio(size.x, size.y); }
    void SetRatio(float ratio)
        { m_ratio = ratio; }
    float GetRatio() const
        { return m_ratio; }

    virtual wxRect GetRect() { return m_rect; }

    // set a sizer item id (different from a window id, all sizer items,
    // including spacers, can have an associated id)
    void SetId(int id) { m_id = id; }
    int GetId() const { return m_id; }

    bool IsWindow() const { return m_kind == Item_Window; }
    bool IsSizer() const { return m_kind == Item_Sizer; }
    bool IsSpacer() const { return m_kind == Item_Spacer; }

#if WXWIN_COMPATIBILITY_2_6
    // Deprecated in 2.6, use {G,S}etProportion instead.
    wxDEPRECATED( void SetOption( int option ) );
    wxDEPRECATED( int GetOption() const );
#endif // WXWIN_COMPATIBILITY_2_6

    void SetProportion( int proportion )
        { m_proportion = proportion; }
    int GetProportion() const
        { return m_proportion; }
    void SetFlag( int flag )
        { m_flag = flag; }
    int GetFlag() const
        { return m_flag; }
    void SetBorder( int border )
        { m_border = border; }
    int GetBorder() const
        { return m_border; }

    wxWindow *GetWindow() const
        { return m_kind == Item_Window ? m_window : NULL; }
    wxSizer *GetSizer() const
        { return m_kind == Item_Sizer ? m_sizer : NULL; }
    wxSize GetSpacer() const;

    // This function behaves obviously for the windows and spacers but for the
    // sizers it returns true if any sizer element is shown and only returns
    // false if all of them are hidden. Also, it always returns true if
    // wxRESERVE_SPACE_EVEN_IF_HIDDEN flag was used.
    bool IsShown() const;

    void Show(bool show);

    void SetUserData(wxObject* userData)
        { delete m_userData; m_userData = userData; }
    wxObject* GetUserData() const
        { return m_userData; }
    wxPoint GetPosition() const
        { return m_pos; }

    // Called once the first component of an item has been decided. This is
    // used in algorithms that depend on knowing the size in one direction
    // before the min size in the other direction can be known.
    // Returns true if it made use of the information (and min size was changed).
    bool InformFirstDirection( int direction, int size, int availableOtherDir=-1 );

    // these functions delete the current contents of the item if it's a sizer
    // or a spacer but not if it is a window
    void AssignWindow(wxWindow *window)
    {
        Free();
        DoSetWindow(window);
    }

    void AssignSizer(wxSizer *sizer)
    {
        Free();
        DoSetSizer(sizer);
    }

    void AssignSpacer(const wxSize& size)
    {
        Free();
        DoSetSpacer(size);
    }

    void AssignSpacer(int w, int h) { AssignSpacer(wxSize(w, h)); }

#if WXWIN_COMPATIBILITY_2_8
    // these functions do not free the old sizer/spacer and so can easily
    // provoke the memory leaks and so shouldn't be used, use Assign() instead
    wxDEPRECATED( void SetWindow(wxWindow *window) );
    wxDEPRECATED( void SetSizer(wxSizer *sizer) );
    wxDEPRECATED( void SetSpacer(const wxSize& size) );
    wxDEPRECATED( void SetSpacer(int width, int height) );
#endif // WXWIN_COMPATIBILITY_2_8

protected:
    // common part of several ctors
    void Init() { m_userData = NULL; m_kind = Item_None; }

    // common part of ctors taking wxSizerFlags
    void Init(const wxSizerFlags& flags);

    // free current contents
    void Free();

    // common parts of Set/AssignXXX()
    void DoSetWindow(wxWindow *window);
    void DoSetSizer(wxSizer *sizer);
    void DoSetSpacer(const wxSize& size);

    // Add the border specified for this item to the given size
    // if it's != wxDefaultSize, just return wxDefaultSize otherwise.
    wxSize AddBorderToSize(const wxSize& size) const;

    // discriminated union: depending on m_kind one of the fields is valid
    enum
    {
        Item_None,
        Item_Window,
        Item_Sizer,
        Item_Spacer,
        Item_Max
    } m_kind;
    union
    {
        wxWindow      *m_window;
        wxSizer       *m_sizer;
        wxSizerSpacer *m_spacer;
    };

    wxPoint      m_pos;
    wxSize       m_minSize;
    int          m_proportion;
    int          m_border;
    int          m_flag;
    int          m_id;

    // on screen rectangle of this item (not including borders)
    wxRect       m_rect;

    // Aspect ratio can always be calculated from m_size,
    // but this would cause precision loss when the window
    // is shrunk.  It is safer to preserve the initial value.
    float        m_ratio;

    wxObject    *m_userData;

private:
    DECLARE_CLASS(wxSizerItem)
    wxDECLARE_NO_COPY_CLASS(wxSizerItem);
};

WX_DECLARE_EXPORTED_LIST( wxSizerItem, wxSizerItemList );


//---------------------------------------------------------------------------
// wxSizer
//---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSizer: public wxObject, public wxClientDataContainer
{
public:
    wxSizer() { m_containingWindow = NULL; }
    virtual ~wxSizer();

    // methods for adding elements to the sizer: there are Add/Insert/Prepend
    // overloads for each of window/sizer/spacer/wxSizerItem
    wxSizerItem* Add(wxWindow *window,
                     int proportion = 0,
                     int flag = 0,
                     int border = 0,
                     wxObject* userData = NULL);
    wxSizerItem* Add(wxSizer *sizer,
                     int proportion = 0,
                     int flag = 0,
                     int border = 0,
                     wxObject* userData = NULL);
    wxSizerItem* Add(int width,
                     int height,
                     int proportion = 0,
                     int flag = 0,
                     int border = 0,
                     wxObject* userData = NULL);
    wxSizerItem* Add( wxWindow *window, const wxSizerFlags& flags);
    wxSizerItem* Add( wxSizer *sizer, const wxSizerFlags& flags);
    wxSizerItem* Add( int width, int height, const wxSizerFlags& flags);
    wxSizerItem* Add( wxSizerItem *item);

    virtual wxSizerItem *AddSpacer(int size);
    wxSizerItem* AddStretchSpacer(int prop = 1);

    wxSizerItem* Insert(size_t index,
                        wxWindow *window,
                        int proportion = 0,
                        int flag = 0,
                        int border = 0,
                        wxObject* userData = NULL);
    wxSizerItem* Insert(size_t index,
                        wxSizer *sizer,
                        int proportion = 0,
                        int flag = 0,
                        int border = 0,
                        wxObject* userData = NULL);
    wxSizerItem* Insert(size_t index,
                        int width,
                        int height,
                        int proportion = 0,
                        int flag = 0,
                        int border = 0,
                        wxObject* userData = NULL);
    wxSizerItem* Insert(size_t index,
                        wxWindow *window,
                        const wxSizerFlags& flags);
    wxSizerItem* Insert(size_t index,
                        wxSizer *sizer,
                        const wxSizerFlags& flags);
    wxSizerItem* Insert(size_t index,
                        int width,
                        int height,
                        const wxSizerFlags& flags);

    // NB: do _not_ override this function in the derived classes, this one is
    //     virtual for compatibility reasons only to allow old code overriding
    //     it to continue to work, override DoInsert() instead in the new code
    virtual wxSizerItem* Insert(size_t index, wxSizerItem *item);

    wxSizerItem* InsertSpacer(size_t index, int size);
    wxSizerItem* InsertStretchSpacer(size_t index, int prop = 1);

    wxSizerItem* Prepend(wxWindow *window,
                         int proportion = 0,
                         int flag = 0,
                         int border = 0,
                         wxObject* userData = NULL);
    wxSizerItem* Prepend(wxSizer *sizer,
                         int proportion = 0,
                         int flag = 0,
                         int border = 0,
                         wxObject* userData = NULL);
    wxSizerItem* Prepend(int width,
                         int height,
                         int proportion = 0,
                         int flag = 0,
                         int border = 0,
                         wxObject* userData = NULL);
    wxSizerItem* Prepend(wxWindow *window, const wxSizerFlags& flags);
    wxSizerItem* Prepend(wxSizer *sizer, const wxSizerFlags& flags);
    wxSizerItem* Prepend(int width, int height, const wxSizerFlags& flags);
    wxSizerItem* Prepend(wxSizerItem *item);

    wxSizerItem* PrependSpacer(int size);
    wxSizerItem* PrependStretchSpacer(int prop = 1);

    // set (or possibly unset if window is NULL) or get the window this sizer
    // is used in
    void SetContainingWindow(wxWindow *window);
    wxWindow *GetContainingWindow() const { return m_containingWindow; }

#if WXWIN_COMPATIBILITY_2_6
    // Deprecated in 2.6 since historically it does not delete the window,
    // use Detach instead.
    wxDEPRECATED( virtual bool Remove( wxWindow *window ) );
#endif // WXWIN_COMPATIBILITY_2_6

    virtual bool Remove( wxSizer *sizer );
    virtual bool Remove( int index );

    virtual bool Detach( wxWindow *window );
    virtual bool Detach( wxSizer *sizer );
    virtual bool Detach( int index );

    virtual bool Replace( wxWindow *oldwin, wxWindow *newwin, bool recursive = false );
    virtual bool Replace( wxSizer *oldsz, wxSizer *newsz, bool recursive = false );
    virtual bool Replace( size_t index, wxSizerItem *newitem );

    virtual void Clear( bool delete_windows = false );
    virtual void DeleteWindows();

    // Inform sizer about the first direction that has been decided (by parent item)
    // Returns true if it made use of the information (and recalculated min size)
    virtual bool InformFirstDirection( int WXUNUSED(direction), int WXUNUSED(size), int WXUNUSED(availableOtherDir) )
        { return false; }

    void SetMinSize( int width, int height )
        { DoSetMinSize( width, height ); }
    void SetMinSize( const wxSize& size )
        { DoSetMinSize( size.x, size.y ); }

    // Searches recursively
    bool SetItemMinSize( wxWindow *window, int width, int height )
        { return DoSetItemMinSize( window, width, height ); }
    bool SetItemMinSize( wxWindow *window, const wxSize& size )
        { return DoSetItemMinSize( window, size.x, size.y ); }

    // Searches recursively
    bool SetItemMinSize( wxSizer *sizer, int width, int height )
        { return DoSetItemMinSize( sizer, width, height ); }
    bool SetItemMinSize( wxSizer *sizer, const wxSize& size )
        { return DoSetItemMinSize( sizer, size.x, size.y ); }

    bool SetItemMinSize( size_t index, int width, int height )
        { return DoSetItemMinSize( index, width, height ); }
    bool SetItemMinSize( size_t index, const wxSize& size )
        { return DoSetItemMinSize( index, size.x, size.y ); }

    wxSize GetSize() const
        { return m_size; }
    wxPoint GetPosition() const
        { return m_position; }

    // Calculate the minimal size or return m_minSize if bigger.
    wxSize GetMinSize();

    // These virtual functions are used by the layout algorithm: first
    // CalcMin() is called to calculate the minimal size of the sizer and
    // prepare for laying it out and then RecalcSizes() is called to really
    // update all the sizer items
    virtual wxSize CalcMin() = 0;
    virtual void RecalcSizes() = 0;

    virtual void Layout();

    wxSize ComputeFittingClientSize(wxWindow *window);
    wxSize ComputeFittingWindowSize(wxWindow *window);

    wxSize Fit( wxWindow *window );
    void FitInside( wxWindow *window );
    void SetSizeHints( wxWindow *window );
#if WXWIN_COMPATIBILITY_2_8
    // This only calls FitInside() since 2.9
    wxDEPRECATED( void SetVirtualSizeHints( wxWindow *window ) );
#endif

    wxSizerItemList& GetChildren()
        { return m_children; }
    const wxSizerItemList& GetChildren() const
        { return m_children; }

    void SetDimension(const wxPoint& pos, const wxSize& size)
    {
        m_position = pos;
        m_size = size;
        Layout();

        // This call is required for wxWrapSizer to be able to calculate its
        // minimal size correctly.
        InformFirstDirection(wxHORIZONTAL, size.x, size.y);
    }
    void SetDimension(int x, int y, int width, int height)
        { SetDimension(wxPoint(x, y), wxSize(width, height)); }

    size_t GetItemCount() const { return m_children.GetCount(); }
    bool IsEmpty() const { return m_children.IsEmpty(); }

    wxSizerItem* GetItem( wxWindow *window, bool recursive = false );
    wxSizerItem* GetItem( wxSizer *sizer, bool recursive = false );
    wxSizerItem* GetItem( size_t index );
    wxSizerItem* GetItemById( int id, bool recursive = false );

    // Manage whether individual scene items are considered
    // in the layout calculations or not.
    bool Show( wxWindow *window, bool show = true, bool recursive = false );
    bool Show( wxSizer *sizer, bool show = true, bool recursive = false );
    bool Show( size_t index, bool show = true );

    bool Hide( wxSizer *sizer, bool recursive = false )
        { return Show( sizer, false, recursive ); }
    bool Hide( wxWindow *window, bool recursive = false )
        { return Show( window, false, recursive ); }
    bool Hide( size_t index )
        { return Show( index, false ); }

    bool IsShown( wxWindow *window ) const;
    bool IsShown( wxSizer *sizer ) const;
    bool IsShown( size_t index ) const;

    // Recursively call wxWindow::Show () on all sizer items.
    virtual void ShowItems (bool show);

    void Show(bool show) { ShowItems(show); }

    // This is the ShowItems() counterpart and returns true if any of the sizer
    // items are shown.
    virtual bool AreAnyItemsShown() const;

protected:
    wxSize              m_size;
    wxSize              m_minSize;
    wxPoint             m_position;
    wxSizerItemList     m_children;

    // the window this sizer is used in, can be NULL
    wxWindow *m_containingWindow;

    wxSize GetMaxClientSize( wxWindow *window ) const;
    wxSize GetMinClientSize( wxWindow *window );
    wxSize VirtualFitSize( wxWindow *window );

    virtual void DoSetMinSize( int width, int height );
    virtual bool DoSetItemMinSize( wxWindow *window, int width, int height );
    virtual bool DoSetItemMinSize( wxSizer *sizer, int width, int height );
    virtual bool DoSetItemMinSize( size_t index, int width, int height );

    // insert a new item into m_children at given index and return the item
    // itself
    virtual wxSizerItem* DoInsert(size_t index, wxSizerItem *item);

private:
    DECLARE_CLASS(wxSizer)
};

//---------------------------------------------------------------------------
// wxGridSizer
//---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGridSizer: public wxSizer
{
public:
    // ctors specifying the number of columns only: number of rows will be
    // deduced automatically depending on the number of sizer elements
    wxGridSizer( int cols, int vgap, int hgap );
    wxGridSizer( int cols, const wxSize& gap = wxSize(0, 0) );

    // ctors specifying the number of rows and columns
    wxGridSizer( int rows, int cols, int vgap, int hgap );
    wxGridSizer( int rows, int cols, const wxSize& gap );

    virtual void RecalcSizes();
    virtual wxSize CalcMin();

    void SetCols( int cols )
    {
        wxASSERT_MSG( cols >= 0, "Number of columns must be non-negative");
        m_cols = cols;
    }

    void SetRows( int rows )
    {
        wxASSERT_MSG( rows >= 0, "Number of rows must be non-negative");
        m_rows = rows;
    }

    void SetVGap( int gap )     { m_vgap = gap; }
    void SetHGap( int gap )     { m_hgap = gap; }
    int GetCols() const         { return m_cols; }
    int GetRows() const         { return m_rows; }
    int GetVGap() const         { return m_vgap; }
    int GetHGap() const         { return m_hgap; }

    int GetEffectiveColsCount() const   { return m_cols ? m_cols : CalcCols(); }
    int GetEffectiveRowsCount() const   { return m_rows ? m_rows : CalcRows(); }

    // return the number of total items and the number of columns and rows
    // (for internal use only)
    int CalcRowsCols(int& rows, int& cols) const;

protected:
    // the number of rows/columns in the sizer, if 0 then it is determined
    // dynamically depending on the total number of items
    int    m_rows;
    int    m_cols;

    // gaps between rows and columns
    int    m_vgap;
    int    m_hgap;

    virtual wxSizerItem *DoInsert(size_t index, wxSizerItem *item);

    void SetItemBounds( wxSizerItem *item, int x, int y, int w, int h );

    // returns the number of columns/rows needed for the current total number
    // of children (and the fixed number of rows/columns)
    int CalcCols() const
    {
        wxCHECK_MSG
        (
            m_rows, 0,
            "Can't calculate number of cols if number of rows is not specified"
        );

        return int(m_children.GetCount() + m_rows - 1) / m_rows;
    }

    int CalcRows() const
    {
        wxCHECK_MSG
        (
            m_cols, 0,
            "Can't calculate number of cols if number of rows is not specified"
        );

        return int(m_children.GetCount() + m_cols - 1) / m_cols;
    }

private:
    DECLARE_CLASS(wxGridSizer)
};

//---------------------------------------------------------------------------
// wxFlexGridSizer
//---------------------------------------------------------------------------

// values which define the behaviour for resizing wxFlexGridSizer cells in the
// "non-flexible" direction
enum wxFlexSizerGrowMode
{
    // don't resize the cells in non-flexible direction at all
    wxFLEX_GROWMODE_NONE,

    // uniformly resize only the specified ones (default)
    wxFLEX_GROWMODE_SPECIFIED,

    // uniformly resize all cells
    wxFLEX_GROWMODE_ALL
};

class WXDLLIMPEXP_CORE wxFlexGridSizer: public wxGridSizer
{
public:
    // ctors specifying the number of columns only: number of rows will be
    // deduced automatically depending on the number of sizer elements
    wxFlexGridSizer( int cols, int vgap, int hgap );
    wxFlexGridSizer( int cols, const wxSize& gap = wxSize(0, 0) );

    // ctors specifying the number of rows and columns
    wxFlexGridSizer( int rows, int cols, int vgap, int hgap );
    wxFlexGridSizer( int rows, int cols, const wxSize& gap );

    // dtor
    virtual ~wxFlexGridSizer();

    // set the rows/columns which will grow (the others will remain of the
    // constant initial size)
    void AddGrowableRow( size_t idx, int proportion = 0 );
    void RemoveGrowableRow( size_t idx );
    void AddGrowableCol( size_t idx, int proportion = 0 );
    void RemoveGrowableCol( size_t idx );

    bool IsRowGrowable( size_t idx );
    bool IsColGrowable( size_t idx );

    // the sizer cells may grow in both directions, not grow at all or only
    // grow in one direction but not the other

    // the direction may be wxVERTICAL, wxHORIZONTAL or wxBOTH (default)
    void SetFlexibleDirection(int direction) { m_flexDirection = direction; }
    int GetFlexibleDirection() const { return m_flexDirection; }

    // note that the grow mode only applies to the direction which is not
    // flexible
    void SetNonFlexibleGrowMode(wxFlexSizerGrowMode mode) { m_growMode = mode; }
    wxFlexSizerGrowMode GetNonFlexibleGrowMode() const { return m_growMode; }

    // Read-only access to the row heights and col widths arrays
    const wxArrayInt& GetRowHeights() const { return m_rowHeights; }
    const wxArrayInt& GetColWidths() const  { return m_colWidths; }

    // implementation
    virtual void RecalcSizes();
    virtual wxSize CalcMin();

protected:
    void AdjustForFlexDirection();
    void AdjustForGrowables(const wxSize& sz);
    void FindWidthsAndHeights(int nrows, int ncols);

    // the heights/widths of all rows/columns
    wxArrayInt  m_rowHeights,
                m_colWidths;

    // indices of the growable columns and rows
    wxArrayInt  m_growableRows,
                m_growableCols;

    // proportion values of the corresponding growable rows and columns
    wxArrayInt  m_growableRowsProportions,
                m_growableColsProportions;

    // parameters describing whether the growable cells should be resized in
    // both directions or only one
    int m_flexDirection;
    wxFlexSizerGrowMode m_growMode;

    // saves CalcMin result to optimize RecalcSizes
    wxSize m_calculatedMinSize;

private:
    DECLARE_CLASS(wxFlexGridSizer)
    wxDECLARE_NO_COPY_CLASS(wxFlexGridSizer);
};

//---------------------------------------------------------------------------
// wxBoxSizer
//---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBoxSizer: public wxSizer
{
public:
    wxBoxSizer(int orient)
    {
        m_orient = orient;
        m_totalProportion = 0;

        wxASSERT_MSG( m_orient == wxHORIZONTAL || m_orient == wxVERTICAL,
                      wxT("invalid value for wxBoxSizer orientation") );
    }

    virtual wxSizerItem *AddSpacer(int size);

    int GetOrientation() const { return m_orient; }

    bool IsVertical() const { return m_orient == wxVERTICAL; }

    void SetOrientation(int orient) { m_orient = orient; }

    // implementation of our resizing logic
    virtual wxSize CalcMin();
    virtual void RecalcSizes();

protected:
    // helpers for our code: this returns the component of the given wxSize in
    // the direction of the sizer and in the other direction, respectively
    int GetSizeInMajorDir(const wxSize& sz) const
    {
        return m_orient == wxHORIZONTAL ? sz.x : sz.y;
    }

    int& SizeInMajorDir(wxSize& sz)
    {
        return m_orient == wxHORIZONTAL ? sz.x : sz.y;
    }

    int& PosInMajorDir(wxPoint& pt)
    {
        return m_orient == wxHORIZONTAL ? pt.x : pt.y;
    }

    int GetSizeInMinorDir(const wxSize& sz) const
    {
        return m_orient == wxHORIZONTAL ? sz.y : sz.x;
    }

    int& SizeInMinorDir(wxSize& sz)
    {
        return m_orient == wxHORIZONTAL ? sz.y : sz.x;
    }

    int& PosInMinorDir(wxPoint& pt)
    {
        return m_orient == wxHORIZONTAL ? pt.y : pt.x;
    }

    // another helper: creates wxSize from major and minor components
    wxSize SizeFromMajorMinor(int major, int minor) const
    {
        if ( m_orient == wxHORIZONTAL )
        {
            return wxSize(major, minor);
        }
        else // wxVERTICAL
        {
            return wxSize(minor, major);
        }
    }


    // either wxHORIZONTAL or wxVERTICAL
    int m_orient;

    // the sum of proportion of all of our elements
    int m_totalProportion;

    // the minimal size needed for this sizer as calculated by the last call to
    // our CalcMin()
    wxSize m_minSize;

private:
    DECLARE_CLASS(wxBoxSizer)
};

//---------------------------------------------------------------------------
// wxStaticBoxSizer
//---------------------------------------------------------------------------

#if wxUSE_STATBOX

class WXDLLIMPEXP_FWD_CORE wxStaticBox;

class WXDLLIMPEXP_CORE wxStaticBoxSizer: public wxBoxSizer
{
public:
    wxStaticBoxSizer(wxStaticBox *box, int orient);
    wxStaticBoxSizer(int orient, wxWindow *win, const wxString& label = wxEmptyString);
    virtual ~wxStaticBoxSizer();

    void RecalcSizes();
    wxSize CalcMin();

    wxStaticBox *GetStaticBox() const
        { return m_staticBox; }

    // override to hide/show the static box as well
    virtual void ShowItems (bool show);
    virtual bool AreAnyItemsShown() const;

    virtual bool Detach( wxWindow *window );
    virtual bool Detach( wxSizer *sizer ) { return wxBoxSizer::Detach(sizer); }
    virtual bool Detach( int index ) { return wxBoxSizer::Detach(index); }

protected:
    wxStaticBox   *m_staticBox;

private:
    DECLARE_CLASS(wxStaticBoxSizer)
    wxDECLARE_NO_COPY_CLASS(wxStaticBoxSizer);
};

#endif // wxUSE_STATBOX

//---------------------------------------------------------------------------
// wxStdDialogButtonSizer
//---------------------------------------------------------------------------

#if wxUSE_BUTTON

class WXDLLIMPEXP_CORE wxStdDialogButtonSizer: public wxBoxSizer
{
public:
    // Constructor just creates a new wxBoxSizer, not much else.
    // Box sizer orientation is automatically determined here:
    // vertical for PDAs, horizontal for everything else?
    wxStdDialogButtonSizer();

    // Checks button ID against system IDs and sets one of the pointers below
    // to this button. Does not do any sizer-related things here.
    void AddButton(wxButton *button);

    // Use these if no standard ID can/should be used
    void SetAffirmativeButton( wxButton *button );
    void SetNegativeButton( wxButton *button );
    void SetCancelButton( wxButton *button );

    // All platform-specific code here, checks which buttons exist and add
    // them to the sizer accordingly.
    // Note - one potential hack on Mac we could use here,
    // if m_buttonAffirmative is wxID_SAVE then ensure wxID_SAVE
    // is set to _("Save") and m_buttonNegative is set to _("Don't Save")
    // I wouldn't add any other hacks like that into here,
    // but this one I can see being useful.
    void Realize();

    wxButton *GetAffirmativeButton() const { return m_buttonAffirmative; }
    wxButton *GetApplyButton() const { return m_buttonApply; }
    wxButton *GetNegativeButton() const { return m_buttonNegative; }
    wxButton *GetCancelButton() const { return m_buttonCancel; }
    wxButton *GetHelpButton() const { return m_buttonHelp; }

protected:
    wxButton *m_buttonAffirmative;  // wxID_OK, wxID_YES, wxID_SAVE go here
    wxButton *m_buttonApply;        // wxID_APPLY
    wxButton *m_buttonNegative;     // wxID_NO
    wxButton *m_buttonCancel;       // wxID_CANCEL, wxID_CLOSE
    wxButton *m_buttonHelp;         // wxID_HELP, wxID_CONTEXT_HELP

private:
    DECLARE_CLASS(wxStdDialogButtonSizer)
    wxDECLARE_NO_COPY_CLASS(wxStdDialogButtonSizer);
};

#endif // wxUSE_BUTTON


// ----------------------------------------------------------------------------
// inline functions implementation
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_8

inline void wxSizerItem::SetWindow(wxWindow *window)
{
    DoSetWindow(window);
}

inline void wxSizerItem::SetSizer(wxSizer *sizer)
{
    DoSetSizer(sizer);
}

inline void wxSizerItem::SetSpacer(const wxSize& size)
{
    DoSetSpacer(size);
}

inline void wxSizerItem::SetSpacer(int width, int height)
{
    DoSetSpacer(wxSize(width, height));
}

#endif // WXWIN_COMPATIBILITY_2_8

inline wxSizerItem*
wxSizer::Insert(size_t index, wxSizerItem *item)
{
    return DoInsert(index, item);
}


inline wxSizerItem*
wxSizer::Add( wxSizerItem *item )
{
    return Insert( m_children.GetCount(), item );
}

inline wxSizerItem*
wxSizer::Add( wxWindow *window, int proportion, int flag, int border, wxObject* userData )
{
    return Add( new wxSizerItem( window, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Add( wxSizer *sizer, int proportion, int flag, int border, wxObject* userData )
{
    return Add( new wxSizerItem( sizer, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Add( int width, int height, int proportion, int flag, int border, wxObject* userData )
{
    return Add( new wxSizerItem( width, height, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Add( wxWindow *window, const wxSizerFlags& flags )
{
    return Add( new wxSizerItem(window, flags) );
}

inline wxSizerItem*
wxSizer::Add( wxSizer *sizer, const wxSizerFlags& flags )
{
    return Add( new wxSizerItem(sizer, flags) );
}

inline wxSizerItem*
wxSizer::Add( int width, int height, const wxSizerFlags& flags )
{
    return Add( new wxSizerItem(width, height, flags) );
}

inline wxSizerItem*
wxSizer::AddSpacer(int size)
{
    return Add(size, size);
}

inline wxSizerItem*
wxSizer::AddStretchSpacer(int prop)
{
    return Add(0, 0, prop);
}

inline wxSizerItem*
wxSizer::Prepend( wxSizerItem *item )
{
    return Insert( 0, item );
}

inline wxSizerItem*
wxSizer::Prepend( wxWindow *window, int proportion, int flag, int border, wxObject* userData )
{
    return Prepend( new wxSizerItem( window, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Prepend( wxSizer *sizer, int proportion, int flag, int border, wxObject* userData )
{
    return Prepend( new wxSizerItem( sizer, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Prepend( int width, int height, int proportion, int flag, int border, wxObject* userData )
{
    return Prepend( new wxSizerItem( width, height, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::PrependSpacer(int size)
{
    return Prepend(size, size);
}

inline wxSizerItem*
wxSizer::PrependStretchSpacer(int prop)
{
    return Prepend(0, 0, prop);
}

inline wxSizerItem*
wxSizer::Prepend( wxWindow *window, const wxSizerFlags& flags )
{
    return Prepend( new wxSizerItem(window, flags) );
}

inline wxSizerItem*
wxSizer::Prepend( wxSizer *sizer, const wxSizerFlags& flags )
{
    return Prepend( new wxSizerItem(sizer, flags) );
}

inline wxSizerItem*
wxSizer::Prepend( int width, int height, const wxSizerFlags& flags )
{
    return Prepend( new wxSizerItem(width, height, flags) );
}

inline wxSizerItem*
wxSizer::Insert( size_t index,
                 wxWindow *window,
                 int proportion,
                 int flag,
                 int border,
                 wxObject* userData )
{
    return Insert( index, new wxSizerItem( window, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Insert( size_t index,
                 wxSizer *sizer,
                 int proportion,
                 int flag,
                 int border,
                 wxObject* userData )
{
    return Insert( index, new wxSizerItem( sizer, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Insert( size_t index,
                 int width,
                 int height,
                 int proportion,
                 int flag,
                 int border,
                 wxObject* userData )
{
    return Insert( index, new wxSizerItem( width, height, proportion, flag, border, userData ) );
}

inline wxSizerItem*
wxSizer::Insert( size_t index, wxWindow *window, const wxSizerFlags& flags )
{
    return Insert( index, new wxSizerItem(window, flags) );
}

inline wxSizerItem*
wxSizer::Insert( size_t index, wxSizer *sizer, const wxSizerFlags& flags )
{
    return Insert( index, new wxSizerItem(sizer, flags) );
}

inline wxSizerItem*
wxSizer::Insert( size_t index, int width, int height, const wxSizerFlags& flags )
{
    return Insert( index, new wxSizerItem(width, height, flags) );
}

inline wxSizerItem*
wxSizer::InsertSpacer(size_t index, int size)
{
    return Insert(index, size, size);
}

inline wxSizerItem*
wxSizer::InsertStretchSpacer(size_t index, int prop)
{
    return Insert(index, 0, 0, prop);
}

#endif // __WXSIZER_H__
