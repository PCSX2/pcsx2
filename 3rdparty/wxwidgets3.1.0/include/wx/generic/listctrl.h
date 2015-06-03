/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/listctrl.h
// Purpose:     Generic list control
// Author:      Robert Roebling
// Created:     01/02/97
// Copyright:   (c) 1998 Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_LISTCTRL_H_
#define _WX_GENERIC_LISTCTRL_H_

#include "wx/containr.h"
#include "wx/scrolwin.h"
#include "wx/textctrl.h"

#if wxUSE_DRAG_AND_DROP
class WXDLLIMPEXP_FWD_CORE wxDropTarget;
#endif

//-----------------------------------------------------------------------------
// internal classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxListHeaderWindow;
class WXDLLIMPEXP_FWD_CORE wxListMainWindow;

//-----------------------------------------------------------------------------
// wxListCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericListCtrl: public wxNavigationEnabled<wxListCtrlBase>,
                                          public wxScrollHelper
{
public:

    wxGenericListCtrl() : wxScrollHelper(this)
    {
        Init();
    }

    wxGenericListCtrl( wxWindow *parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = wxLC_ICON,
                const wxValidator& validator = wxDefaultValidator,
                const wxString &name = wxListCtrlNameStr)
            : wxScrollHelper(this)
    {
        Create(parent, winid, pos, size, style, validator, name);
    }

    virtual ~wxGenericListCtrl();

    void Init();

    bool Create( wxWindow *parent,
                 wxWindowID winid = wxID_ANY,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = wxLC_ICON,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString &name = wxListCtrlNameStr);

    bool GetColumn( int col, wxListItem& item ) const wxOVERRIDE;
    bool SetColumn( int col, const wxListItem& item ) wxOVERRIDE;
    int GetColumnWidth( int col ) const wxOVERRIDE;
    bool SetColumnWidth( int col, int width) wxOVERRIDE;
    int GetCountPerPage() const; // not the same in wxGLC as in Windows, I think
    wxRect GetViewRect() const;

    bool GetItem( wxListItem& info ) const;
    bool SetItem( wxListItem& info ) ;
    long SetItem( long index, int col, const wxString& label, int imageId = -1 );
    int  GetItemState( long item, long stateMask ) const;
    bool SetItemState( long item, long state, long stateMask);
    bool SetItemImage( long item, int image, int selImage = -1 );
    bool SetItemColumnImage( long item, long column, int image );
    wxString GetItemText( long item, int col = 0 ) const;
    void SetItemText( long item, const wxString& str );
    wxUIntPtr GetItemData( long item ) const;
    bool SetItemPtrData(long item, wxUIntPtr data);
    bool SetItemData(long item, long data) { return SetItemPtrData(item, data); }
    bool GetItemRect( long item, wxRect& rect, int code = wxLIST_RECT_BOUNDS ) const;
    bool GetSubItemRect( long item, long subItem, wxRect& rect, int code = wxLIST_RECT_BOUNDS ) const;
    bool GetItemPosition( long item, wxPoint& pos ) const;
    bool SetItemPosition( long item, const wxPoint& pos ); // not supported in wxGLC
    int GetItemCount() const;
    int GetColumnCount() const wxOVERRIDE;
    void SetItemSpacing( int spacing, bool isSmall = false );
    wxSize GetItemSpacing() const;
    void SetItemTextColour( long item, const wxColour& col);
    wxColour GetItemTextColour( long item ) const;
    void SetItemBackgroundColour( long item, const wxColour &col);
    wxColour GetItemBackgroundColour( long item ) const;
    void SetItemFont( long item, const wxFont &f);
    wxFont GetItemFont( long item ) const;
    int GetSelectedItemCount() const;
    wxColour GetTextColour() const;
    void SetTextColour(const wxColour& col);
    long GetTopItem() const;

    void SetSingleStyle( long style, bool add = true ) ;
    void SetWindowStyleFlag( long style ) wxOVERRIDE;
    void RecreateWindow() {}
    long GetNextItem( long item, int geometry = wxLIST_NEXT_ALL, int state = wxLIST_STATE_DONTCARE ) const;
    wxImageList *GetImageList( int which ) const wxOVERRIDE;
    void SetImageList( wxImageList *imageList, int which ) wxOVERRIDE;
    void AssignImageList( wxImageList *imageList, int which ) wxOVERRIDE;
    bool Arrange( int flag = wxLIST_ALIGN_DEFAULT ); // always wxLIST_ALIGN_LEFT in wxGLC

    void ClearAll();
    bool DeleteItem( long item );
    bool DeleteAllItems();
    bool DeleteAllColumns() wxOVERRIDE;
    bool DeleteColumn( int col ) wxOVERRIDE;

    void SetItemCount(long count);

    wxTextCtrl *EditLabel(long item,
                          wxClassInfo* textControlClass = wxCLASSINFO(wxTextCtrl));

    // End label editing, optionally cancelling the edit
    bool EndEditLabel(bool cancel);

    wxTextCtrl* GetEditControl() const;
    void Edit( long item ) { EditLabel(item); }

    bool EnsureVisible( long item );
    long FindItem( long start, const wxString& str, bool partial = false );
    long FindItem( long start, wxUIntPtr data );
    long FindItem( long start, const wxPoint& pt, int direction ); // not supported in wxGLC
    long HitTest( const wxPoint& point, int& flags, long *pSubItem = NULL ) const;
    long InsertItem(wxListItem& info);
    long InsertItem( long index, const wxString& label );
    long InsertItem( long index, int imageIndex );
    long InsertItem( long index, const wxString& label, int imageIndex );
    bool ScrollList( int dx, int dy );
    bool SortItems( wxListCtrlCompare fn, wxIntPtr data );

    // do we have a header window?
    bool HasHeader() const
        { return InReportView() && !HasFlag(wxLC_NO_HEADER); }

    // refresh items selectively (only useful for virtual list controls)
    void RefreshItem(long item);
    void RefreshItems(long itemFrom, long itemTo);

    virtual void EnableBellOnNoMatch(bool on = true) wxOVERRIDE;

    // overridden base class virtuals
    // ------------------------------

    virtual wxVisualAttributes GetDefaultAttributes() const wxOVERRIDE
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual void Update() wxOVERRIDE;


    // implementation only from now on
    // -------------------------------

    // generic version extension, don't use in portable code
    bool Update( long item );

    void OnInternalIdle( ) wxOVERRIDE;

    // We have to hand down a few functions
    virtual void Refresh(bool eraseBackground = true,
                         const wxRect *rect = NULL) wxOVERRIDE;

    virtual bool SetBackgroundColour( const wxColour &colour ) wxOVERRIDE;
    virtual bool SetForegroundColour( const wxColour &colour ) wxOVERRIDE;
    virtual wxColour GetBackgroundColour() const;
    virtual wxColour GetForegroundColour() const;
    virtual bool SetFont( const wxFont &font ) wxOVERRIDE;
    virtual bool SetCursor( const wxCursor &cursor ) wxOVERRIDE;

#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget( wxDropTarget *dropTarget ) wxOVERRIDE;
    virtual wxDropTarget *GetDropTarget() const wxOVERRIDE;
#endif

    virtual bool ShouldInheritColours() const wxOVERRIDE { return false; }

    // implementation
    // --------------

    wxImageList         *m_imageListNormal;
    wxImageList         *m_imageListSmall;
    wxImageList         *m_imageListState;  // what's that ?
    bool                 m_ownsImageListNormal,
                         m_ownsImageListSmall,
                         m_ownsImageListState;
    wxListHeaderWindow  *m_headerWin;
    wxListMainWindow    *m_mainWin;

protected:
    // Implement base class pure virtual methods.
    long DoInsertColumn(long col, const wxListItem& info) wxOVERRIDE;


    virtual bool DoPopupMenu( wxMenu *menu, int x, int y ) wxOVERRIDE;

    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

    // return the text for the given column of the given item
    virtual wxString OnGetItemText(long item, long column) const;

    // return the icon for the given item. In report view, OnGetItemImage will
    // only be called for the first column. See OnGetItemColumnImage for
    // details.
    virtual int OnGetItemImage(long item) const;

    // return the icon for the given item and column.
    virtual int OnGetItemColumnImage(long item, long column) const;

    // it calls our OnGetXXX() functions
    friend class WXDLLIMPEXP_FWD_CORE wxListMainWindow;

    virtual wxBorder GetDefaultBorder() const wxOVERRIDE;

    virtual wxSize GetSizeAvailableForScrollTarget(const wxSize& size) wxOVERRIDE;

private:
    void CreateOrDestroyHeaderWindowAsNeeded();
    void OnScroll( wxScrollWinEvent& event );
    void OnSize( wxSizeEvent &event );

    // we need to return a special WM_GETDLGCODE value to process just the
    // arrows but let the other navigation characters through
#if defined(__WXMSW__) && !defined(__WXWINCE__) && !defined(__WXUNIVERSAL__)
    virtual WXLRESULT
    MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif // __WXMSW__

    WX_FORWARD_TO_SCROLL_HELPER()

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxGenericListCtrl);
};

#if (!defined(__WXMSW__) || defined(__WXUNIVERSAL__)) && (!(defined(__WXMAC__) && wxOSX_USE_CARBON) || defined(__WXUNIVERSAL__ ))
/*
 * wxListCtrl has to be a real class or we have problems with
 * the run-time information.
 */

class WXDLLIMPEXP_CORE wxListCtrl: public wxGenericListCtrl
{
    wxDECLARE_DYNAMIC_CLASS(wxListCtrl);

public:
    wxListCtrl() {}

    wxListCtrl(wxWindow *parent, wxWindowID winid = wxID_ANY,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxLC_ICON,
               const wxValidator &validator = wxDefaultValidator,
               const wxString &name = wxListCtrlNameStr)
    : wxGenericListCtrl(parent, winid, pos, size, style, validator, name)
    {
    }

};
#endif // !__WXMSW__ || __WXUNIVERSAL__

#endif // _WX_GENERIC_LISTCTRL_H_
