/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/statline.h
// Purpose:     a generic wxStaticLine class used for mac before adaptation
// Author:      Vadim Zeitlin
// Created:     28.06.99
// Copyright:   (c) 1998 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_STATLINE_H_
#define _WX_GENERIC_STATLINE_H_

class wxStaticBox;

// ----------------------------------------------------------------------------
// wxStaticLine
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStaticLine : public wxStaticLineBase
{
public:
    // constructors and pseudo-constructors
    wxStaticLine() : m_statbox(NULL) { }

    wxStaticLine( wxWindow *parent,
                  wxWindowID id = wxID_ANY,
                  const wxPoint &pos = wxDefaultPosition,
                  const wxSize &size = wxDefaultSize,
                  long style = wxLI_HORIZONTAL,
                  const wxString &name = wxStaticLineNameStr )
        : m_statbox(NULL)
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = wxLI_HORIZONTAL,
                 const wxString &name = wxStaticLineNameStr );

    // it's necessary to override this wxWindow function because we
    // will want to return the main widget for m_statbox
    //
    WXWidget GetMainWidget() const;

protected:
    // we implement the static line using a static box
    wxStaticBox *m_statbox;

    DECLARE_DYNAMIC_CLASS(wxStaticLine)
};

#endif // _WX_GENERIC_STATLINE_H_

