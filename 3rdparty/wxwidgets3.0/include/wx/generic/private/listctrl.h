/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/private/listctrl.h
// Purpose:     private definitions of wxListCtrl helpers
// Author:      Robert Roebling
//              Vadim Zeitlin (virtual list control support)
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_LISTCTRL_PRIVATE_H_
#define _WX_GENERIC_LISTCTRL_PRIVATE_H_

#include "wx/defs.h"

#if wxUSE_LISTCTRL

#include "wx/listctrl.h"
#include "wx/selstore.h"
#include "wx/timer.h"
#include "wx/settings.h"

// ============================================================================
// private classes
// ============================================================================

//-----------------------------------------------------------------------------
//  wxColWidthInfo (internal)
//-----------------------------------------------------------------------------

struct wxColWidthInfo
{
    int     nMaxWidth;
    bool    bNeedsUpdate;   //  only set to true when an item whose
                            //  width == nMaxWidth is removed

    wxColWidthInfo(int w = 0, bool needsUpdate = false)
    {
        nMaxWidth = w;
        bNeedsUpdate = needsUpdate;
    }
};

WX_DEFINE_ARRAY_PTR(wxColWidthInfo *, ColWidthArray);

//-----------------------------------------------------------------------------
//  wxListItemData (internal)
//-----------------------------------------------------------------------------

class wxListItemData
{
public:
    wxListItemData(wxListMainWindow *owner);
    ~wxListItemData();

    void SetItem( const wxListItem &info );
    void SetImage( int image ) { m_image = image; }
    void SetData( wxUIntPtr data ) { m_data = data; }
    void SetPosition( int x, int y );
    void SetSize( int width, int height );

    bool HasText() const { return !m_text.empty(); }
    const wxString& GetText() const { return m_text; }
    void SetText(const wxString& text) { m_text = text; }

    // we can't use empty string for measuring the string width/height, so
    // always return something
    wxString GetTextForMeasuring() const
    {
        wxString s = GetText();
        if ( s.empty() )
            s = wxT('H');

        return s;
    }

    bool IsHit( int x, int y ) const;

    int GetX() const;
    int GetY() const;
    int GetWidth() const;
    int GetHeight() const;

    int GetImage() const { return m_image; }
    bool HasImage() const { return GetImage() != -1; }

    void GetItem( wxListItem &info ) const;

    void SetAttr(wxListItemAttr *attr) { m_attr = attr; }
    wxListItemAttr *GetAttr() const { return m_attr; }

public:
    // the item image or -1
    int m_image;

    // user data associated with the item
    wxUIntPtr m_data;

    // the item coordinates are not used in report mode; instead this pointer is
    // NULL and the owner window is used to retrieve the item position and size
    wxRect *m_rect;

    // the list ctrl we are in
    wxListMainWindow *m_owner;

    // custom attributes or NULL
    wxListItemAttr *m_attr;

protected:
    // common part of all ctors
    void Init();

    wxString m_text;
};

//-----------------------------------------------------------------------------
//  wxListHeaderData (internal)
//-----------------------------------------------------------------------------

class wxListHeaderData : public wxObject
{
public:
    wxListHeaderData();
    wxListHeaderData( const wxListItem &info );
    void SetItem( const wxListItem &item );
    void SetPosition( int x, int y );
    void SetWidth( int w );
    void SetState( int state );
    void SetFormat( int format );
    void SetHeight( int h );
    bool HasImage() const;

    bool HasText() const { return !m_text.empty(); }
    const wxString& GetText() const { return m_text; }
    void SetText(const wxString& text) { m_text = text; }

    void GetItem( wxListItem &item );

    bool IsHit( int x, int y ) const;
    int GetImage() const;
    int GetWidth() const;
    int GetFormat() const;
    int GetState() const;

protected:
    long      m_mask;
    int       m_image;
    wxString  m_text;
    int       m_format;
    int       m_width;
    int       m_xpos,
              m_ypos;
    int       m_height;
    int       m_state;

private:
    void Init();
};

//-----------------------------------------------------------------------------
//  wxListLineData (internal)
//-----------------------------------------------------------------------------

WX_DECLARE_LIST(wxListItemData, wxListItemDataList);

class wxListLineData
{
public:
    // the list of subitems: only may have more than one item in report mode
    wxListItemDataList m_items;

    // this is not used in report view
    struct GeometryInfo
    {
        // total item rect
        wxRect m_rectAll;

        // label only
        wxRect m_rectLabel;

        // icon only
        wxRect m_rectIcon;

        // the part to be highlighted
        wxRect m_rectHighlight;

        // extend all our rects to be centered inside the one of given width
        void ExtendWidth(wxCoord w)
        {
            wxASSERT_MSG( m_rectAll.width <= w,
                            wxT("width can only be increased") );

            m_rectAll.width = w;
            m_rectLabel.x = m_rectAll.x + (w - m_rectLabel.width) / 2;
            m_rectIcon.x = m_rectAll.x + (w - m_rectIcon.width) / 2;
            m_rectHighlight.x = m_rectAll.x + (w - m_rectHighlight.width) / 2;
        }
    }
    *m_gi;

    // is this item selected? [NB: not used in virtual mode]
    bool m_highlighted;

    // back pointer to the list ctrl
    wxListMainWindow *m_owner;

public:
    wxListLineData(wxListMainWindow *owner);

    ~wxListLineData()
    {
        WX_CLEAR_LIST(wxListItemDataList, m_items);
        delete m_gi;
    }

    // called by the owner when it toggles report view
    void SetReportView(bool inReportView)
    {
        // we only need m_gi when we're not in report view so update as needed
        if ( inReportView )
        {
            delete m_gi;
            m_gi = NULL;
        }
        else
        {
            m_gi = new GeometryInfo;
        }
    }

    // are we in report mode?
    inline bool InReportView() const;

    // are we in virtual report mode?
    inline bool IsVirtual() const;

    // these 2 methods shouldn't be called for report view controls, in that
    // case we determine our position/size ourselves

    // calculate the size of the line
    void CalculateSize( wxDC *dc, int spacing );

    // remember the position this line appears at
    void SetPosition( int x, int y, int spacing );

    // wxListCtrl API

    void SetImage( int image ) { SetImage(0, image); }
    int GetImage() const { return GetImage(0); }
    void SetImage( int index, int image );
    int GetImage( int index ) const;

    bool HasImage() const { return GetImage() != -1; }
    bool HasText() const { return !GetText(0).empty(); }

    void SetItem( int index, const wxListItem &info );
    void GetItem( int index, wxListItem &info );

    wxString GetText(int index) const;
    void SetText( int index, const wxString& s );

    wxListItemAttr *GetAttr() const;
    void SetAttr(wxListItemAttr *attr);

    // return true if the highlighting really changed
    bool Highlight( bool on );

    void ReverseHighlight();

    bool IsHighlighted() const
    {
        wxASSERT_MSG( !IsVirtual(), wxT("unexpected call to IsHighlighted") );

        return m_highlighted;
    }

    // draw the line on the given DC in icon/list mode
    void Draw( wxDC *dc, bool current );

    // the same in report mode: it needs more parameters as we don't store
    // everything in the item in report mode
    void DrawInReportMode( wxDC *dc,
                           const wxRect& rect,
                           const wxRect& rectHL,
                           bool highlighted,
                           bool current );

private:
    // set the line to contain num items (only can be > 1 in report mode)
    void InitItems( int num );

    // get the mode (i.e. style)  of the list control
    inline int GetMode() const;

    // Apply this item attributes to the given DC: set the text font and colour
    // and also erase the background appropriately.
    void ApplyAttributes(wxDC *dc,
                         const wxRect& rectHL,
                         bool highlighted,
                         bool current);

    // draw the text on the DC with the correct justification; also add an
    // ellipsis if the text is too large to fit in the current width
    void DrawTextFormatted(wxDC *dc,
                           const wxString &text,
                           int col,
                           int x,
                           int yMid,    // this is middle, not top, of the text
                           int width);
};

WX_DECLARE_OBJARRAY(wxListLineData, wxListLineDataArray);

//-----------------------------------------------------------------------------
//  wxListHeaderWindow (internal)
//-----------------------------------------------------------------------------

class wxListHeaderWindow : public wxWindow
{
protected:
    wxListMainWindow  *m_owner;
    const wxCursor    *m_currentCursor;
    wxCursor          *m_resizeCursor;
    bool               m_isDragging;

    // column being resized or -1
    int m_column;

    // divider line position in logical (unscrolled) coords
    int m_currentX;

    // minimal position beyond which the divider line
    // can't be dragged in logical coords
    int m_minX;

public:
    wxListHeaderWindow();

    // We provide only Create(), not the ctor, because we need to create the
    // C++ object before creating the window, see the explanations in
    // CreateOrDestroyHeaderWindowAsNeeded()
    bool Create( wxWindow *win,
                 wxWindowID id,
                 wxListMainWindow *owner,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = 0,
                 const wxString &name = wxT("wxlistctrlcolumntitles") );

    virtual ~wxListHeaderWindow();

    // We never need focus as we don't have any keyboard interface.
    virtual bool AcceptsFocus() const { return false; }

    void DrawCurrent();
    void AdjustDC( wxDC& dc );

    void OnPaint( wxPaintEvent &event );
    void OnMouse( wxMouseEvent &event );

    // needs refresh
    bool m_dirty;

    // Update main window's column later
    bool m_sendSetColumnWidth;
    int m_colToSend;
    int m_widthToSend;

    virtual void OnInternalIdle();

private:
    // common part of all ctors
    void Init();

    // generate and process the list event of the given type, return true if
    // it wasn't vetoed, i.e. if we should proceed
    bool SendListEvent(wxEventType type, const wxPoint& pos);

    DECLARE_EVENT_TABLE()
};

//-----------------------------------------------------------------------------
// wxListRenameTimer (internal)
//-----------------------------------------------------------------------------

class wxListRenameTimer: public wxTimer
{
private:
    wxListMainWindow *m_owner;

public:
    wxListRenameTimer( wxListMainWindow *owner );
    void Notify();
};

//-----------------------------------------------------------------------------
// wxListFindTimer (internal)
//-----------------------------------------------------------------------------

class wxListFindTimer: public wxTimer
{
public:
    // reset the current prefix after half a second of inactivity
    enum { DELAY = 500 };

    wxListFindTimer( wxListMainWindow *owner )
        : m_owner(owner)
    {
    }

    virtual void Notify();

private:
    wxListMainWindow *m_owner;
};

//-----------------------------------------------------------------------------
// wxListTextCtrlWrapper: wraps a wxTextCtrl to make it work for inline editing
//-----------------------------------------------------------------------------

class wxListTextCtrlWrapper : public wxEvtHandler
{
public:
    // NB: text must be a valid object but not Create()d yet
    wxListTextCtrlWrapper(wxListMainWindow *owner,
                          wxTextCtrl *text,
                          size_t itemEdit);

    wxTextCtrl *GetText() const { return m_text; }

    // Check if the given key event should stop editing and return true if it
    // does or false otherwise.
    bool CheckForEndEditKey(const wxKeyEvent& event);

    // Different reasons for calling EndEdit():
    //
    // It was called because:
    enum EndReason
    {
        End_Accept,     // user has accepted the changes.
        End_Discard,    // user has cancelled editing.
        End_Destroy     // the entire control is being destroyed.
    };

    void EndEdit(EndReason reason);

protected:
    void OnChar( wxKeyEvent &event );
    void OnKeyUp( wxKeyEvent &event );
    void OnKillFocus( wxFocusEvent &event );

    bool AcceptChanges();
    void Finish( bool setfocus );

private:
    wxListMainWindow   *m_owner;
    wxTextCtrl         *m_text;
    wxString            m_startValue;
    size_t              m_itemEdited;
    bool                m_aboutToFinish;

    DECLARE_EVENT_TABLE()
};

//-----------------------------------------------------------------------------
//  wxListMainWindow (internal)
//-----------------------------------------------------------------------------

WX_DECLARE_LIST(wxListHeaderData, wxListHeaderDataList);

class wxListMainWindow : public wxWindow
{
public:
    wxListMainWindow();
    wxListMainWindow( wxWindow *parent,
                      wxWindowID id,
                      const wxPoint& pos,
                      const wxSize& size );

    virtual ~wxListMainWindow();

    // called by the main control when its mode changes
    void SetReportView(bool inReportView);

    // helper to simplify testing for wxLC_XXX flags
    bool HasFlag(int flag) const { return m_parent->HasFlag(flag); }

    // return true if this is a virtual list control
    bool IsVirtual() const { return HasFlag(wxLC_VIRTUAL); }

    // return true if the control is in report mode
    bool InReportView() const { return HasFlag(wxLC_REPORT); }

    // return true if we are in single selection mode, false if multi sel
    bool IsSingleSel() const { return HasFlag(wxLC_SINGLE_SEL); }

    // do we have a header window?
    bool HasHeader() const
        { return InReportView() && !HasFlag(wxLC_NO_HEADER); }

    void HighlightAll( bool on );

    // all these functions only do something if the line is currently visible

    // change the line "selected" state, return true if it really changed
    bool HighlightLine( size_t line, bool highlight = true);

    // as HighlightLine() but do it for the range of lines: this is incredibly
    // more efficient for virtual list controls!
    //
    // NB: unlike HighlightLine() this one does refresh the lines on screen
    void HighlightLines( size_t lineFrom, size_t lineTo, bool on = true );

    // toggle the line state and refresh it
    void ReverseHighlight( size_t line )
        { HighlightLine(line, !IsHighlighted(line)); RefreshLine(line); }

    // return true if the line is highlighted
    bool IsHighlighted(size_t line) const;

    // refresh one or several lines at once
    void RefreshLine( size_t line );
    void RefreshLines( size_t lineFrom, size_t lineTo );

    // refresh all selected items
    void RefreshSelected();

    // refresh all lines below the given one: the difference with
    // RefreshLines() is that the index here might not be a valid one (happens
    // when the last line is deleted)
    void RefreshAfter( size_t lineFrom );

    // the methods which are forwarded to wxListLineData itself in list/icon
    // modes but are here because the lines don't store their positions in the
    // report mode

    // get the bound rect for the entire line
    wxRect GetLineRect(size_t line) const;

    // get the bound rect of the label
    wxRect GetLineLabelRect(size_t line) const;

    // get the bound rect of the items icon (only may be called if we do have
    // an icon!)
    wxRect GetLineIconRect(size_t line) const;

    // get the rect to be highlighted when the item has focus
    wxRect GetLineHighlightRect(size_t line) const;

    // get the size of the total line rect
    wxSize GetLineSize(size_t line) const
        { return GetLineRect(line).GetSize(); }

    // return the hit code for the corresponding position (in this line)
    long HitTestLine(size_t line, int x, int y) const;

    // bring the selected item into view, scrolling to it if necessary
    void MoveToItem(size_t item);

    bool ScrollList( int WXUNUSED(dx), int dy );

    // bring the current item into view
    void MoveToFocus() { MoveToItem(m_current); }

    // start editing the label of the given item
    wxTextCtrl *EditLabel(long item,
                          wxClassInfo* textControlClass = wxCLASSINFO(wxTextCtrl));

    bool EndEditLabel(bool cancel);

    wxTextCtrl *GetEditControl() const
    {
        return m_textctrlWrapper ? m_textctrlWrapper->GetText() : NULL;
    }

    void ResetTextControl(wxTextCtrl *text)
    {
        delete text;
        m_textctrlWrapper = NULL;
    }

    void OnRenameTimer();
    bool OnRenameAccept(size_t itemEdit, const wxString& value);
    void OnRenameCancelled(size_t itemEdit);

    void OnFindTimer();
    // set whether or not to ring the find bell
    // (does nothing on MSW - bell is always rung)
    void EnableBellOnNoMatch( bool on );

    void OnMouse( wxMouseEvent &event );

    // called to switch the selection from the current item to newCurrent,
    void OnArrowChar( size_t newCurrent, const wxKeyEvent& event );

    void OnCharHook( wxKeyEvent &event );
    void OnChar( wxKeyEvent &event );
    void OnKeyDown( wxKeyEvent &event );
    void OnKeyUp( wxKeyEvent &event );
    void OnSetFocus( wxFocusEvent &event );
    void OnKillFocus( wxFocusEvent &event );
    void OnScroll( wxScrollWinEvent& event );

    void OnPaint( wxPaintEvent &event );

    void OnChildFocus(wxChildFocusEvent& event);

    void DrawImage( int index, wxDC *dc, int x, int y );
    void GetImageSize( int index, int &width, int &height ) const;

    void SetImageList( wxImageList *imageList, int which );
    void SetItemSpacing( int spacing, bool isSmall = false );
    int GetItemSpacing( bool isSmall = false );

    void SetColumn( int col, const wxListItem &item );
    void SetColumnWidth( int col, int width );
    void GetColumn( int col, wxListItem &item ) const;
    int GetColumnWidth( int col ) const;
    int GetColumnCount() const { return m_columns.GetCount(); }

    // returns the sum of the heights of all columns
    int GetHeaderWidth() const;

    int GetCountPerPage() const;

    void SetItem( wxListItem &item );
    void GetItem( wxListItem &item ) const;
    void SetItemState( long item, long state, long stateMask );
    void SetItemStateAll( long state, long stateMask );
    int GetItemState( long item, long stateMask ) const;
    bool GetItemRect( long item, wxRect &rect ) const
    {
        return GetSubItemRect(item, wxLIST_GETSUBITEMRECT_WHOLEITEM, rect);
    }
    bool GetSubItemRect( long item, long subItem, wxRect& rect ) const;
    wxRect GetViewRect() const;
    bool GetItemPosition( long item, wxPoint& pos ) const;
    int GetSelectedItemCount() const;

    wxString GetItemText(long item, int col = 0) const
    {
        wxListItem info;
        info.m_mask = wxLIST_MASK_TEXT;
        info.m_itemId = item;
        info.m_col = col;
        GetItem( info );
        return info.m_text;
    }

    void SetItemText(long item, const wxString& value)
    {
        wxListItem info;
        info.m_mask = wxLIST_MASK_TEXT;
        info.m_itemId = item;
        info.m_text = value;
        SetItem( info );
    }

    wxImageList* GetSmallImageList() const
        { return m_small_image_list; }

    // set the scrollbars and update the positions of the items
    void RecalculatePositions(bool noRefresh = false);

    // refresh the window and the header
    void RefreshAll();

    long GetNextItem( long item, int geometry, int state ) const;
    void DeleteItem( long index );
    void DeleteAllItems();
    void DeleteColumn( int col );
    void DeleteEverything();
    void EnsureVisible( long index );
    long FindItem( long start, const wxString& str, bool partial = false );
    long FindItem( long start, wxUIntPtr data);
    long FindItem( const wxPoint& pt );
    long HitTest( int x, int y, int &flags ) const;
    void InsertItem( wxListItem &item );
    long InsertColumn( long col, const wxListItem &item );
    int GetItemWidthWithImage(wxListItem * item);
    void SortItems( wxListCtrlCompare fn, wxIntPtr data );

    size_t GetItemCount() const;
    bool IsEmpty() const { return GetItemCount() == 0; }
    void SetItemCount(long count);

    // change the current (== focused) item, send a notification event
    void ChangeCurrent(size_t current);
    void ResetCurrent() { ChangeCurrent((size_t)-1); }
    bool HasCurrent() const { return m_current != (size_t)-1; }

    // send out a wxListEvent
    void SendNotify( size_t line,
                     wxEventType command,
                     const wxPoint& point = wxDefaultPosition );

    // override base class virtual to reset m_lineHeight when the font changes
    virtual bool SetFont(const wxFont& font)
    {
        if ( !wxWindow::SetFont(font) )
            return false;

        m_lineHeight = 0;

        return true;
    }

    // these are for wxListLineData usage only

    // get the backpointer to the list ctrl
    wxGenericListCtrl *GetListCtrl() const
    {
        return wxStaticCast(GetParent(), wxGenericListCtrl);
    }

    // get the height of all lines (assuming they all do have the same height)
    wxCoord GetLineHeight() const;

    // get the y position of the given line (only for report view)
    wxCoord GetLineY(size_t line) const;

    // get the brush to use for the item highlighting
    wxBrush *GetHighlightBrush() const
    {
        return m_hasFocus ? m_highlightBrush : m_highlightUnfocusedBrush;
    }

    bool HasFocus() const
    {
        return m_hasFocus;
    }

protected:
    // the array of all line objects for a non virtual list control (for the
    // virtual list control we only ever use m_lines[0])
    wxListLineDataArray  m_lines;

    // the list of column objects
    wxListHeaderDataList m_columns;

    // currently focused item or -1
    size_t               m_current;

    // the number of lines per page
    int                  m_linesPerPage;

    // this flag is set when something which should result in the window
    // redrawing happens (i.e. an item was added or deleted, or its appearance
    // changed) and OnPaint() doesn't redraw the window while it is set which
    // allows to minimize the number of repaintings when a lot of items are
    // being added. The real repainting occurs only after the next OnIdle()
    // call
    bool                 m_dirty;

    wxColour            *m_highlightColour;
    wxImageList         *m_small_image_list;
    wxImageList         *m_normal_image_list;
    int                  m_small_spacing;
    int                  m_normal_spacing;
    bool                 m_hasFocus;

    bool                 m_lastOnSame;
    wxTimer             *m_renameTimer;

    // incremental search data
    wxString             m_findPrefix;
    wxTimer             *m_findTimer;
    // This flag is set to 0 if the bell is disabled, 1 if it is enabled and -1
    // if it is globally enabled but has been temporarily disabled because we
    // had already beeped for this particular search.
    int                  m_findBell;

    bool                 m_isCreated;
    int                  m_dragCount;
    wxPoint              m_dragStart;
    ColWidthArray        m_aColWidths;

    // for double click logic
    size_t m_lineLastClicked,
           m_lineBeforeLastClicked,
           m_lineSelectSingleOnUp;

protected:
    wxWindow *GetMainWindowOfCompositeControl() { return GetParent(); }

    // the total count of items in a virtual list control
    size_t m_countVirt;

    // the object maintaining the items selection state, only used in virtual
    // controls
    wxSelectionStore m_selStore;

    // common part of all ctors
    void Init();

    // get the line data for the given index
    wxListLineData *GetLine(size_t n) const
    {
        wxASSERT_MSG( n != (size_t)-1, wxT("invalid line index") );

        if ( IsVirtual() )
        {
            wxConstCast(this, wxListMainWindow)->CacheLineData(n);
            n = 0;
        }

        return &m_lines[n];
    }

    // get a dummy line which can be used for geometry calculations and such:
    // you must use GetLine() if you want to really draw the line
    wxListLineData *GetDummyLine() const;

    // cache the line data of the n-th line in m_lines[0]
    void CacheLineData(size_t line);

    // get the range of visible lines
    void GetVisibleLinesRange(size_t *from, size_t *to);

    // force us to recalculate the range of visible lines
    void ResetVisibleLinesRange() { m_lineFrom = (size_t)-1; }

    // find the first item starting with the given prefix after the given item
    size_t PrefixFindItem(size_t item, const wxString& prefix) const;

    // get the colour to be used for drawing the rules
    wxColour GetRuleColour() const
    {
        return wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT);
    }

private:
    // initialize the current item if needed
    void UpdateCurrent();

    // delete all items but don't refresh: called from dtor
    void DoDeleteAllItems();

    // Compute the minimal width needed to fully display the column header.
    int ComputeMinHeaderWidth(const wxListHeaderData* header) const;


    // the height of one line using the current font
    wxCoord m_lineHeight;

    // the total header width or 0 if not calculated yet
    wxCoord m_headerWidth;

    // the first and last lines being shown on screen right now (inclusive),
    // both may be -1 if they must be calculated so never access them directly:
    // use GetVisibleLinesRange() above instead
    size_t m_lineFrom,
           m_lineTo;

    // the brushes to use for item highlighting when we do/don't have focus
    wxBrush *m_highlightBrush,
            *m_highlightUnfocusedBrush;

    // wrapper around the text control currently used for in place editing or
    // NULL if no item is being edited
    wxListTextCtrlWrapper *m_textctrlWrapper;


    DECLARE_EVENT_TABLE()

    friend class wxGenericListCtrl;
};

#endif // wxUSE_LISTCTRL
#endif // _WX_GENERIC_LISTCTRL_PRIVATE_H_
