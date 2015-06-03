/////////////////////////////////////////////////////////////////////////////
// Name:        wx/busyinfo.h
// Purpose:     Information window (when app is busy)
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __BUSYINFO_H_BASE__
#define __BUSYINFO_H_BASE__

#include "wx/defs.h"

#if wxUSE_BUSYINFO

#include "wx/colour.h"
#include "wx/icon.h"

class WXDLLIMPEXP_FWD_CORE wxWindow;

// This class is used to pass all the various parameters to wxBusyInfo ctor.
// According to the usual naming conventions (see wxAboutDialogInfo,
// wxFontInfo, ...) it would be called wxBusyInfoInfo, but this would have been
// rather strange, so we call it wxBusyInfoFlags instead.
//
// Methods are mostly self-explanatory except for the difference between "Text"
// and "Label": the former can contain markup, while the latter is just plain
// string which is not parsed in any way.
class wxBusyInfoFlags
{
public:
    wxBusyInfoFlags()
    {
        m_parent = NULL;
        m_alpha = wxALPHA_OPAQUE;
    }

    wxBusyInfoFlags& Parent(wxWindow* parent)
        { m_parent = parent; return *this; }

    wxBusyInfoFlags& Icon(const wxIcon& icon)
        { m_icon = icon; return *this; }
    wxBusyInfoFlags& Title(const wxString& title)
        { m_title = title; return *this; }
    wxBusyInfoFlags& Text(const wxString& text)
        { m_text = text; return *this; }
    wxBusyInfoFlags& Label(const wxString& label)
        { m_label = label; return *this; }

    wxBusyInfoFlags& Foreground(const wxColour& foreground)
        { m_foreground = foreground; return *this; }
    wxBusyInfoFlags& Background(const wxColour& background)
        { m_background = background; return *this; }

    wxBusyInfoFlags& Transparency(wxByte alpha)
        { m_alpha = alpha; return *this; }

private:
    wxWindow* m_parent;

    wxIcon m_icon;
    wxString m_title,
             m_text,
             m_label;

    wxColour m_foreground,
             m_background;

    wxByte m_alpha;

    friend class wxBusyInfo;
};

#include "wx/generic/busyinfo.h"

#endif // wxUSE_BUSYINFO

#endif // __BUSYINFO_H_BASE__
