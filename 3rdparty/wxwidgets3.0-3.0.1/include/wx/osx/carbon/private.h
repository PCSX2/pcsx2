/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/private.h
// Purpose:     Private declarations: as this header is only included by
//              wxWidgets itself, it may contain identifiers which don't start
//              with "wx".
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_H_
#define _WX_PRIVATE_H_

#if wxUSE_GUI

#include "wx/osx/uma.h"

#include "wx/listbox.h"
#include "wx/osx/dc.h"
#include "wx/osx/dcclient.h"
#include "wx/osx/dcmemory.h"

// app.h

#endif // wxUSE_GUI

// filefn.h
WXDLLIMPEXP_BASE wxString wxMacFSSpec2MacFilename( const FSSpec *spec );
WXDLLIMPEXP_BASE void wxMacFilename2FSSpec( const wxString &path , FSSpec *spec );

// utils.h
WXDLLIMPEXP_BASE wxString wxMacFindFolderNoSeparator(short vRefNum,
                                                     OSType folderType,
                                                     Boolean createFolder);
WXDLLIMPEXP_BASE wxString wxMacFindFolder(short vRefNum,
                                          OSType folderType,
                                          Boolean createFolder);

template<typename T> EventParamType wxMacGetEventParamType() { wxFAIL_MSG( wxT("Unknown Param Type") ); return 0; }
template<> inline EventParamType wxMacGetEventParamType<HIShapeRef>() { return typeHIShapeRef; }
template<> inline EventParamType wxMacGetEventParamType<RgnHandle>() { return typeQDRgnHandle; }
template<> inline EventParamType wxMacGetEventParamType<ControlRef>() { return typeControlRef; }
template<> inline EventParamType wxMacGetEventParamType<WindowRef>() { return typeWindowRef; }
template<> inline EventParamType wxMacGetEventParamType<MenuRef>() { return typeMenuRef; }
template<> inline EventParamType wxMacGetEventParamType<EventRef>() { return typeEventRef; }
template<> inline EventParamType wxMacGetEventParamType<Point>() { return typeQDPoint; }
template<> inline EventParamType wxMacGetEventParamType<Rect>() { return typeQDRectangle; }
template<> inline EventParamType wxMacGetEventParamType<Boolean>() { return typeBoolean; }
template<> inline EventParamType wxMacGetEventParamType<SInt16>() { return typeSInt16; }
template<> inline EventParamType wxMacGetEventParamType<SInt32>() { return typeSInt32; }
template<> inline EventParamType wxMacGetEventParamType<UInt32>() { return typeUInt32; }
template<> inline EventParamType wxMacGetEventParamType<RGBColor>() { return typeRGBColor; }
template<> inline EventParamType wxMacGetEventParamType<HICommand>() { return typeHICommand; }
template<> inline EventParamType wxMacGetEventParamType<HIPoint>() { return typeHIPoint; }
template<> inline EventParamType wxMacGetEventParamType<HISize>() { return typeHISize; }
template<> inline EventParamType wxMacGetEventParamType<HIRect>() { return typeHIRect; }
template<> inline EventParamType wxMacGetEventParamType<void*>() { return typeVoidPtr; }
template<> inline EventParamType wxMacGetEventParamType<CFDictionaryRef>() { return typeCFDictionaryRef; }
template<> inline EventParamType wxMacGetEventParamType<Collection>() { return typeCollection; }
template<> inline EventParamType wxMacGetEventParamType<CGContextRef>() { return typeCGContextRef; }
/*
 These are ambiguous
 template<> EventParamType wxMacGetEventParamType<GrafPtr>() { return typeGrafPtr; }
 template<> EventParamType wxMacGetEventParamType<OSStatus>() { return typeOSStatus; }
 template<> EventParamType wxMacGetEventParamType<CFIndex>() { return typeCFIndex; }
 template<> EventParamType wxMacGetEventParamType<GWorldPtr>() { return typeGWorldPtr; }
 */

class WXDLLIMPEXP_CORE wxMacCarbonEvent
{

public :
    wxMacCarbonEvent()
    {
        m_eventRef = 0;
        m_release = false;
    }

    wxMacCarbonEvent( EventRef event , bool release = false )
    {
        m_eventRef = event;
        m_release = release;
    }

    wxMacCarbonEvent(UInt32 inClassID,UInt32 inKind,EventTime inWhen = 0 /*now*/,EventAttributes inAttributes=kEventAttributeNone)
    {
        m_eventRef = NULL;
        verify_noerr( MacCreateEvent( NULL , inClassID, inKind,inWhen,inAttributes,&m_eventRef) );
        m_release = true;
    }

    ~wxMacCarbonEvent()
    {
        if ( m_release )
            ReleaseEvent( m_eventRef );
    }

    OSStatus Create(UInt32 inClassID,UInt32 inKind,EventTime inWhen = 0 /*now*/,EventAttributes inAttributes=kEventAttributeNone)
    {
        verify( (m_eventRef == NULL) || m_release );
        if ( m_eventRef && m_release )
        {
            ReleaseEvent( m_eventRef );
            m_release = false;
            m_eventRef = NULL;
        }
        OSStatus err = MacCreateEvent( NULL , inClassID, inKind,inWhen,inAttributes,&m_eventRef);
        if ( err == noErr )
            m_release = true;
        return err;
    }

    OSStatus GetParameter( EventParamName inName, EventParamType inDesiredType, UInt32 inBufferSize, void * outData);

    template <typename T> OSStatus GetParameter( EventParamName inName, EventParamType type , T *data )
    {
        return GetParameter( inName, type , sizeof( T ) , data );
    }
    template <typename T> OSStatus GetParameter( EventParamName inName, T *data )
    {
        return GetParameter<T>( inName, wxMacGetEventParamType<T>() , data );
    }

    template <typename T> T GetParameter( EventParamName inName )
    {
        T value;
        verify_noerr( GetParameter<T>( inName, &value ) );
        return value;
    }
    template <typename T> T GetParameter( EventParamName inName, EventParamType inDesiredType )
    {
        T value;
        verify_noerr( GetParameter<T>( inName, inDesiredType , &value ) );
        return value;
    }

    OSStatus SetParameter( EventParamName inName, EventParamType inType, UInt32 inSize, const void * inData);
    template <typename T> OSStatus SetParameter( EventParamName inName, EventParamType inDesiredType , const T *data )
    {
        return SetParameter( inName, inDesiredType , sizeof( T ) , data );
    }
    template <typename T> OSStatus SetParameter( EventParamName inName, EventParamType inDesiredType , const T& data )
    {
        return SetParameter<T>( inName, inDesiredType , &data );
    }
    template <typename T> OSStatus SetParameter( EventParamName inName, const T *data )
    {
        return SetParameter<T>( inName, wxMacGetEventParamType<T>() , data );
    }
    template <typename T> OSStatus SetParameter( EventParamName inName, const T& data )
    {
        return SetParameter<T>( inName, wxMacGetEventParamType<T>() , &data );
    }
    UInt32 GetClass()
    {
        return ::GetEventClass( m_eventRef );
    }
    UInt32 GetKind()
    {
        return ::GetEventKind( m_eventRef );
    }
    EventTime GetTime()
    {
        return ::GetEventTime( m_eventRef );
    }
    UInt32 GetTicks()
    {
        return EventTimeToTicks( GetTime() );
    }
    OSStatus SetCurrentTime( )
    {
        return ::SetEventTime( m_eventRef , GetCurrentEventTime() );
    }
    OSStatus SetTime( EventTime when )
    {
        return ::SetEventTime( m_eventRef , when );
    }
    operator EventRef () { return m_eventRef; }

    bool IsValid() { return m_eventRef != 0; }
protected :
    EventRef m_eventRef;
    bool     m_release;
};

#if wxUSE_GUI

class WXDLLIMPEXP_FWD_CORE wxMacToolTipTimer ;

class WXDLLIMPEXP_CORE wxMacToolTip
{
public :
    wxMacToolTip() ;
    ~wxMacToolTip() ;

    void Setup( WindowRef window , const wxString& text , const wxPoint& localPosition ) ;
    void Draw() ;
    void Clear() ;

    long GetMark()
    { return m_mark ; }

    bool IsShown()
    { return m_shown ; }

private :
    wxString    m_label ;
    wxPoint m_position ;
    Rect            m_rect ;
    WindowRef    m_window ;
    PicHandle    m_backpict ;
    bool        m_shown ;
    long        m_mark ;
#if wxUSE_TIMER
    wxMacToolTipTimer* m_timer ;
#endif
    wxCFStringRef m_helpTextRef ;
} ;

// Quartz

WXDLLIMPEXP_CORE void wxMacCreateBitmapButton( ControlButtonContentInfo*info , const wxBitmap& bitmap , int forceType = 0 );
WXDLLIMPEXP_CORE void wxMacReleaseBitmapButton( ControlButtonContentInfo*info );

#define MAC_WXHBITMAP(a) (GWorldPtr(a))
#define MAC_WXHMETAFILE(a) (PicHandle(a))
#define MAC_WXHICON(a) (IconRef(a))
#define MAC_WXHCURSOR(a) (CursHandle(a))
#define MAC_WXHRGN(a) (RgnHandle(a))
#define MAC_WXHWND(a) (WindowPtr(a))
#define MAC_WXRECPTR(a) ((Rect*)a)
#define MAC_WXPOINTPTR(a) ((Point*)a)
#define MAC_WXHMENU(a) ((MenuHandle)a)

struct wxOpaqueWindowRef
{
    wxOpaqueWindowRef( WindowRef ref ) { m_data = ref; }
    operator WindowRef() { return m_data; }
private :
    WindowRef m_data;
};

WXDLLIMPEXP_CORE void wxMacRectToNative( const wxRect *wx , Rect *n );
WXDLLIMPEXP_CORE void wxMacNativeToRect( const Rect *n , wxRect* wx );
WXDLLIMPEXP_CORE void wxMacPointToNative( const wxPoint* wx , Point *n );
WXDLLIMPEXP_CORE void wxMacNativeToPoint( const Point *n , wxPoint* wx );

WXDLLIMPEXP_CORE wxMenu* wxFindMenuFromMacMenu(MenuRef inMenuRef);

WXDLLIMPEXP_CORE int wxMacCommandToId( UInt32 macCommandId );
WXDLLIMPEXP_CORE UInt32 wxIdToMacCommand( int wxId );
WXDLLIMPEXP_CORE wxMenu* wxFindMenuFromMacCommand( const HICommand &macCommandId , wxMenuItem* &item );

WXDLLIMPEXP_CORE pascal OSStatus wxMacTopLevelMouseEventHandler( EventHandlerCallRef handler , EventRef event , void *data );
WXDLLIMPEXP_CORE Rect wxMacGetBoundsForControl( wxWindowMac* window , const wxPoint& pos , const wxSize &size , bool adjustForOrigin = true );

ControlActionUPP GetwxMacLiveScrollbarActionProc();

// additional optional event defines

class WXDLLIMPEXP_CORE wxMacControl : public wxWidgetImpl
{
public :
    wxMacControl( wxWindowMac* peer , bool isRootControl = false, bool isUserPane = false );
    wxMacControl() ;
    virtual ~wxMacControl();

    void Init();

    void SetReferenceInNativeControl();
    static wxMacControl* GetReferenceFromNativeControl(ControlRef control);

    virtual ControlRef * GetControlRefAddr() { return &m_controlRef; }
    virtual ControlRef GetControlRef() const { return m_controlRef; }

    virtual WXWidget GetWXWidget() const { return (WXWidget) m_controlRef; }

    virtual bool        IsVisible() const;

    virtual void        Raise();

    virtual void        Lower();

    virtual void        ScrollRect( const wxRect *rect, int dx, int dy );

    virtual void        GetContentArea( int &left , int &top , int &width , int &height ) const;
    virtual void        Move(int x, int y, int width, int height);
    virtual void        GetPosition( int &x, int &y ) const;
    virtual void        GetSize( int &width, int &height ) const;
    virtual void        SetControlSize( wxWindowVariant variant ) ;

    // where is in native window relative coordinates
    virtual void        SetNeedsDisplay( const wxRect* where = NULL );
    virtual bool        GetNeedsDisplay() const;

    virtual bool        CanFocus() const;
    // return true if successful
    virtual bool        SetFocus();
    virtual bool        HasFocus() const;

    void                RemoveFromParent();
    void                Embed( wxWidgetImpl *parent );

    void                SetDefaultButton( bool isDefault );
    void                PerformClick();
    void                SetLabel( const wxString& title, wxFontEncoding encoding );

    void                SetCursor( const wxCursor & cursor );
    void                CaptureMouse();
    void                ReleaseMouse();

    wxInt32             GetValue() const;
    void                SetValue( wxInt32 v );
    wxBitmap            GetBitmap() const;
    void                SetBitmap( const wxBitmap& bitmap );
    void                SetBitmapPosition( wxDirection dir );

    void                GetBestRect( wxRect *r ) const;
    bool                IsEnabled() const;
    void                Enable( bool enable );
    bool                ButtonClickDidStateChange() { return false ;}
    void                SetMinimum( wxInt32 v );
    void                SetMaximum( wxInt32 v );
    void                PulseGauge() ;
    void                SetScrollThumb( wxInt32 value, wxInt32 thumbSize );

    // temp convenience methods

    void                GetBestRect( Rect *r ) const;
    /*
    void operator= (ControlRef c) { m_controlRef = c; }
    operator ControlRef () { return m_controlRef; }
    operator ControlRef * () { return &m_controlRef; }
    */
    // accessing data and values

    virtual OSStatus SetData( ControlPartCode inPartCode , ResType inTag , Size inSize , const void * inData );
    virtual OSStatus GetData( ControlPartCode inPartCode , ResType inTag , Size inBufferSize , void * inOutBuffer , Size * outActualSize ) const;
    virtual OSStatus GetDataSize( ControlPartCode inPartCode , ResType inTag , Size * outActualSize ) const;
    virtual OSStatus SendEvent(  EventRef ref , OptionBits inOptions = 0 );
    virtual OSStatus SendHICommand( HICommand &command , OptionBits inOptions = 0 );

    virtual OSStatus SendHICommand( UInt32 commandID , OptionBits inOptions = 0 );

    virtual wxInt32 GetMaximum() const;
    virtual wxInt32 GetMinimum() const;

    virtual void SetValueAndRange( SInt32 value , SInt32 minimum , SInt32 maximum );
    virtual void SetRange( SInt32 minimum , SInt32 maximum );

    // templated helpers

    Size GetDataSize( ControlPartCode inPartCode , ResType inTag ) const
    {
        Size sz;
        verify_noerr( GetDataSize( inPartCode , inTag , &sz ) );
        return sz;
    }
    template <typename T> OSStatus SetData( ControlPartCode inPartCode , ResType inTag , const T *data )
    {
        return SetData( inPartCode , inTag , sizeof( T ) , data );
    }
    template <typename T> OSStatus SetData( ControlPartCode inPartCode , ResType inTag , const T& data )
    {
        return SetData( inPartCode , inTag , sizeof( T ) , &data );
    }
    template <typename T> OSStatus SetData( ResType inTag , const T *data )
    {
        return SetData( kControlEntireControl , inTag , sizeof( T ) , data );
    }
    template <typename T> OSStatus SetData( ResType inTag , const T& data )
    {
        return SetData( kControlEntireControl , inTag , sizeof( T ) , &data );
    }
    template <typename T> OSStatus GetData( ControlPartCode inPartCode , ResType inTag , T *data ) const
    {
        Size dummy;
        return GetData( inPartCode , inTag , sizeof( T ) , data , &dummy );
    }
    template <typename T> T GetData( ControlPartCode inPartCode , ResType inTag ) const
    {
        T value;
        OSStatus err = GetData<T>( inPartCode , inTag , &value );

        if ( err != noErr )
        {
            wxFAIL_MSG( wxString::Format(wxT("GetData Failed for Part [%i] and Tag [%i]"),
                                        inPartCode, (int)inTag) );
        }

        return value;
    }
    template <typename T> OSStatus GetData( ResType inTag , T *data ) const
    {
        Size dummy;
        return GetData( kControlEntireControl , inTag , sizeof( T ) , data , &dummy );
    }
    template <typename T> T GetData( ResType inTag ) const
    {
        return GetData<T>( kControlEntireControl , inTag );
    }

    // Flash the control for the specified amount of time

    virtual void VisibilityChanged( bool shown );
    virtual void SuperChangedPosition();


    virtual void SetFont( const wxFont & font , const wxColour& foreground , long windowStyle, bool ignoreBlack = true );
    virtual void SetBackgroundColour( const wxColour& col );
    virtual bool SetBackgroundStyle(wxBackgroundStyle style);
    virtual ControlPartCode HandleKey(  SInt16 keyCode,  SInt16 charCode, EventModifiers modifiers );
    void SetActionProc( ControlActionUPP   actionProc );
    SInt32 GetViewSize() const;

    virtual void SetVisibility( bool visible );

    virtual bool IsActive() const;

    // invalidates this control and all children
    virtual void InvalidateWithChildren();
    virtual void SetDrawingEnabled( bool enable );

    // in native parent window relative coordinates

    virtual void GetRectInWindowCoords( Rect *r );


    virtual void GetFeatures( UInt32 *features );

    // to be moved into a tab control class

    virtual OSStatus SetTabEnabled( SInt16 tabNo , bool enable );

    virtual void InstallEventHandler( WXWidget control = NULL );
protected :
    WXEVENTHANDLERREF    m_macControlEventHandler ;
    ControlRef  m_controlRef;
    wxFont      m_font;
    long        m_windowStyle;
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMacControl)
};

// ============================================================================
// DataBrowser Wrapper
// ============================================================================
//
// basing on DataBrowserItemIDs
//

class WXDLLIMPEXP_CORE wxMacDataBrowserControl : public wxMacControl
{
public :
    wxMacDataBrowserControl( wxWindow* peer, const wxPoint& pos, const wxSize& size, long style);
    wxMacDataBrowserControl() {}

    OSStatus SetCallbacks( const DataBrowserCallbacks *callbacks );

    OSStatus GetItemCount( DataBrowserItemID container,
            Boolean recurse,
            DataBrowserItemState state,
            ItemCount *numItems) const;

    OSStatus GetItems( DataBrowserItemID container,
            Boolean recurse,
            DataBrowserItemState state,
            Handle items) const;


    OSStatus AddColumn( DataBrowserListViewColumnDesc *columnDesc,
        DataBrowserTableViewColumnIndex position );

    OSStatus RemoveColumn( DataBrowserTableViewColumnIndex position );

    OSStatus AutoSizeColumns();

    OSStatus SetHasScrollBars( bool horiz, bool vert );
    OSStatus SetHiliteStyle( DataBrowserTableViewHiliteStyle hiliteStyle );

    OSStatus SetHeaderButtonHeight( UInt16 height );
    OSStatus GetHeaderButtonHeight( UInt16 *height );

    OSStatus UpdateItems( DataBrowserItemID container, UInt32 numItems,
            const DataBrowserItemID *items,
            DataBrowserPropertyID preSortProperty,
            DataBrowserPropertyID propertyID ) const;

    OSStatus AddItems( DataBrowserItemID container, UInt32 numItems,
            const DataBrowserItemID *items,
            DataBrowserPropertyID preSortProperty );
    OSStatus RemoveItems( DataBrowserItemID container, UInt32 numItems,
            const DataBrowserItemID *items,
            DataBrowserPropertyID preSortProperty );
    OSStatus RevealItem( DataBrowserItemID item,
            DataBrowserPropertyID propertyID,
            DataBrowserRevealOptions options ) const;

    OSStatus SetSelectionFlags( DataBrowserSelectionFlags );
    OSStatus GetSelectionAnchor( DataBrowserItemID *first, DataBrowserItemID *last ) const;
    bool IsItemSelected( DataBrowserItemID item ) const;
    OSStatus SetSelectedItems( UInt32 numItems,
            const DataBrowserItemID *items,
            DataBrowserSetOption operation );

    OSStatus GetItemID( DataBrowserTableViewRowIndex row,
            DataBrowserItemID * item ) const;
    OSStatus GetItemRow( DataBrowserItemID item,
            DataBrowserTableViewRowIndex * row ) const;

    OSStatus SetDefaultRowHeight( UInt16 height );
    OSStatus GetDefaultRowHeight( UInt16 * height ) const;

    OSStatus SetRowHeight( DataBrowserItemID item , UInt16 height);
    OSStatus GetRowHeight( DataBrowserItemID item , UInt16 *height) const;

    OSStatus GetColumnWidth( DataBrowserPropertyID column , UInt16 *width ) const;
    OSStatus SetColumnWidth( DataBrowserPropertyID column , UInt16 width );

    OSStatus GetDefaultColumnWidth( UInt16 *width ) const;
    OSStatus SetDefaultColumnWidth( UInt16 width );

    OSStatus GetColumnCount( UInt32* numColumns) const;

    OSStatus GetColumnIDFromIndex( DataBrowserTableViewColumnIndex position, DataBrowserTableViewColumnID* id );

    OSStatus GetColumnPosition( DataBrowserPropertyID column, DataBrowserTableViewColumnIndex *position) const;
    OSStatus SetColumnPosition( DataBrowserPropertyID column, DataBrowserTableViewColumnIndex position);

    OSStatus GetScrollPosition( UInt32 *top , UInt32 *left ) const;
    OSStatus SetScrollPosition( UInt32 top , UInt32 left );

    OSStatus GetSortProperty( DataBrowserPropertyID *column ) const;
    OSStatus SetSortProperty( DataBrowserPropertyID column );

    OSStatus GetSortOrder( DataBrowserSortOrder *order ) const;
    OSStatus SetSortOrder( DataBrowserSortOrder order );

    OSStatus GetPropertyFlags( DataBrowserPropertyID property, DataBrowserPropertyFlags *flags ) const;
    OSStatus SetPropertyFlags( DataBrowserPropertyID property, DataBrowserPropertyFlags flags );

    OSStatus GetHeaderDesc( DataBrowserPropertyID property, DataBrowserListViewHeaderDesc *desc ) const;
    OSStatus SetHeaderDesc( DataBrowserPropertyID property, DataBrowserListViewHeaderDesc *desc );

    OSStatus SetDisclosureColumn( DataBrowserPropertyID property , Boolean expandableRows );

    OSStatus GetItemPartBounds( DataBrowserItemID item, DataBrowserPropertyID property, DataBrowserPropertyPart part, Rect * bounds );
protected :

    static pascal void DataBrowserItemNotificationProc(
        ControlRef browser,
        DataBrowserItemID itemID,
        DataBrowserItemNotification message,
        DataBrowserItemDataRef itemData );

    virtual void ItemNotification(
        DataBrowserItemID itemID,
        DataBrowserItemNotification message,
        DataBrowserItemDataRef itemData) = 0;

    static pascal OSStatus DataBrowserGetSetItemDataProc(
        ControlRef browser,
        DataBrowserItemID itemID,
        DataBrowserPropertyID property,
        DataBrowserItemDataRef itemData,
        Boolean changeValue );

    virtual OSStatus GetSetItemData(
        DataBrowserItemID itemID,
        DataBrowserPropertyID property,
        DataBrowserItemDataRef itemData,
        Boolean changeValue ) = 0;

    static pascal Boolean DataBrowserCompareProc(
        ControlRef browser,
        DataBrowserItemID itemOneID,
        DataBrowserItemID itemTwoID,
        DataBrowserPropertyID sortProperty);

    virtual Boolean CompareItems(DataBrowserItemID itemOneID,
        DataBrowserItemID itemTwoID,
        DataBrowserPropertyID sortProperty) = 0;
    DECLARE_ABSTRACT_CLASS(wxMacDataBrowserControl)
};

// ============================================================================
// Higher-level Databrowser
// ============================================================================
//
// basing on data item objects
//

// forward decl

class wxMacDataItemBrowserControl;
class wxMacListBoxItem;

const DataBrowserPropertyID kTextColumnId = 1024;
const DataBrowserPropertyID kNumericOrderColumnId = 1025;

// for multi-column controls, we will use this + the column ID to identify the
// column. We don't use kTextColumnId there, and ideally the two should merge.
const DataBrowserPropertyID kMinColumnId = 1050;

// base API for high-level databrowser operations

// base class for databrowser items

enum DataItemType {
    DataItem_Text
};

/*
class WXDLLIMPEXP_CORE wxMacDataItem
{
public :
    wxMacDataItem();
    virtual ~wxMacDataItem();
} ;
*/

class WXDLLIMPEXP_CORE wxMacDataItem
{
public :
    wxMacDataItem();
    virtual ~wxMacDataItem();

    virtual bool IsLessThan(wxMacDataItemBrowserControl *owner ,
        const wxMacDataItem*,
        DataBrowserPropertyID property) const;

    // returns true if access was successful, otherwise false
    virtual OSStatus GetSetData(wxMacDataItemBrowserControl *owner ,
        DataBrowserPropertyID property,
        DataBrowserItemDataRef itemData,
        bool changeValue );

    virtual void Notification(wxMacDataItemBrowserControl *owner ,
        DataBrowserItemNotification message,
        DataBrowserItemDataRef itemData ) const;

    void SetOrder( SInt32 order );
    SInt32 GetOrder() const;

protected :
    SInt32      m_order;

};

enum ListSortOrder {
    SortOrder_None,
    SortOrder_Text_Ascending,
    SortOrder_Text_Descending
};

typedef wxMacDataItem* wxMacDataItemPtr;
const wxMacDataItemPtr wxMacDataBrowserRootContainer = NULL;
typedef void * wxListColumnId ;

WX_DEFINE_USER_EXPORTED_ARRAY_PTR(wxMacDataItemPtr, wxArrayMacDataItemPtr, class WXDLLIMPEXP_CORE);

class WXDLLIMPEXP_CORE wxMacDataItemBrowserControl : public wxMacDataBrowserControl
{
public :
    wxMacDataItemBrowserControl( wxWindow* peer , const wxPoint& pos, const wxSize& size, long style);
    wxMacDataItemBrowserControl() {}
    // create a list item (can be a subclass of wxMacListBoxItem)

    unsigned int    GetItemCount(const wxMacDataItem* container, bool recurse , DataBrowserItemState state) const;
    void            GetItems(const wxMacDataItem* container, bool recurse ,
                        DataBrowserItemState state, wxArrayMacDataItemPtr &items ) const;

    unsigned int    GetSelectedItemCount( const wxMacDataItem* container, bool recurse ) const;

    unsigned int    GetLineFromItem(const wxMacDataItem *item) const;
    wxMacDataItem * GetItemFromLine(unsigned int n) const;

    void            UpdateItem(const wxMacDataItem *container, const wxMacDataItem *item,
                        DataBrowserPropertyID property) const;
    void            UpdateItems(const wxMacDataItem *container, wxArrayMacDataItemPtr &items,
                        DataBrowserPropertyID property) const;

    void            InsertColumn(int colId, DataBrowserPropertyType colType,
                            const wxString& title, SInt16 just = teFlushDefault, int defaultWidth = -1);

    int             GetColumnWidth(int colId);
    void            SetColumnWidth(int colId, int width);

    void            AddItem(wxMacDataItem *container, wxMacDataItem *item);
    void            AddItems(wxMacDataItem *container, wxArrayMacDataItemPtr &items );

    void            RemoveAllItems(wxMacDataItem *container);
    void            RemoveItem(wxMacDataItem *container, wxMacDataItem* item);
    void            RemoveItems(wxMacDataItem *container, wxArrayMacDataItemPtr &items);

    void            SetSelectedItem( wxMacDataItem* item , DataBrowserSetOption option);
    void            SetSelectedItems( wxArrayMacDataItemPtr &items , DataBrowserSetOption option);
    void            SetSelectedAllItems( DataBrowserSetOption option);
    Boolean         IsItemSelected( const wxMacDataItem* item) const;

    void            RevealItem( wxMacDataItem* item, DataBrowserRevealOptions options);

    void            GetSelectionAnchor( wxMacDataItemPtr* first , wxMacDataItemPtr* last) const;

    // add and remove

    virtual void            MacDelete( unsigned int n );
    virtual void            MacInsert( unsigned int n, wxMacDataItem* item);
    virtual void            MacClear();

    // accessing content

    virtual unsigned int    MacGetCount() const;

public :

    // as we are getting the same events for human and API selection we have to suppress
    // events in the latter case, since this will be used from many subclasses we keep it here

    bool            IsSelectionSuppressed() const { return m_suppressSelection; }
    bool            SuppressSelection( bool suppress );

    // client data

    virtual wxClientDataType GetClientDataType() const;
    virtual void            SetClientDataType(wxClientDataType clientDataItemsType);
    //virtual ListSortOrder   GetSortOrder() const;
    //virtual void            SetSortOrder(const ListSortOrder sort);



protected:

    ListSortOrder m_sortOrder;
    wxClientDataType m_clientDataItemsType;

    // ID aware base methods, should be 'final' ie not changed in subclasses

    virtual Boolean CompareItems(DataBrowserItemID itemOneID,
                        DataBrowserItemID itemTwoID,
                        DataBrowserPropertyID sortProperty);

    virtual OSStatus GetSetItemData(DataBrowserItemID itemID,
                        DataBrowserPropertyID property,
                        DataBrowserItemDataRef itemData,
                        Boolean changeValue );

    virtual void    ItemNotification(
                        DataBrowserItemID itemID,
                        DataBrowserItemNotification message,
                        DataBrowserItemDataRef itemData);


private :

    bool m_suppressSelection;
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMacDataItemBrowserControl)
};

class WXDLLIMPEXP_CORE wxMacDataItemBrowserSelectionSuppressor
{
public :
    wxMacDataItemBrowserSelectionSuppressor(wxMacDataItemBrowserControl *browser);
    ~wxMacDataItemBrowserSelectionSuppressor();

private :

    bool m_former;
    wxMacDataItemBrowserControl* m_browser;
};

// ============================================================================
// platform listbox implementation
// ============================================================================

// exposed for reuse in wxCheckListBox

class WXDLLIMPEXP_CORE wxMacListBoxItem : public wxMacDataItem
{
public :
    wxMacListBoxItem();

    virtual ~wxMacListBoxItem();

    virtual OSStatus GetSetData(wxMacDataItemBrowserControl *owner ,
        DataBrowserPropertyID property,
        DataBrowserItemDataRef itemData,
        bool changeValue );

    virtual void Notification(wxMacDataItemBrowserControl *owner ,
        DataBrowserItemNotification message,
        DataBrowserItemDataRef itemData ) const;
protected :
};

class WXDLLIMPEXP_CORE wxMacDataBrowserColumn : public wxListWidgetColumn
{
public :
    wxMacDataBrowserColumn( DataBrowserPropertyID propertyId, DataBrowserPropertyType colType, bool editable )
        : m_property(propertyId), m_editable(editable), m_type( colType )
    {
    }
    ~wxMacDataBrowserColumn()
    {
    }
    DataBrowserPropertyID GetProperty() const { return m_property ; }

    bool IsEditable() const { return m_editable; }

    DataBrowserPropertyType GetType() const { return m_type; }

protected :
    DataBrowserPropertyID m_property;
    bool m_editable;
    DataBrowserPropertyType m_type;
} ;

WX_DEFINE_ARRAY_PTR(wxMacDataBrowserColumn *, wxArrayMacDataBrowserColumns);


class WXDLLIMPEXP_CORE wxMacDataBrowserCellValue : public wxListWidgetCellValue
{
public :
    wxMacDataBrowserCellValue(DataBrowserItemDataRef data) : m_data(data) {}
    virtual ~wxMacDataBrowserCellValue() {}

    virtual void Set( CFStringRef value );
    virtual void Set( const wxString& value );
    virtual void Set( int value ) ;
    virtual void Check( bool check );

    virtual int GetIntValue() const ;
    virtual wxString GetStringValue() const ;
protected :
    DataBrowserItemDataRef m_data;
} ;


class WXDLLIMPEXP_CORE wxMacDataBrowserListControl : public wxMacDataItemBrowserControl, public wxListWidgetImpl
{
public:
    wxMacDataBrowserListControl( wxWindow *peer, const wxPoint& pos, const wxSize& size, long style );
    wxMacDataBrowserListControl() {}
    virtual ~wxMacDataBrowserListControl();

    // wxListWidgetImpl Methods

    wxListWidgetColumn*     InsertTextColumn( unsigned int pos, const wxString& title, bool editable = false,
                                wxAlignment just = wxALIGN_LEFT , int defaultWidth = -1) ;
    wxListWidgetColumn*     InsertCheckColumn( unsigned int pos , const wxString& title, bool editable = false,
                                wxAlignment just = wxALIGN_LEFT , int defaultWidth = -1) ;

    wxMacDataBrowserColumn* DoInsertColumn( unsigned int pos, DataBrowserPropertyID property,
                                const wxString& title, bool editable,
                                DataBrowserPropertyType colType, SInt16 just, int width );
    // add and remove

    virtual void            ListDelete( unsigned int n );
    virtual void            ListInsert( unsigned int n );
    virtual void            ListClear();

    // selecting

    virtual void            ListDeselectAll();
    virtual void            ListSetSelection( unsigned int n, bool select, bool multi = false );
    virtual int             ListGetSelection() const;
    virtual int             ListGetSelections( wxArrayInt& aSelections ) const;
    virtual bool            ListIsSelected( unsigned int n ) const;

    // display

    virtual void            ListScrollTo( unsigned int n );

    // accessing content

    virtual unsigned int    ListGetCount() const;
    virtual int             DoListHitTest( const wxPoint& inpoint ) const;

    virtual void            UpdateLine( unsigned int n, wxListWidgetColumn* col = NULL );
    virtual void            UpdateLineToEnd( unsigned int n) ;

    // pointing back

    wxMacDataBrowserColumn*     GetColumnFromProperty( DataBrowserPropertyID );

protected:
    virtual void            ItemNotification(
                                DataBrowserItemID itemID,
                                DataBrowserItemNotification message,
                                DataBrowserItemDataRef itemData);

private:
    wxArrayMacDataBrowserColumns m_columns;
    int m_nextColumnId ;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMacDataBrowserListControl)
};

// ============================================================================
// graphics implementation
// ============================================================================

// draw the image 'upside down' corrected as HIViewDrawCGImage does

OSStatus WXDLLIMPEXP_CORE wxMacDrawCGImage(
                               CGContextRef    inContext,
                               const HIRect *  inBounds,
                               CGImageRef      inImage) ;

CGColorRef WXDLLIMPEXP_CORE wxMacCreateCGColorFromHITheme( ThemeBrush brush ) ;

#endif // wxUSE_GUI

#define wxMAC_DEFINE_PROC_GETTER( UPP , x ) \
UPP Get##x()                                \
{                                           \
    static UPP sHandler = NULL;             \
    if ( sHandler == NULL )                 \
        sHandler = New##UPP( x );           \
    return sHandler;                        \
}

//---------------------------------------------------------------------------
// cocoa bridging utilities
//---------------------------------------------------------------------------

bool wxMacInitCocoa();

typedef Cursor ClassicCursor;

// -------------
// Common to all
// -------------

// Cursor support

const short kwxCursorBullseye = 0;
const short kwxCursorBlank = 1;
const short kwxCursorPencil = 2;
const short kwxCursorMagnifier = 3;
const short kwxCursorNoEntry = 4;
const short kwxCursorPaintBrush = 5;
const short kwxCursorPointRight = 6;
const short kwxCursorPointLeft = 7;
const short kwxCursorQuestionArrow = 8;
const short kwxCursorRightArrow = 9;
const short kwxCursorSizeNS = 10;
const short kwxCursorSize = 11;
const short kwxCursorSizeNESW = 12;
const short kwxCursorSizeNWSE = 13;
const short kwxCursorRoller = 14;
const short kwxCursorWatch = 15;
const short kwxCursorLast = kwxCursorWatch;

// exposing our fallback cursor map

extern ClassicCursor gMacCursors[];

//
//
//

#if wxUSE_GUI

class wxNonOwnedWindowCarbonImpl : public wxNonOwnedWindowImpl
{
public :
    wxNonOwnedWindowCarbonImpl( wxNonOwnedWindow* nonownedwnd) ;

    wxNonOwnedWindowCarbonImpl();
    virtual ~wxNonOwnedWindowCarbonImpl();

    virtual void WillBeDestroyed() ;
    void Create( wxWindow* parent, const wxPoint& pos, const wxSize& size,
    long style, long extraStyle, const wxString& name ) ;
    void Create( wxWindow* parent, WXWindow nativeWindow );

    WXWindow GetWXWindow() const;
    void Raise();
    void Lower();
    bool Show(bool show);
    bool ShowWithEffect(bool show, wxShowEffect effect, unsigned timeout);


    void Update();
    bool SetTransparent(wxByte alpha);
    bool SetBackgroundColour(const wxColour& col );
    void SetExtraStyle( long exStyle );
    bool SetBackgroundStyle(wxBackgroundStyle style);
    bool CanSetTransparent();
    void MoveWindow(int x, int y, int width, int height);
    void GetPosition( int &x, int &y ) const;
    void GetSize( int &width, int &height ) const;
    void GetContentArea( int &left , int &top , int &width , int &height ) const;

    bool SetShape(const wxRegion& region);

    virtual void SetTitle( const wxString& title, wxFontEncoding encoding ) ;

    virtual bool IsMaximized() const;

    virtual bool IsIconized() const;

    virtual void Iconize( bool iconize );

    virtual void Maximize(bool maximize);

    virtual bool IsFullScreen() const;

    virtual bool ShowFullScreen(bool show, long style);

    virtual void ShowWithoutActivating();

    virtual void RequestUserAttention(int flags);

    virtual void ScreenToWindow( int *x, int *y );

    virtual void WindowToScreen( int *x, int *y );
    virtual bool IsActive();


    bool MacGetUnifiedAppearance() const ;
    void MacChangeWindowAttributes( wxUint32 attributesToSet , wxUint32 attributesToClear ) ;
    wxUint32 MacGetWindowAttributes() const ;
    void MacSetMetalAppearance( bool set ) ;
    bool MacGetMetalAppearance() const ;
    void MacSetUnifiedAppearance( bool set );

    WXEVENTHANDLERREF    MacGetEventHandler() { return m_macEventHandler ; }

    wxNonOwnedWindow*   GetWXPeer() { return m_wxPeer; }
protected :
    void                MacInstallTopLevelWindowEventHandler();

    WXEVENTHANDLERREF   m_macEventHandler ;
    WindowRef           m_macWindow;
    void *              m_macFullScreenData ;
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxNonOwnedWindowCarbonImpl)
};

#endif // wxUSE_GUI

#endif
    // _WX_PRIVATE_H_
