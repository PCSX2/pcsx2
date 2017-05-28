/////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/statline.h
// Purpose:     wxStaticLine class for wxUniversal
// Author:      Vadim Zeitlin
// Created:     28.06.99
// Copyright:   (c) 1999 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_STATLINE_H_
#define _WX_UNIV_STATLINE_H_

class WXDLLIMPEXP_CORE wxStaticLine : public wxStaticLineBase
{
public:
    // constructors and pseudo-constructors
    wxStaticLine() { }

    wxStaticLine(wxWindow *parent,
                 const wxPoint &pos,
                 wxCoord length,
                 long style = wxLI_HORIZONTAL)
    {
        Create(parent, wxID_ANY, pos,
               style & wxLI_VERTICAL ? wxSize(wxDefaultCoord, length)
                                     : wxSize(length, wxDefaultCoord),
               style);
    }

    wxStaticLine(wxWindow *parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = wxLI_HORIZONTAL,
                 const wxString &name = wxStaticLineNameStr )
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = wxLI_HORIZONTAL,
                const wxString &name = wxStaticLineNameStr );

protected:
    virtual void DoDraw(wxControlRenderer *renderer) wxOVERRIDE;

private:
    wxDECLARE_DYNAMIC_CLASS(wxStaticLine);
};

#endif // _WX_UNIV_STATLINE_H_

