/////////////////////////////////////////////////////////////////////////////
// Name:        wx/clipbrd.h
// Purpose:     wxClipboad class and clipboard functions
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.10.99
// Copyright:   (c) wxWidgets Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CLIPBRD_H_BASE_
#define _WX_CLIPBRD_H_BASE_

#include "wx/defs.h"

#if wxUSE_CLIPBOARD


#include "wx/event.h"
#include "wx/chartype.h"
#include "wx/dataobj.h"     // for wxDataFormat
#include "wx/vector.h"

class WXDLLIMPEXP_FWD_CORE wxClipboard;

// ----------------------------------------------------------------------------
// wxClipboard represents the system clipboard. Normally, you should use
// wxTheClipboard which is a global pointer to the (unique) clipboard.
//
// Clipboard can be used to copy data to/paste data from. It works together
// with wxDataObject.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxClipboardBase : public wxObject
{
public:
    wxClipboardBase() { m_usePrimary = false; }

    // open the clipboard before Add/SetData() and GetData()
    virtual bool Open() = 0;

    // close the clipboard after Add/SetData() and GetData()
    virtual void Close() = 0;

    // query whether the clipboard is opened
    virtual bool IsOpened() const = 0;

    // add to the clipboard data
    //
    // NB: the clipboard owns the pointer and will delete it, so data must be
    //     allocated on the heap
    virtual bool AddData( wxDataObject *data ) = 0;

    // set the clipboard data, this is the same as Clear() followed by
    // AddData()
    virtual bool SetData( wxDataObject *data ) = 0;

    // ask if data in correct format is available
    virtual bool IsSupported( const wxDataFormat& format ) = 0;

    // ask if data in correct format is available
    virtual bool IsSupportedAsync( wxEvtHandler *sink );

    // fill data with data on the clipboard (if available)
    virtual bool GetData( wxDataObject& data ) = 0;

    // clears wxTheClipboard and the system's clipboard if possible
    virtual void Clear() = 0;

    // flushes the clipboard: this means that the data which is currently on
    // clipboard will stay available even after the application exits (possibly
    // eating memory), otherwise the clipboard will be emptied on exit
    virtual bool Flush() { return false; }

    // this allows to choose whether we work with CLIPBOARD (default) or
    // PRIMARY selection on X11-based systems
    //
    // on the other ones, working with primary selection does nothing: this
    // allows to write code which sets the primary selection when something is
    // selected without any ill effects (i.e. without overwriting the
    // clipboard which would be wrong on the platforms without X11 PRIMARY)
    virtual void UsePrimarySelection(bool usePrimary = false)
    {
        m_usePrimary = usePrimary;
    }

    // return true if we're using primary selection
    bool IsUsingPrimarySelection() const { return m_usePrimary; }

    // Returns global instance (wxTheClipboard) of the object:
    static wxClipboard *Get();


    // don't use this directly, it is public for compatibility with some ports
    // (wxX11, wxMotif, ...) only
    bool m_usePrimary;
};

// ----------------------------------------------------------------------------
// asynchronous clipboard event
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxClipboardEvent : public wxEvent
{
public:
    wxClipboardEvent(wxEventType evtType = wxEVT_NULL)
        : wxEvent(0, evtType)
    {
    }

    wxClipboardEvent(const wxClipboardEvent& event)
        : wxEvent(event),
          m_formats(event.m_formats)
    {
    }

    bool SupportsFormat(const wxDataFormat& format) const;
    void AddFormat(const wxDataFormat& format);

    virtual wxEvent *Clone() const
    {
        return new wxClipboardEvent(*this);
    }


protected:
    wxVector<wxDataFormat> m_formats;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxClipboardEvent)
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_CLIPBOARD_CHANGED, wxClipboardEvent );

typedef void (wxEvtHandler::*wxClipboardEventFunction)(wxClipboardEvent&);

#define wxClipboardEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxClipboardEventFunction, func)

#define EVT_CLIPBOARD_CHANGED(func) wx__DECLARE_EVT0(wxEVT_CLIPBOARD_CHANGED, wxClipboardEventHandler(func))

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

// The global clipboard object - backward compatible access macro:
#define wxTheClipboard   (wxClipboard::Get())

// ----------------------------------------------------------------------------
// include platform-specific class declaration
// ----------------------------------------------------------------------------

#if defined(__WXMSW__)
    #include "wx/msw/clipbrd.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/clipbrd.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/clipbrd.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/clipbrd.h"
#elif defined(__WXX11__)
    #include "wx/x11/clipbrd.h"
#elif defined(__WXMAC__)
    #include "wx/osx/clipbrd.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/clipbrd.h"
#elif defined(__WXPM__)
    #include "wx/os2/clipbrd.h"
#endif

// ----------------------------------------------------------------------------
// helpful class for opening the clipboard and automatically closing it
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxClipboardLocker
{
public:
    wxClipboardLocker(wxClipboard *clipboard = NULL)
    {
        m_clipboard = clipboard ? clipboard : wxTheClipboard;
        if ( m_clipboard )
        {
            m_clipboard->Open();
        }
    }

    bool operator!() const { return !m_clipboard->IsOpened(); }

    ~wxClipboardLocker()
    {
        if ( m_clipboard )
        {
            m_clipboard->Close();
        }
    }

private:
    wxClipboard *m_clipboard;

    wxDECLARE_NO_COPY_CLASS(wxClipboardLocker);
};

#endif // wxUSE_CLIPBOARD

#endif // _WX_CLIPBRD_H_BASE_
