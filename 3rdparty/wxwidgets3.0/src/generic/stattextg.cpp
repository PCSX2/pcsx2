/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/stattextg.cpp
// Purpose:     wxGenericStaticText
// Author:      Marcin Wojdyr
// Created:     2008-06-26
// Copyright:   Marcin Wojdyr
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STATTEXT

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/validate.h"
#endif

#include "wx/generic/stattextg.h"

#if wxUSE_MARKUP
    #include "wx/generic/private/markuptext.h"
#endif // wxUSE_MARKUP

IMPLEMENT_DYNAMIC_CLASS(wxGenericStaticText, wxStaticTextBase)


bool wxGenericStaticText::Create(wxWindow *parent,
                                 wxWindowID id,
                                 const wxString &label,
                                 const wxPoint &pos,
                                 const wxSize &size,
                                 long style,
                                 const wxString &name)
{
    if ( !wxControl::Create(parent, id, pos, size, style,
                            wxDefaultValidator, name) )
        return false;

    SetLabel(label);
    SetInitialSize(size);
    Connect(wxEVT_PAINT, wxPaintEventHandler(wxGenericStaticText::OnPaint));
    return true;
}

wxGenericStaticText::~wxGenericStaticText()
{
#if wxUSE_MARKUP
    delete m_markupText;
#endif // wxUSE_MARKUP
}

void wxGenericStaticText::DoDrawLabel(wxDC& dc, const wxRect& rect)
{
#if wxUSE_MARKUP
    if ( m_markupText )
        m_markupText->Render(dc, rect, wxMarkupText::Render_ShowAccels);
    else
#endif // wxUSE_MARKUP
        dc.DrawLabel(m_label, rect, GetAlignment(), m_mnemonic);
}

void wxGenericStaticText::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    wxRect rect = GetClientRect();
    if ( IsEnabled() )
    {
        dc.SetTextForeground(
                       wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    }
    else // paint disabled text
    {
        // draw shadow of the text
        dc.SetTextForeground(
                       wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
        wxRect rectShadow = rect;
        rectShadow.Offset(1, 1);
        DoDrawLabel(dc, rectShadow);
        dc.SetTextForeground(
                       wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    }
    DoDrawLabel(dc, rect);
}


wxSize wxGenericStaticText::DoGetBestClientSize() const
{
    wxClientDC dc(wxConstCast(this, wxGenericStaticText));

#if wxUSE_MARKUP
    if ( m_markupText )
        return m_markupText->Measure(dc);
#endif // wxUSE_MARKUP

    return dc.GetMultiLineTextExtent(GetLabel());
}

void wxGenericStaticText::SetLabel(const wxString& label)
{
    wxControl::SetLabel(label);
    DoSetLabel(GetEllipsizedLabel());
    if ( !HasFlag(wxST_NO_AUTORESIZE) && !IsEllipsized() )
        InvalidateBestSize();

#if wxUSE_MARKUP
    if ( m_markupText )
    {
        delete m_markupText;
        m_markupText = NULL;
    }
#endif // wxUSE_MARKUP

    Refresh();
}

void wxGenericStaticText::DoSetLabel(const wxString& label)
{
    m_mnemonic = FindAccelIndex(label, &m_label);
}

#if wxUSE_MARKUP

bool wxGenericStaticText::DoSetLabelMarkup(const wxString& markup)
{
    if ( !wxStaticTextBase::DoSetLabelMarkup(markup) )
        return false;

    if ( !m_markupText )
        m_markupText = new wxMarkupText(markup);
    else
        m_markupText->SetMarkup(markup);

    if ( !HasFlag(wxST_NO_AUTORESIZE) )
        InvalidateBestSize();

    Refresh();

    return true;
}

#endif // wxUSE_MARKUP

bool wxGenericStaticText::SetFont(const wxFont &font)
{
    if ( !wxControl::SetFont(font) )
        return false;
    if ( !HasFlag(wxST_NO_AUTORESIZE) )
        InvalidateBestSize();
    Refresh();
    return true;
}

void wxGenericStaticText::DoSetSize(int x, int y, int width, int height,
                                    int sizeFlags)
{
    wxStaticTextBase::DoSetSize(x, y, width, height, sizeFlags);
    UpdateLabel();
}


#endif // wxUSE_STATTEXT
