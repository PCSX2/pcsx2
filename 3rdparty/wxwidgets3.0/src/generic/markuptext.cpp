///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/markuptext.cpp
// Purpose:     wxMarkupText implementation
// Author:      Vadim Zeitlin
// Created:     2011-02-21
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MARKUP

#ifndef WX_PRECOMP
    #include "wx/gdicmn.h"
    #include "wx/control.h"
    #include "wx/dc.h"
#endif // WX_PRECOMP

#include "wx/generic/private/markuptext.h"

#include "wx/private/markupparserattr.h"

namespace
{

// ----------------------------------------------------------------------------
// wxMarkupParserMeasureOutput: measure the extends of a markup string.
// ----------------------------------------------------------------------------

class wxMarkupParserMeasureOutput : public wxMarkupParserAttrOutput
{
public:
    // Initialize the base class with the font to use. As we don't care about
    // colours (which don't affect the text measurements), don't bother to
    // specify them at all.
    wxMarkupParserMeasureOutput(wxDC& dc, int *visibleHeight)
        : wxMarkupParserAttrOutput(dc.GetFont(), wxColour(), wxColour()),
          m_dc(dc),
          m_visibleHeight(visibleHeight)
    {
        if ( visibleHeight )
            *visibleHeight = 0;
    }

    const wxSize& GetSize() const { return m_size; }


    virtual void OnText(const wxString& text_)
    {
        const wxString text(wxControl::RemoveMnemonics(text_));

        // TODO-MULTILINE-MARKUP: Must use GetMultiLineTextExtent().
        const wxSize size = m_dc.GetTextExtent(text);

        m_size.x += size.x;
        if ( size.y > m_size.y )
            m_size.y = size.y;

        if ( m_visibleHeight )
        {
            wxFontMetrics tm = m_dc.GetFontMetrics();
            int visibleHeight = tm.ascent - tm.internalLeading;
            if ( *m_visibleHeight < visibleHeight )
                *m_visibleHeight = visibleHeight;
        }
    }

    virtual void OnAttrStart(const Attr& attr)
    {
        m_dc.SetFont(attr.font);
    }

    virtual void OnAttrEnd(const Attr& WXUNUSED(attr))
    {
        m_dc.SetFont(GetFont());
    }

private:
    wxDC& m_dc;

    // The values that we compute.
    wxSize m_size;
    int * const m_visibleHeight;    // may be NULL

    wxDECLARE_NO_COPY_CLASS(wxMarkupParserMeasureOutput);
};

// ----------------------------------------------------------------------------
// wxMarkupParserRenderOutput: render a markup string.
// ----------------------------------------------------------------------------

class wxMarkupParserRenderOutput : public wxMarkupParserAttrOutput
{
public:
    // Notice that the bottom of rectangle passed to our ctor is used as the
    // baseline for the text we draw, i.e. it needs to be adjusted to exclude
    // descent by the caller.
    wxMarkupParserRenderOutput(wxDC& dc,
                               const wxRect& rect,
                               int flags)
        : wxMarkupParserAttrOutput(dc.GetFont(),
                                   dc.GetTextForeground(),
                                   wxColour()),
          m_dc(dc),
          m_rect(rect),
          m_flags(flags)
    {
        m_pos = m_rect.x;

        // We don't initialize the base class initial text background colour to
        // the valid value because we want to be able to detect when we revert
        // to the "absence of background colour" and set the background mode to
        // be transparent in OnAttrStart() below. But do remember it to be able
        // to restore it there later -- this doesn't affect us as the text
        // background isn't used anyhow when the background mode is transparent
        // but it might affect the caller if it sets the background mode to
        // opaque and draws some text after using us.
        m_origTextBackground = dc.GetTextBackground();
    }

    virtual void OnText(const wxString& text_)
    {
        wxString text;
        int indexAccel = wxControl::FindAccelIndex(text_, &text);
        if ( !(m_flags & wxMarkupText::Render_ShowAccels) )
            indexAccel = wxNOT_FOUND;

        // Adjust the position (unfortunately we need to do this manually as
        // there is no notion of current text position in wx API) rectangle to
        // ensure that all text segments use the same baseline (as there is
        // nothing equivalent to Windows SetTextAlign(TA_BASELINE) neither).
        wxRect rect(m_rect);
        rect.x = m_pos;

        int descent;
        m_dc.GetTextExtent(text, &rect.width, &rect.height, &descent);
        rect.height -= descent;
        rect.y += m_rect.height - rect.height;

        wxRect bounds;
        m_dc.DrawLabel(text, wxBitmap(),
                       rect, wxALIGN_LEFT | wxALIGN_TOP,
                       indexAccel,
                       &bounds);

        // TODO-MULTILINE-MARKUP: Must update vertical position too.
        m_pos += bounds.width;
    }

    virtual void OnAttrStart(const Attr& attr)
    {
        m_dc.SetFont(attr.font);
        if ( attr.foreground.IsOk() )
            m_dc.SetTextForeground(attr.foreground);

        if ( attr.background.IsOk() )
        {
            // Setting the background colour is not enough, we must also change
            // the mode to ensure that it is actually used.
            m_dc.SetBackgroundMode(wxSOLID);
            m_dc.SetTextBackground(attr.background);
        }
    }

    virtual void OnAttrEnd(const Attr& attr)
    {
        // We always restore the font because we always change it...
        m_dc.SetFont(GetFont());

        // ...but we only need to restore the colours if we had changed them.
        if ( attr.foreground.IsOk() )
            m_dc.SetTextForeground(GetAttr().foreground);

        if ( attr.background.IsOk() )
        {
            wxColour background = GetAttr().background;
            if ( !background.IsOk() )
            {
                // Invalid background colour indicates that the background
                // should actually be made transparent and in this case the
                // actual value of background colour doesn't matter but we also
                // restore it just in case, see comment in the ctor.
                m_dc.SetBackgroundMode(wxTRANSPARENT);
                background = m_origTextBackground;
            }

            m_dc.SetTextBackground(background);
        }
    }

private:
    wxDC& m_dc;
    const wxRect m_rect;
    const int m_flags;

    wxColour m_origTextBackground;

    // Current horizontal text output position.
    //
    // TODO-MULTILINE-MARKUP: Must keep vertical position too.
    int m_pos;

    wxDECLARE_NO_COPY_CLASS(wxMarkupParserRenderOutput);
};

} // anonymous namespace

// ============================================================================
// wxMarkupText implementation
// ============================================================================

wxSize wxMarkupText::Measure(wxDC& dc, int *visibleHeight) const
{
    wxMarkupParserMeasureOutput out(dc, visibleHeight);
    wxMarkupParser parser(out);
    if ( !parser.Parse(m_markup) )
    {
        wxFAIL_MSG( "Invalid markup" );
        return wxDefaultSize;
    }

    return out.GetSize();
}

void wxMarkupText::Render(wxDC& dc, const wxRect& rect, int flags)
{
    // We want to center the above-baseline parts of the letter vertically, so
    // we use the visible height and not the total height (which includes
    // descent and internal leading) here.
    int visibleHeight;
    wxRect rectText(rect.GetPosition(), Measure(dc, &visibleHeight));
    rectText.height = visibleHeight;

    wxMarkupParserRenderOutput out(dc, rectText.CentreIn(rect), flags);
    wxMarkupParser parser(out);
    parser.Parse(m_markup);
}

#endif // wxUSE_MARKUP
