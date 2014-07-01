/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/iphone/private.h
// Purpose:     Private declarations: as this header is only included by
//              wxWidgets itself, it may contain identifiers which don't start
//              with "wx".
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_IPHONE_H_
#define _WX_PRIVATE_IPHONE_H_

#ifdef __OBJC__
    #import <UIKit/UIKit.h>
#endif

#include <CoreText/CTFont.h>
#include <CoreText/CTStringAttributes.h>
#include <CoreText/CTLine.h>


#if wxUSE_GUI

OSStatus WXDLLIMPEXP_CORE wxMacDrawCGImage(
                               CGContextRef    inContext,
                               const CGRect *  inBounds,
                               CGImageRef      inImage) ;

WX_UIImage WXDLLIMPEXP_CORE wxOSXGetUIImageFromCGImage( CGImageRef image );
wxBitmap WXDLLIMPEXP_CORE wxOSXCreateSystemBitmap(const wxString& id, const wxString &client, const wxSize& size);

class WXDLLIMPEXP_CORE wxWidgetIPhoneImpl : public wxWidgetImpl
{
public :
    wxWidgetIPhoneImpl( wxWindowMac* peer , WXWidget w, bool isRootControl = false, bool isUserPane = false ) ;
    wxWidgetIPhoneImpl() ;
    ~wxWidgetIPhoneImpl();

    void Init();

    virtual bool        IsVisible() const ;
    virtual void        SetVisibility( bool visible );

    virtual void        Raise();

    virtual void        Lower();

    virtual void        ScrollRect( const wxRect *rect, int dx, int dy );

    virtual WXWidget    GetWXWidget() const { return m_osxView; }

    virtual void        SetBackgroundColour( const wxColour& col ) ;
    virtual bool        SetBackgroundStyle(wxBackgroundStyle style) ;

    virtual void        GetContentArea( int &left , int &top , int &width , int &height ) const;
    virtual void        Move(int x, int y, int width, int height);
    virtual void        GetPosition( int &x, int &y ) const;
    virtual void        GetSize( int &width, int &height ) const;
    virtual void        SetControlSize( wxWindowVariant variant );
    virtual double      GetContentScaleFactor() const ;
    
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
    virtual void        SetLabel(const wxString& title, wxFontEncoding encoding);

    void                SetCursor( const wxCursor & cursor );
    void                CaptureMouse();
    void                ReleaseMouse();

    wxInt32             GetValue() const;
    void                SetValue( wxInt32 v );

    virtual wxBitmap    GetBitmap() const;
    virtual void        SetBitmap( const wxBitmap& bitmap );
    virtual void        SetBitmapPosition( wxDirection dir );

    void                SetupTabs( const wxNotebook &notebook );
    void                GetBestRect( wxRect *r ) const;
    bool                IsEnabled() const;
    void                Enable( bool enable );
    bool                ButtonClickDidStateChange() { return true ;}
    void                SetMinimum( wxInt32 v );
    void                SetMaximum( wxInt32 v );
    wxInt32             GetMinimum() const;
    wxInt32             GetMaximum() const;
    void                PulseGauge();
    void                SetScrollThumb( wxInt32 value, wxInt32 thumbSize );

    void                SetFont( const wxFont & font , const wxColour& foreground , long windowStyle, bool ignoreBlack = true );

    void                InstallEventHandler( WXWidget control = NULL );

    virtual void        DoNotifyFocusEvent(bool receivedFocus, wxWidgetImpl* otherWindow);

    // thunk connected calls

    virtual void        drawRect(CGRect* rect, WXWidget slf, void* _cmd);
    virtual void        touchEvent(WX_NSSet touches, WX_UIEvent event, WXWidget slf, void* _cmd);
    virtual bool        becomeFirstResponder(WXWidget slf, void* _cmd);
    virtual bool        resignFirstResponder(WXWidget slf, void* _cmd);

    // action

    virtual void        controlAction(void* sender, wxUint32 controlEvent, WX_UIEvent rawEvent);
    virtual void         controlTextDidChange();
protected:
    WXWidget m_osxView;
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxWidgetIPhoneImpl)
};

class wxNonOwnedWindowIPhoneImpl : public wxNonOwnedWindowImpl
{
public :
    wxNonOwnedWindowIPhoneImpl( wxNonOwnedWindow* nonownedwnd) ;
    wxNonOwnedWindowIPhoneImpl();

    virtual ~wxNonOwnedWindowIPhoneImpl();

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

    virtual void RequestUserAttention(int flags);

    virtual void ScreenToWindow( int *x, int *y );

    virtual void WindowToScreen( int *x, int *y );

    // FIXME: Does iPhone have a concept of inactive windows?
    virtual bool IsActive() { return true; }

    wxNonOwnedWindow*   GetWXPeer() { return m_wxPeer; }

    virtual bool InitialShowEventSent() { return m_initialShowSent; }
protected :
    WX_UIWindow          m_macWindow;
    void *              m_macFullScreenData ;
    bool                m_initialShowSent;
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxNonOwnedWindowIPhoneImpl)
};

#ifdef __OBJC__

    WXDLLIMPEXP_CORE CGRect wxToNSRect( UIView* parent, const wxRect& r );
    WXDLLIMPEXP_CORE wxRect wxFromNSRect( UIView* parent, const CGRect& rect );
    WXDLLIMPEXP_CORE CGPoint wxToNSPoint( UIView* parent, const wxPoint& p );
    WXDLLIMPEXP_CORE wxPoint wxFromNSPoint( UIView* parent, const CGPoint& p );

    CGRect WXDLLIMPEXP_CORE wxOSXGetFrameForControl( wxWindowMac* window , const wxPoint& pos , const wxSize &size ,
        bool adjustForOrigin = true );

    @interface wxUIButton : UIButton
    {
    }

    @end

    @interface wxUIView : UIView
    {
    }

    @end // wxUIView


    void WXDLLIMPEXP_CORE wxOSXIPhoneClassAddWXMethods(Class c);

#endif

#endif // wxUSE_GUI

#endif
    // _WX_PRIVATE_IPHONE_H_
