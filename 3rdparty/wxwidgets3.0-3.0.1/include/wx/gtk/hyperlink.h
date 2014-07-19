/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/hyperlink.h
// Purpose:     Hyperlink control
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/2/2007
// Copyright:   (c) 2007 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKHYPERLINKCTRL_H_
#define _WX_GTKHYPERLINKCTRL_H_

#include "wx/generic/hyperlink.h"

// ----------------------------------------------------------------------------
// wxHyperlinkCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxHyperlinkCtrl : public wxGenericHyperlinkCtrl
{
public:
    // Default constructor (for two-step construction).
    wxHyperlinkCtrl() { }

    // Constructor.
    wxHyperlinkCtrl(wxWindow *parent,
                    wxWindowID id,
                    const wxString& label, const wxString& url,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxHL_DEFAULT_STYLE,
                    const wxString& name = wxHyperlinkCtrlNameStr)
    {
        (void)Create(parent, id, label, url, pos, size, style, name);
    }

    // Creation function (for two-step construction).
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label, const wxString& url,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxHL_DEFAULT_STYLE,
                const wxString& name = wxHyperlinkCtrlNameStr);


    // get/set
    virtual wxColour GetHoverColour() const;
    virtual void SetHoverColour(const wxColour &colour);

    virtual wxColour GetNormalColour() const;
    virtual void SetNormalColour(const wxColour &colour);

    virtual wxColour GetVisitedColour() const;
    virtual void SetVisitedColour(const wxColour &colour);

    virtual wxString GetURL() const;
    virtual void SetURL(const wxString &url);

    virtual void SetLabel(const wxString &label);

protected:
    virtual wxSize DoGetBestSize() const;
    virtual wxSize DoGetBestClientSize() const;

    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    DECLARE_DYNAMIC_CLASS(wxHyperlinkCtrl)
};

#endif // _WX_GTKHYPERLINKCTRL_H_
