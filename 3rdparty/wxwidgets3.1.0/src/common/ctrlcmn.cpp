/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/ctrlcmn.cpp
// Purpose:     wxControl common interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     26.07.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CONTROLS

#include "wx/control.h"

#ifndef WX_PRECOMP
    #include "wx/dc.h"
    #include "wx/log.h"
    #include "wx/radiobut.h"
    #include "wx/statbmp.h"
    #include "wx/bitmap.h"
    #include "wx/utils.h"       // for wxStripMenuCodes()
    #include "wx/settings.h"
#endif

#include "wx/private/markupparser.h"

const char wxControlNameStr[] = "control";

// ============================================================================
// implementation
// ============================================================================

wxControlBase::~wxControlBase()
{
    // this destructor is required for Darwin
}

bool wxControlBase::Create(wxWindow *parent,
                           wxWindowID id,
                           const wxPoint &pos,
                           const wxSize &size,
                           long style,
                           const wxValidator& wxVALIDATOR_PARAM(validator),
                           const wxString &name)
{
    bool ret = wxWindow::Create(parent, id, pos, size, style, name);

#if wxUSE_VALIDATORS
    if ( ret )
        SetValidator(validator);
#endif // wxUSE_VALIDATORS

    return ret;
}

bool wxControlBase::CreateControl(wxWindowBase *parent,
                                  wxWindowID id,
                                  const wxPoint& pos,
                                  const wxSize& size,
                                  long style,
                                  const wxValidator& validator,
                                  const wxString& name)
{
    // even if it's possible to create controls without parents in some port,
    // it should surely be discouraged because it doesn't work at all under
    // Windows
    wxCHECK_MSG( parent, false, wxT("all controls must have parents") );

    if ( !CreateBase(parent, id, pos, size, style, validator, name) )
        return false;

    parent->AddChild(this);

    return true;
}

void wxControlBase::Command(wxCommandEvent& event)
{
    (void)GetEventHandler()->ProcessEvent(event);
}

void wxControlBase::InitCommandEvent(wxCommandEvent& event) const
{
    event.SetEventObject(const_cast<wxControlBase *>(this));

    // event.SetId(GetId()); -- this is usuall done in the event ctor

    switch ( m_clientDataType )
    {
        case wxClientData_Void:
            event.SetClientData(GetClientData());
            break;

        case wxClientData_Object:
            event.SetClientObject(GetClientObject());
            break;

        case wxClientData_None:
            // nothing to do
            ;
    }
}

bool wxControlBase::SetFont(const wxFont& font)
{
    InvalidateBestSize();
    return wxWindow::SetFont(font);
}

// wxControl-specific processing after processing the update event
void wxControlBase::DoUpdateWindowUI(wxUpdateUIEvent& event)
{
    // call inherited
    wxWindowBase::DoUpdateWindowUI(event);

    // update label
    if ( event.GetSetText() )
    {
        if ( event.GetText() != GetLabel() )
            SetLabel(event.GetText());
    }

    // Unfortunately we don't yet have common base class for
    // wxRadioButton, so we handle updates of radiobuttons here.
    // TODO: If once wxRadioButtonBase will exist, move this code there.
#if wxUSE_RADIOBTN
    if ( event.GetSetChecked() )
    {
        wxRadioButton *radiobtn = wxDynamicCastThis(wxRadioButton);
        if ( radiobtn )
            radiobtn->SetValue(event.GetChecked());
    }
#endif // wxUSE_RADIOBTN
}

wxSize wxControlBase::DoGetSizeFromTextSize(int WXUNUSED(xlen),
                                            int WXUNUSED(ylen)) const
{
    return wxSize(-1, -1);
}

/* static */
wxString wxControlBase::GetLabelText(const wxString& label)
{
    // we don't want strip the TABs here, just the mnemonics
    return wxStripMenuCodes(label, wxStrip_Mnemonics);
}

/* static */
wxString wxControlBase::RemoveMnemonics(const wxString& str)
{
    // we don't want strip the TABs here, just the mnemonics
    return wxStripMenuCodes(str, wxStrip_Mnemonics);
}

/* static */
wxString wxControlBase::EscapeMnemonics(const wxString& text)
{
    wxString label(text);
    label.Replace("&", "&&");
    return label;
}

/* static */
int wxControlBase::FindAccelIndex(const wxString& label, wxString *labelOnly)
{
    // the character following MNEMONIC_PREFIX is the accelerator for this
    // control unless it is MNEMONIC_PREFIX too - this allows to insert
    // literal MNEMONIC_PREFIX chars into the label
    static const wxChar MNEMONIC_PREFIX = wxT('&');

    if ( labelOnly )
    {
        labelOnly->Empty();
        labelOnly->Alloc(label.length());
    }

    int indexAccel = -1;
    for ( wxString::const_iterator pc = label.begin(); pc != label.end(); ++pc )
    {
        if ( *pc == MNEMONIC_PREFIX )
        {
            ++pc; // skip it
            if ( pc == label.end() )
                break;
            else if ( *pc != MNEMONIC_PREFIX )
            {
                if ( indexAccel == -1 )
                {
                    // remember it (-1 is for MNEMONIC_PREFIX itself
                    indexAccel = pc - label.begin() - 1;
                }
                else
                {
                    wxFAIL_MSG(wxT("duplicate accel char in control label"));
                }
            }
        }

        if ( labelOnly )
        {
            *labelOnly += *pc;
        }
    }

    return indexAccel;
}

wxBorder wxControlBase::GetDefaultBorder() const
{
    return wxBORDER_THEME;
}

/* static */ wxVisualAttributes
wxControlBase::GetCompositeControlsDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    wxVisualAttributes attrs;
    attrs.font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    attrs.colFg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    attrs.colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

    return attrs;
}

// ----------------------------------------------------------------------------
// wxControl markup support
// ----------------------------------------------------------------------------

#if wxUSE_MARKUP

/* static */
wxString wxControlBase::RemoveMarkup(const wxString& markup)
{
    return wxMarkupParser::Strip(markup);
}

bool wxControlBase::DoSetLabelMarkup(const wxString& markup)
{
    const wxString label = RemoveMarkup(markup);
    if ( label.empty() && !markup.empty() )
        return false;

    SetLabel(label);

    return true;
}

#endif // wxUSE_MARKUP

// ----------------------------------------------------------------------------
// wxControlBase - ellipsization code
// ----------------------------------------------------------------------------

#define wxELLIPSE_REPLACEMENT       wxS("...")

namespace
{

struct EllipsizeCalculator
{
    EllipsizeCalculator(const wxString& s, const wxDC& dc,
                        int maxFinalWidthPx, int replacementWidthPx)
        : 
          m_initialCharToRemove(0),
          m_nCharsToRemove(0),
          m_outputNeedsUpdate(true),
          m_str(s),
          m_dc(dc),
          m_maxFinalWidthPx(maxFinalWidthPx),
          m_replacementWidthPx(replacementWidthPx)
    {
        m_isOk = dc.GetPartialTextExtents(s, m_charOffsetsPx);
        wxASSERT( m_charOffsetsPx.GetCount() == s.length() );
    }

    bool IsOk() const { return m_isOk; }

    bool EllipsizationNotNeeded() const
    {
        // NOTE: charOffsetsPx[n] is the width in pixels of the first n characters (with the last one INCLUDED)
        //       thus charOffsetsPx[len-1] is the total width of the string
        return m_charOffsetsPx.Last() <= m_maxFinalWidthPx;
    }

    void Init(size_t initialCharToRemove, size_t nCharsToRemove)
    {
        m_initialCharToRemove = initialCharToRemove;
        m_nCharsToRemove = nCharsToRemove;
    }

    void RemoveFromEnd()
    {
        m_nCharsToRemove++;
    }

    void RemoveFromStart()
    {
        m_initialCharToRemove--;
        m_nCharsToRemove++;
    }

    size_t GetFirstRemoved() const { return m_initialCharToRemove; }
    size_t GetLastRemoved() const { return m_initialCharToRemove + m_nCharsToRemove - 1; }

    const wxString& GetEllipsizedText()
    {
        if ( m_outputNeedsUpdate )
        {
            wxASSERT(m_initialCharToRemove <= m_str.length() - 1);  // see valid range for initialCharToRemove above
            wxASSERT(m_nCharsToRemove >= 1 && m_nCharsToRemove <= m_str.length() - m_initialCharToRemove);  // see valid range for nCharsToRemove above

            // erase m_nCharsToRemove characters after m_initialCharToRemove (included);
            // e.g. if we have the string "foobar" (len = 6)
            //                               ^
            //                               \--- m_initialCharToRemove = 2
            //      and m_nCharsToRemove = 2, then we get "foar"
            m_output = m_str;
            m_output.replace(m_initialCharToRemove, m_nCharsToRemove, wxELLIPSE_REPLACEMENT);
        }

        return m_output;
    }

    bool IsShortEnough()
    {
        if ( m_nCharsToRemove == m_str.length() )
            return true; // that's the best we could do

        // Width calculation using partial extents is just an inaccurate
        // estimate: partial extents have sub-pixel precision and are rounded
        // by GetPartialTextExtents(); replacing part of the string with "..."
        // may change them too thanks to changes in ligatures, kerning etc.
        //
        // The correct algorithm would be to call GetTextExtent() in every step
        // of ellipsization, but that would be too expensive, especially when
        // the difference is just a few pixels. So we use partial extents to
        // estimate string width and only verify it with GetTextExtent() when
        // it looks good.

        int estimatedWidth = m_replacementWidthPx; // length of "..."

        // length of text before the removed part:
        if ( m_initialCharToRemove > 0 )
            estimatedWidth += m_charOffsetsPx[m_initialCharToRemove - 1];

        // length of text after the removed part:

        if ( GetLastRemoved() < m_str.length() )
           estimatedWidth += m_charOffsetsPx.Last() - m_charOffsetsPx[GetLastRemoved()];

        if ( estimatedWidth > m_maxFinalWidthPx )
            return false;

        return m_dc.GetTextExtent(GetEllipsizedText()).GetWidth() <= m_maxFinalWidthPx;
    }

    // calculation state:

    // REMEMBER: indexes inside the string have a valid range of [0;len-1] if not otherwise constrained
    //           lengths/counts of characters (e.g. nCharsToRemove) have a
    //           valid range of [0;len] if not otherwise constrained
    // NOTE: since this point we know we have for sure a non-empty string from which we need
    //       to remove _at least_ one character (thus nCharsToRemove below is constrained to be >= 1)

    // index of first character to erase, valid range is [0;len-1]:
    size_t m_initialCharToRemove;
    // how many chars do we need to erase? valid range is [0;len-m_initialCharToRemove]
    size_t m_nCharsToRemove;

    wxString m_output;
    bool m_outputNeedsUpdate;

    // inputs:
    wxString m_str;
    const wxDC& m_dc;
    int m_maxFinalWidthPx;
    int m_replacementWidthPx;
    wxArrayInt m_charOffsetsPx;

    bool m_isOk;
};

} // anonymous namespace

/* static and protected */
wxString wxControlBase::DoEllipsizeSingleLine(const wxString& curLine, const wxDC& dc,
                                              wxEllipsizeMode mode, int maxFinalWidthPx,
                                              int replacementWidthPx)
{
    wxASSERT_MSG(replacementWidthPx > 0, "Invalid parameters");
    wxASSERT_LEVEL_2_MSG(!curLine.Contains('\n'),
                         "Use Ellipsize() instead!");

    wxASSERT_MSG( mode != wxELLIPSIZE_NONE, "shouldn't be called at all then" );

    // NOTE: this function assumes that any mnemonic/tab character has already
    //       been handled if it was necessary to handle them (see Ellipsize())

    if (maxFinalWidthPx <= 0)
        return wxEmptyString;

    size_t len = curLine.length();
    if (len <= 1 )
        return curLine;

    EllipsizeCalculator calc(curLine, dc, maxFinalWidthPx, replacementWidthPx);

    if ( !calc.IsOk() )
        return curLine;

    if ( calc.EllipsizationNotNeeded() )
        return curLine;

    // let's compute the range of characters to remove depending on the ellipsization mode:
    switch (mode)
    {
        case wxELLIPSIZE_START:
            {
                calc.Init(0, 1);
                while ( !calc.IsShortEnough() )
                    calc.RemoveFromEnd();

                // always show at least one character of the string:
                if ( calc.m_nCharsToRemove == len )
                    return wxString(wxELLIPSE_REPLACEMENT) + curLine[len-1];

                break;
            }

        case wxELLIPSIZE_MIDDLE:
            {
                // NOTE: the following piece of code works also when len == 1

                // start the removal process from the middle of the string
                // i.e. separe the string in three parts:
                // - the first one to preserve, valid range [0;initialCharToRemove-1] or the empty range if initialCharToRemove==0
                // - the second one to remove, valid range [initialCharToRemove;endCharToRemove]
                // - the third one to preserve, valid range [endCharToRemove+1;len-1] or the empty range if endCharToRemove==len-1
                // NOTE: empty range != range [0;0] since the range [0;0] contains 1 character (the zero-th one)!

                calc.Init(len/2, 0);

                bool removeFromStart = true;

                while ( !calc.IsShortEnough() )
                {
                    const bool canRemoveFromStart = calc.GetFirstRemoved() > 0;
                    const bool canRemoveFromEnd = calc.GetLastRemoved() < len - 1;

                    if ( !canRemoveFromStart && !canRemoveFromEnd )
                    {
                        // we need to remove all the characters of the string!
                        break;
                    }

                    // Remove from the beginning in even steps and from the end
                    // in odd steps, unless we exhausted one side already:
                    removeFromStart = !removeFromStart;
                    if ( removeFromStart && !canRemoveFromStart )
                        removeFromStart = false;
                    else if ( !removeFromStart && !canRemoveFromEnd )
                        removeFromStart = true;

                    if ( removeFromStart )
                        calc.RemoveFromStart();
                    else
                        calc.RemoveFromEnd();
                }

                // Always show at least one character of the string.
                // Additionally, if there's only one character left, prefer
                // "a..." to "...a":
                if ( calc.m_nCharsToRemove == len ||
                     calc.m_nCharsToRemove == len - 1 )
                {
                    return curLine[0] + wxString(wxELLIPSE_REPLACEMENT);
                }
            }
            break;

        case wxELLIPSIZE_END:
            {
                calc.Init(len - 1, 1);
                while ( !calc.IsShortEnough() )
                    calc.RemoveFromStart();

                // always show at least one character of the string:
                if ( calc.m_nCharsToRemove == len )
                    return curLine[0] + wxString(wxELLIPSE_REPLACEMENT);

                break;
            }

        case wxELLIPSIZE_NONE:
        default:
            wxFAIL_MSG("invalid ellipsize mode");
            return curLine;
    }

    return calc.GetEllipsizedText();
}

/* static */
wxString wxControlBase::Ellipsize(const wxString& label, const wxDC& dc,
                                  wxEllipsizeMode mode, int maxFinalWidth,
                                  int flags)
{
    wxString ret;

    // these cannot be cached between different Ellipsize() calls as they can
    // change because of e.g. a font change; however we calculate them only once
    // when ellipsizing multiline labels:
    int replacementWidth = dc.GetTextExtent(wxELLIPSE_REPLACEMENT).GetWidth();

    // NB: we must handle correctly labels with newlines:
    wxString curLine;
    for ( wxString::const_iterator pc = label.begin(); ; ++pc )
    {
        if ( pc == label.end() || *pc == wxS('\n') )
        {
            curLine = DoEllipsizeSingleLine(curLine, dc, mode, maxFinalWidth,
                                            replacementWidth);

            // add this (ellipsized) row to the rest of the label
            ret << curLine;
            if ( pc == label.end() )
                break;

            ret << *pc;
            curLine.clear();
        }
        // we need to remove mnemonics from the label for correct calculations
        else if ( *pc == wxS('&') && (flags & wxELLIPSIZE_FLAGS_PROCESS_MNEMONICS) )
        {
            // pc+1 is safe: at worst we'll be at end()
            wxString::const_iterator next = pc + 1;
            if ( next != label.end() && *next == wxS('&') )
                curLine += wxS('&');          // && becomes &
            //else: remove this ampersand
        }
        // we need also to expand tabs to properly calc their size
        else if ( *pc == wxS('\t') && (flags & wxELLIPSIZE_FLAGS_EXPAND_TABS) )
        {
            // Windows natively expands the TABs to 6 spaces. Do the same:
            curLine += wxS("      ");
        }
        else
        {
            curLine += *pc;
        }
    }

    return ret;
}

// ----------------------------------------------------------------------------
// wxStaticBitmap
// ----------------------------------------------------------------------------

#if wxUSE_STATBMP

wxStaticBitmapBase::~wxStaticBitmapBase()
{
    // this destructor is required for Darwin
}

wxSize wxStaticBitmapBase::DoGetBestSize() const
{
    wxSize best;
    wxBitmap bmp = GetBitmap();
    if ( bmp.IsOk() )
        best = bmp.GetScaledSize();
    else
        // this is completely arbitrary
        best = wxSize(16, 16);
    CacheBestSize(best);
    return best;
}

#endif // wxUSE_STATBMP

#endif // wxUSE_CONTROLS
