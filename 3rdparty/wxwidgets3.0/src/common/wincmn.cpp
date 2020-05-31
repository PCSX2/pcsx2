/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/wincmn.cpp
// Purpose:     common (to all ports) wxWindow functions
// Author:      Julian Smart, Vadim Zeitlin
// Modified by:
// Created:     13/07/98
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/frame.h"
    #include "wx/window.h"
    #include "wx/control.h"
    #include "wx/checkbox.h"
    #include "wx/radiobut.h"
    #include "wx/statbox.h"
    #include "wx/textctrl.h"
    #include "wx/settings.h"
    #include "wx/dialog.h"
    #include "wx/msgdlg.h"
    #include "wx/msgout.h"
    #include "wx/statusbr.h"
    #include "wx/toolbar.h"
    #include "wx/dcclient.h"
    #include "wx/scrolbar.h"
    #include "wx/layout.h"
    #include "wx/sizer.h"
    #include "wx/menu.h"
#endif //WX_PRECOMP

#if wxUSE_DRAG_AND_DROP
    #include "wx/dnd.h"
#endif // wxUSE_DRAG_AND_DROP

#if wxUSE_ACCESSIBILITY
    #include "wx/access.h"
#endif

#if wxUSE_HELP
    #include "wx/cshelp.h"
#endif // wxUSE_HELP

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif // wxUSE_TOOLTIPS

#if wxUSE_CARET
    #include "wx/caret.h"
#endif // wxUSE_CARET

#if wxUSE_SYSTEM_OPTIONS
    #include "wx/sysopt.h"
#endif

#include "wx/platinfo.h"
#include "wx/recguard.h"
#include "wx/private/window.h"

#ifdef __WINDOWS__
    #include "wx/msw/wrapwin.h"
#endif

// Windows List
WXDLLIMPEXP_DATA_CORE(wxWindowList) wxTopLevelWindows;

// globals
#if wxUSE_MENUS
wxMenu *wxCurrentPopupMenu = NULL;
#endif // wxUSE_MENUS

extern WXDLLEXPORT_DATA(const char) wxPanelNameStr[] = "panel";

namespace wxMouseCapture
{

// Check if the given window is in the capture stack.
bool IsInCaptureStack(wxWindowBase* win);

} // wxMouseCapture

// ----------------------------------------------------------------------------
// static data
// ----------------------------------------------------------------------------


IMPLEMENT_ABSTRACT_CLASS(wxWindowBase, wxEvtHandler)

// ----------------------------------------------------------------------------
// event table
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxWindowBase, wxEvtHandler)
    EVT_SYS_COLOUR_CHANGED(wxWindowBase::OnSysColourChanged)
    EVT_INIT_DIALOG(wxWindowBase::OnInitDialog)
    EVT_MIDDLE_DOWN(wxWindowBase::OnMiddleClick)

#if wxUSE_HELP
    EVT_HELP(wxID_ANY, wxWindowBase::OnHelp)
#endif // wxUSE_HELP

    EVT_SIZE(wxWindowBase::InternalOnSize)
END_EVENT_TABLE()

// ============================================================================
// implementation of the common functionality of the wxWindow class
// ============================================================================

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

#if wxUSE_EXTENDED_RTTI

// windows that are created from a parent window during its Create method, 
// eg. spin controls in a calendar controls must never been streamed out 
// separately otherwise chaos occurs. Right now easiest is to test for negative ids, 
// as windows with negative ids never can be recreated anyway


bool wxWindowStreamingCallback( const wxObject *object, wxObjectWriter *, 
                               wxObjectWriterCallback *, const wxStringToAnyHashMap & )
{
    const wxWindow * win = wx_dynamic_cast(const wxWindow*, object);
    if ( win && win->GetId() < 0 )
        return false;
    return true;
}

wxIMPLEMENT_DYNAMIC_CLASS_XTI_CALLBACK(wxWindow, wxWindowBase, "wx/window.h", \
                                       wxWindowStreamingCallback)

// make wxWindowList known before the property is used

wxCOLLECTION_TYPE_INFO( wxWindow*, wxWindowList );

template<> void wxCollectionToVariantArray( wxWindowList const &theList, 
                                           wxAnyList &value)
{
    wxListCollectionToAnyList<wxWindowList::compatibility_iterator>( theList, value );
}

wxDEFINE_FLAGS( wxWindowStyle )

wxBEGIN_FLAGS( wxWindowStyle )
// new style border flags, we put them first to
// use them for streaming out

wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxEND_FLAGS( wxWindowStyle )

wxBEGIN_PROPERTIES_TABLE(wxWindow)
wxEVENT_PROPERTY( Close, wxEVT_CLOSE_WINDOW, wxCloseEvent)
wxEVENT_PROPERTY( Create, wxEVT_CREATE, wxWindowCreateEvent )
wxEVENT_PROPERTY( Destroy, wxEVT_DESTROY, wxWindowDestroyEvent )
// Always constructor Properties first

wxREADONLY_PROPERTY( Parent,wxWindow*, GetParent, wxEMPTY_PARAMETER_VALUE, \
                    0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxPROPERTY( Id,wxWindowID, SetId, GetId, -1 /*wxID_ANY*/, 0 /*flags*/, \
           wxT("Helpstring"), wxT("group") )
wxPROPERTY( Position,wxPoint, SetPosition, GetPosition, wxDefaultPosition, \
           0 /*flags*/, wxT("Helpstring"), wxT("group")) // pos
wxPROPERTY( Size,wxSize, SetSize, GetSize, wxDefaultSize, 0 /*flags*/, \
           wxT("Helpstring"), wxT("group")) // size
wxPROPERTY( WindowStyle, long, SetWindowStyleFlag, GetWindowStyleFlag, \
           wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), wxT("group")) // style
wxPROPERTY( Name,wxString, SetName, GetName, wxEmptyString, 0 /*flags*/, \
           wxT("Helpstring"), wxT("group") )

// Then all relations of the object graph

wxREADONLY_PROPERTY_COLLECTION( Children, wxWindowList, wxWindowBase*, \
                               GetWindowChildren, wxPROP_OBJECT_GRAPH /*flags*/, \
                               wxT("Helpstring"), wxT("group"))

// and finally all other properties

wxPROPERTY( ExtraStyle, long, SetExtraStyle, GetExtraStyle, wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group")) // extstyle
wxPROPERTY( BackgroundColour, wxColour, SetBackgroundColour, GetBackgroundColour, \
           wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), wxT("group")) // bg
wxPROPERTY( ForegroundColour, wxColour, SetForegroundColour, GetForegroundColour, \
           wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), wxT("group")) // fg
wxPROPERTY( Enabled, bool, Enable, IsEnabled, wxAny((bool)true), 0 /*flags*/, \
           wxT("Helpstring"), wxT("group"))
wxPROPERTY( Shown, bool, Show, IsShown, wxAny((bool)true), 0 /*flags*/, \
           wxT("Helpstring"), wxT("group"))

#if 0
// possible property candidates (not in xrc) or not valid in all subclasses
wxPROPERTY( Title,wxString, SetTitle, GetTitle, wxEmptyString )
wxPROPERTY( Font, wxFont, SetFont, GetWindowFont , )
wxPROPERTY( Label,wxString, SetLabel, GetLabel, wxEmptyString )
// MaxHeight, Width, MinHeight, Width
// TODO switch label to control and title to toplevels

wxPROPERTY( ThemeEnabled, bool, SetThemeEnabled, GetThemeEnabled, )
//wxPROPERTY( Cursor, wxCursor, SetCursor, GetCursor, )
// wxPROPERTY( ToolTip, wxString, SetToolTip, GetToolTipText, )
wxPROPERTY( AutoLayout, bool, SetAutoLayout, GetAutoLayout, )
#endif
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxWindow)

wxCONSTRUCTOR_DUMMY(wxWindow)

#else

#ifndef __WXUNIVERSAL__
IMPLEMENT_DYNAMIC_CLASS(wxWindow, wxWindowBase)
#endif

#endif

// ----------------------------------------------------------------------------
// initialization
// ----------------------------------------------------------------------------

// the default initialization
wxWindowBase::wxWindowBase()
{
    // no window yet, no parent nor children
    m_parent = NULL;
    m_windowId = wxID_ANY;

    // no constraints on the minimal window size
    m_minWidth =
    m_maxWidth = wxDefaultCoord;
    m_minHeight =
    m_maxHeight = wxDefaultCoord;

    // invalidiated cache value
    m_bestSizeCache = wxDefaultSize;

    // window are created enabled and visible by default
    m_isShown =
    m_isEnabled = true;

    // the default event handler is just this window
    m_eventHandler = this;

#if wxUSE_VALIDATORS
    // no validator
    m_windowValidator = NULL;
#endif // wxUSE_VALIDATORS

    // the colours/fonts are default for now, so leave m_font,
    // m_backgroundColour and m_foregroundColour uninitialized and set those
    m_hasBgCol =
    m_hasFgCol =
    m_hasFont = false;
    m_inheritBgCol =
    m_inheritFgCol =
    m_inheritFont = false;

    // no style bits
    m_exStyle =
    m_windowStyle = 0;

    m_backgroundStyle = wxBG_STYLE_ERASE;

#if wxUSE_CONSTRAINTS
    // no constraints whatsoever
    m_constraints = NULL;
    m_constraintsInvolvedIn = NULL;
#endif // wxUSE_CONSTRAINTS

    m_windowSizer = NULL;
    m_containingSizer = NULL;
    m_autoLayout = false;

#if wxUSE_DRAG_AND_DROP
    m_dropTarget = NULL;
#endif // wxUSE_DRAG_AND_DROP

#if wxUSE_TOOLTIPS
    m_tooltip = NULL;
#endif // wxUSE_TOOLTIPS

#if wxUSE_CARET
    m_caret = NULL;
#endif // wxUSE_CARET

#if wxUSE_PALETTE
    m_hasCustomPalette = false;
#endif // wxUSE_PALETTE

#if wxUSE_ACCESSIBILITY
    m_accessible = NULL;
#endif

    m_virtualSize = wxDefaultSize;

    m_scrollHelper = NULL;

    m_windowVariant = wxWINDOW_VARIANT_NORMAL;
#if wxUSE_SYSTEM_OPTIONS
    if ( wxSystemOptions::HasOption(wxWINDOW_DEFAULT_VARIANT) )
    {
       m_windowVariant = (wxWindowVariant) wxSystemOptions::GetOptionInt( wxWINDOW_DEFAULT_VARIANT ) ;
    }
#endif

    // Whether we're using the current theme for this window (wxGTK only for now)
    m_themeEnabled = false;

    // This is set to true by SendDestroyEvent() which should be called by the
    // most derived class to ensure that the destruction event is sent as soon
    // as possible to allow its handlers to still see the undestroyed window
    m_isBeingDeleted = false;

    m_freezeCount = 0;
}

// common part of window creation process
bool wxWindowBase::CreateBase(wxWindowBase *parent,
                              wxWindowID id,
                              const wxPoint& WXUNUSED(pos),
                              const wxSize& size,
                              long style,
                              const wxString& name)
{
    // ids are limited to 16 bits under MSW so if you care about portability,
    // it's not a good idea to use ids out of this range (and negative ids are
    // reserved for wxWidgets own usage)
    wxASSERT_MSG( id == wxID_ANY || (id >= 0 && id < 32767) ||
                  (id >= wxID_AUTO_LOWEST && id <= wxID_AUTO_HIGHEST),
                  wxT("invalid id value") );

    // generate a new id if the user doesn't care about it
    if ( id == wxID_ANY )
    {
        m_windowId = NewControlId();
    }
    else // valid id specified
    {
        m_windowId = id;
    }

    // don't use SetWindowStyleFlag() here, this function should only be called
    // to change the flag after creation as it tries to reflect the changes in
    // flags by updating the window dynamically and we don't need this here
    m_windowStyle = style;

    // assume the user doesn't want this window to shrink beneath its initial
    // size, this worked like this in wxWidgets 2.8 and before and generally
    // often makes sense for child windows (for top level ones it definitely
    // does not as the user should be able to resize the window)
    //
    // note that we can't use IsTopLevel() from ctor
    if ( size != wxDefaultSize && !wxTopLevelWindows.Find((wxWindow *)this) )
        SetMinSize(size);

    SetName(name);
    SetParent(parent);

    return true;
}

bool wxWindowBase::CreateBase(wxWindowBase *parent,
                              wxWindowID id,
                              const wxPoint& pos,
                              const wxSize& size,
                              long style,
                              const wxValidator& wxVALIDATOR_PARAM(validator),
                              const wxString& name)
{
    if ( !CreateBase(parent, id, pos, size, style, name) )
        return false;

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif // wxUSE_VALIDATORS

    // if the parent window has wxWS_EX_VALIDATE_RECURSIVELY set, we want to
    // have it too - like this it's possible to set it only in the top level
    // dialog/frame and all children will inherit it by defult
    if ( parent && (parent->GetExtraStyle() & wxWS_EX_VALIDATE_RECURSIVELY) )
    {
        SetExtraStyle(GetExtraStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
    }

    return true;
}

bool wxWindowBase::ToggleWindowStyle(int flag)
{
    wxASSERT_MSG( flag, wxT("flags with 0 value can't be toggled") );

    bool rc;
    long style = GetWindowStyleFlag();
    if ( style & flag )
    {
        style &= ~flag;
        rc = false;
    }
    else // currently off
    {
        style |= flag;
        rc = true;
    }

    SetWindowStyleFlag(style);

    return rc;
}

// ----------------------------------------------------------------------------
// destruction
// ----------------------------------------------------------------------------

// common clean up
wxWindowBase::~wxWindowBase()
{
    wxASSERT_MSG( !wxMouseCapture::IsInCaptureStack(this),
                    "Destroying window before releasing mouse capture: this "
                    "will result in a crash later." );

    // FIXME if these 2 cases result from programming errors in the user code
    //       we should probably assert here instead of silently fixing them

    // Just in case the window has been Closed, but we're then deleting
    // immediately: don't leave dangling pointers.
    wxPendingDelete.DeleteObject(this);

    // Just in case we've loaded a top-level window via LoadNativeDialog but
    // we weren't a dialog class
    wxTopLevelWindows.DeleteObject((wxWindow*)this);

    // Any additional event handlers should be popped before the window is
    // deleted as otherwise the last handler will be left with a dangling
    // pointer to this window result in a difficult to diagnose crash later on.
    wxASSERT_MSG( GetEventHandler() == this,
                    wxT("any pushed event handlers must have been removed") );

#if wxUSE_MENUS
    // The associated popup menu can still be alive, disassociate from it in
    // this case
    if ( wxCurrentPopupMenu && wxCurrentPopupMenu->GetInvokingWindow() == this )
        wxCurrentPopupMenu->SetInvokingWindow(NULL);
#endif // wxUSE_MENUS

    wxASSERT_MSG( GetChildren().GetCount() == 0, wxT("children not destroyed") );

    // notify the parent about this window destruction
    if ( m_parent )
        m_parent->RemoveChild(this);

#if wxUSE_CARET
    delete m_caret;
#endif // wxUSE_CARET

#if wxUSE_VALIDATORS
    delete m_windowValidator;
#endif // wxUSE_VALIDATORS

#if wxUSE_CONSTRAINTS
    // Have to delete constraints/sizer FIRST otherwise sizers may try to look
    // at deleted windows as they delete themselves.
    DeleteRelatedConstraints();

    if ( m_constraints )
    {
        // This removes any dangling pointers to this window in other windows'
        // constraintsInvolvedIn lists.
        UnsetConstraints(m_constraints);
        wxDELETE(m_constraints);
    }
#endif // wxUSE_CONSTRAINTS

    if ( m_containingSizer )
        m_containingSizer->Detach( (wxWindow*)this );

    delete m_windowSizer;

#if wxUSE_DRAG_AND_DROP
    delete m_dropTarget;
#endif // wxUSE_DRAG_AND_DROP

#if wxUSE_TOOLTIPS
    delete m_tooltip;
#endif // wxUSE_TOOLTIPS

#if wxUSE_ACCESSIBILITY
    delete m_accessible;
#endif

#if wxUSE_HELP
    // NB: this has to be called unconditionally, because we don't know
    //     whether this window has associated help text or not
    wxHelpProvider *helpProvider = wxHelpProvider::Get();
    if ( helpProvider )
        helpProvider->RemoveHelp(this);
#endif
}

bool wxWindowBase::IsBeingDeleted() const
{
    return m_isBeingDeleted ||
            (!IsTopLevel() && m_parent && m_parent->IsBeingDeleted());
}

void wxWindowBase::SendDestroyEvent()
{
    if ( m_isBeingDeleted )
    {
        // we could have been already called from a more derived class dtor,
        // e.g. ~wxTLW calls us and so does ~wxWindow and the latter call
        // should be simply ignored
        return;
    }

    m_isBeingDeleted = true;

    wxWindowDestroyEvent event;
    event.SetEventObject(this);
    event.SetId(GetId());
    GetEventHandler()->ProcessEvent(event);
}

bool wxWindowBase::Destroy()
{
    // If our handle is invalid, it means that this window has never been
    // created, either because creating it failed or, more typically, because
    // this wxWindow object was default-constructed and its Create() method had
    // never been called. As we didn't send wxWindowCreateEvent in this case
    // (which is sent after successful creation), don't send the matching
    // wxWindowDestroyEvent neither.
    if ( GetHandle() )
        SendDestroyEvent();

    delete this;

    return true;
}

bool wxWindowBase::Close(bool force)
{
    wxCloseEvent event(wxEVT_CLOSE_WINDOW, m_windowId);
    event.SetEventObject(this);
    event.SetCanVeto(!force);

    // return false if window wasn't closed because the application vetoed the
    // close event
    return HandleWindowEvent(event) && !event.GetVeto();
}

bool wxWindowBase::DestroyChildren()
{
    wxWindowList::compatibility_iterator node;
    for ( ;; )
    {
        // we iterate until the list becomes empty
        node = GetChildren().GetFirst();
        if ( !node )
            break;

        wxWindow *child = node->GetData();

        // note that we really want to delete it immediately so don't call the
        // possible overridden Destroy() version which might not delete the
        // child immediately resulting in problems with our (top level) child
        // outliving its parent
        child->wxWindowBase::Destroy();

        wxASSERT_MSG( !GetChildren().Find(child),
                      wxT("child didn't remove itself using RemoveChild()") );
    }

    return true;
}

// ----------------------------------------------------------------------------
// size/position related methods
// ----------------------------------------------------------------------------

// centre the window with respect to its parent in either (or both) directions
void wxWindowBase::DoCentre(int dir)
{
    wxCHECK_RET( !(dir & wxCENTRE_ON_SCREEN) && GetParent(),
                 wxT("this method only implements centering child windows") );

    SetSize(GetRect().CentreIn(GetParent()->GetClientSize(), dir));
}

// fits the window around the children
void wxWindowBase::Fit()
{
    SetSize(GetBestSize());
}

// fits virtual size (ie. scrolled area etc.) around children
void wxWindowBase::FitInside()
{
    SetVirtualSize( GetBestVirtualSize() );
}

// On Mac, scrollbars are explicitly children.
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__)
static bool wxHasRealChildren(const wxWindowBase* win)
{
    int realChildCount = 0;

    for ( wxWindowList::compatibility_iterator node = win->GetChildren().GetFirst();
          node;
          node = node->GetNext() )
    {
        wxWindow *win = node->GetData();
        if ( !win->IsTopLevel() && win->IsShown()
#if wxUSE_SCROLLBAR
            && !wxDynamicCast(win, wxScrollBar)
#endif
            )
            realChildCount ++;
    }
    return (realChildCount > 0);
}
#endif

void wxWindowBase::InvalidateBestSize()
{
    m_bestSizeCache = wxDefaultSize;

    // parent's best size calculation may depend on its children's
    // as long as child window we are in is not top level window itself
    // (because the TLW size is never resized automatically)
    // so let's invalidate it as well to be safe:
    if (m_parent && !IsTopLevel())
        m_parent->InvalidateBestSize();
}

// return the size best suited for the current window
wxSize wxWindowBase::DoGetBestSize() const
{
    wxSize best;

    if ( m_windowSizer )
    {
        best = m_windowSizer->GetMinSize();
    }
#if wxUSE_CONSTRAINTS
    else if ( m_constraints )
    {
        wxConstCast(this, wxWindowBase)->SatisfyConstraints();

        // our minimal acceptable size is such that all our windows fit inside
        int maxX = 0,
            maxY = 0;

        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxLayoutConstraints *c = node->GetData()->GetConstraints();
            if ( !c )
            {
                // it's not normal that we have an unconstrained child, but
                // what can we do about it?
                continue;
            }

            int x = c->right.GetValue(),
                y = c->bottom.GetValue();

            if ( x > maxX )
                maxX = x;

            if ( y > maxY )
                maxY = y;

            // TODO: we must calculate the overlaps somehow, otherwise we
            //       will never return a size bigger than the current one :-(
        }

        best = wxSize(maxX, maxY);
    }
#endif // wxUSE_CONSTRAINTS
    else if ( !GetChildren().empty()
#if defined( __WXMAC__ ) && !defined(__WXUNIVERSAL__)
              && wxHasRealChildren(this)
#endif
              )
    {
        // our minimal acceptable size is such that all our visible child
        // windows fit inside
        int maxX = 0,
            maxY = 0;

        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxWindow *win = node->GetData();
            if ( win->IsTopLevel()
                    || !win->IsShown()
#if wxUSE_STATUSBAR
                        || wxDynamicCast(win, wxStatusBar)
#endif // wxUSE_STATUSBAR
               )
            {
                // dialogs and frames lie in different top level windows -
                // don't deal with them here; as for the status bars, they
                // don't lie in the client area at all
                continue;
            }

            int wx, wy, ww, wh;
            win->GetPosition(&wx, &wy);

            // if the window hadn't been positioned yet, assume that it is in
            // the origin
            if ( wx == wxDefaultCoord )
                wx = 0;
            if ( wy == wxDefaultCoord )
                wy = 0;

            win->GetSize(&ww, &wh);
            if ( wx + ww > maxX )
                maxX = wx + ww;
            if ( wy + wh > maxY )
                maxY = wy + wh;
        }

        best = wxSize(maxX, maxY);
    }
    else // ! has children
    {
        wxSize size = GetMinSize();
        if ( !size.IsFullySpecified() )
        {
            // if the window doesn't define its best size we assume that it can
            // be arbitrarily small -- usually this is not the case, of course,
            // but we have no way to know what the limit is, it should really
            // override DoGetBestClientSize() itself to tell us
            size.SetDefaults(wxSize(1, 1));
        }

        // return as-is, unadjusted by the client size difference.
        return size;
    }

    // Add any difference between size and client size
    wxSize diff = GetSize() - GetClientSize();
    best.x += wxMax(0, diff.x);
    best.y += wxMax(0, diff.y);

    return best;
}

// helper of GetWindowBorderSize(): as many ports don't implement support for
// wxSYS_BORDER/EDGE_X/Y metrics in their wxSystemSettings, use hard coded
// fallbacks in this case
static int wxGetMetricOrDefault(wxSystemMetric what, const wxWindowBase* win)
{
    int rc = wxSystemSettings::GetMetric(
        what, static_cast<wxWindow*>(const_cast<wxWindowBase*>(win)));
    if ( rc == -1 )
    {
        switch ( what )
        {
            case wxSYS_BORDER_X:
            case wxSYS_BORDER_Y:
                // 2D border is by default 1 pixel wide
                rc = 1;
                break;

            case wxSYS_EDGE_X:
            case wxSYS_EDGE_Y:
                // 3D borders are by default 2 pixels
                rc = 2;
                break;

            default:
                wxFAIL_MSG( wxT("unexpected wxGetMetricOrDefault() argument") );
                rc = 0;
        }
    }

    return rc;
}

wxSize wxWindowBase::GetWindowBorderSize() const
{
    wxSize size;

    switch ( GetBorder() )
    {
        case wxBORDER_NONE:
            // nothing to do, size is already (0, 0)
            break;

        case wxBORDER_SIMPLE:
        case wxBORDER_STATIC:
            size.x = wxGetMetricOrDefault(wxSYS_BORDER_X, this);
            size.y = wxGetMetricOrDefault(wxSYS_BORDER_Y, this);
            break;

        case wxBORDER_SUNKEN:
        case wxBORDER_RAISED:
            size.x = wxMax(wxGetMetricOrDefault(wxSYS_EDGE_X, this),
                           wxGetMetricOrDefault(wxSYS_BORDER_X, this));
            size.y = wxMax(wxGetMetricOrDefault(wxSYS_EDGE_Y, this),
                           wxGetMetricOrDefault(wxSYS_BORDER_Y, this));
            break;

        case wxBORDER_DOUBLE:
            size.x = wxGetMetricOrDefault(wxSYS_EDGE_X, this) +
                        wxGetMetricOrDefault(wxSYS_BORDER_X, this);
            size.y = wxGetMetricOrDefault(wxSYS_EDGE_Y, this) +
                        wxGetMetricOrDefault(wxSYS_BORDER_Y, this);
            break;

        default:
            wxFAIL_MSG(wxT("Unknown border style."));
            break;
    }

    // we have borders on both sides
    return size*2;
}

bool
wxWindowBase::InformFirstDirection(int direction,
                                   int size,
                                   int availableOtherDir)
{
    return GetSizer() && GetSizer()->InformFirstDirection(direction,
                                                          size,
                                                          availableOtherDir);
}

wxSize wxWindowBase::GetEffectiveMinSize() const
{
    // merge the best size with the min size, giving priority to the min size
    wxSize min = GetMinSize();

    if (min.x == wxDefaultCoord || min.y == wxDefaultCoord)
    {
        wxSize best = GetBestSize();
        if (min.x == wxDefaultCoord) min.x =  best.x;
        if (min.y == wxDefaultCoord) min.y =  best.y;
    }

    return min;
}

wxSize wxWindowBase::DoGetBorderSize() const
{
    // there is one case in which we can implement it for all ports easily
    if ( GetBorder() == wxBORDER_NONE )
        return wxSize(0, 0);

    // otherwise use the difference between the real size and the client size
    // as a fallback: notice that this is incorrect in general as client size
    // also doesn't take the scrollbars into account
    return GetSize() - GetClientSize();
}

wxSize wxWindowBase::GetBestSize() const
{
    if ( !m_windowSizer && m_bestSizeCache.IsFullySpecified() )
        return m_bestSizeCache;

    // call DoGetBestClientSize() first, if a derived class overrides it wants
    // it to be used
    wxSize size = DoGetBestClientSize();
    if ( size != wxDefaultSize )
        size += DoGetBorderSize();
    else
        size = DoGetBestSize();

    // Ensure that the best size is at least as large as min size.
    size.IncTo(GetMinSize());

    // And not larger than max size.
    size.DecToIfSpecified(GetMaxSize());

    // Finally cache result and return.
    CacheBestSize(size);
    return size;
}

int wxWindowBase::GetBestHeight(int width) const
{
    const int height = DoGetBestClientHeight(width);

    return height == wxDefaultCoord
            ? GetBestSize().y
            : height + DoGetBorderSize().y;
}

int wxWindowBase::GetBestWidth(int height) const
{
    const int width = DoGetBestClientWidth(height);

    return width == wxDefaultCoord
            ? GetBestSize().x
            : width + DoGetBorderSize().x;
}

void wxWindowBase::SetMinSize(const wxSize& minSize)
{
    m_minWidth = minSize.x;
    m_minHeight = minSize.y;

    InvalidateBestSize();
}

void wxWindowBase::SetMaxSize(const wxSize& maxSize)
{
    m_maxWidth = maxSize.x;
    m_maxHeight = maxSize.y;

    InvalidateBestSize();
}

void wxWindowBase::SetInitialSize(const wxSize& size)
{
    // Set the min size to the size passed in.  This will usually either be
    // wxDefaultSize or the size passed to this window's ctor/Create function.
    SetMinSize(size);

    // Merge the size with the best size if needed
    wxSize best = GetEffectiveMinSize();

    // If the current size doesn't match then change it
    if (GetSize() != best)
        SetSize(best);
}


// by default the origin is not shifted
wxPoint wxWindowBase::GetClientAreaOrigin() const
{
    return wxPoint(0,0);
}

wxSize wxWindowBase::ClientToWindowSize(const wxSize& size) const
{
    const wxSize diff(GetSize() - GetClientSize());

    return wxSize(size.x == -1 ? -1 : size.x + diff.x,
                  size.y == -1 ? -1 : size.y + diff.y);
}

wxSize wxWindowBase::WindowToClientSize(const wxSize& size) const
{
    const wxSize diff(GetSize() - GetClientSize());

    return wxSize(size.x == -1 ? -1 : size.x - diff.x,
                  size.y == -1 ? -1 : size.y - diff.y);
}

void wxWindowBase::SetWindowVariant( wxWindowVariant variant )
{
    if ( m_windowVariant != variant )
    {
        m_windowVariant = variant;

        DoSetWindowVariant(variant);
    }
}

void wxWindowBase::DoSetWindowVariant( wxWindowVariant variant )
{
    // adjust the font height to correspond to our new variant (notice that
    // we're only called if something really changed)
    wxFont font = GetFont();
    int size = font.GetPointSize();
    switch ( variant )
    {
        case wxWINDOW_VARIANT_NORMAL:
            break;

        case wxWINDOW_VARIANT_SMALL:
            size = wxRound(size * 3.0 / 4.0);
            break;

        case wxWINDOW_VARIANT_MINI:
            size = wxRound(size * 2.0 / 3.0);
            break;

        case wxWINDOW_VARIANT_LARGE:
            size = wxRound(size * 5.0 / 4.0);
            break;

        default:
            wxFAIL_MSG(wxT("unexpected window variant"));
            break;
    }

    font.SetPointSize(size);
    SetFont(font);
}

void wxWindowBase::DoSetSizeHints( int minW, int minH,
                                   int maxW, int maxH,
                                   int WXUNUSED(incW), int WXUNUSED(incH) )
{
    wxCHECK_RET( (minW == wxDefaultCoord || maxW == wxDefaultCoord || minW <= maxW) &&
                    (minH == wxDefaultCoord || maxH == wxDefaultCoord || minH <= maxH),
                 wxT("min width/height must be less than max width/height!") );

    m_minWidth = minW;
    m_maxWidth = maxW;
    m_minHeight = minH;
    m_maxHeight = maxH;
}


#if WXWIN_COMPATIBILITY_2_8
void wxWindowBase::SetVirtualSizeHints(int WXUNUSED(minW), int WXUNUSED(minH),
                                       int WXUNUSED(maxW), int WXUNUSED(maxH))
{
}

void wxWindowBase::SetVirtualSizeHints(const wxSize& WXUNUSED(minsize),
                                       const wxSize& WXUNUSED(maxsize))
{
}
#endif // WXWIN_COMPATIBILITY_2_8

void wxWindowBase::DoSetVirtualSize( int x, int y )
{
    m_virtualSize = wxSize(x, y);
}

wxSize wxWindowBase::DoGetVirtualSize() const
{
    // we should use the entire client area so if it is greater than our
    // virtual size, expand it to fit (otherwise if the window is big enough we
    // wouldn't be using parts of it)
    wxSize size = GetClientSize();
    if ( m_virtualSize.x > size.x )
        size.x = m_virtualSize.x;

    if ( m_virtualSize.y >= size.y )
        size.y = m_virtualSize.y;

    return size;
}

void wxWindowBase::DoGetScreenPosition(int *x, int *y) const
{
    // screen position is the same as (0, 0) in client coords for non TLWs (and
    // TLWs override this method)
    if ( x )
        *x = 0;
    if ( y )
        *y = 0;

    ClientToScreen(x, y);
}

void wxWindowBase::SendSizeEvent(int flags)
{
    wxSizeEvent event(GetSize(), GetId());
    event.SetEventObject(this);
    if ( flags & wxSEND_EVENT_POST )
        wxPostEvent(GetEventHandler(), event);
    else
        HandleWindowEvent(event);
}

void wxWindowBase::SendSizeEventToParent(int flags)
{
    wxWindow * const parent = GetParent();
    if ( parent && !parent->IsBeingDeleted() )
        parent->SendSizeEvent(flags);
}

bool wxWindowBase::CanScroll(int orient) const
{
    return (m_windowStyle &
            (orient == wxHORIZONTAL ? wxHSCROLL : wxVSCROLL)) != 0;
}

bool wxWindowBase::HasScrollbar(int orient) const
{
    // if scrolling in the given direction is disabled, we can't have the
    // corresponding scrollbar no matter what
    if ( !CanScroll(orient) )
        return false;

    const wxSize sizeVirt = GetVirtualSize();
    const wxSize sizeClient = GetClientSize();

    return orient == wxHORIZONTAL ? sizeVirt.x > sizeClient.x
                                  : sizeVirt.y > sizeClient.y;
}

// ----------------------------------------------------------------------------
// show/hide/enable/disable the window
// ----------------------------------------------------------------------------

bool wxWindowBase::Show(bool show)
{
    if ( show != m_isShown )
    {
        m_isShown = show;

        return true;
    }
    else
    {
        return false;
    }
}

bool wxWindowBase::IsEnabled() const
{
    return IsThisEnabled() && (IsTopLevel() || !GetParent() || GetParent()->IsEnabled());
}

void wxWindowBase::NotifyWindowOnEnableChange(bool enabled)
{
    // Under some platforms there is no need to update the window state
    // explicitly, it will become disabled when its parent is. On other ones we
    // do need to disable all windows recursively though.
#ifndef wxHAS_NATIVE_ENABLED_MANAGEMENT
    DoEnable(enabled);
#endif // !defined(wxHAS_NATIVE_ENABLED_MANAGEMENT)

    // Disabling a top level window is typically done when showing a modal
    // dialog and we don't need to disable its children in this case, they will
    // be logically disabled anyhow (i.e. their IsEnabled() will return false)
    // and the TLW won't accept any input for them. Moreover, explicitly
    // disabling them would look ugly as the entire TLW would be greyed out
    // whenever a modal dialog is shown and no native applications under any
    // platform behave like this.
    if ( IsTopLevel() && !enabled )
        return;

    // When disabling (or enabling back) a non-TLW window we need to
    // recursively propagate the change of the state to its children, otherwise
    // they would still show as enabled even though they wouldn't actually
    // accept any input (at least under MSW where children don't accept input
    // if any of the windows in their parent chain is enabled).
#ifndef wxHAS_NATIVE_ENABLED_MANAGEMENT
    for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
          node;
          node = node->GetNext() )
    {
        wxWindowBase * const child = node->GetData();
        if ( !child->IsTopLevel() && child->IsThisEnabled() )
            child->NotifyWindowOnEnableChange(enabled);
    }
#endif // !defined(wxHAS_NATIVE_ENABLED_MANAGEMENT)
}

bool wxWindowBase::Enable(bool enable)
{
    if ( enable == IsThisEnabled() )
        return false;

    m_isEnabled = enable;

    // If we call DoEnable() from NotifyWindowOnEnableChange(), we don't need
    // to do it from here.
#ifdef wxHAS_NATIVE_ENABLED_MANAGEMENT
    DoEnable(enable);
#endif // !defined(wxHAS_NATIVE_ENABLED_MANAGEMENT)

    NotifyWindowOnEnableChange(enable);

    return true;
}

bool wxWindowBase::IsShownOnScreen() const
{
    // A window is shown on screen if it itself is shown and so are all its
    // parents. But if a window is toplevel one, then its always visible on
    // screen if IsShown() returns true, even if it has a hidden parent.
    return IsShown() &&
           (IsTopLevel() || GetParent() == NULL || GetParent()->IsShownOnScreen());
}

// ----------------------------------------------------------------------------
// RTTI
// ----------------------------------------------------------------------------

bool wxWindowBase::IsTopLevel() const
{
    return false;
}

// ----------------------------------------------------------------------------
// Freeze/Thaw
// ----------------------------------------------------------------------------

void wxWindowBase::Freeze()
{
    if ( !m_freezeCount++ )
    {
        // physically freeze this window:
        DoFreeze();

        // and recursively freeze all children:
        for ( wxWindowList::iterator i = GetChildren().begin();
              i != GetChildren().end(); ++i )
        {
            wxWindow *child = *i;
            if ( child->IsTopLevel() )
                continue;

            child->Freeze();
        }
    }
}

void wxWindowBase::Thaw()
{
    wxASSERT_MSG( m_freezeCount, "Thaw() without matching Freeze()" );

    if ( !--m_freezeCount )
    {
        // recursively thaw all children:
        for ( wxWindowList::iterator i = GetChildren().begin();
              i != GetChildren().end(); ++i )
        {
            wxWindow *child = *i;
            if ( child->IsTopLevel() )
                continue;

            child->Thaw();
        }

        // physically thaw this window:
        DoThaw();
    }
}

// ----------------------------------------------------------------------------
// Dealing with parents and children.
// ----------------------------------------------------------------------------

bool wxWindowBase::IsDescendant(wxWindowBase* win) const
{
    // Iterate until we find this window in the parent chain or exhaust it.
    while ( win )
    {
        if ( win == this )
            return true;

        // Stop iterating on reaching the top level window boundary.
        if ( win->IsTopLevel() )
            break;

        win = win->GetParent();
    }

    return false;
}

void wxWindowBase::AddChild(wxWindowBase *child)
{
    wxCHECK_RET( child, wxT("can't add a NULL child") );

    // this should never happen and it will lead to a crash later if it does
    // because RemoveChild() will remove only one node from the children list
    // and the other(s) one(s) will be left with dangling pointers in them
    wxASSERT_MSG( !GetChildren().Find((wxWindow*)child), wxT("AddChild() called twice") );

    GetChildren().Append((wxWindow*)child);
    child->SetParent(this);

    // adding a child while frozen will assert when thawed, so freeze it as if
    // it had been already present when we were frozen
    if ( IsFrozen() && !child->IsTopLevel() )
        child->Freeze();
}

void wxWindowBase::RemoveChild(wxWindowBase *child)
{
    wxCHECK_RET( child, wxT("can't remove a NULL child") );

    // removing a child while frozen may result in permanently frozen window
    // if used e.g. from Reparent(), so thaw it
    //
    // NB: IsTopLevel() doesn't return true any more when a TLW child is being
    //     removed from its ~wxWindowBase, so check for IsBeingDeleted() too
    if ( IsFrozen() && !child->IsBeingDeleted() && !child->IsTopLevel() )
        child->Thaw();

    GetChildren().DeleteObject((wxWindow *)child);
    child->SetParent(NULL);
}

void wxWindowBase::SetParent(wxWindowBase *parent)
{
    // This assert catches typos which may result in using "this" instead of
    // "parent" when creating the window. This doesn't happen often but when it
    // does the results are unpleasant because the program typically just
    // crashes when due to a stack overflow or something similar and this
    // assert doesn't cost much (OTOH doing a more general check that the
    // parent is not one of our children would be more expensive and probably
    // not worth it).
    wxASSERT_MSG( parent != this, wxS("Can't use window as its own parent") );

    m_parent = (wxWindow *)parent;
}

bool wxWindowBase::Reparent(wxWindowBase *newParent)
{
    wxWindow *oldParent = GetParent();
    if ( newParent == oldParent )
    {
        // nothing done
        return false;
    }

    const bool oldEnabledState = IsEnabled();

    // unlink this window from the existing parent.
    if ( oldParent )
    {
        oldParent->RemoveChild(this);
    }
    else
    {
        wxTopLevelWindows.DeleteObject((wxWindow *)this);
    }

    // add it to the new one
    if ( newParent )
    {
        newParent->AddChild(this);
    }
    else
    {
        wxTopLevelWindows.Append((wxWindow *)this);
    }

    // We need to notify window (and its subwindows) if by changing the parent
    // we also change our enabled/disabled status.
    const bool newEnabledState = IsEnabled();
    if ( newEnabledState != oldEnabledState )
    {
        NotifyWindowOnEnableChange(newEnabledState);
    }

    return true;
}

// ----------------------------------------------------------------------------
// event handler stuff
// ----------------------------------------------------------------------------

void wxWindowBase::SetEventHandler(wxEvtHandler *handler)
{
    wxCHECK_RET(handler != NULL, "SetEventHandler(NULL) called");

    m_eventHandler = handler;
}

void wxWindowBase::SetNextHandler(wxEvtHandler *WXUNUSED(handler))
{
    // disable wxEvtHandler chain mechanism for wxWindows:
    // wxWindow uses its own stack mechanism which doesn't mix well with wxEvtHandler's one

    wxFAIL_MSG("wxWindow cannot be part of a wxEvtHandler chain");
}
void wxWindowBase::SetPreviousHandler(wxEvtHandler *WXUNUSED(handler))
{
    // we can't simply wxFAIL here as in SetNextHandler: in fact the last
    // handler of our stack when is destroyed will be Unlink()ed and thus
    // will call this function to update the pointer of this window...

    //wxFAIL_MSG("wxWindow cannot be part of a wxEvtHandler chain");
}

void wxWindowBase::PushEventHandler(wxEvtHandler *handlerToPush)
{
    wxCHECK_RET( handlerToPush != NULL, "PushEventHandler(NULL) called" );

    // the new handler is going to be part of the wxWindow stack of event handlers:
    // it can't be part also of an event handler double-linked chain:
    wxASSERT_MSG(handlerToPush->IsUnlinked(),
        "The handler being pushed in the wxWindow stack shouldn't be part of "
        "a wxEvtHandler chain; call Unlink() on it first");

    wxEvtHandler *handlerOld = GetEventHandler();
    wxCHECK_RET( handlerOld, "an old event handler is NULL?" );

    // now use wxEvtHandler double-linked list to implement a stack:
    handlerToPush->SetNextHandler(handlerOld);

    if (handlerOld != this)
        handlerOld->SetPreviousHandler(handlerToPush);

    SetEventHandler(handlerToPush);

#if wxDEBUG_LEVEL
    // final checks of the operations done above:
    wxASSERT_MSG( handlerToPush->GetPreviousHandler() == NULL,
        "the first handler of the wxWindow stack should "
        "have no previous handlers set" );
    wxASSERT_MSG( handlerToPush->GetNextHandler() != NULL,
        "the first handler of the wxWindow stack should "
        "have non-NULL next handler" );

    wxEvtHandler* pLast = handlerToPush;
    while ( pLast && pLast != this )
        pLast = pLast->GetNextHandler();
    wxASSERT_MSG( pLast->GetNextHandler() == NULL,
        "the last handler of the wxWindow stack should "
        "have this window as next handler" );
#endif // wxDEBUG_LEVEL
}

wxEvtHandler *wxWindowBase::PopEventHandler(bool deleteHandler)
{
    // we need to pop the wxWindow stack, i.e. we need to remove the first handler

    wxEvtHandler *firstHandler = GetEventHandler();
    wxCHECK_MSG( firstHandler != NULL, NULL, "wxWindow cannot have a NULL event handler" );
    wxCHECK_MSG( firstHandler != this, NULL, "cannot pop the wxWindow itself" );
    wxCHECK_MSG( firstHandler->GetPreviousHandler() == NULL, NULL,
        "the first handler of the wxWindow stack should have no previous handlers set" );

    wxEvtHandler *secondHandler = firstHandler->GetNextHandler();
    wxCHECK_MSG( secondHandler != NULL, NULL,
        "the first handler of the wxWindow stack should have non-NULL next handler" );

    firstHandler->SetNextHandler(NULL);

    // It is harmless but useless to unset the previous handler of the window
    // itself as it's always NULL anyhow, so don't do this.
    if ( secondHandler != this )
        secondHandler->SetPreviousHandler(NULL);

    // now firstHandler is completely unlinked; set secondHandler as the new window event handler
    SetEventHandler(secondHandler);

    if ( deleteHandler )
    {
        wxDELETE(firstHandler);
    }

    return firstHandler;
}

bool wxWindowBase::RemoveEventHandler(wxEvtHandler *handlerToRemove)
{
    wxCHECK_MSG( handlerToRemove != NULL, false, "RemoveEventHandler(NULL) called" );
    wxCHECK_MSG( handlerToRemove != this, false, "Cannot remove the window itself" );

    if (handlerToRemove == GetEventHandler())
    {
        // removing the first event handler is equivalent to "popping" the stack
        PopEventHandler(false);
        return true;
    }

    // NOTE: the wxWindow event handler list is always terminated with "this" handler
    wxEvtHandler *handlerCur = GetEventHandler()->GetNextHandler();
    while ( handlerCur != this && handlerCur )
    {
        wxEvtHandler *handlerNext = handlerCur->GetNextHandler();

        if ( handlerCur == handlerToRemove )
        {
            handlerCur->Unlink();

            wxASSERT_MSG( handlerCur != GetEventHandler(),
                        "the case Remove == Pop should was already handled" );
            return true;
        }

        handlerCur = handlerNext;
    }

    wxFAIL_MSG( wxT("where has the event handler gone?") );

    return false;
}

bool wxWindowBase::HandleWindowEvent(wxEvent& event) const
{
    // SafelyProcessEvent() will handle exceptions nicely
    return GetEventHandler()->SafelyProcessEvent(event);
}

// ----------------------------------------------------------------------------
// colours, fonts &c
// ----------------------------------------------------------------------------

void wxWindowBase::InheritAttributes()
{
    const wxWindowBase * const parent = GetParent();
    if ( !parent )
        return;

    // we only inherit attributes which had been explicitly set for the parent
    // which ensures that this only happens if the user really wants it and
    // not by default which wouldn't make any sense in modern GUIs where the
    // controls don't all use the same fonts (nor colours)
    if ( parent->m_inheritFont && !m_hasFont )
        SetFont(parent->GetFont());

    // in addition, there is a possibility to explicitly forbid inheriting
    // colours at each class level by overriding ShouldInheritColours()
    if ( ShouldInheritColours() )
    {
        if ( parent->m_inheritFgCol && !m_hasFgCol )
            SetForegroundColour(parent->GetForegroundColour());

        // inheriting (solid) background colour is wrong as it totally breaks
        // any kind of themed backgrounds
        //
        // instead, the controls should use the same background as their parent
        // (ideally by not drawing it at all)
#if 0
        if ( parent->m_inheritBgCol && !m_hasBgCol )
            SetBackgroundColour(parent->GetBackgroundColour());
#endif // 0
    }
}

/* static */ wxVisualAttributes
wxWindowBase::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    // it is important to return valid values for all attributes from here,
    // GetXXX() below rely on this
    wxVisualAttributes attrs;
    attrs.font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    attrs.colFg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    // On Smartphone/PocketPC, wxSYS_COLOUR_WINDOW is a better reflection of
    // the usual background colour than wxSYS_COLOUR_BTNFACE.
    // It's a pity that wxSYS_COLOUR_WINDOW isn't always a suitable background
    // colour on other platforms.

#if defined(__WXWINCE__) && (defined(__SMARTPHONE__) || defined(__POCKETPC__))
    attrs.colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
#else
    attrs.colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
#endif
    return attrs;
}

wxColour wxWindowBase::GetBackgroundColour() const
{
    if ( !m_backgroundColour.IsOk() )
    {
        wxASSERT_MSG( !m_hasBgCol, wxT("we have invalid explicit bg colour?") );

        // get our default background colour
        wxColour colBg = GetDefaultAttributes().colBg;

        // we must return some valid colour to avoid redoing this every time
        // and also to avoid surprising the applications written for older
        // wxWidgets versions where GetBackgroundColour() always returned
        // something -- so give them something even if it doesn't make sense
        // for this window (e.g. it has a themed background)
        if ( !colBg.IsOk() )
            colBg = GetClassDefaultAttributes().colBg;

        return colBg;
    }
    else
        return m_backgroundColour;
}

wxColour wxWindowBase::GetForegroundColour() const
{
    // logic is the same as above
    if ( !m_hasFgCol && !m_foregroundColour.IsOk() )
    {
        wxColour colFg = GetDefaultAttributes().colFg;

        if ( !colFg.IsOk() )
            colFg = GetClassDefaultAttributes().colFg;

        return colFg;
    }
    else
        return m_foregroundColour;
}

bool wxWindowBase::SetBackgroundStyle(wxBackgroundStyle style)
{
    // The checks below shouldn't be triggered if we're not really changing the
    // style.
    if ( style == m_backgroundStyle )
        return true;

    // Transparent background style can be only set before creation because of
    // wxGTK limitation.
    wxCHECK_MSG( (style != wxBG_STYLE_TRANSPARENT) || !GetHandle(),
                 false,
                 "wxBG_STYLE_TRANSPARENT style can only be set before "
                 "Create()-ing the window." );

    // And once it is set, wxBG_STYLE_TRANSPARENT can't be unset.
    wxCHECK_MSG( (m_backgroundStyle != wxBG_STYLE_TRANSPARENT) ||
                 (style == wxBG_STYLE_TRANSPARENT),
                 false,
                 "wxBG_STYLE_TRANSPARENT can't be unset once it was set." );

    m_backgroundStyle = style;

    return true;
}

bool wxWindowBase::IsTransparentBackgroundSupported(wxString *reason) const
{
    if ( reason )
        *reason = _("This platform does not support background transparency.");

    return false;
}

bool wxWindowBase::SetBackgroundColour( const wxColour &colour )
{
    if ( colour == m_backgroundColour )
        return false;

    m_hasBgCol = colour.IsOk();

    m_inheritBgCol = m_hasBgCol;
    m_backgroundColour = colour;
    SetThemeEnabled( !m_hasBgCol && !m_foregroundColour.IsOk() );
    return true;
}

bool wxWindowBase::SetForegroundColour( const wxColour &colour )
{
    if (colour == m_foregroundColour )
        return false;

    m_hasFgCol = colour.IsOk();
    m_inheritFgCol = m_hasFgCol;
    m_foregroundColour = colour;
    SetThemeEnabled( !m_hasFgCol && !m_backgroundColour.IsOk() );
    return true;
}

bool wxWindowBase::SetCursor(const wxCursor& cursor)
{
    // setting an invalid cursor is ok, it means that we don't have any special
    // cursor
    if ( m_cursor.IsSameAs(cursor) )
    {
        // no change
        return false;
    }

    m_cursor = cursor;

    return true;
}

wxFont wxWindowBase::GetFont() const
{
    // logic is the same as in GetBackgroundColour()
    if ( !m_font.IsOk() )
    {
        wxASSERT_MSG( !m_hasFont, wxT("we have invalid explicit font?") );

        wxFont font = GetDefaultAttributes().font;
        if ( !font.IsOk() )
            font = GetClassDefaultAttributes().font;

        return font;
    }
    else
        return m_font;
}

bool wxWindowBase::SetFont(const wxFont& font)
{
    if ( font == m_font )
    {
        // no change
        return false;
    }

    m_font = font;
    m_hasFont = font.IsOk();
    m_inheritFont = m_hasFont;

    InvalidateBestSize();

    return true;
}

#if wxUSE_PALETTE

void wxWindowBase::SetPalette(const wxPalette& pal)
{
    m_hasCustomPalette = true;
    m_palette = pal;

    // VZ: can anyone explain me what do we do here?
    wxWindowDC d((wxWindow *) this);
    d.SetPalette(pal);
}

wxWindow *wxWindowBase::GetAncestorWithCustomPalette() const
{
    wxWindow *win = (wxWindow *)this;
    while ( win && !win->HasCustomPalette() )
    {
        win = win->GetParent();
    }

    return win;
}

#endif // wxUSE_PALETTE

#if wxUSE_CARET
void wxWindowBase::SetCaret(wxCaret *caret)
{
    if ( m_caret )
    {
        delete m_caret;
    }

    m_caret = caret;

    if ( m_caret )
    {
        wxASSERT_MSG( m_caret->GetWindow() == this,
                      wxT("caret should be created associated to this window") );
    }
}
#endif // wxUSE_CARET

#if wxUSE_VALIDATORS
// ----------------------------------------------------------------------------
// validators
// ----------------------------------------------------------------------------

void wxWindowBase::SetValidator(const wxValidator& validator)
{
    if ( m_windowValidator )
        delete m_windowValidator;

    m_windowValidator = (wxValidator *)validator.Clone();

    if ( m_windowValidator )
        m_windowValidator->SetWindow(this);
}
#endif // wxUSE_VALIDATORS

// ----------------------------------------------------------------------------
// update region stuff
// ----------------------------------------------------------------------------

wxRect wxWindowBase::GetUpdateClientRect() const
{
    wxRegion rgnUpdate = GetUpdateRegion();
    rgnUpdate.Intersect(GetClientRect());
    wxRect rectUpdate = rgnUpdate.GetBox();
    wxPoint ptOrigin = GetClientAreaOrigin();
    rectUpdate.x -= ptOrigin.x;
    rectUpdate.y -= ptOrigin.y;

    return rectUpdate;
}

bool wxWindowBase::DoIsExposed(int x, int y) const
{
    return m_updateRegion.Contains(x, y) != wxOutRegion;
}

bool wxWindowBase::DoIsExposed(int x, int y, int w, int h) const
{
    return m_updateRegion.Contains(x, y, w, h) != wxOutRegion;
}

void wxWindowBase::ClearBackground()
{
    // wxGTK uses its own version, no need to add never used code
#ifndef __WXGTK__
    wxClientDC dc((wxWindow *)this);
    wxBrush brush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID);
    dc.SetBackground(brush);
    dc.Clear();
#endif // __WXGTK__
}

// ----------------------------------------------------------------------------
// find child window by id or name
// ----------------------------------------------------------------------------

wxWindow *wxWindowBase::FindWindow(long id) const
{
    if ( id == m_windowId )
        return (wxWindow *)this;

    wxWindowBase *res = NULL;
    wxWindowList::compatibility_iterator node;
    for ( node = m_children.GetFirst(); node && !res; node = node->GetNext() )
    {
        wxWindowBase *child = node->GetData();

        // As usual, don't recurse into child dialogs, finding a button in a
        // child dialog when looking in this window would be unexpected.
        if ( child->IsTopLevel() )
            continue;

        res = child->FindWindow( id );
    }

    return (wxWindow *)res;
}

wxWindow *wxWindowBase::FindWindow(const wxString& name) const
{
    if ( name == m_windowName )
        return (wxWindow *)this;

    wxWindowBase *res = NULL;
    wxWindowList::compatibility_iterator node;
    for ( node = m_children.GetFirst(); node && !res; node = node->GetNext() )
    {
        wxWindow *child = node->GetData();

        // As in FindWindow() overload above, never recurse into child dialogs.
        if ( child->IsTopLevel() )
            continue;

        res = child->FindWindow(name);
    }

    return (wxWindow *)res;
}


// find any window by id or name or label: If parent is non-NULL, look through
// children for a label or title matching the specified string. If NULL, look
// through all top-level windows.
//
// to avoid duplicating code we reuse the same helper function but with
// different comparators

typedef bool (*wxFindWindowCmp)(const wxWindow *win,
                                const wxString& label, long id);

static
bool wxFindWindowCmpLabels(const wxWindow *win, const wxString& label,
                           long WXUNUSED(id))
{
    return win->GetLabel() == label;
}

static
bool wxFindWindowCmpNames(const wxWindow *win, const wxString& label,
                          long WXUNUSED(id))
{
    return win->GetName() == label;
}

static
bool wxFindWindowCmpIds(const wxWindow *win, const wxString& WXUNUSED(label),
                        long id)
{
    return win->GetId() == id;
}

// recursive helper for the FindWindowByXXX() functions
static
wxWindow *wxFindWindowRecursively(const wxWindow *parent,
                                  const wxString& label,
                                  long id,
                                  wxFindWindowCmp cmp)
{
    if ( parent )
    {
        // see if this is the one we're looking for
        if ( (*cmp)(parent, label, id) )
            return (wxWindow *)parent;

        // It wasn't, so check all its children
        for ( wxWindowList::compatibility_iterator node = parent->GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            // recursively check each child
            wxWindow *win = (wxWindow *)node->GetData();
            wxWindow *retwin = wxFindWindowRecursively(win, label, id, cmp);
            if (retwin)
                return retwin;
        }
    }

    // Not found
    return NULL;
}

// helper for FindWindowByXXX()
static
wxWindow *wxFindWindowHelper(const wxWindow *parent,
                             const wxString& label,
                             long id,
                             wxFindWindowCmp cmp)
{
    if ( parent )
    {
        // just check parent and all its children
        return wxFindWindowRecursively(parent, label, id, cmp);
    }

    // start at very top of wx's windows
    for ( wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
          node;
          node = node->GetNext() )
    {
        // recursively check each window & its children
        wxWindow *win = node->GetData();
        wxWindow *retwin = wxFindWindowRecursively(win, label, id, cmp);
        if (retwin)
            return retwin;
    }

    return NULL;
}

/* static */
wxWindow *
wxWindowBase::FindWindowByLabel(const wxString& title, const wxWindow *parent)
{
    return wxFindWindowHelper(parent, title, 0, wxFindWindowCmpLabels);
}

/* static */
wxWindow *
wxWindowBase::FindWindowByName(const wxString& title, const wxWindow *parent)
{
    wxWindow *win = wxFindWindowHelper(parent, title, 0, wxFindWindowCmpNames);

    if ( !win )
    {
        // fall back to the label
        win = FindWindowByLabel(title, parent);
    }

    return win;
}

/* static */
wxWindow *
wxWindowBase::FindWindowById( long id, const wxWindow* parent )
{
    return wxFindWindowHelper(parent, wxEmptyString, id, wxFindWindowCmpIds);
}

// ----------------------------------------------------------------------------
// dialog oriented functions
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_8
void wxWindowBase::MakeModal(bool modal)
{
    // Disable all other windows
    if ( IsTopLevel() )
    {
        wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
        while (node)
        {
            wxWindow *win = node->GetData();
            if (win != this)
                win->Enable(!modal);

            node = node->GetNext();
        }
    }
}
#endif // WXWIN_COMPATIBILITY_2_8

#if wxUSE_VALIDATORS

namespace
{

// This class encapsulates possibly recursive iteration on window children done
// by Validate() and TransferData{To,From}Window() and allows to avoid code
// duplication in all three functions.
class ValidationTraverserBase
{
public:
    wxEXPLICIT ValidationTraverserBase(wxWindowBase* win)
        : m_win(static_cast<wxWindow*>(win))
    {
    }

    // Traverse all the direct children calling OnDo() on them and also all
    // grandchildren if wxWS_EX_VALIDATE_RECURSIVELY is used, calling
    // OnRecurse() for them.
    bool DoForAllChildren()
    {
        const bool recurse = m_win->HasExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);

        wxWindowList& children = m_win->GetChildren();
        for ( wxWindowList::iterator i = children.begin();
              i != children.end();
              ++i )
        {
            wxWindow* const child = static_cast<wxWindow*>(*i);
            wxValidator* const validator = child->GetValidator();
            if ( validator && !OnDo(validator) )
            {
                return false;
            }

            // Notice that validation should never recurse into top level
            // children, e.g. some other dialog which might happen to be
            // currently shown.
            if ( recurse && !child->IsTopLevel() && !OnRecurse(child) )
            {
                return false;
            }
        }

        return true;
    }

    // Give it a virtual dtor just to suppress gcc warnings about a class with
    // virtual methods but non-virtual dtor -- even if this is completely safe
    // here as we never use the objects of this class polymorphically.
    virtual ~ValidationTraverserBase() { }

protected:
    // Called for each child, validator is guaranteed to be non-NULL.
    virtual bool OnDo(wxValidator* validator) = 0;

    // Called for each child if we need to recurse into its children.
    virtual bool OnRecurse(wxWindow* child) = 0;


    // The window whose children we're traversing.
    wxWindow* const m_win;

    wxDECLARE_NO_COPY_CLASS(ValidationTraverserBase);
};

} // anonymous namespace

#endif // wxUSE_VALIDATORS

bool wxWindowBase::Validate()
{
#if wxUSE_VALIDATORS
    class ValidateTraverser : public ValidationTraverserBase
    {
    public:
        wxEXPLICIT ValidateTraverser(wxWindowBase* win)
            : ValidationTraverserBase(win)
        {
        }

        virtual bool OnDo(wxValidator* validator)
        {
            return validator->Validate(m_win);
        }

        virtual bool OnRecurse(wxWindow* child)
        {
            return child->Validate();
        }
    };

    return ValidateTraverser(this).DoForAllChildren();
#else // !wxUSE_VALIDATORS
    return true;
#endif // wxUSE_VALIDATORS/!wxUSE_VALIDATORS
}

bool wxWindowBase::TransferDataToWindow()
{
#if wxUSE_VALIDATORS
    class DataToWindowTraverser : public ValidationTraverserBase
    {
    public:
        wxEXPLICIT DataToWindowTraverser(wxWindowBase* win)
            : ValidationTraverserBase(win)
        {
        }

        virtual bool OnDo(wxValidator* validator)
        {
            if ( !validator->TransferToWindow() )
            {
                wxLogWarning(_("Could not transfer data to window"));
#if wxUSE_LOG
                wxLog::FlushActive();
#endif // wxUSE_LOG

                return false;
            }

            return true;
        }

        virtual bool OnRecurse(wxWindow* child)
        {
            return child->TransferDataToWindow();
        }
    };

    return DataToWindowTraverser(this).DoForAllChildren();
#else // !wxUSE_VALIDATORS
    return true;
#endif // wxUSE_VALIDATORS/!wxUSE_VALIDATORS
}

bool wxWindowBase::TransferDataFromWindow()
{
#if wxUSE_VALIDATORS
    class DataFromWindowTraverser : public ValidationTraverserBase
    {
    public:
        DataFromWindowTraverser(wxWindowBase* win)
            : ValidationTraverserBase(win)
        {
        }

        virtual bool OnDo(wxValidator* validator)
        {
            return validator->TransferFromWindow();
        }

        virtual bool OnRecurse(wxWindow* child)
        {
            return child->TransferDataFromWindow();
        }
    };

    return DataFromWindowTraverser(this).DoForAllChildren();
#else // !wxUSE_VALIDATORS
    return true;
#endif // wxUSE_VALIDATORS/!wxUSE_VALIDATORS
}

void wxWindowBase::InitDialog()
{
    wxInitDialogEvent event(GetId());
    event.SetEventObject( this );
    GetEventHandler()->ProcessEvent(event);
}

// ----------------------------------------------------------------------------
// context-sensitive help support
// ----------------------------------------------------------------------------

#if wxUSE_HELP

// associate this help text with this window
void wxWindowBase::SetHelpText(const wxString& text)
{
    wxHelpProvider *helpProvider = wxHelpProvider::Get();
    if ( helpProvider )
    {
        helpProvider->AddHelp(this, text);
    }
}

#if WXWIN_COMPATIBILITY_2_8
// associate this help text with all windows with the same id as this
// one
void wxWindowBase::SetHelpTextForId(const wxString& text)
{
    wxHelpProvider *helpProvider = wxHelpProvider::Get();
    if ( helpProvider )
    {
        helpProvider->AddHelp(GetId(), text);
    }
}
#endif // WXWIN_COMPATIBILITY_2_8

// get the help string associated with this window (may be empty)
// default implementation forwards calls to the help provider
wxString
wxWindowBase::GetHelpTextAtPoint(const wxPoint & WXUNUSED(pt),
                                 wxHelpEvent::Origin WXUNUSED(origin)) const
{
    wxString text;
    wxHelpProvider *helpProvider = wxHelpProvider::Get();
    if ( helpProvider )
    {
        text = helpProvider->GetHelp(this);
    }

    return text;
}

// show help for this window
void wxWindowBase::OnHelp(wxHelpEvent& event)
{
    wxHelpProvider *helpProvider = wxHelpProvider::Get();
    if ( helpProvider )
    {
        wxPoint pos = event.GetPosition();
        const wxHelpEvent::Origin origin = event.GetOrigin();
        if ( origin == wxHelpEvent::Origin_Keyboard )
        {
            // if the help event was generated from keyboard it shouldn't
            // appear at the mouse position (which is still the only position
            // associated with help event) if the mouse is far away, although
            // we still do use the mouse position if it's over the window
            // because we suppose the user looks approximately at the mouse
            // already and so it would be more convenient than showing tooltip
            // at some arbitrary position which can be quite far from it
            const wxRect rectClient = GetClientRect();
            if ( !rectClient.Contains(ScreenToClient(pos)) )
            {
                // position help slightly under and to the right of this window
                pos = ClientToScreen(wxPoint(
                        2*GetCharWidth(),
                        rectClient.height + GetCharHeight()
                      ));
            }
        }

        if ( helpProvider->ShowHelpAtPoint(this, pos, origin) )
        {
            // skip the event.Skip() below
            return;
        }
    }

    event.Skip();
}

#endif // wxUSE_HELP

// ----------------------------------------------------------------------------
// tooltips
// ----------------------------------------------------------------------------

#if wxUSE_TOOLTIPS

wxString wxWindowBase::GetToolTipText() const
{
    return m_tooltip ? m_tooltip->GetTip() : wxString();
}

void wxWindowBase::SetToolTip( const wxString &tip )
{
    // don't create the new tooltip if we already have one
    if ( m_tooltip )
    {
        m_tooltip->SetTip( tip );
    }
    else
    {
        SetToolTip( new wxToolTip( tip ) );
    }

    // setting empty tooltip text does not remove the tooltip any more - use
    // SetToolTip(NULL) for this
}

void wxWindowBase::DoSetToolTip(wxToolTip *tooltip)
{
    if ( m_tooltip != tooltip )
    {
        if ( m_tooltip )
            delete m_tooltip;

        m_tooltip = tooltip;
    }
}

bool wxWindowBase::CopyToolTip(wxToolTip *tip)
{
    SetToolTip(tip ? new wxToolTip(tip->GetTip()) : NULL);

    return tip != NULL;
}

#endif // wxUSE_TOOLTIPS

// ----------------------------------------------------------------------------
// constraints and sizers
// ----------------------------------------------------------------------------

#if wxUSE_CONSTRAINTS

void wxWindowBase::SetConstraints( wxLayoutConstraints *constraints )
{
    if ( m_constraints )
    {
        UnsetConstraints(m_constraints);
        delete m_constraints;
    }
    m_constraints = constraints;
    if ( m_constraints )
    {
        // Make sure other windows know they're part of a 'meaningful relationship'
        if ( m_constraints->left.GetOtherWindow() && (m_constraints->left.GetOtherWindow() != this) )
            m_constraints->left.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->top.GetOtherWindow() && (m_constraints->top.GetOtherWindow() != this) )
            m_constraints->top.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->right.GetOtherWindow() && (m_constraints->right.GetOtherWindow() != this) )
            m_constraints->right.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->bottom.GetOtherWindow() && (m_constraints->bottom.GetOtherWindow() != this) )
            m_constraints->bottom.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->width.GetOtherWindow() && (m_constraints->width.GetOtherWindow() != this) )
            m_constraints->width.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->height.GetOtherWindow() && (m_constraints->height.GetOtherWindow() != this) )
            m_constraints->height.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->centreX.GetOtherWindow() && (m_constraints->centreX.GetOtherWindow() != this) )
            m_constraints->centreX.GetOtherWindow()->AddConstraintReference(this);
        if ( m_constraints->centreY.GetOtherWindow() && (m_constraints->centreY.GetOtherWindow() != this) )
            m_constraints->centreY.GetOtherWindow()->AddConstraintReference(this);
    }
}

// This removes any dangling pointers to this window in other windows'
// constraintsInvolvedIn lists.
void wxWindowBase::UnsetConstraints(wxLayoutConstraints *c)
{
    if ( c )
    {
        if ( c->left.GetOtherWindow() && (c->left.GetOtherWindow() != this) )
            c->left.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->top.GetOtherWindow() && (c->top.GetOtherWindow() != this) )
            c->top.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->right.GetOtherWindow() && (c->right.GetOtherWindow() != this) )
            c->right.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->bottom.GetOtherWindow() && (c->bottom.GetOtherWindow() != this) )
            c->bottom.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->width.GetOtherWindow() && (c->width.GetOtherWindow() != this) )
            c->width.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->height.GetOtherWindow() && (c->height.GetOtherWindow() != this) )
            c->height.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->centreX.GetOtherWindow() && (c->centreX.GetOtherWindow() != this) )
            c->centreX.GetOtherWindow()->RemoveConstraintReference(this);
        if ( c->centreY.GetOtherWindow() && (c->centreY.GetOtherWindow() != this) )
            c->centreY.GetOtherWindow()->RemoveConstraintReference(this);
    }
}

// Back-pointer to other windows we're involved with, so if we delete this
// window, we must delete any constraints we're involved with.
void wxWindowBase::AddConstraintReference(wxWindowBase *otherWin)
{
    if ( !m_constraintsInvolvedIn )
        m_constraintsInvolvedIn = new wxWindowList;
    if ( !m_constraintsInvolvedIn->Find((wxWindow *)otherWin) )
        m_constraintsInvolvedIn->Append((wxWindow *)otherWin);
}

// REMOVE back-pointer to other windows we're involved with.
void wxWindowBase::RemoveConstraintReference(wxWindowBase *otherWin)
{
    if ( m_constraintsInvolvedIn )
        m_constraintsInvolvedIn->DeleteObject((wxWindow *)otherWin);
}

// Reset any constraints that mention this window
void wxWindowBase::DeleteRelatedConstraints()
{
    if ( m_constraintsInvolvedIn )
    {
        wxWindowList::compatibility_iterator node = m_constraintsInvolvedIn->GetFirst();
        while (node)
        {
            wxWindow *win = node->GetData();
            wxLayoutConstraints *constr = win->GetConstraints();

            // Reset any constraints involving this window
            if ( constr )
            {
                constr->left.ResetIfWin(this);
                constr->top.ResetIfWin(this);
                constr->right.ResetIfWin(this);
                constr->bottom.ResetIfWin(this);
                constr->width.ResetIfWin(this);
                constr->height.ResetIfWin(this);
                constr->centreX.ResetIfWin(this);
                constr->centreY.ResetIfWin(this);
            }

            wxWindowList::compatibility_iterator next = node->GetNext();
            m_constraintsInvolvedIn->Erase(node);
            node = next;
        }

        wxDELETE(m_constraintsInvolvedIn);
    }
}

#endif // wxUSE_CONSTRAINTS

void wxWindowBase::SetSizer(wxSizer *sizer, bool deleteOld)
{
    if ( sizer == m_windowSizer)
        return;

    if ( m_windowSizer )
    {
        m_windowSizer->SetContainingWindow(NULL);

        if ( deleteOld )
            delete m_windowSizer;
    }

    m_windowSizer = sizer;
    if ( m_windowSizer )
    {
        m_windowSizer->SetContainingWindow((wxWindow *)this);
    }

    SetAutoLayout(m_windowSizer != NULL);
}

void wxWindowBase::SetSizerAndFit(wxSizer *sizer, bool deleteOld)
{
    SetSizer( sizer, deleteOld );
    sizer->SetSizeHints( (wxWindow*) this );
}


void wxWindowBase::SetContainingSizer(wxSizer* sizer)
{
    // adding a window to a sizer twice is going to result in fatal and
    // hard to debug problems later because when deleting the second
    // associated wxSizerItem we're going to dereference a dangling
    // pointer; so try to detect this as early as possible
    wxASSERT_MSG( !sizer || m_containingSizer != sizer,
                  wxT("Adding a window to the same sizer twice?") );

    m_containingSizer = sizer;
}

#if wxUSE_CONSTRAINTS

void wxWindowBase::SatisfyConstraints()
{
    wxLayoutConstraints *constr = GetConstraints();
    bool wasOk = constr && constr->AreSatisfied();

    ResetConstraints();   // Mark all constraints as unevaluated

    int noChanges = 1;

    // if we're a top level panel (i.e. our parent is frame/dialog), our
    // own constraints will never be satisfied any more unless we do it
    // here
    if ( wasOk )
    {
        while ( noChanges > 0 )
        {
            LayoutPhase1(&noChanges);
        }
    }

    LayoutPhase2(&noChanges);
}

#endif // wxUSE_CONSTRAINTS

bool wxWindowBase::Layout()
{
    // If there is a sizer, use it instead of the constraints
    if ( GetSizer() )
    {
        int w = 0, h = 0;
        GetVirtualSize(&w, &h);
        GetSizer()->SetDimension( 0, 0, w, h );
    }
#if wxUSE_CONSTRAINTS
    else
    {
        SatisfyConstraints(); // Find the right constraints values
        SetConstraintSizes(); // Recursively set the real window sizes
    }
#endif

    return true;
}

void wxWindowBase::InternalOnSize(wxSizeEvent& event)
{
    if ( GetAutoLayout() )
        Layout();

    event.Skip();
}

#if wxUSE_CONSTRAINTS

// first phase of the constraints evaluation: set our own constraints
bool wxWindowBase::LayoutPhase1(int *noChanges)
{
    wxLayoutConstraints *constr = GetConstraints();

    return !constr || constr->SatisfyConstraints(this, noChanges);
}

// second phase: set the constraints for our children
bool wxWindowBase::LayoutPhase2(int *noChanges)
{
    *noChanges = 0;

    // Layout children
    DoPhase(1);

    // Layout grand children
    DoPhase(2);

    return true;
}

// Do a phase of evaluating child constraints
bool wxWindowBase::DoPhase(int phase)
{
    // the list containing the children for which the constraints are already
    // set correctly
    wxWindowList succeeded;

    // the max number of iterations we loop before concluding that we can't set
    // the constraints
    static const int maxIterations = 500;

    for ( int noIterations = 0; noIterations < maxIterations; noIterations++ )
    {
        int noChanges = 0;

        // loop over all children setting their constraints
        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxWindow *child = node->GetData();
            if ( child->IsTopLevel() )
            {
                // top level children are not inside our client area
                continue;
            }

            if ( !child->GetConstraints() || succeeded.Find(child) )
            {
                // this one is either already ok or nothing we can do about it
                continue;
            }

            int tempNoChanges = 0;
            bool success = phase == 1 ? child->LayoutPhase1(&tempNoChanges)
                                      : child->LayoutPhase2(&tempNoChanges);
            noChanges += tempNoChanges;

            if ( success )
            {
                succeeded.Append(child);
            }
        }

        if ( !noChanges )
        {
            // constraints are set
            break;
        }
    }

    return true;
}

void wxWindowBase::ResetConstraints()
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr )
    {
        constr->left.SetDone(false);
        constr->top.SetDone(false);
        constr->right.SetDone(false);
        constr->bottom.SetDone(false);
        constr->width.SetDone(false);
        constr->height.SetDone(false);
        constr->centreX.SetDone(false);
        constr->centreY.SetDone(false);
    }

    wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
    while (node)
    {
        wxWindow *win = node->GetData();
        if ( !win->IsTopLevel() )
            win->ResetConstraints();
        node = node->GetNext();
    }
}

// Need to distinguish between setting the 'fake' size for windows and sizers,
// and setting the real values.
void wxWindowBase::SetConstraintSizes(bool recurse)
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr && constr->AreSatisfied() )
    {
        ChildrenRepositioningGuard repositionGuard(this);

        int x = constr->left.GetValue();
        int y = constr->top.GetValue();
        int w = constr->width.GetValue();
        int h = constr->height.GetValue();

        if ( (constr->width.GetRelationship() != wxAsIs ) ||
             (constr->height.GetRelationship() != wxAsIs) )
        {
            // We really shouldn't set negative sizes for the windows so make
            // them at least of 1*1 size
            SetSize(x, y, w > 0 ? w : 1, h > 0 ? h : 1);
        }
        else
        {
            // If we don't want to resize this window, just move it...
            Move(x, y);
        }
    }
    else if ( constr )
    {
        wxLogDebug(wxT("Constraints not satisfied for %s named '%s'."),
                   GetClassInfo()->GetClassName(),
                   GetName().c_str());
    }

    if ( recurse )
    {
        wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
        while (node)
        {
            wxWindow *win = node->GetData();
            if ( !win->IsTopLevel() && win->GetConstraints() )
                win->SetConstraintSizes();
            node = node->GetNext();
        }
    }
}

// Only set the size/position of the constraint (if any)
void wxWindowBase::SetSizeConstraint(int x, int y, int w, int h)
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr )
    {
        if ( x != wxDefaultCoord )
        {
            constr->left.SetValue(x);
            constr->left.SetDone(true);
        }
        if ( y != wxDefaultCoord )
        {
            constr->top.SetValue(y);
            constr->top.SetDone(true);
        }
        if ( w != wxDefaultCoord )
        {
            constr->width.SetValue(w);
            constr->width.SetDone(true);
        }
        if ( h != wxDefaultCoord )
        {
            constr->height.SetValue(h);
            constr->height.SetDone(true);
        }
    }
}

void wxWindowBase::MoveConstraint(int x, int y)
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr )
    {
        if ( x != wxDefaultCoord )
        {
            constr->left.SetValue(x);
            constr->left.SetDone(true);
        }
        if ( y != wxDefaultCoord )
        {
            constr->top.SetValue(y);
            constr->top.SetDone(true);
        }
    }
}

void wxWindowBase::GetSizeConstraint(int *w, int *h) const
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr )
    {
        *w = constr->width.GetValue();
        *h = constr->height.GetValue();
    }
    else
        GetSize(w, h);
}

void wxWindowBase::GetClientSizeConstraint(int *w, int *h) const
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr )
    {
        *w = constr->width.GetValue();
        *h = constr->height.GetValue();
    }
    else
        GetClientSize(w, h);
}

void wxWindowBase::GetPositionConstraint(int *x, int *y) const
{
    wxLayoutConstraints *constr = GetConstraints();
    if ( constr )
    {
        *x = constr->left.GetValue();
        *y = constr->top.GetValue();
    }
    else
        GetPosition(x, y);
}

#endif // wxUSE_CONSTRAINTS

void wxWindowBase::AdjustForParentClientOrigin(int& x, int& y, int sizeFlags) const
{
    wxWindow *parent = GetParent();
    if ( !(sizeFlags & wxSIZE_NO_ADJUSTMENTS) && parent )
    {
        wxPoint pt(parent->GetClientAreaOrigin());
        x += pt.x;
        y += pt.y;
    }
}

// ----------------------------------------------------------------------------
// Update UI processing
// ----------------------------------------------------------------------------

void wxWindowBase::UpdateWindowUI(long flags)
{
    wxUpdateUIEvent event(GetId());
    event.SetEventObject(this);

    if ( GetEventHandler()->ProcessEvent(event) )
    {
        DoUpdateWindowUI(event);
    }

    if (flags & wxUPDATE_UI_RECURSE)
    {
        wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
        while (node)
        {
            wxWindow* child = (wxWindow*) node->GetData();
            child->UpdateWindowUI(flags);
            node = node->GetNext();
        }
    }
}

// do the window-specific processing after processing the update event
void wxWindowBase::DoUpdateWindowUI(wxUpdateUIEvent& event)
{
    if ( event.GetSetEnabled() )
        Enable(event.GetEnabled());

    if ( event.GetSetShown() )
        Show(event.GetShown());
}

// ----------------------------------------------------------------------------
// Idle processing
// ----------------------------------------------------------------------------

// Send idle event to window and all subwindows
bool wxWindowBase::SendIdleEvents(wxIdleEvent& event)
{
    bool needMore = false;

    OnInternalIdle();

    // should we send idle event to this window?
    if (wxIdleEvent::GetMode() == wxIDLE_PROCESS_ALL ||
        HasExtraStyle(wxWS_EX_PROCESS_IDLE))
    {
        event.SetEventObject(this);
        HandleWindowEvent(event);

        if (event.MoreRequested())
            needMore = true;
    }
    wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
    for (; node; node = node->GetNext())
    {
        wxWindow* child = node->GetData();
        if (child->SendIdleEvents(event))
            needMore = true;
    }

    return needMore;
}

void wxWindowBase::OnInternalIdle()
{
    if ( wxUpdateUIEvent::CanUpdate(this) )
        UpdateWindowUI(wxUPDATE_UI_FROMIDLE);
}

// ----------------------------------------------------------------------------
// dialog units translations
// ----------------------------------------------------------------------------

// Windows' computes dialog units using average character width over upper-
// and lower-case ASCII alphabet and not using the average character width
// metadata stored in the font; see
// http://support.microsoft.com/default.aspx/kb/145994 for detailed discussion.
// It's important that we perform the conversion in identical way, because
// dialog units natively exist only on Windows and Windows HIG is expressed
// using them.
wxSize wxWindowBase::GetDlgUnitBase() const
{
    const wxWindowBase * const parent = wxGetTopLevelParent((wxWindow*)this);

    wxCHECK_MSG( parent, wxDefaultSize, wxS("Must have TLW parent") );

    if ( !parent->m_font.IsOk() )
    {
        // Default GUI font is used. This is the most common case, so
        // cache the results.
        static wxSize s_defFontSize;
        if ( s_defFontSize.x == 0 )
            s_defFontSize = wxPrivate::GetAverageASCIILetterSize(*parent);
        return s_defFontSize;
    }
    else
    {
        // Custom font, we always need to compute the result
        return wxPrivate::GetAverageASCIILetterSize(*parent);
    }
}

wxPoint wxWindowBase::ConvertPixelsToDialog(const wxPoint& pt) const
{
    const wxSize base = GetDlgUnitBase();

    // NB: wxMulDivInt32() is used, because it correctly rounds the result

    wxPoint pt2 = wxDefaultPosition;
    if (pt.x != wxDefaultCoord)
        pt2.x = wxMulDivInt32(pt.x, 4, base.x);
    if (pt.y != wxDefaultCoord)
        pt2.y = wxMulDivInt32(pt.y, 8, base.y);

    return pt2;
}

wxPoint wxWindowBase::ConvertDialogToPixels(const wxPoint& pt) const
{
    const wxSize base = GetDlgUnitBase();

    wxPoint pt2 = wxDefaultPosition;
    if (pt.x != wxDefaultCoord)
        pt2.x = wxMulDivInt32(pt.x, base.x, 4);
    if (pt.y != wxDefaultCoord)
        pt2.y = wxMulDivInt32(pt.y, base.y, 8);

    return pt2;
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------

// propagate the colour change event to the subwindows
void wxWindowBase::OnSysColourChanged(wxSysColourChangedEvent& WXUNUSED(event))
{
    wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
    while ( node )
    {
        // Only propagate to non-top-level windows
        wxWindow *win = node->GetData();
        if ( !win->IsTopLevel() )
        {
            wxSysColourChangedEvent event2;
            event2.SetEventObject(win);
            win->GetEventHandler()->ProcessEvent(event2);
        }

        node = node->GetNext();
    }

    Refresh();
}

// the default action is to populate dialog with data when it's created,
// and nudge the UI into displaying itself correctly in case
// we've turned the wxUpdateUIEvents frequency down low.
void wxWindowBase::OnInitDialog( wxInitDialogEvent &WXUNUSED(event) )
{
    TransferDataToWindow();

    // Update the UI at this point
    UpdateWindowUI(wxUPDATE_UI_RECURSE);
}

// ----------------------------------------------------------------------------
// menu-related functions
// ----------------------------------------------------------------------------

#if wxUSE_MENUS

bool wxWindowBase::PopupMenu(wxMenu *menu, int x, int y)
{
    wxCHECK_MSG( menu, false, "can't popup NULL menu" );

    wxMenuInvokingWindowSetter
        setInvokingWin(*menu, static_cast<wxWindow *>(this));

    wxCurrentPopupMenu = menu;
    const bool rc = DoPopupMenu(menu, x, y);
    wxCurrentPopupMenu = NULL;

    return rc;
}

// this is used to pass the id of the selected item from the menu event handler
// to the main function itself
//
// it's ok to use a global here as there can be at most one popup menu shown at
// any time
static int gs_popupMenuSelection = wxID_NONE;

void wxWindowBase::InternalOnPopupMenu(wxCommandEvent& event)
{
    // store the id in a global variable where we'll retrieve it from later
    gs_popupMenuSelection = event.GetId();
}

void wxWindowBase::InternalOnPopupMenuUpdate(wxUpdateUIEvent& WXUNUSED(event))
{
    // nothing to do but do not skip it
}

int
wxWindowBase::DoGetPopupMenuSelectionFromUser(wxMenu& menu, int x, int y)
{
    gs_popupMenuSelection = wxID_NONE;

    Connect(wxEVT_MENU,
            wxCommandEventHandler(wxWindowBase::InternalOnPopupMenu),
            NULL,
            this);

    // it is common to construct the menu passed to this function dynamically
    // using some fixed range of ids which could clash with the ids used
    // elsewhere in the program, which could result in some menu items being
    // unintentionally disabled or otherwise modified by update UI handlers
    // elsewhere in the program code and this is difficult to avoid in the
    // program itself, so instead we just temporarily suspend UI updating while
    // this menu is shown
    Connect(wxEVT_UPDATE_UI,
            wxUpdateUIEventHandler(wxWindowBase::InternalOnPopupMenuUpdate),
            NULL,
            this);

    PopupMenu(&menu, x, y);

    Disconnect(wxEVT_UPDATE_UI,
               wxUpdateUIEventHandler(wxWindowBase::InternalOnPopupMenuUpdate),
               NULL,
               this);
    Disconnect(wxEVT_MENU,
               wxCommandEventHandler(wxWindowBase::InternalOnPopupMenu),
               NULL,
               this);

    return gs_popupMenuSelection;
}

#endif // wxUSE_MENUS

// methods for drawing the sizers in a visible way: this is currently only
// enabled for "full debug" builds with wxDEBUG_LEVEL==2 as it doesn't work
// that well and also because we don't want to leave it enabled in default
// builds used for production
#if wxDEBUG_LEVEL > 1

static void DrawSizers(wxWindowBase *win);

static void DrawBorder(wxWindowBase *win, const wxRect& rect, bool fill, const wxPen* pen)
{
    wxClientDC dc((wxWindow *)win);
    dc.SetPen(*pen);
    dc.SetBrush(fill ? wxBrush(pen->GetColour(), wxBRUSHSTYLE_CROSSDIAG_HATCH) :
                       *wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(rect.Deflate(1, 1));
}

static void DrawSizer(wxWindowBase *win, wxSizer *sizer)
{
    const wxSizerItemList& items = sizer->GetChildren();
    for ( wxSizerItemList::const_iterator i = items.begin(),
                                        end = items.end();
          i != end;
          ++i )
    {
        wxSizerItem *item = *i;
        if ( item->IsSizer() )
        {
            DrawBorder(win, item->GetRect().Deflate(2), false, wxRED_PEN);
            DrawSizer(win, item->GetSizer());
        }
        else if ( item->IsSpacer() )
        {
            DrawBorder(win, item->GetRect().Deflate(2), true, wxBLUE_PEN);
        }
        else if ( item->IsWindow() )
        {
            DrawSizers(item->GetWindow());
        }
        else
            wxFAIL_MSG("inconsistent wxSizerItem status!");
    }
}

static void DrawSizers(wxWindowBase *win)
{
    DrawBorder(win, win->GetClientSize(), false, wxGREEN_PEN);

    wxSizer *sizer = win->GetSizer();
    if ( sizer )
    {
        DrawSizer(win, sizer);
    }
    else // no sizer, still recurse into the children
    {
        const wxWindowList& children = win->GetChildren();
        for ( wxWindowList::const_iterator i = children.begin(),
                                         end = children.end();
              i != end;
              ++i )
        {
            DrawSizers(*i);
        }

        // show all kind of sizes of this window; see the "window sizing" topic
        // overview for more info about the various differences:
        wxSize fullSz = win->GetSize();
        wxSize clientSz = win->GetClientSize();
        wxSize bestSz = win->GetBestSize();
        wxSize minSz = win->GetMinSize();
        wxSize maxSz = win->GetMaxSize();
        wxSize virtualSz = win->GetVirtualSize();

        wxMessageOutputDebug dbgout;
        dbgout.Printf(
            "%-10s => fullsz=%4d;%-4d  clientsz=%4d;%-4d  bestsz=%4d;%-4d  minsz=%4d;%-4d  maxsz=%4d;%-4d virtualsz=%4d;%-4d\n",
            win->GetName(),
            fullSz.x, fullSz.y,
            clientSz.x, clientSz.y,
            bestSz.x, bestSz.y,
            minSz.x, minSz.y,
            maxSz.x, maxSz.y,
            virtualSz.x, virtualSz.y);
    }
}

#endif // wxDEBUG_LEVEL

// process special middle clicks
void wxWindowBase::OnMiddleClick( wxMouseEvent& event )
{
    if ( event.ControlDown() && event.AltDown() )
    {
#if wxDEBUG_LEVEL > 1
        // Ctrl-Alt-Shift-mclick makes the sizers visible in debug builds
        if ( event.ShiftDown() )
        {
            DrawSizers(this);
        }
        else
#endif // __WXDEBUG__
        {
#if wxUSE_MSGDLG
            // just Ctrl-Alt-middle click shows information about wx version
            ::wxInfoMessageBox((wxWindow*)this);
#endif // wxUSE_MSGDLG
        }
    }
    else
    {
        event.Skip();
    }
}

// ----------------------------------------------------------------------------
// accessibility
// ----------------------------------------------------------------------------

#if wxUSE_ACCESSIBILITY
void wxWindowBase::SetAccessible(wxAccessible* accessible)
{
    if (m_accessible && (accessible != m_accessible))
        delete m_accessible;
    m_accessible = accessible;
    if (m_accessible)
        m_accessible->SetWindow((wxWindow*) this);
}

// Returns the accessible object, creating if necessary.
wxAccessible* wxWindowBase::GetOrCreateAccessible()
{
    if (!m_accessible)
        m_accessible = CreateAccessible();
    return m_accessible;
}

// Override to create a specific accessible object.
wxAccessible* wxWindowBase::CreateAccessible()
{
    return new wxWindowAccessible((wxWindow*) this);
}

#endif

// ----------------------------------------------------------------------------
// list classes implementation
// ----------------------------------------------------------------------------

#if wxUSE_STD_CONTAINERS

#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxWindowList)

#else // !wxUSE_STD_CONTAINERS

void wxWindowListNode::DeleteData()
{
    delete (wxWindow *)GetData();
}

#endif // wxUSE_STD_CONTAINERS/!wxUSE_STD_CONTAINERS

// ----------------------------------------------------------------------------
// borders
// ----------------------------------------------------------------------------

wxBorder wxWindowBase::GetBorder(long flags) const
{
    wxBorder border = (wxBorder)(flags & wxBORDER_MASK);
    if ( border == wxBORDER_DEFAULT )
    {
        border = GetDefaultBorder();
    }
    else if ( border == wxBORDER_THEME )
    {
        border = GetDefaultBorderForControl();
    }

    return border;
}

wxBorder wxWindowBase::GetDefaultBorder() const
{
    return wxBORDER_NONE;
}

// ----------------------------------------------------------------------------
// hit testing
// ----------------------------------------------------------------------------

wxHitTest wxWindowBase::DoHitTest(wxCoord x, wxCoord y) const
{
    // here we just check if the point is inside the window or not

    // check the top and left border first
    bool outside = x < 0 || y < 0;
    if ( !outside )
    {
        // check the right and bottom borders too
        wxSize size = GetSize();
        outside = x >= size.x || y >= size.y;
    }

    return outside ? wxHT_WINDOW_OUTSIDE : wxHT_WINDOW_INSIDE;
}

// ----------------------------------------------------------------------------
// mouse capture
// ----------------------------------------------------------------------------

// Private data used for mouse capture tracking.
namespace wxMouseCapture
{

// Stack of the windows which previously had the capture, the top most element
// is the window that has the mouse capture now.
//
// NB: We use wxVector and not wxStack to be able to examine all of the stack
//     elements for debug checks, but only the stack operations should be
//     performed with this vector.
wxVector<wxWindow*> stack;

// Flag preventing reentrancy in {Capture,Release}Mouse().
wxRecursionGuardFlag changing;

bool IsInCaptureStack(wxWindowBase* win)
{
    for ( wxVector<wxWindow*>::const_iterator it = stack.begin();
          it != stack.end();
          ++it )
    {
        if ( *it == win )
            return true;
    }

    return false;
}

} // wxMouseCapture

void wxWindowBase::CaptureMouse()
{
    wxLogTrace(wxT("mousecapture"), wxT("CaptureMouse(%p)"), static_cast<void*>(this));

    wxRecursionGuard guard(wxMouseCapture::changing);
    wxASSERT_MSG( !guard.IsInside(), wxT("recursive CaptureMouse call?") );

    wxASSERT_MSG( !wxMouseCapture::IsInCaptureStack(this),
                    "Recapturing the mouse in the same window?" );

    wxWindow *winOld = GetCapture();
    if ( winOld )
        ((wxWindowBase*) winOld)->DoReleaseMouse();

    DoCaptureMouse();

    wxMouseCapture::stack.push_back(static_cast<wxWindow*>(this));
}

void wxWindowBase::ReleaseMouse()
{
    wxLogTrace(wxT("mousecapture"), wxT("ReleaseMouse(%p)"), static_cast<void*>(this));

    wxRecursionGuard guard(wxMouseCapture::changing);
    wxASSERT_MSG( !guard.IsInside(), wxT("recursive ReleaseMouse call?") );

#if wxDEBUG_LEVEL
    wxWindow* const winCapture = GetCapture();
    if ( !winCapture )
    {
        wxFAIL_MSG
        (
          wxString::Format
          (
            "Releasing mouse in %p(%s) but it is not captured",
            this, GetClassInfo()->GetClassName()
          )
        );
    }
    else if ( winCapture != this )
    {
        wxFAIL_MSG
        (
          wxString::Format
          (
            "Releasing mouse in %p(%s) but it is captured by %p(%s)",
            this, GetClassInfo()->GetClassName(),
            winCapture, winCapture->GetClassInfo()->GetClassName()
          )
        );
    }
#endif // wxDEBUG_LEVEL

    DoReleaseMouse();

    wxCHECK_RET( !wxMouseCapture::stack.empty(),
                    "Releasing mouse capture but capture stack empty?" );
    wxCHECK_RET( wxMouseCapture::stack.back() == this,
                    "Window releasing mouse capture not top of capture stack?" );

    wxMouseCapture::stack.pop_back();

    // Restore the capture to the previous window, if any.
    if ( !wxMouseCapture::stack.empty() )
    {
        ((wxWindowBase*)wxMouseCapture::stack.back())->DoCaptureMouse();
    }

    wxLogTrace(wxT("mousecapture"),
        wxT("After ReleaseMouse() mouse is captured by %p"),
        static_cast<void*>(GetCapture()));
}

static void DoNotifyWindowAboutCaptureLost(wxWindow *win)
{
    wxMouseCaptureLostEvent event(win->GetId());
    event.SetEventObject(win);
    if ( !win->GetEventHandler()->ProcessEvent(event) )
    {
        // windows must handle this event, otherwise the app wouldn't behave
        // correctly if it loses capture unexpectedly; see the discussion here:
        // http://sourceforge.net/tracker/index.php?func=detail&aid=1153662&group_id=9863&atid=109863
        // http://article.gmane.org/gmane.comp.lib.wxwidgets.devel/82376
        wxFAIL_MSG( wxT("window that captured the mouse didn't process wxEVT_MOUSE_CAPTURE_LOST") );
    }
}

/* static */
void wxWindowBase::NotifyCaptureLost()
{
    // don't do anything if capture lost was expected, i.e. resulted from
    // a wx call to ReleaseMouse or CaptureMouse:
    wxRecursionGuard guard(wxMouseCapture::changing);
    if ( guard.IsInside() )
        return;

    // if the capture was lost unexpectedly, notify every window that has
    // capture (on stack or current) about it and clear the stack:
    while ( !wxMouseCapture::stack.empty() )
    {
        DoNotifyWindowAboutCaptureLost(wxMouseCapture::stack.back());

        wxMouseCapture::stack.pop_back();
    }
}

#if wxUSE_HOTKEY

bool
wxWindowBase::RegisterHotKey(int WXUNUSED(hotkeyId),
                             int WXUNUSED(modifiers),
                             int WXUNUSED(keycode))
{
    // not implemented
    return false;
}

bool wxWindowBase::UnregisterHotKey(int WXUNUSED(hotkeyId))
{
    // not implemented
    return false;
}

#endif // wxUSE_HOTKEY

// ----------------------------------------------------------------------------
// event processing
// ----------------------------------------------------------------------------

bool wxWindowBase::TryBefore(wxEvent& event)
{
#if wxUSE_VALIDATORS
    // Can only use the validator of the window which
    // is receiving the event
    if ( event.GetEventObject() == this )
    {
        wxValidator * const validator = GetValidator();
        if ( validator && validator->ProcessEventLocally(event) )
        {
            return true;
        }
    }
#endif // wxUSE_VALIDATORS

    return wxEvtHandler::TryBefore(event);
}

bool wxWindowBase::TryAfter(wxEvent& event)
{
    // carry on up the parent-child hierarchy if the propagation count hasn't
    // reached zero yet
    if ( event.ShouldPropagate() )
    {
        // honour the requests to stop propagation at this window: this is
        // used by the dialogs, for example, to prevent processing the events
        // from the dialog controls in the parent frame which rarely, if ever,
        // makes sense
        if ( !(GetExtraStyle() & wxWS_EX_BLOCK_EVENTS) )
        {
            wxWindow *parent = GetParent();
            if ( parent && !parent->IsBeingDeleted() )
            {
                wxPropagateOnce propagateOnce(event, this);

                return parent->GetEventHandler()->ProcessEvent(event);
            }
        }
    }

    return wxEvtHandler::TryAfter(event);
}

// ----------------------------------------------------------------------------
// window relationships
// ----------------------------------------------------------------------------

wxWindow *wxWindowBase::DoGetSibling(WindowOrder order) const
{
    wxCHECK_MSG( GetParent(), NULL,
                    wxT("GetPrev/NextSibling() don't work for TLWs!") );

    wxWindowList& siblings = GetParent()->GetChildren();
    wxWindowList::compatibility_iterator i = siblings.Find((wxWindow *)this);
    wxCHECK_MSG( i, NULL, wxT("window not a child of its parent?") );

    if ( order == OrderBefore )
        i = i->GetPrevious();
    else // OrderAfter
        i = i->GetNext();

    return i ? i->GetData() : NULL;
}

// ----------------------------------------------------------------------------
// keyboard navigation
// ----------------------------------------------------------------------------

// Navigates in the specified direction inside this window
bool wxWindowBase::DoNavigateIn(int flags)
{
#ifdef wxHAS_NATIVE_TAB_TRAVERSAL
    // native code doesn't process our wxNavigationKeyEvents anyhow
    wxUnusedVar(flags);
    return false;
#else // !wxHAS_NATIVE_TAB_TRAVERSAL
    wxNavigationKeyEvent eventNav;
    wxWindow *focused = FindFocus();
    eventNav.SetCurrentFocus(focused);
    eventNav.SetEventObject(focused);
    eventNav.SetFlags(flags);
    return GetEventHandler()->ProcessEvent(eventNav);
#endif // wxHAS_NATIVE_TAB_TRAVERSAL/!wxHAS_NATIVE_TAB_TRAVERSAL
}

bool wxWindowBase::HandleAsNavigationKey(const wxKeyEvent& event)
{
    if ( event.GetKeyCode() != WXK_TAB )
        return false;

    int flags = wxNavigationKeyEvent::FromTab;

    if ( event.ShiftDown() )
        flags |= wxNavigationKeyEvent::IsBackward;
    else
        flags |= wxNavigationKeyEvent::IsForward;

    if ( event.ControlDown() )
        flags |= wxNavigationKeyEvent::WinChange;

    Navigate(flags);
    return true;
}

void wxWindowBase::DoMoveInTabOrder(wxWindow *win, WindowOrder move)
{
    // check that we're not a top level window
    wxCHECK_RET( GetParent(),
                    wxT("MoveBefore/AfterInTabOrder() don't work for TLWs!") );

    // detect the special case when we have nothing to do anyhow and when the
    // code below wouldn't work
    if ( win == this )
        return;

    // find the target window in the siblings list
    wxWindowList& siblings = GetParent()->GetChildren();
    wxWindowList::compatibility_iterator i = siblings.Find(win);
    wxCHECK_RET( i, wxT("MoveBefore/AfterInTabOrder(): win is not a sibling") );

    // unfortunately, when wxUSE_STD_CONTAINERS == 1 DetachNode() is not
    // implemented so we can't just move the node around
    wxWindow *self = (wxWindow *)this;
    siblings.DeleteObject(self);
    if ( move == OrderAfter )
    {
        i = i->GetNext();
    }

    if ( i )
    {
        siblings.Insert(i, self);
    }
    else // OrderAfter and win was the last sibling
    {
        siblings.Append(self);
    }
}

// ----------------------------------------------------------------------------
// focus handling
// ----------------------------------------------------------------------------

/*static*/ wxWindow* wxWindowBase::FindFocus()
{
    wxWindowBase *win = DoFindFocus();
    return win ? win->GetMainWindowOfCompositeControl() : NULL;
}

bool wxWindowBase::HasFocus() const
{
    wxWindowBase* const win = DoFindFocus();
    return win &&
            (this == win || this == win->GetMainWindowOfCompositeControl());
}

// ----------------------------------------------------------------------------
// drag and drop
// ----------------------------------------------------------------------------

#if wxUSE_DRAG_AND_DROP && !defined(__WXMSW__)

namespace
{

class DragAcceptFilesTarget : public wxFileDropTarget
{
public:
    DragAcceptFilesTarget(wxWindowBase *win) : m_win(win) {}

    virtual bool OnDropFiles(wxCoord x, wxCoord y,
                             const wxArrayString& filenames)
    {
        wxDropFilesEvent event(wxEVT_DROP_FILES,
                               filenames.size(),
                               wxCArrayString(filenames).Release());
        event.SetEventObject(m_win);
        event.m_pos.x = x;
        event.m_pos.y = y;

        return m_win->HandleWindowEvent(event);
    }

private:
    wxWindowBase * const m_win;

    wxDECLARE_NO_COPY_CLASS(DragAcceptFilesTarget);
};


} // anonymous namespace

// Generic version of DragAcceptFiles(). It works by installing a simple
// wxFileDropTarget-to-EVT_DROP_FILES adaptor and therefore cannot be used
// together with explicit SetDropTarget() calls.
void wxWindowBase::DragAcceptFiles(bool accept)
{
    if ( accept )
    {
        wxASSERT_MSG( !GetDropTarget(),
                      "cannot use DragAcceptFiles() and SetDropTarget() together" );
        SetDropTarget(new DragAcceptFilesTarget(this));
    }
    else
    {
        SetDropTarget(NULL);
    }
}

#endif // wxUSE_DRAG_AND_DROP && !defined(__WXMSW__)

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

wxWindow* wxGetTopLevelParent(wxWindow *win)
{
    while ( win && !win->IsTopLevel() )
         win = win->GetParent();

    return win;
}

#if wxUSE_ACCESSIBILITY
// ----------------------------------------------------------------------------
// accessible object for windows
// ----------------------------------------------------------------------------

// Can return either a child object, or an integer
// representing the child element, starting from 1.
wxAccStatus wxWindowAccessible::HitTest(const wxPoint& WXUNUSED(pt), int* WXUNUSED(childId), wxAccessible** WXUNUSED(childObject))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

// Returns the rectangle for this object (id = 0) or a child element (id > 0).
wxAccStatus wxWindowAccessible::GetLocation(wxRect& rect, int elementId)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    wxWindow* win = NULL;
    if (elementId == 0)
    {
        win = GetWindow();
    }
    else
    {
        if (elementId <= (int) GetWindow()->GetChildren().GetCount())
        {
            win = GetWindow()->GetChildren().Item(elementId-1)->GetData();
        }
        else
            return wxACC_FAIL;
    }
    if (win)
    {
        rect = win->GetRect();
        if (win->GetParent() && !wxDynamicCast(win, wxTopLevelWindow))
            rect.SetPosition(win->GetParent()->ClientToScreen(rect.GetPosition()));
        return wxACC_OK;
    }

    return wxACC_NOT_IMPLEMENTED;
}

// Navigates from fromId to toId/toObject.
wxAccStatus wxWindowAccessible::Navigate(wxNavDir navDir, int fromId,
                             int* WXUNUSED(toId), wxAccessible** toObject)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    switch (navDir)
    {
    case wxNAVDIR_FIRSTCHILD:
        {
            if (GetWindow()->GetChildren().GetCount() == 0)
                return wxACC_FALSE;
            wxWindow* childWindow = (wxWindow*) GetWindow()->GetChildren().GetFirst()->GetData();
            *toObject = childWindow->GetOrCreateAccessible();

            return wxACC_OK;
        }
    case wxNAVDIR_LASTCHILD:
        {
            if (GetWindow()->GetChildren().GetCount() == 0)
                return wxACC_FALSE;
            wxWindow* childWindow = (wxWindow*) GetWindow()->GetChildren().GetLast()->GetData();
            *toObject = childWindow->GetOrCreateAccessible();

            return wxACC_OK;
        }
    case wxNAVDIR_RIGHT:
    case wxNAVDIR_DOWN:
    case wxNAVDIR_NEXT:
        {
            wxWindowList::compatibility_iterator node =
                wxWindowList::compatibility_iterator();
            if (fromId == 0)
            {
                // Can't navigate to sibling of this window
                // if we're a top-level window.
                if (!GetWindow()->GetParent())
                    return wxACC_NOT_IMPLEMENTED;

                node = GetWindow()->GetParent()->GetChildren().Find(GetWindow());
            }
            else
            {
                if (fromId <= (int) GetWindow()->GetChildren().GetCount())
                    node = GetWindow()->GetChildren().Item(fromId-1);
            }

            if (node && node->GetNext())
            {
                wxWindow* nextWindow = node->GetNext()->GetData();
                *toObject = nextWindow->GetOrCreateAccessible();
                return wxACC_OK;
            }
            else
                return wxACC_FALSE;
        }
    case wxNAVDIR_LEFT:
    case wxNAVDIR_UP:
    case wxNAVDIR_PREVIOUS:
        {
            wxWindowList::compatibility_iterator node =
                wxWindowList::compatibility_iterator();
            if (fromId == 0)
            {
                // Can't navigate to sibling of this window
                // if we're a top-level window.
                if (!GetWindow()->GetParent())
                    return wxACC_NOT_IMPLEMENTED;

                node = GetWindow()->GetParent()->GetChildren().Find(GetWindow());
            }
            else
            {
                if (fromId <= (int) GetWindow()->GetChildren().GetCount())
                    node = GetWindow()->GetChildren().Item(fromId-1);
            }

            if (node && node->GetPrevious())
            {
                wxWindow* previousWindow = node->GetPrevious()->GetData();
                *toObject = previousWindow->GetOrCreateAccessible();
                return wxACC_OK;
            }
            else
                return wxACC_FALSE;
        }
    }

    return wxACC_NOT_IMPLEMENTED;
}

// Gets the name of the specified object.
wxAccStatus wxWindowAccessible::GetName(int childId, wxString* name)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    wxString title;

    // If a child, leave wxWidgets to call the function on the actual
    // child object.
    if (childId > 0)
        return wxACC_NOT_IMPLEMENTED;

    // This will eventually be replaced by specialised
    // accessible classes, one for each kind of wxWidgets
    // control or window.
#if wxUSE_BUTTON
    if (wxDynamicCast(GetWindow(), wxButton))
        title = ((wxButton*) GetWindow())->GetLabel();
    else
#endif
        title = GetWindow()->GetName();

    if (!title.empty())
    {
        *name = title;
        return wxACC_OK;
    }
    else
        return wxACC_NOT_IMPLEMENTED;
}

// Gets the number of children.
wxAccStatus wxWindowAccessible::GetChildCount(int* childId)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    *childId = (int) GetWindow()->GetChildren().GetCount();
    return wxACC_OK;
}

// Gets the specified child (starting from 1).
// If *child is NULL and return value is wxACC_OK,
// this means that the child is a simple element and
// not an accessible object.
wxAccStatus wxWindowAccessible::GetChild(int childId, wxAccessible** child)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    if (childId == 0)
    {
        *child = this;
        return wxACC_OK;
    }

    if (childId > (int) GetWindow()->GetChildren().GetCount())
        return wxACC_FAIL;

    wxWindow* childWindow = GetWindow()->GetChildren().Item(childId-1)->GetData();
    *child = childWindow->GetOrCreateAccessible();
    if (*child)
        return wxACC_OK;
    else
        return wxACC_FAIL;
}

// Gets the parent, or NULL.
wxAccStatus wxWindowAccessible::GetParent(wxAccessible** parent)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    wxWindow* parentWindow = GetWindow()->GetParent();
    if (!parentWindow)
    {
        *parent = NULL;
        return wxACC_OK;
    }
    else
    {
        *parent = parentWindow->GetOrCreateAccessible();
        if (*parent)
            return wxACC_OK;
        else
            return wxACC_FAIL;
    }
}

// Performs the default action. childId is 0 (the action for this object)
// or > 0 (the action for a child).
// Return wxACC_NOT_SUPPORTED if there is no default action for this
// window (e.g. an edit control).
wxAccStatus wxWindowAccessible::DoDefaultAction(int WXUNUSED(childId))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

// Gets the default action for this object (0) or > 0 (the action for a child).
// Return wxACC_OK even if there is no action. actionName is the action, or the empty
// string if there is no action.
// The retrieved string describes the action that is performed on an object,
// not what the object does as a result. For example, a toolbar button that prints
// a document has a default action of "Press" rather than "Prints the current document."
wxAccStatus wxWindowAccessible::GetDefaultAction(int WXUNUSED(childId), wxString* WXUNUSED(actionName))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

// Returns the description for this object or a child.
wxAccStatus wxWindowAccessible::GetDescription(int WXUNUSED(childId), wxString* description)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    wxString ht(GetWindow()->GetHelpTextAtPoint(wxDefaultPosition, wxHelpEvent::Origin_Keyboard));
    if (!ht.empty())
    {
        *description = ht;
        return wxACC_OK;
    }
    return wxACC_NOT_IMPLEMENTED;
}

// Returns help text for this object or a child, similar to tooltip text.
wxAccStatus wxWindowAccessible::GetHelpText(int WXUNUSED(childId), wxString* helpText)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    wxString ht(GetWindow()->GetHelpTextAtPoint(wxDefaultPosition, wxHelpEvent::Origin_Keyboard));
    if (!ht.empty())
    {
        *helpText = ht;
        return wxACC_OK;
    }
    return wxACC_NOT_IMPLEMENTED;
}

// Returns the keyboard shortcut for this object or child.
// Return e.g. ALT+K
wxAccStatus wxWindowAccessible::GetKeyboardShortcut(int WXUNUSED(childId), wxString* WXUNUSED(shortcut))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

// Returns a role constant.
wxAccStatus wxWindowAccessible::GetRole(int childId, wxAccRole* role)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    // If a child, leave wxWidgets to call the function on the actual
    // child object.
    if (childId > 0)
        return wxACC_NOT_IMPLEMENTED;

    if (wxDynamicCast(GetWindow(), wxControl))
        return wxACC_NOT_IMPLEMENTED;
#if wxUSE_STATUSBAR
    if (wxDynamicCast(GetWindow(), wxStatusBar))
        return wxACC_NOT_IMPLEMENTED;
#endif
#if wxUSE_TOOLBAR
    if (wxDynamicCast(GetWindow(), wxToolBar))
        return wxACC_NOT_IMPLEMENTED;
#endif

    //*role = wxROLE_SYSTEM_CLIENT;
    *role = wxROLE_SYSTEM_CLIENT;
    return wxACC_OK;

    #if 0
    return wxACC_NOT_IMPLEMENTED;
    #endif
}

// Returns a state constant.
wxAccStatus wxWindowAccessible::GetState(int childId, long* state)
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    // If a child, leave wxWidgets to call the function on the actual
    // child object.
    if (childId > 0)
        return wxACC_NOT_IMPLEMENTED;

    if (wxDynamicCast(GetWindow(), wxControl))
        return wxACC_NOT_IMPLEMENTED;

#if wxUSE_STATUSBAR
    if (wxDynamicCast(GetWindow(), wxStatusBar))
        return wxACC_NOT_IMPLEMENTED;
#endif
#if wxUSE_TOOLBAR
    if (wxDynamicCast(GetWindow(), wxToolBar))
        return wxACC_NOT_IMPLEMENTED;
#endif

    *state = 0;
    return wxACC_OK;

    #if 0
    return wxACC_NOT_IMPLEMENTED;
    #endif
}

// Returns a localized string representing the value for the object
// or child.
wxAccStatus wxWindowAccessible::GetValue(int WXUNUSED(childId), wxString* WXUNUSED(strValue))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

// Selects the object or child.
wxAccStatus wxWindowAccessible::Select(int WXUNUSED(childId), wxAccSelectionFlags WXUNUSED(selectFlags))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

// Gets the window with the keyboard focus.
// If childId is 0 and child is NULL, no object in
// this subhierarchy has the focus.
// If this object has the focus, child should be 'this'.
wxAccStatus wxWindowAccessible::GetFocus(int* WXUNUSED(childId), wxAccessible** WXUNUSED(child))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}

#if wxUSE_VARIANT
// Gets a variant representing the selected children
// of this object.
// Acceptable values:
// - a null variant (IsNull() returns true)
// - a list variant (GetType() == wxT("list")
// - an integer representing the selected child element,
//   or 0 if this object is selected (GetType() == wxT("long")
// - a "void*" pointer to a wxAccessible child object
wxAccStatus wxWindowAccessible::GetSelections(wxVariant* WXUNUSED(selections))
{
    wxASSERT( GetWindow() != NULL );
    if (!GetWindow())
        return wxACC_FAIL;

    return wxACC_NOT_IMPLEMENTED;
}
#endif // wxUSE_VARIANT

#endif // wxUSE_ACCESSIBILITY

// ----------------------------------------------------------------------------
// RTL support
// ----------------------------------------------------------------------------

wxCoord
wxWindowBase::AdjustForLayoutDirection(wxCoord x,
                                       wxCoord width,
                                       wxCoord widthTotal) const
{
    if ( GetLayoutDirection() == wxLayout_RightToLeft )
    {
        x = widthTotal - x - width;
    }

    return x;
}


