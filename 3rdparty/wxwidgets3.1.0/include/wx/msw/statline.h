/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/statline.h
// Purpose:     MSW version of wxStaticLine class
// Author:      Vadim Zeitlin
// Created:     28.06.99
// Copyright:   (c) 1998 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_STATLINE_H_
#define _WX_MSW_STATLINE_H_

// ----------------------------------------------------------------------------
// wxStaticLine
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStaticLine : public wxStaticLineBase
{
public:
    // constructors and pseudo-constructors
    wxStaticLine() { }

    wxStaticLine( wxWindow *parent,
                  wxWindowID id = wxID_ANY,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = wxLI_HORIZONTAL,
                  const wxString &name = wxStaticLineNameStr )
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxLI_HORIZONTAL,
                 const wxString &name = wxStaticLineNameStr );

    // overridden base class virtuals
    virtual bool AcceptsFocus() const { return false; }

    // usually overridden base class virtuals
    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxStaticLine);
};

#endif // _WX_MSW_STATLINE_H_


