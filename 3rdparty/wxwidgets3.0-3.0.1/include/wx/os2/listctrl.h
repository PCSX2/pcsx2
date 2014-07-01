///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/listctrl.h
// Purpose:     wxListCtrl class
// Author:
// Modified by:
// Created:
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LISTCTRL_H_
#define _WX_LISTCTRL_H_

#if wxUSE_LISTCTRL

#include "wx/control.h"
#include "wx/event.h"
#include "wx/hash.h"
#include "wx/textctrl.h"


class WXDLLIMPEXP_FWD_CORE wxImageList;

typedef int (wxCALLBACK *wxListCtrlCompare)(long lItem1, long lItem2, long lSortData);

class WXDLLIMPEXP_CORE wxListCtrl: public wxControl
{
public:
    wxListCtrl() { Init(); }
    wxListCtrl( wxWindow*          pParent
               ,wxWindowID         vId = -1
               ,const wxPoint&     rPos = wxDefaultPosition
               ,const wxSize&      rSize = wxDefaultSize
               ,long               lStyle = wxLC_ICON
               ,const wxValidator& rValidator = wxDefaultValidator
               ,const wxString&    rsName = wxListCtrlNameStr)
    {
        Init();
        Create( pParent
               ,vId
               ,rPos
               ,rSize
               ,lStyle
               ,rValidator
               ,rsName
              );
    }
    virtual ~wxListCtrl();

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId = -1
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = wxLC_ICON
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxListCtrlNameStr
               );


    // Attributes
    ////////////////////////////////////////////////////////////////////////////
    //

    //
    // Set the control colours
    //
    bool SetForegroundColour(const wxColour& rCol);
    bool SetBackgroundColour(const wxColour& rCol);

    //
    // Information about this column
    //
    bool GetColumn( int nCol
                   ,wxListItem& rItem
                  ) const;
    bool SetColumn( int         nCol
                   ,wxListItem& rItem
                  );

    //
    // Column width
    //
    int  GetColumnWidth(int nCol) const;
    bool SetColumnWidth( int nCol
                        ,int nWidth
                       );

    //
    // Gets the number of items that can fit vertically in the
    // visible area of the list control (list or report view)
    // or the total number of items in the list control (icon
    // or small icon view)
    //
    int GetCountPerPage(void) const;

    wxRect GetViewRect() const;
    //
    // Gets the edit control for editing labels.
    //
    wxTextCtrl* GetEditControl(void) const;

    //
    // Information about the item
    //
    bool GetItem(wxListItem& rInfo) const;
    bool SetItem(wxListItem& rInfo);

    //
    // Sets a string field at a particular column
    //
    long SetItem( long            lIndex
                 ,int             nCol
                 ,const wxString& rsLabel
                 ,int             nImageId = -1
                );

    //
    // Item state
    //
    int  GetItemState( long lItem
                      ,long lStateMask
                     ) const;
    bool SetItemState( long lItem
                      ,long lState
                      ,long lStateMask
                      );

    //
    // Sets the item image
    //
    bool SetItemImage( long lItem
                      ,int  nImage
                      ,int  lSelImage
                     );
    bool SetItemColumnImage( long lItem
                            ,long lColumn
                            ,int  nImage
                           );

    //
    // Item text
    //
    wxString GetItemText(long lItem) const;
    void     SetItemText( long            lItem
                         ,const wxString& rsStr
                        );

    //
    // Item data
    //
    long GetItemData(long lItem) const;
    bool SetItemPtrData(long item, wxUIntPtr data);
    bool SetItemData(long item, long data) { return SetItemPtrData(item, data); }

    //
    // Gets the item rectangle
    //
    bool GetItemRect( long    lItem
                     ,wxRect& rRect
                     ,int     nCode = wxLIST_RECT_BOUNDS
                    ) const;

    //
    // Item position
    //
    bool GetItemPosition( long     lItem
                         ,wxPoint& rPos
                        ) const;
    bool SetItemPosition( long           lItem
                         ,const wxPoint& rPos
                        );

    //
    // Gets the number of items in the list control
    //
    int  GetItemCount(void) const;

    //
    // Gets the number of columns in the list control
    //
    inline int GetColumnCount(void) const { return m_nColCount; }

    //
    // Retrieves the spacing between icons in pixels.
    // If bIsSmall is true, gets the spacing for the small icon
    // view, otherwise the large icon view.
    //
    int  GetItemSpacing(bool bIsSmall) const;

    //
    // Foreground colour of an item.
    //
    wxColour GetItemTextColour(long lItem) const;
    void     SetItemTextColour( long            lItem
                               ,const wxColour& rCol
                              );

    //
    // Background colour of an item.
    //
    wxColour GetItemBackgroundColour(long lItem ) const;
    void     SetItemBackgroundColour( long            lItem
                                     ,const wxColour& rCol
                                    );

    //
    // Gets the number of selected items in the list control
    //
    int      GetSelectedItemCount(void) const;

    //
    // Text colour of the listview
    //
    wxColour GetTextColour(void) const;
    void     SetTextColour(const wxColour& rCol);

    //
    // Gets the index of the topmost visible item when in
    // list or report view
    //
    long GetTopItem(void) const;

    //
    // Add or remove a single window style
    void SetSingleStyle( long lStyle
                        ,bool bAdd = true
                       );

    //
    // Set the whole window style
    //
    void SetWindowStyleFlag(long lStyle);

    //
    // Searches for an item, starting from 'item'.
    // item can be -1 to find the first item that matches the
    // specified flags.
    // Returns the item or -1 if unsuccessful.
    long GetNextItem( long lItem
                     ,int  nGeometry = wxLIST_NEXT_ALL
                     ,int  lState = wxLIST_STATE_DONTCARE
                    ) const;

    //
    // Gets one of the three image lists
    //
    wxImageList* GetImageList(int nWhich) const;

    //
    // Sets the image list
    //
    void SetImageList( wxImageList* pImageList
                      ,int          nWhich
                     );
    void AssignImageList( wxImageList* pImageList
                         ,int          nWhich
                        );

    //
    // Returns true if it is a virtual list control
    //
    inline bool IsVirtual() const { return (GetWindowStyle() & wxLC_VIRTUAL) != 0; }

    //
    // Refresh items selectively (only useful for virtual list controls)
    //
    void RefreshItem(long lItem);
    void RefreshItems( long lItemFrom
                      ,long lItemTo
                     );

    //
    // Operations
    ////////////////////////////////////////////////////////////////////////////
    //

    //
    // Arranges the items
    //
    bool Arrange(int nFlag = wxLIST_ALIGN_DEFAULT);

    //
    // Deletes an item
    //
    bool DeleteItem(long lItem);

    //
    // Deletes all items
    bool DeleteAllItems(void);

    //
    // Deletes a column
    //
    bool DeleteColumn(int nCol);

    //
    // Deletes all columns
    //
    bool DeleteAllColumns(void);

    //
    // Clears items, and columns if there are any.
    //
    void ClearAll(void);

    //
    // Edit the label
    //
    wxTextCtrl* EditLabel( long         lItem
                          ,wxClassInfo* pTextControlClass = wxCLASSINFO(wxTextCtrl)
                         );

    //
    // End label editing, optionally cancelling the edit
    //
    bool EndEditLabel(bool bCancel);

    //
    // Ensures this item is visible
    //
    bool EnsureVisible(long lItem);

    //
    // Find an item whose label matches this string, starting from the item after 'start'
    // or the beginning if 'start' is -1.
    //
    long FindItem( long            lStart
                  ,const wxString& rsStr
                  ,bool            bPartial = false
                 );

    //
    // Find an item whose data matches this data, starting from the item after 'start'
    // or the beginning if 'start' is -1.
    //
    long FindItem( long lStart
                  ,long lData
                 );

    //
    // Find an item nearest this position in the specified direction, starting from
    // the item after 'start' or the beginning if 'start' is -1.
    //
    long FindItem( long           lStart
                  ,const wxPoint& rPoint
                  ,int            lDirection
                 );

    //
    // Determines which item (if any) is at the specified point,
    // giving details in 'flags' (see wxLIST_HITTEST_... flags above)
    //
    long HitTest( const wxPoint& rPoint
                 ,int&           rFlags
                );

    //
    // Inserts an item, returning the index of the new item if successful,
    // -1 otherwise.
    //
    long InsertItem(wxListItem& rInfo);

    //
    // Insert a string item
    //
    long InsertItem( long            lIndex
                    ,const wxString& rsLabel
                   );

    //
    // Insert an image item
    //
    long InsertItem( long lIndex
                    ,int  nImageIndex
                   );

    //
    // Insert an image/string item
    //
    long InsertItem( long            lIndex
                    ,const wxString& rsLabel
                    ,int             nImageIndex
                   );

    //
    // For list view mode (only), inserts a column.
    //
    long InsertColumn( long        lCol
                      ,wxListItem& rInfo
                     );

    long InsertColumn( long            lCol
                      ,const wxString& rsHeading
                      ,int             nFormat = wxLIST_FORMAT_LEFT
                      ,int             lWidth = -1
                     );

    //
    // set the number of items in a virtual list control
    //
    void SetItemCount(long lCount);

    //
    // Scrolls the list control. If in icon, small icon or report view mode,
    // x specifies the number of pixels to scroll. If in list view mode, x
    // specifies the number of columns to scroll.
    // If in icon, small icon or list view mode, y specifies the number of pixels
    // to scroll. If in report view mode, y specifies the number of lines to scroll.
    //
    bool ScrollList( int nDx
                    ,int nDy
                   );

    // Sort items.

    //
    // fn is a function which takes 3 long arguments: item1, item2, data.
    // item1 is the long data associated with a first item (NOT the index).
    // item2 is the long data associated with a second item (NOT the index).
    // data is the same value as passed to SortItems.
    // The return value is a negative number if the first item should precede the second
    // item, a positive number of the second item should precede the first,
    // or zero if the two items are equivalent.
    //
    // data is arbitrary data to be passed to the sort function.
    //
    bool SortItems( wxListCtrlCompare fn
                   ,long              lData
                  );

    //
    // IMPLEMENTATION
    // --------------
    //
    virtual bool OS2Command( WXUINT uParam
                            ,WXWORD wId
                           );
    //
    // Bring the control in sync with current m_windowStyle value
    //
    void UpdateStyle(void);

    //
    // Implementation: converts wxWidgets style to MSW style.
    // Can be a single style flag or a bit list.
    // oldStyle is 'normalised' so that it doesn't contain
    // conflicting styles.
    //
    long ConvertToOS2Style( long& lOldStyle
                           ,long  lStyle
                          ) const;
    long ConvertArrangeToOS2Style(long lStyle);
    long ConvertViewToOS2Style(long lStyle);

    virtual MRESULT OS2WindowProc( WXUINT   uMsg
                                  ,WXWPARAM wParam
                                  ,WXLPARAM lParam
                                 );

    // Event handlers
    ////////////////////////////////////////////////////////////////////////////
    // Necessary for drawing hrules and vrules, if specified
    void OnPaint(wxPaintEvent& rEvent);

protected:
    //
    // common part of all ctors
    //
    void Init(void);

    //
    // Free memory taken by all internal data
    //
    void FreeAllInternalData(void);

    wxTextCtrl*                     m_pTextCtrl;        // The control used for editing a label
    wxImageList*                    m_pImageListNormal; // The image list for normal icons
    wxImageList*                    m_pImageListSmall;  // The image list for small icons
    wxImageList*                    m_pImageListState;  // The image list state icons (not implemented yet)
    bool                            m_bOwnsImageListNormal;
    bool                            m_bOwnsImageListSmall;
    bool                            m_bOwnsImageListState;
    long                            m_lBaseStyle;       // Basic PM style flags, for recreation purposes
    int                             m_nColCount;        // PM doesn't have GetColumnCount so must
                                                        // keep track of inserted/deleted columns

    //
    // true if we have any internal data (user data & attributes)
    //
    bool                            m_bAnyInternalData;

    //
    // true if we have any items with custom attributes
    //
    bool                            m_bHasAnyAttr;

    //
    // These functions are only used for virtual list view controls, i.e. the
    // ones with wxLC_VIRTUAL style
    //
    // return the text for the given column of the given item
    //
    virtual wxString OnGetItemText( long lItem
                                   ,long lColumn
                                  ) const;

    //
    // Return the icon for the given item. In report view, OnGetItemImage will
    // only be called for the first column. See OnGetItemColumnImage for
    // details.
    //
    virtual int OnGetItemImage(long lItem) const;

    //
    // Return the icon for the given item and column
    //
    virtual int OnGetItemColumnImage(long lItem, long lColumn) const;

    //
    // Return the attribute for the item (may return NULL if none)
    //
    virtual wxListItemAttr* OnGetItemAttr(long lItem) const;

private:
    bool DoCreateControl( int nX
                         ,int nY
                         ,int nWidth
                         ,int nHeight
                        );

    DECLARE_DYNAMIC_CLASS(wxListCtrl)
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxListCtrl);
}; // end of CLASS wxListCtrl

#endif // wxUSE_LISTCTRL

#endif // _WX_LISTCTRL_H_

