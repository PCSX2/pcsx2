/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/private.h
// Purpose:     Private declarations: as this header is only included by
//              wxWidgets itself, it may contain identifiers which don't start
//              with "wx".
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_CORE_H_
#define _WX_PRIVATE_CORE_H_

#include "wx/defs.h"

#include <CoreFoundation/CoreFoundation.h>

#include "wx/osx/core/cfstring.h"
#include "wx/osx/core/cfdataref.h"

// Define helper macros allowing to insert small snippets of code to be
// compiled for high enough OS X version only: this shouldn't be abused for
// anything big but it's handy for e.g. specifying OS X 10.6-only protocols in
// the Objective C classes declarations when they're not supported under the
// previous versions
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
    #define wxOSX_10_6_AND_LATER(x) x
#else
    #define wxOSX_10_6_AND_LATER(x)
#endif

// platform specific Clang analyzer support
#ifndef NS_RETURNS_RETAINED
#   if WX_HAS_CLANG_FEATURE(attribute_ns_returns_retained)
#       define NS_RETURNS_RETAINED __attribute__((ns_returns_retained))
#   else
#       define NS_RETURNS_RETAINED
#   endif
#endif

#ifndef CF_RETURNS_RETAINED
#   if WX_HAS_CLANG_FEATURE(attribute_cf_returns_retained)
#       define CF_RETURNS_RETAINED __attribute__((cf_returns_retained))
#   else
#       define CF_RETURNS_RETAINED
#   endif
#endif

#if ( !wxUSE_GUI && !wxOSX_USE_IPHONE ) || wxOSX_USE_COCOA_OR_CARBON

// Carbon functions are currently still used in wxOSX/Cocoa too (including
// wxBase part of it).
#include <Carbon/Carbon.h>

WXDLLIMPEXP_BASE long UMAGetSystemVersion() ;

void WXDLLIMPEXP_CORE wxMacStringToPascal( const wxString&from , unsigned char * to );
wxString WXDLLIMPEXP_CORE wxMacMakeStringFromPascal( const unsigned char * from );

WXDLLIMPEXP_BASE wxString wxMacFSRefToPath( const FSRef *fsRef , CFStringRef additionalPathComponent = NULL );
WXDLLIMPEXP_BASE OSStatus wxMacPathToFSRef( const wxString&path , FSRef *fsRef );
WXDLLIMPEXP_BASE wxString wxMacHFSUniStrToString( ConstHFSUniStr255Param uniname );

// keycode utils from app.cpp

WXDLLIMPEXP_BASE CGKeyCode wxCharCodeWXToOSX(wxKeyCode code);
WXDLLIMPEXP_BASE long wxMacTranslateKey(unsigned char key, unsigned char code);

#endif

#if wxUSE_GUI

#if wxOSX_USE_IPHONE
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "wx/bitmap.h"
#include "wx/window.h"

class WXDLLIMPEXP_CORE wxMacCGContextStateSaver
{
    wxDECLARE_NO_COPY_CLASS(wxMacCGContextStateSaver);

public:
    wxMacCGContextStateSaver( CGContextRef cg )
    {
        m_cg = cg;
        CGContextSaveGState( cg );
    }
    ~wxMacCGContextStateSaver()
    {
        CGContextRestoreGState( m_cg );
    }
private:
    CGContextRef m_cg;
};

class WXDLLIMPEXP_CORE wxDeferredObjectDeleter : public wxObject
{
public :
    wxDeferredObjectDeleter( wxObject* obj ) : m_obj(obj)
    {
    }
    virtual ~wxDeferredObjectDeleter()
    {
        delete m_obj;
    }
protected :
    wxObject* m_obj ;
} ;

// Quartz

WXDLLIMPEXP_CORE CGImageRef wxMacCreateCGImageFromBitmap( const wxBitmap& bitmap );

WXDLLIMPEXP_CORE CGDataProviderRef wxMacCGDataProviderCreateWithCFData( CFDataRef data );
WXDLLIMPEXP_CORE CGDataConsumerRef wxMacCGDataConsumerCreateWithCFData( CFMutableDataRef data );
WXDLLIMPEXP_CORE CGDataProviderRef wxMacCGDataProviderCreateWithMemoryBuffer( const wxMemoryBuffer& buf );

WXDLLIMPEXP_CORE CGColorSpaceRef wxMacGetGenericRGBColorSpace(void);

WXDLLIMPEXP_CORE double wxOSXGetMainScreenContentScaleFactor();

class wxWindowMac;
// to
extern wxWindow* g_MacLastWindow;
class wxNonOwnedWindow;

// temporary typedef so that no additional casts are necessary within carbon code at the moment

class wxMacControl;
class wxWidgetImpl;
class wxComboBox;
class wxNotebook;
class wxTextCtrl;
class wxSearchCtrl;

WXDLLIMPEXP_CORE wxWindowMac * wxFindWindowFromWXWidget(WXWidget inControl );

#if wxOSX_USE_CARBON
typedef wxMacControl wxWidgetImplType;
#else
typedef wxWidgetImpl wxWidgetImplType;
#endif

#if wxUSE_MENUS
class wxMenuItemImpl : public wxObject
{
public :
    wxMenuItemImpl( wxMenuItem* peer ) : m_peer(peer)
    {
    }

    virtual ~wxMenuItemImpl() ;
    virtual void SetBitmap( const wxBitmap& bitmap ) = 0;
    virtual void Enable( bool enable ) = 0;
    virtual void Check( bool check ) = 0;
    virtual void SetLabel( const wxString& text, wxAcceleratorEntry *entry ) = 0;
    virtual void Hide( bool hide = true ) = 0;

    virtual void * GetHMenuItem() = 0;

    wxMenuItem* GetWXPeer() { return m_peer ; }

    static wxMenuItemImpl* Create( wxMenuItem* peer, wxMenu *pParentMenu,
                       int id,
                       const wxString& text,
                       wxAcceleratorEntry *entry,
                       const wxString& strHelp,
                       wxItemKind kind,
                       wxMenu *pSubMenu );
    
    // handle OS specific menu items if they weren't handled during normal processing
    virtual bool DoDefault() { return false; }
protected :
    wxMenuItem* m_peer;

    DECLARE_ABSTRACT_CLASS(wxMenuItemImpl)
} ;

class wxMenuImpl : public wxObject
{
public :
    wxMenuImpl( wxMenu* peer ) : m_peer(peer)
    {
    }

    virtual ~wxMenuImpl() ;
    virtual void InsertOrAppend(wxMenuItem *pItem, size_t pos) = 0;
    virtual void Remove( wxMenuItem *pItem ) = 0;

    virtual void MakeRoot() = 0;

    virtual void SetTitle( const wxString& text ) = 0;

    virtual WXHMENU GetHMenu() = 0;

    wxMenu* GetWXPeer() { return m_peer ; }

    virtual void PopUp( wxWindow *win, int x, int y ) = 0;

    static wxMenuImpl* Create( wxMenu* peer, const wxString& title );
    static wxMenuImpl* CreateRootMenu( wxMenu* peer );
protected :
    wxMenu* m_peer;

    DECLARE_ABSTRACT_CLASS(wxMenuItemImpl)
} ;
#endif


class WXDLLIMPEXP_CORE wxWidgetImpl : public wxObject
{
public :
    wxWidgetImpl( wxWindowMac* peer , bool isRootControl = false, bool isUserPane = false );
    wxWidgetImpl();
    virtual ~wxWidgetImpl();

    void Init();

    bool                IsRootControl() const { return m_isRootControl; }
    
    bool                IsUserPane() const { return m_isUserPane; }

    wxWindowMac*        GetWXPeer() const { return m_wxPeer; }

    bool IsOk() const { return GetWXWidget() != NULL; }

    // not only the control itself, but also all its parents must be visible
    // in order for this function to return true
    virtual bool        IsVisible() const = 0;
    // set the visibility of this widget (maybe latent)
    virtual void        SetVisibility( bool visible ) = 0;

    virtual bool ShowWithEffect(bool WXUNUSED(show),
                                wxShowEffect WXUNUSED(effect),
                                unsigned WXUNUSED(timeout))
    {
        return false;
    }

    virtual void        Raise() = 0;

    virtual void        Lower() = 0;

    virtual void        ScrollRect( const wxRect *rect, int dx, int dy ) = 0;

    virtual WXWidget    GetWXWidget() const = 0;

    virtual void        SetBackgroundColour( const wxColour& col ) = 0;
    virtual bool        SetBackgroundStyle(wxBackgroundStyle style) = 0;

    // all coordinates in native parent widget relative coordinates
    virtual void        GetContentArea( int &left , int &top , int &width , int &height ) const = 0;
    virtual void        Move(int x, int y, int width, int height) = 0;
    virtual void        GetPosition( int &x, int &y ) const = 0;
    virtual void        GetSize( int &width, int &height ) const = 0;
    virtual void        SetControlSize( wxWindowVariant variant ) = 0;
    virtual double      GetContentScaleFactor() const
    {
        return 1.0;
    }
    
    // the native coordinates may have an 'aura' for shadows etc, if this is the case the layout
    // inset indicates on which insets the real control is drawn
    virtual void        GetLayoutInset(int &left , int &top , int &right, int &bottom) const
    {
        left = top = right = bottom = 0;
    }

    // native view coordinates are topleft to bottom right (flipped regarding CoreGraphics origin)
    virtual bool        IsFlipped() const { return true; }

    virtual void        SetNeedsDisplay( const wxRect* where = NULL ) = 0;
    virtual bool        GetNeedsDisplay() const = 0;

    virtual bool        NeedsFocusRect() const;
    virtual void        SetNeedsFocusRect( bool needs );

    virtual bool        NeedsFrame() const;
    virtual void        SetNeedsFrame( bool needs );
    
    virtual void        SetDrawingEnabled(bool enabled);

    virtual bool        CanFocus() const = 0;
    // return true if successful
    virtual bool        SetFocus() = 0;
    virtual bool        HasFocus() const = 0;

    virtual void        RemoveFromParent() = 0;
    virtual void        Embed( wxWidgetImpl *parent ) = 0;

    virtual void        SetDefaultButton( bool isDefault ) = 0;
    virtual void        PerformClick() = 0;
    virtual void        SetLabel( const wxString& title, wxFontEncoding encoding ) = 0;
#if wxUSE_MARKUP && wxOSX_USE_COCOA
    virtual void        SetLabelMarkup( const wxString& WXUNUSED(markup) ) { }
#endif

    virtual void        SetCursor( const wxCursor & cursor ) = 0;
    virtual void        CaptureMouse() = 0;
    virtual void        ReleaseMouse() = 0;
    
    virtual void        SetDropTarget( wxDropTarget * WXUNUSED(dropTarget) ) {}

    virtual wxInt32     GetValue() const = 0;
    virtual void        SetValue( wxInt32 v ) = 0;
    virtual wxBitmap    GetBitmap() const = 0;
    virtual void        SetBitmap( const wxBitmap& bitmap ) = 0;
    virtual void        SetBitmapPosition( wxDirection dir ) = 0;
    virtual void        SetupTabs( const wxNotebook& WXUNUSED(notebook) ) {}
    virtual int         TabHitTest( const wxPoint & WXUNUSED(pt), long *flags ) {*flags=1; return -1;}
    virtual void        GetBestRect( wxRect *r ) const = 0;
    virtual bool        IsEnabled() const = 0;
    virtual void        Enable( bool enable ) = 0;
    virtual void        SetMinimum( wxInt32 v ) = 0;
    virtual void        SetMaximum( wxInt32 v ) = 0;
    virtual wxInt32     GetMinimum() const = 0;
    virtual wxInt32     GetMaximum() const = 0;
    virtual void        PulseGauge() = 0;
    virtual void        SetScrollThumb( wxInt32 value, wxInt32 thumbSize ) = 0;

    virtual void        SetFont( const wxFont & font , const wxColour& foreground , long windowStyle, bool ignoreBlack = true ) = 0;

    virtual void        SetToolTip(wxToolTip* WXUNUSED(tooltip)) { }

    // is the clicked event sent AFTER the state already changed, so no additional
    // state changing logic is required from the outside
    virtual bool        ButtonClickDidStateChange() = 0;

    virtual void        InstallEventHandler( WXWidget control = NULL ) = 0;

    // Mechanism used to keep track of whether a change should send an event
    // Do SendEvents(false) when starting actions that would trigger programmatic events
    // and SendEvents(true) at the end of the block.
    virtual void        SendEvents(bool shouldSendEvents) { m_shouldSendEvents = shouldSendEvents; }
    virtual bool        ShouldSendEvents() { return m_shouldSendEvents; }

    // static methods for associating native controls and their implementations

    // finds the impl associated with this native control
    static wxWidgetImpl*
                        FindFromWXWidget(WXWidget control);

    // finds the impl associated with this native control, if the native control itself is not known
    // also checks whether its parent is eg a registered scrollview, ie whether the control is a native subpart
    // of a known control
    static wxWidgetImpl*
                        FindBestFromWXWidget(WXWidget control);
    
    static void         RemoveAssociations( wxWidgetImpl* impl);

    static void         Associate( WXWidget control, wxWidgetImpl *impl );

    static WXWidget     FindFocus();

    // static creation methods, must be implemented by all toolkits

    static wxWidgetImplType*    CreateUserPane( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;
    static wxWidgetImplType*    CreateContentView( wxNonOwnedWindow* now ) ;

    static wxWidgetImplType*    CreateButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateDisclosureTriangle( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateStaticLine( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateGroupBox( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateStaticText( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateTextControl( wxTextCtrl* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& content,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateSearchControl( wxSearchCtrl* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& content,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle) ;

    static wxWidgetImplType*    CreateCheckBox( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateRadioButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateToggleButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateBitmapToggleButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxBitmap& bitmap,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateBitmapButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxBitmap& bitmap,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateTabView( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateGauge( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    wxInt32 value,
                                    wxInt32 minimum,
                                    wxInt32 maximum,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateSlider( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    wxInt32 value,
                                    wxInt32 minimum,
                                    wxInt32 maximum,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateSpinButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    wxInt32 value,
                                    wxInt32 minimum,
                                    wxInt32 maximum,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateScrollBar( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateChoice( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    wxMenu* menu,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

    static wxWidgetImplType*    CreateListBox( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);

#if wxOSX_USE_COCOA
    static wxWidgetImplType*    CreateComboBox( wxComboBox* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID id,
                                    wxMenu* menu,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long extraStyle);
#endif

    // converts from Toplevel-Content relative to local
    static void Convert( wxPoint *pt , wxWidgetImpl *from , wxWidgetImpl *to );
protected :
    bool                m_isRootControl;
    bool                m_isUserPane;
    wxWindowMac*        m_wxPeer;
    bool                m_needsFocusRect;
    bool                m_needsFrame;
    bool                m_shouldSendEvents;

    DECLARE_ABSTRACT_CLASS(wxWidgetImpl)
};

//
// the interface to be implemented eg by a listbox
//

class WXDLLIMPEXP_CORE wxListWidgetColumn
{
public :
    virtual ~wxListWidgetColumn() {}
} ;

class WXDLLIMPEXP_CORE wxListWidgetCellValue
{
public :
    wxListWidgetCellValue() {}
    virtual ~wxListWidgetCellValue() {}

   virtual void Set( CFStringRef value ) = 0;
    virtual void Set( const wxString& value ) = 0;
    virtual void Set( int value ) = 0;
    virtual void Check( bool check );

    virtual bool IsChecked() const;
    virtual int GetIntValue() const = 0;
    virtual wxString GetStringValue() const = 0;
} ;

class WXDLLIMPEXP_CORE wxListWidgetImpl
{
public:
    wxListWidgetImpl() {}
    virtual ~wxListWidgetImpl() { }

    virtual wxListWidgetColumn*     InsertTextColumn( unsigned pos, const wxString& title, bool editable = false,
                                wxAlignment just = wxALIGN_LEFT , int defaultWidth = -1) = 0 ;
    virtual wxListWidgetColumn*     InsertCheckColumn( unsigned pos , const wxString& title, bool editable = false,
                                wxAlignment just = wxALIGN_LEFT , int defaultWidth =  -1) = 0 ;

    // add and remove

    // TODO will be replaced
    virtual void            ListDelete( unsigned int n ) = 0;
    virtual void            ListInsert( unsigned int n ) = 0;
    virtual void            ListClear() = 0;

    // selecting

    virtual void            ListDeselectAll() = 0;
    virtual void            ListSetSelection( unsigned int n, bool select, bool multi ) = 0;
    virtual int             ListGetSelection() const = 0;
    virtual int             ListGetSelections( wxArrayInt& aSelections ) const = 0;
    virtual bool            ListIsSelected( unsigned int n ) const = 0;

    // display

    virtual void            ListScrollTo( unsigned int n ) = 0;
    virtual void            UpdateLine( unsigned int n, wxListWidgetColumn* col = NULL ) = 0;
    virtual void            UpdateLineToEnd( unsigned int n) = 0;

    // accessing content

    virtual unsigned int    ListGetCount() const = 0;

    virtual int             DoListHitTest( const wxPoint& inpoint ) const = 0;
};

//
// interface to be implemented by a textcontrol
//

class WXDLLIMPEXP_FWD_CORE wxTextAttr;
class WXDLLIMPEXP_FWD_CORE wxTextEntry;

// common interface for all implementations
class WXDLLIMPEXP_CORE wxTextWidgetImpl

{
public :
    // Any widgets implementing this interface must be associated with a
    // wxTextEntry so instead of requiring the derived classes to implement
    // another (pure) virtual function, just take the pointer to this entry in
    // our ctor and implement GetTextEntry() ourselves.
    wxTextWidgetImpl(wxTextEntry *entry) : m_entry(entry) {}

    virtual ~wxTextWidgetImpl() {}

    wxTextEntry *GetTextEntry() const { return m_entry; }

    virtual bool CanFocus() const { return true; }

    virtual wxString GetStringValue() const = 0 ;
    virtual void SetStringValue( const wxString &val ) = 0 ;
    virtual void SetSelection( long from, long to ) = 0 ;
    virtual void GetSelection( long* from, long* to ) const = 0 ;
    virtual void WriteText( const wxString& str ) = 0 ;

    virtual bool CanClipMaxLength() const { return false; }
    virtual void SetMaxLength(unsigned long WXUNUSED(len)) {}
    
    virtual bool GetStyle( long position, wxTextAttr& style);
    virtual void SetStyle( long start, long end, const wxTextAttr& style ) ;
    virtual void Copy() ;
    virtual void Cut() ;
    virtual void Paste() ;
    virtual bool CanPaste() const ;
    virtual void SetEditable( bool editable ) ;
    virtual long GetLastPosition() const ;
    virtual void Replace( long from, long to, const wxString &str ) ;
    virtual void Remove( long from, long to ) ;


    virtual bool HasOwnContextMenu() const
        { return false ; }

    virtual bool SetupCursor( const wxPoint& WXUNUSED(pt) )
        { return false ; }

    virtual void Clear() ;
    virtual bool CanUndo() const;
    virtual void Undo() ;
    virtual bool CanRedo() const;
    virtual void Redo() ;
    virtual int GetNumberOfLines() const ;
    virtual long XYToPosition(long x, long y) const;
    virtual bool PositionToXY(long pos, long *x, long *y) const ;
    virtual void ShowPosition(long WXUNUSED(pos)) ;
    virtual int GetLineLength(long lineNo) const ;
    virtual wxString GetLineText(long lineNo) const ;
    virtual void CheckSpelling(bool WXUNUSED(check)) { }

    virtual wxSize GetBestSize() const { return wxDefaultSize; }

    virtual bool SetHint(const wxString& WXUNUSED(hint)) { return false; }
private:
    wxTextEntry * const m_entry;

    wxDECLARE_NO_COPY_CLASS(wxTextWidgetImpl);
};

// common interface for all implementations
class WXDLLIMPEXP_CORE wxComboWidgetImpl

{
public :
    wxComboWidgetImpl() {}

    virtual ~wxComboWidgetImpl() {}

    virtual int GetSelectedItem() const { return -1; }
    virtual void SetSelectedItem(int WXUNUSED(item)) {}

    virtual int GetNumberOfItems() const { return -1; }

    virtual void InsertItem(int WXUNUSED(pos), const wxString& WXUNUSED(item)) {}

    virtual void RemoveItem(int WXUNUSED(pos)) {}

    virtual void Clear() {}
    virtual void Popup() {}
    virtual void Dismiss() {}

    virtual wxString GetStringAtIndex(int WXUNUSED(pos)) const { return wxEmptyString; }

    virtual int FindString(const wxString& WXUNUSED(text)) const { return -1; }
};

//
// common interface for buttons
//

class wxButtonImpl
{
    public :
    wxButtonImpl(){}
    virtual ~wxButtonImpl(){}

    virtual void SetPressedBitmap( const wxBitmap& bitmap ) = 0;
} ;

//
// common interface for search controls
//

class wxSearchWidgetImpl
{
public :
    wxSearchWidgetImpl(){}
    virtual ~wxSearchWidgetImpl(){}

    // search field options
    virtual void ShowSearchButton( bool show ) = 0;
    virtual bool IsSearchButtonVisible() const = 0;

    virtual void ShowCancelButton( bool show ) = 0;
    virtual bool IsCancelButtonVisible() const = 0;

    virtual void SetSearchMenu( wxMenu* menu ) = 0;

    virtual void SetDescriptiveText(const wxString& text) = 0;
} ;

//
// toplevel window implementation class
//

class wxNonOwnedWindowImpl : public wxObject
{
public :
    wxNonOwnedWindowImpl( wxNonOwnedWindow* nonownedwnd) : m_wxPeer(nonownedwnd)
    {
    }
    wxNonOwnedWindowImpl()
    {
    }
    virtual ~wxNonOwnedWindowImpl()
    {
    }

    virtual void WillBeDestroyed()
    {
    }

    virtual void Create( wxWindow* parent, const wxPoint& pos, const wxSize& size,
    long style, long extraStyle, const wxString& name ) = 0;


    virtual WXWindow GetWXWindow() const = 0;

    virtual void Raise()
    {
    }

    virtual void Lower()
    {
    }

    virtual bool Show(bool WXUNUSED(show))
    {
        return false;
    }

    virtual bool ShowWithEffect(bool show, wxShowEffect WXUNUSED(effect), unsigned WXUNUSED(timeout))
    {
        return Show(show);
    }

    virtual void Update()
    {
    }

    virtual bool SetTransparent(wxByte WXUNUSED(alpha))
    {
        return false;
    }

    virtual bool SetBackgroundColour(const wxColour& WXUNUSED(col) )
    {
        return false;
    }

    virtual void SetExtraStyle( long WXUNUSED(exStyle) )
    {
    }

    virtual void SetWindowStyleFlag( long WXUNUSED(style) )
    {
    }

    virtual bool SetBackgroundStyle(wxBackgroundStyle WXUNUSED(style))
    {
        return false ;
    }

    virtual bool CanSetTransparent()
    {
        return false;
    }

    virtual void GetContentArea( int &left , int &top , int &width , int &height ) const = 0;
    virtual void MoveWindow(int x, int y, int width, int height) = 0;
    virtual void GetPosition( int &x, int &y ) const = 0;
    virtual void GetSize( int &width, int &height ) const = 0;

    virtual bool SetShape(const wxRegion& WXUNUSED(region))
    {
        return false;
    }

    virtual void SetTitle( const wxString& title, wxFontEncoding encoding ) = 0;

    virtual bool IsMaximized() const = 0;

    virtual bool IsIconized() const= 0;

    virtual void Iconize( bool iconize )= 0;

    virtual void Maximize(bool maximize) = 0;

    virtual bool IsFullScreen() const= 0;

    virtual void ShowWithoutActivating() { Show(true); }

    virtual bool ShowFullScreen(bool show, long style)= 0;

    virtual void RequestUserAttention(int flags) = 0;

    virtual void ScreenToWindow( int *x, int *y ) = 0;

    virtual void WindowToScreen( int *x, int *y ) = 0;
    
    virtual bool IsActive() = 0;

    wxNonOwnedWindow*   GetWXPeer() { return m_wxPeer; }

    static wxNonOwnedWindowImpl*
                FindFromWXWindow(WXWindow window);

    static void  RemoveAssociations( wxNonOwnedWindowImpl* impl);

    static void  Associate( WXWindow window, wxNonOwnedWindowImpl *impl );

    // static creation methods, must be implemented by all toolkits

    static wxNonOwnedWindowImpl* CreateNonOwnedWindow( wxNonOwnedWindow* wxpeer, wxWindow* parent, WXWindow native) ;

    static wxNonOwnedWindowImpl* CreateNonOwnedWindow( wxNonOwnedWindow* wxpeer, wxWindow* parent, const wxPoint& pos, const wxSize& size,
    long style, long extraStyle, const wxString& name  ) ;

    virtual void SetModified(bool WXUNUSED(modified)) { }
    virtual bool IsModified() const { return false; }

    virtual void SetRepresentedFilename(const wxString& WXUNUSED(filename)) { }

#if wxOSX_USE_IPHONE
    virtual CGFloat GetWindowLevel() const { return 0.0; }
#else
    virtual CGWindowLevel GetWindowLevel() const { return kCGNormalWindowLevel; }
#endif
    virtual void RestoreWindowLevel() {}
protected :
    wxNonOwnedWindow*   m_wxPeer;
    DECLARE_ABSTRACT_CLASS(wxNonOwnedWindowImpl)
};

#endif // wxUSE_GUI

//---------------------------------------------------------------------------
// cocoa bridging utilities
//---------------------------------------------------------------------------

bool wxMacInitCocoa();

class WXDLLIMPEXP_CORE wxMacAutoreleasePool
{
public :
    wxMacAutoreleasePool();
    ~wxMacAutoreleasePool();
private :
    void* m_pool;
};

// NSObject

void wxMacCocoaRelease( void* obj );
void wxMacCocoaAutorelease( void* obj );
void* wxMacCocoaRetain( void* obj );


#endif
    // _WX_PRIVATE_CORE_H_
