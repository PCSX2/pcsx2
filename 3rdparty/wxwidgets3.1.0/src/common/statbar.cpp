///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/statbar.cpp
// Purpose:     wxStatusBarBase implementation
// Author:      Vadim Zeitlin
// Modified by: Francesco Montorsi
// Created:     14.10.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#if wxUSE_STATUSBAR

#include "wx/statusbr.h"

#ifndef WX_PRECOMP
    #include "wx/frame.h"
#endif //WX_PRECOMP

const char wxStatusBarNameStr[] = "statusBar";

// ============================================================================
// wxStatusBarPane implementation
// ============================================================================

bool wxStatusBarPane::SetText(const wxString& text)
{
    if ( text == m_text )
        return false;

    /*
        If we have a message to restore on the stack, we update it to
        correspond to the current one so that a sequence of calls such as

        1. SetStatusText("foo")
        2. PushStatusText("bar")
        3. SetStatusText("new foo")
        4. PopStatusText()

        doesn't overwrite the "new foo" which should be shown at the end with
        the old value "foo". This would be unexpected and hard to avoid,
        especially when PushStatusText() is used internally by wxWidgets
        without knowledge of the user program, as it is for showing the menu
        and toolbar help strings.

        By updating the top of the stack we ensure that the next call to
        PopStatusText() basically becomes a NOP without breaking the balance
        between the calls to push and pop as we would have done if we really
        called PopStatusText() here.
     */
    if ( !m_arrStack.empty() )
    {
        m_arrStack.back() = text;
    }

    m_text = text;

    return true;
}

bool wxStatusBarPane::PushText(const wxString& text)
{
    // save the currently shown text
    m_arrStack.push_back(m_text);

    // and update the new one if necessary
    if ( text == m_text )
        return false;

    m_text = text;

    return true;
}

bool wxStatusBarPane::PopText()
{
    wxCHECK_MSG( !m_arrStack.empty(), false, "no status message to pop" );

    const wxString text = m_arrStack.back();

    m_arrStack.pop_back();

    if ( text == m_text )
        return false;

    m_text = text;

    return true;
}

// ============================================================================
// wxStatusBarBase implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxStatusBar, wxWindow);

#include "wx/arrimpl.cpp" // This is a magic incantation which must be done!
WX_DEFINE_EXPORTED_OBJARRAY(wxStatusBarPaneArray)


// ----------------------------------------------------------------------------
// ctor/dtor
// ----------------------------------------------------------------------------

wxStatusBarBase::wxStatusBarBase()
{
    m_bSameWidthForAllPanes = true;
}

wxStatusBarBase::~wxStatusBarBase()
{
    // notify the frame that it doesn't have a status bar any longer to avoid
    // dangling pointers
    wxFrame *frame = wxDynamicCast(GetParent(), wxFrame);
    if ( frame && frame->GetStatusBar() == this )
        frame->SetStatusBar(NULL);
}

// ----------------------------------------------------------------------------
// field widths
// ----------------------------------------------------------------------------

void wxStatusBarBase::SetFieldsCount(int number, const int *widths)
{
    wxCHECK_RET( number > 0, wxT("invalid field number in SetFieldsCount") );

    if ( (size_t)number > m_panes.GetCount() )
    {
        wxStatusBarPane newPane;

        // add more entries with the default style and zero width
        // (this will be set later)
        for (size_t i = m_panes.GetCount(); i < (size_t)number; ++i)
            m_panes.Add(newPane);
    }
    else if ( (size_t)number < m_panes.GetCount() )
    {
        // remove entries in excess
        m_panes.RemoveAt(number, m_panes.GetCount()-number);
    }

    // SetStatusWidths will automatically refresh
    SetStatusWidths(number, widths);
}

void wxStatusBarBase::SetStatusWidths(int WXUNUSED_UNLESS_DEBUG(n),
                                    const int widths[])
{
    wxASSERT_MSG( (size_t)n == m_panes.GetCount(), wxT("field number mismatch") );

    if (widths == NULL)
    {
        // special value meaning: override explicit pane widths and make them all
        // of the same size
        m_bSameWidthForAllPanes = true;
    }
    else
    {
        for ( size_t i = 0; i < m_panes.GetCount(); i++ )
            m_panes[i].SetWidth(widths[i]);

        m_bSameWidthForAllPanes = false;
    }

    // update the display after the widths changed
    Refresh();
}

void wxStatusBarBase::SetStatusStyles(int WXUNUSED_UNLESS_DEBUG(n),
                                    const int styles[])
{
    wxCHECK_RET( styles, wxT("NULL pointer in SetStatusStyles") );

    wxASSERT_MSG( (size_t)n == m_panes.GetCount(), wxT("field number mismatch") );

    for ( size_t i = 0; i < m_panes.GetCount(); i++ )
        m_panes[i].SetStyle(styles[i]);

    // update the display after the widths changed
    Refresh();
}

wxArrayInt wxStatusBarBase::CalculateAbsWidths(wxCoord widthTotal) const
{
    wxArrayInt widths;

    if ( m_bSameWidthForAllPanes )
    {
        // Default: all fields have the same width. This is not always
        // possible to do exactly (if widthTotal is not divisible by
        // m_panes.GetCount()) - if that happens, we distribute the extra
        // pixels among all fields:
        int widthToUse = widthTotal;

        for ( size_t i = m_panes.GetCount(); i > 0; i-- )
        {
            // divide the unassigned width evently between the
            // not yet processed fields:
            int w = widthToUse / i;
            widths.Add(w);
            widthToUse -= w;
        }
    }
    else // do not override explicit pane widths
    {
        // calculate the total width of all the fixed width fields and the
        // total number of var field widths counting with multiplicity
        size_t nTotalWidth = 0,
            nVarCount = 0,
            i;

        for ( i = 0; i < m_panes.GetCount(); i++ )
        {
            if ( m_panes[i].GetWidth() >= 0 )
                nTotalWidth += m_panes[i].GetWidth();
            else
                nVarCount += -m_panes[i].GetWidth();
        }

        // the amount of extra width we have per each var width field
        int widthExtra = widthTotal - nTotalWidth;

        // do fill the array
        for ( i = 0; i < m_panes.GetCount(); i++ )
        {
            if ( m_panes[i].GetWidth() >= 0 )
                widths.Add(m_panes[i].GetWidth());
            else
            {
                int nVarWidth = widthExtra > 0 ? (widthExtra * (-m_panes[i].GetWidth())) / nVarCount : 0;
                nVarCount += m_panes[i].GetWidth();
                widthExtra -= nVarWidth;
                widths.Add(nVarWidth);
            }
        }
    }

    return widths;
}

// ----------------------------------------------------------------------------
// setting/getting status text
// ----------------------------------------------------------------------------

void wxStatusBarBase::SetStatusText(const wxString& text, int number)
{
    wxCHECK_RET( (unsigned)number < m_panes.size(),
                    "invalid status bar field index" );

    if ( m_panes[number].SetText(text) )
        DoUpdateStatusText(number);
}

wxString wxStatusBarBase::GetStatusText(int number) const
{
    wxCHECK_MSG( (unsigned)number < m_panes.size(), wxString(),
                    "invalid status bar field index" );

    return m_panes[number].GetText();
}

void wxStatusBarBase::SetEllipsizedFlag(int number, bool isEllipsized)
{
    wxCHECK_RET( (unsigned)number < m_panes.size(),
                    "invalid status bar field index" );

    m_panes[number].SetIsEllipsized(isEllipsized);
}

// ----------------------------------------------------------------------------
// pushing/popping status text
// ----------------------------------------------------------------------------

void wxStatusBarBase::PushStatusText(const wxString& text, int number)
{
    wxCHECK_RET( (unsigned)number < m_panes.size(),
                    "invalid status bar field index" );

    if ( m_panes[number].PushText(text) )
        DoUpdateStatusText(number);
}

void wxStatusBarBase::PopStatusText(int number)
{
    wxCHECK_RET( (unsigned)number < m_panes.size(),
                    "invalid status bar field index" );

    if ( m_panes[number].PopText() )
        DoUpdateStatusText(number);
}

#endif // wxUSE_STATUSBAR
