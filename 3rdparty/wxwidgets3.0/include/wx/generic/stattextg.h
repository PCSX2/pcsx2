/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/stattextg.h
// Purpose:     wxGenericStaticText header
// Author:      Marcin Wojdyr
// Created:     2008-06-26
// Copyright:   Marcin Wojdyr
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_STATTEXTG_H_
#define _WX_GENERIC_STATTEXTG_H_

// prevent it from including the platform-specific wxStaticText declaration as
// this is not going to compile if it derives from wxGenericStaticText defined
// below (currently this is only the case in wxUniv but it could also happen
// with other ports)
#define wxNO_PORT_STATTEXT_INCLUDE
#include "wx/stattext.h"
#undef wxNO_PORT_STATTEXT_INCLUDE

class WXDLLIMPEXP_CORE wxGenericStaticText : public wxStaticTextBase
{
public:
    wxGenericStaticText() { Init(); }

    wxGenericStaticText(wxWindow *parent,
                 wxWindowID id,
                 const wxString& label,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxString& name = wxStaticTextNameStr)
    {
        Init();

        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxStaticTextNameStr);

    virtual ~wxGenericStaticText();


    // overridden base class virtual methods
    virtual void SetLabel(const wxString& label);
    virtual bool SetFont(const wxFont &font);

protected:
    virtual wxSize DoGetBestClientSize() const;

    virtual wxString DoGetLabel() const { return m_label; }
    virtual void DoSetLabel(const wxString& label);

    void DoSetSize(int x, int y, int width, int height, int sizeFlags);

#if wxUSE_MARKUP
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP

private:
    void Init()
    {
#if wxUSE_MARKUP
        m_markupText = NULL;
#endif // wxUSE_MARKUP
    }

    void OnPaint(wxPaintEvent& event);

    void DoDrawLabel(wxDC& dc, const wxRect& rect);

    // These fields are only used if m_markupText == NULL.
    wxString m_label;
    int m_mnemonic;

#if wxUSE_MARKUP
    class wxMarkupText *m_markupText;
#endif // wxUSE_MARKUP

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxGenericStaticText)
};

#endif // _WX_GENERIC_STATTEXTG_H_

