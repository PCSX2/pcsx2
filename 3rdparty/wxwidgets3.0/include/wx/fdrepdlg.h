/////////////////////////////////////////////////////////////////////////////
// Name:        wx/fdrepdlg.h
// Purpose:     wxFindReplaceDialog class
// Author:      Markus Greither and Vadim Zeitlin
// Modified by:
// Created:     23/03/2001
// Copyright:   (c) Markus Greither
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FINDREPLACEDLG_H_
#define _WX_FINDREPLACEDLG_H_

#include "wx/defs.h"

#if wxUSE_FINDREPLDLG

#include "wx/dialog.h"

class WXDLLIMPEXP_FWD_CORE wxFindDialogEvent;
class WXDLLIMPEXP_FWD_CORE wxFindReplaceDialog;
class WXDLLIMPEXP_FWD_CORE wxFindReplaceData;
class WXDLLIMPEXP_FWD_CORE wxFindReplaceDialogImpl;

// ----------------------------------------------------------------------------
// Flags for wxFindReplaceData.Flags
// ----------------------------------------------------------------------------

// flages used by wxFindDialogEvent::GetFlags()
enum wxFindReplaceFlags
{
    // downward search/replace selected (otherwise - upwards)
    wxFR_DOWN       = 1,

    // whole word search/replace selected
    wxFR_WHOLEWORD  = 2,

    // case sensitive search/replace selected (otherwise - case insensitive)
    wxFR_MATCHCASE  = 4
};

// these flags can be specified in wxFindReplaceDialog ctor or Create()
enum wxFindReplaceDialogStyles
{
    // replace dialog (otherwise find dialog)
    wxFR_REPLACEDIALOG = 1,

    // don't allow changing the search direction
    wxFR_NOUPDOWN      = 2,

    // don't allow case sensitive searching
    wxFR_NOMATCHCASE   = 4,

    // don't allow whole word searching
    wxFR_NOWHOLEWORD   = 8
};

// ----------------------------------------------------------------------------
// wxFindReplaceData: holds Setup Data/Feedback Data for wxFindReplaceDialog
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFindReplaceData : public wxObject
{
public:
    wxFindReplaceData() { Init(); }
    wxFindReplaceData(wxUint32 flags) { Init(); SetFlags(flags); }

    // accessors
    const wxString& GetFindString() const { return m_FindWhat; }
    const wxString& GetReplaceString() const { return m_ReplaceWith; }

    int GetFlags() const { return m_Flags; }

    // setters: may only be called before showing the dialog, no effect later
    void SetFlags(wxUint32 flags) { m_Flags = flags; }

    void SetFindString(const wxString& str) { m_FindWhat = str; }
    void SetReplaceString(const wxString& str) { m_ReplaceWith = str; }

protected:
    void Init();

private:
    wxUint32 m_Flags;
    wxString m_FindWhat,
             m_ReplaceWith;

    friend class wxFindReplaceDialogBase;
};

// ----------------------------------------------------------------------------
// wxFindReplaceDialogBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFindReplaceDialogBase : public wxDialog
{
public:
    // ctors and such
    wxFindReplaceDialogBase() { m_FindReplaceData = NULL; }
    wxFindReplaceDialogBase(wxWindow * WXUNUSED(parent),
                            wxFindReplaceData *data,
                            const wxString& WXUNUSED(title),
                            int WXUNUSED(style) = 0)
    {
        m_FindReplaceData = data;
    }

    virtual ~wxFindReplaceDialogBase();

    // find dialog data access
    const wxFindReplaceData *GetData() const { return m_FindReplaceData; }
    void SetData(wxFindReplaceData *data) { m_FindReplaceData = data; }

    // implementation only, don't use
    void Send(wxFindDialogEvent& event);

protected:
    wxFindReplaceData *m_FindReplaceData;

    // the last string we searched for
    wxString m_lastSearch;

    wxDECLARE_NO_COPY_CLASS(wxFindReplaceDialogBase);
};

// include wxFindReplaceDialog declaration
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__) && !defined(__WXWINCE__)
    #include "wx/msw/fdrepdlg.h"
#else
    #define wxGenericFindReplaceDialog wxFindReplaceDialog

    #include "wx/generic/fdrepdlg.h"
#endif

// ----------------------------------------------------------------------------
// wxFindReplaceDialog events
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFindDialogEvent : public wxCommandEvent
{
public:
    wxFindDialogEvent(wxEventType commandType = wxEVT_NULL, int id = 0)
        : wxCommandEvent(commandType, id) { }
    wxFindDialogEvent(const wxFindDialogEvent& event)
        : wxCommandEvent(event), m_strReplace(event.m_strReplace) { }

    int GetFlags() const { return GetInt(); }
    wxString GetFindString() const { return GetString(); }
    const wxString& GetReplaceString() const { return m_strReplace; }

    wxFindReplaceDialog *GetDialog() const
        { return wxStaticCast(GetEventObject(), wxFindReplaceDialog); }

    // implementation only
    void SetFlags(int flags) { SetInt(flags); }
    void SetFindString(const wxString& str) { SetString(str); }
    void SetReplaceString(const wxString& str) { m_strReplace = str; }

    virtual wxEvent *Clone() const { return new wxFindDialogEvent(*this); }

private:
    wxString m_strReplace;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxFindDialogEvent)
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FIND, wxFindDialogEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FIND_NEXT, wxFindDialogEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FIND_REPLACE, wxFindDialogEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FIND_REPLACE_ALL, wxFindDialogEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FIND_CLOSE, wxFindDialogEvent );

typedef void (wxEvtHandler::*wxFindDialogEventFunction)(wxFindDialogEvent&);

#define wxFindDialogEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxFindDialogEventFunction, func)

#define EVT_FIND(id, fn) \
    wx__DECLARE_EVT1(wxEVT_FIND, id, wxFindDialogEventHandler(fn))

#define EVT_FIND_NEXT(id, fn) \
    wx__DECLARE_EVT1(wxEVT_FIND_NEXT, id, wxFindDialogEventHandler(fn))

#define EVT_FIND_REPLACE(id, fn) \
    wx__DECLARE_EVT1(wxEVT_FIND_REPLACE, id, wxFindDialogEventHandler(fn))

#define EVT_FIND_REPLACE_ALL(id, fn) \
    wx__DECLARE_EVT1(wxEVT_FIND_REPLACE_ALL, id, wxFindDialogEventHandler(fn))

#define EVT_FIND_CLOSE(id, fn) \
    wx__DECLARE_EVT1(wxEVT_FIND_CLOSE, id, wxFindDialogEventHandler(fn))

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_FIND               wxEVT_FIND
#define wxEVT_COMMAND_FIND_NEXT          wxEVT_FIND_NEXT
#define wxEVT_COMMAND_FIND_REPLACE       wxEVT_FIND_REPLACE
#define wxEVT_COMMAND_FIND_REPLACE_ALL   wxEVT_FIND_REPLACE_ALL
#define wxEVT_COMMAND_FIND_CLOSE         wxEVT_FIND_CLOSE

#endif // wxUSE_FINDREPLDLG

#endif
    // _WX_FDREPDLG_H
