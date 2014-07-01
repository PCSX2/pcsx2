/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/stattext.h
// Purpose:     wxStaticText class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATTEXT_H_
#define _WX_STATTEXT_H_

class WXDLLIMPEXP_CORE wxStaticText: public wxStaticTextBase
{
    DECLARE_DYNAMIC_CLASS(wxStaticText)

public:
    wxStaticText() { }

    wxStaticText(wxWindow *parent, wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxStaticTextNameStr)
    {
        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxString& name = wxStaticTextNameStr);

    // implementation
    // --------------

    // operations
    virtual bool ProcessCommand(wxCommandEvent& WXUNUSED(event))
    {
        return false;
    }

    virtual void SetLabel(const wxString& label);

    // Get the widget that corresponds to the label
    // (for font setting, label setting etc.)
    virtual WXWidget GetLabelWidget() const
        { return m_labelWidget; }

    virtual void DoSetLabel(const wxString& str);
    virtual wxString DoGetLabel() const;

    virtual wxSize DoGetBestSize() const;
protected:
    WXWidget              m_labelWidget;
};

#endif
// _WX_STATTEXT_H_
