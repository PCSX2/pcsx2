///////////////////////////////////////////////////////////////////////////////
// Name:        wx/mediactrl.h
// Purpose:     wxMediaCtrl class
// Author:      Ryan Norton <wxprojects@comcast.net>
// Modified by:
// Created:     11/07/04
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Definitions
// ============================================================================

// ----------------------------------------------------------------------------
// Header guard
// ----------------------------------------------------------------------------
#ifndef _WX_MEDIACTRL_H_
#define _WX_MEDIACTRL_H_

// ----------------------------------------------------------------------------
// Pre-compiled header stuff
// ----------------------------------------------------------------------------

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// Compilation guard
// ----------------------------------------------------------------------------

#if wxUSE_MEDIACTRL

// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

#include "wx/control.h"
#include "wx/uri.h"

// ============================================================================
// Declarations
// ============================================================================

// ----------------------------------------------------------------------------
//
// Enumerations
//
// ----------------------------------------------------------------------------

enum wxMediaState
{
    wxMEDIASTATE_STOPPED,
    wxMEDIASTATE_PAUSED,
    wxMEDIASTATE_PLAYING
};

enum wxMediaCtrlPlayerControls
{
    wxMEDIACTRLPLAYERCONTROLS_NONE           =   0,
    //Step controls like fastforward, step one frame etc.
    wxMEDIACTRLPLAYERCONTROLS_STEP           =   1 << 0,
    //Volume controls like the speaker icon, volume slider, etc.
    wxMEDIACTRLPLAYERCONTROLS_VOLUME         =   1 << 1,
    wxMEDIACTRLPLAYERCONTROLS_DEFAULT        =
                    wxMEDIACTRLPLAYERCONTROLS_STEP |
                    wxMEDIACTRLPLAYERCONTROLS_VOLUME
};

#define wxMEDIABACKEND_DIRECTSHOW   wxT("wxAMMediaBackend")
#define wxMEDIABACKEND_MCI          wxT("wxMCIMediaBackend")
#define wxMEDIABACKEND_QUICKTIME    wxT("wxQTMediaBackend")
#define wxMEDIABACKEND_GSTREAMER    wxT("wxGStreamerMediaBackend")
#define wxMEDIABACKEND_REALPLAYER   wxT("wxRealPlayerMediaBackend")
#define wxMEDIABACKEND_WMP10        wxT("wxWMP10MediaBackend")

// ----------------------------------------------------------------------------
//
// wxMediaEvent
//
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_MEDIA wxMediaEvent : public wxNotifyEvent
{
public:
    // ------------------------------------------------------------------------
    // wxMediaEvent Constructor
    //
    // Normal constructor, much the same as wxNotifyEvent
    // ------------------------------------------------------------------------
    wxMediaEvent(wxEventType commandType = wxEVT_NULL, int winid = 0)
        : wxNotifyEvent(commandType, winid)
    {                                       }

    // ------------------------------------------------------------------------
    // wxMediaEvent Copy Constructor
    //
    // Normal copy constructor, much the same as wxNotifyEvent
    // ------------------------------------------------------------------------
    wxMediaEvent(const wxMediaEvent &clone)
            : wxNotifyEvent(clone)
    {                                       }

    // ------------------------------------------------------------------------
    // wxMediaEvent::Clone
    //
    // Allocates a copy of this object.
    // Required for wxEvtHandler::AddPendingEvent
    // ------------------------------------------------------------------------
    virtual wxEvent *Clone() const
    {   return new wxMediaEvent(*this);     }


    // Put this class on wxWidget's RTTI table
    DECLARE_DYNAMIC_CLASS(wxMediaEvent)
};

// ----------------------------------------------------------------------------
//
// wxMediaCtrl
//
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_MEDIA wxMediaCtrl : public wxControl
{
public:
    wxMediaCtrl() : m_imp(NULL), m_bLoaded(false)
    {                                                                   }

    wxMediaCtrl(wxWindow* parent, wxWindowID winid,
                const wxString& fileName = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& szBackend = wxEmptyString,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxT("mediaCtrl"))
                : m_imp(NULL), m_bLoaded(false)
    {   Create(parent, winid, fileName, pos, size, style,
               szBackend, validator, name);                             }

    wxMediaCtrl(wxWindow* parent, wxWindowID winid,
                const wxURI& location,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& szBackend = wxEmptyString,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxT("mediaCtrl"))
                : m_imp(NULL), m_bLoaded(false)
    {   Create(parent, winid, location, pos, size, style,
               szBackend, validator, name);                             }

    virtual ~wxMediaCtrl();

    bool Create(wxWindow* parent, wxWindowID winid,
                const wxString& fileName = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& szBackend = wxEmptyString,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxT("mediaCtrl"));

    bool Create(wxWindow* parent, wxWindowID winid,
                const wxURI& location,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& szBackend = wxEmptyString,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxT("mediaCtrl"));

    bool DoCreate(const wxClassInfo* instance,
                wxWindow* parent, wxWindowID winid,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxT("mediaCtrl"));

    bool Play();
    bool Pause();
    bool Stop();

    bool Load(const wxString& fileName);

    wxMediaState GetState();

    wxFileOffset Seek(wxFileOffset where, wxSeekMode mode = wxFromStart);
    wxFileOffset Tell(); //FIXME: This should be const
    wxFileOffset Length(); //FIXME: This should be const

    double GetPlaybackRate();           //All but MCI & GStreamer
    bool SetPlaybackRate(double dRate); //All but MCI & GStreamer

    bool Load(const wxURI& location);
    bool Load(const wxURI& location, const wxURI& proxy);

    wxFileOffset GetDownloadProgress(); // DirectShow only
    wxFileOffset GetDownloadTotal();    // DirectShow only

    double GetVolume();
    bool   SetVolume(double dVolume);

    bool    ShowPlayerControls(
        wxMediaCtrlPlayerControls flags = wxMEDIACTRLPLAYERCONTROLS_DEFAULT);

    //helpers for the wxPython people
    bool LoadURI(const wxString& fileName)
    {   return Load(wxURI(fileName));       }
    bool LoadURIWithProxy(const wxString& fileName, const wxString& proxy)
    {   return Load(wxURI(fileName), wxURI(proxy));       }

protected:
    static const wxClassInfo* NextBackend(wxClassInfo::const_iterator* it);

    void OnMediaFinished(wxMediaEvent& evt);
    virtual void DoMoveWindow(int x, int y, int w, int h);
    wxSize DoGetBestSize() const;

    //FIXME:  This is nasty... find a better way to work around
    //inheritance issues
#if defined(__WXOSX_CARBON__)
    virtual void MacVisibilityChanged();
#endif
#if defined(__WXOSX_CARBON__) || defined(__WXCOCOA__)
    friend class wxQTMediaBackend;
#endif
    class wxMediaBackend* m_imp;
    bool m_bLoaded;

    DECLARE_DYNAMIC_CLASS(wxMediaCtrl)
};

// ----------------------------------------------------------------------------
//
// wxMediaBackend
//
// Derive from this and use standard wxWidgets RTTI
// (DECLARE_DYNAMIC_CLASS and IMPLEMENT_CLASS) to make a backend
// for wxMediaCtrl.  Backends are searched alphabetically -
// the one with the earliest letter is tried first.
//
// Note that this is currently not API or ABI compatible -
// so statically link or make the client compile on-site.
//
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_MEDIA wxMediaBackend : public wxObject
{
public:
    wxMediaBackend()
    {                                   }

    virtual ~wxMediaBackend();

    virtual bool CreateControl(wxControl* WXUNUSED(ctrl),
                               wxWindow* WXUNUSED(parent),
                               wxWindowID WXUNUSED(winid),
                               const wxPoint& WXUNUSED(pos),
                               const wxSize& WXUNUSED(size),
                               long WXUNUSED(style),
                               const wxValidator& WXUNUSED(validator),
                               const wxString& WXUNUSED(name))
    {   return false;                   }

    virtual bool Play()
    {   return false;                   }
    virtual bool Pause()
    {   return false;                   }
    virtual bool Stop()
    {   return false;                   }

    virtual bool Load(const wxString& WXUNUSED(fileName))
    {   return false;                   }
    virtual bool Load(const wxURI& WXUNUSED(location))
    {   return false;                   }

    virtual bool SetPosition(wxLongLong WXUNUSED(where))
    {   return 0;                       }
    virtual wxLongLong GetPosition()
    {   return 0;                       }
    virtual wxLongLong GetDuration()
    {   return 0;                       }

    virtual void Move(int WXUNUSED(x), int WXUNUSED(y),
                      int WXUNUSED(w), int WXUNUSED(h))
    {                                   }
    virtual wxSize GetVideoSize() const
    {   return wxSize(0,0);             }

    virtual double GetPlaybackRate()
    {   return 0.0;                     }
    virtual bool SetPlaybackRate(double WXUNUSED(dRate))
    {   return false;                   }

    virtual wxMediaState GetState()
    {   return wxMEDIASTATE_STOPPED;    }

    virtual double GetVolume()
    {   return 0.0;                     }
    virtual bool SetVolume(double WXUNUSED(dVolume))
    {   return false;                   }

    virtual bool Load(const wxURI& WXUNUSED(location),
                      const wxURI& WXUNUSED(proxy))
    {   return false;                   }

    virtual bool   ShowPlayerControls(
                    wxMediaCtrlPlayerControls WXUNUSED(flags))
    {   return false;                   }
    virtual bool   IsInterfaceShown()
    {   return false;                   }

    virtual wxLongLong GetDownloadProgress()
    {    return 0;                      }
    virtual wxLongLong GetDownloadTotal()
    {    return 0;                      }

    virtual void MacVisibilityChanged()
    {                                   }
    virtual void RESERVED9() {}

    DECLARE_DYNAMIC_CLASS(wxMediaBackend)
};


//Our events
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_MEDIA, wxEVT_MEDIA_FINISHED, wxMediaEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_MEDIA, wxEVT_MEDIA_STOP, wxMediaEvent );

//Function type(s) our events need
typedef void (wxEvtHandler::*wxMediaEventFunction)(wxMediaEvent&);

#define wxMediaEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxMediaEventFunction, func)

//Macro for usage with message maps
#define EVT_MEDIA_FINISHED(winid, fn)   wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_MEDIA_FINISHED, winid, wxID_ANY, wxMediaEventHandler(fn), NULL ),
#define EVT_MEDIA_STOP(winid, fn)       wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_MEDIA_STOP, winid, wxID_ANY, wxMediaEventHandler(fn), NULL ),

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_MEDIA, wxEVT_MEDIA_LOADED, wxMediaEvent );
#define EVT_MEDIA_LOADED(winid, fn)     wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_MEDIA_LOADED, winid, wxID_ANY, wxMediaEventHandler(fn), NULL ),

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_MEDIA, wxEVT_MEDIA_STATECHANGED, wxMediaEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_MEDIA, wxEVT_MEDIA_PLAY, wxMediaEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_MEDIA, wxEVT_MEDIA_PAUSE, wxMediaEvent );
#define EVT_MEDIA_STATECHANGED(winid, fn)   wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_MEDIA_STATECHANGED, winid, wxID_ANY, wxMediaEventHandler(fn), NULL ),
#define EVT_MEDIA_PLAY(winid, fn)           wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_MEDIA_PLAY, winid, wxID_ANY, wxMediaEventHandler(fn), NULL ),
#define EVT_MEDIA_PAUSE(winid, fn)          wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_MEDIA_PAUSE, winid, wxID_ANY, wxMediaEventHandler(fn), NULL ),

// ----------------------------------------------------------------------------
// common backend base class used by many other backends
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_MEDIA wxMediaBackendCommonBase : public wxMediaBackend
{
public:
    // add a pending wxMediaEvent of the given type
    void QueueEvent(wxEventType evtType);

    // notify that the movie playback is finished
    void QueueFinishEvent()
    {
        QueueEvent(wxEVT_MEDIA_STATECHANGED);
        QueueEvent(wxEVT_MEDIA_FINISHED);
    }

    // send the stop event and return true if it hasn't been vetoed
    bool SendStopEvent();

    // Queue pause event
    void QueuePlayEvent();

    // Queue pause event
    void QueuePauseEvent();

    // Queue stop event (no veto)
    void QueueStopEvent();

protected:
    // call this when the movie size has changed but not because it has just
    // been loaded (in this case, call NotifyMovieLoaded() below)
    void NotifyMovieSizeChanged();

    // call this when the movie is fully loaded
    void NotifyMovieLoaded();


    wxMediaCtrl *m_ctrl;      // parent control
};

// ----------------------------------------------------------------------------
// End compilation guard
// ----------------------------------------------------------------------------
#endif // wxUSE_MEDIACTRL

// ----------------------------------------------------------------------------
// End header guard and header itself
// ----------------------------------------------------------------------------
#endif // _WX_MEDIACTRL_H_


