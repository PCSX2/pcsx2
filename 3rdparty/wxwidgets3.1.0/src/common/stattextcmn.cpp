/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/stattextcmn.cpp
// Purpose:     common (to all ports) wxStaticText functions
// Author:      Vadim Zeitlin, Francesco Montorsi
// Created:     2007-01-07 (extracted from dlgcmn.cpp)
// Copyright:   (c) 1999-2006 Vadim Zeitlin
//              (c) 2007 Francesco Montorsi
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

#if wxUSE_STATTEXT

#ifndef WX_PRECOMP
    #include "wx/stattext.h"
    #include "wx/button.h"
    #include "wx/dcclient.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/settings.h"
    #include "wx/sizer.h"
    #include "wx/containr.h"
#endif

#include "wx/textwrapper.h"

#include "wx/private/markupparser.h"

extern WXDLLEXPORT_DATA(const char) wxStaticTextNameStr[] = "staticText";

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxStaticTextStyle )
wxBEGIN_FLAGS( wxStaticTextStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxST_NO_AUTORESIZE)
wxFLAGS_MEMBER(wxALIGN_LEFT)
wxFLAGS_MEMBER(wxALIGN_RIGHT)
wxFLAGS_MEMBER(wxALIGN_CENTRE)
wxEND_FLAGS( wxStaticTextStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxStaticText, wxControl, "wx/stattext.h");

wxBEGIN_PROPERTIES_TABLE(wxStaticText)
wxPROPERTY( Label,wxString, SetLabel, GetLabel, wxString(), 0 /*flags*/, \
           wxT("Helpstring"), wxT("group"))
wxPROPERTY_FLAGS( WindowStyle, wxStaticTextStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxStaticText)

wxCONSTRUCTOR_6( wxStaticText, wxWindow*, Parent, wxWindowID, Id, \
                wxString, Label, wxPoint, Position, wxSize, Size, long, WindowStyle )


// ----------------------------------------------------------------------------
// wxTextWrapper
// ----------------------------------------------------------------------------

void wxTextWrapper::Wrap(wxWindow *win, const wxString& text, int widthMax)
{
    wxString line;

    wxString::const_iterator lastSpace = text.end();
    wxString::const_iterator lineStart = text.begin();
    for ( wxString::const_iterator p = lineStart; ; ++p )
    {
        if ( IsStartOfNewLine() )
        {
            OnNewLine();

            lastSpace = text.end();
            line.clear();
            lineStart = p;
        }

        if ( p == text.end() || *p == wxT('\n') )
        {
            DoOutputLine(line);

            if ( p == text.end() )
                break;
        }
        else // not EOL
        {
            if ( *p == wxT(' ') )
                lastSpace = p;

            line += *p;

            if ( widthMax >= 0 && lastSpace != text.end() )
            {
                int width;
                win->GetTextExtent(line, &width, NULL);

                if ( width > widthMax )
                {
                    // remove the last word from this line
                    line.erase(lastSpace - lineStart, p + 1 - lineStart);
                    DoOutputLine(line);

                    // go back to the last word of this line which we didn't
                    // output yet
                    p = lastSpace;
                }
            }
            //else: no wrapping at all or impossible to wrap
        }
    }
}


// ----------------------------------------------------------------------------
// wxLabelWrapper: helper class for wxStaticTextBase::Wrap()
// ----------------------------------------------------------------------------

class wxLabelWrapper : public wxTextWrapper
{
public:
    void WrapLabel(wxWindow *text, int widthMax)
    {
        m_text.clear();
        Wrap(text, text->GetLabel(), widthMax);
        text->SetLabel(m_text);
    }

protected:
    virtual void OnOutputLine(const wxString& line) wxOVERRIDE
    {
        m_text += line;
    }

    virtual void OnNewLine() wxOVERRIDE
    {
        m_text += wxT('\n');
    }

private:
    wxString m_text;
};


// ----------------------------------------------------------------------------
// wxStaticTextBase
// ----------------------------------------------------------------------------

void wxStaticTextBase::Wrap(int width)
{
    wxLabelWrapper wrapper;
    wrapper.WrapLabel(this, width);
}

void wxStaticTextBase::AutoResizeIfNecessary()
{
    // adjust the size of the window to fit to the label unless autoresizing is
    // disabled
    if ( !HasFlag(wxST_NO_AUTORESIZE) )
    {
        DoSetSize(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, wxDefaultCoord,
                  wxSIZE_AUTO_WIDTH | wxSIZE_AUTO_HEIGHT);
    }
}

// ----------------------------------------------------------------------------
// wxStaticTextBase - generic implementation for wxST_ELLIPSIZE_* support
// ----------------------------------------------------------------------------

void wxStaticTextBase::UpdateLabel()
{
    if (!IsEllipsized())
        return;

    wxString newlabel = GetEllipsizedLabel();

    // we need to touch the "real" label (i.e. the text set inside the control,
    // using port-specific functions) instead of the string returned by GetLabel().
    //
    // In fact, we must be careful not to touch the original label passed to
    // SetLabel() otherwise GetLabel() will behave in a strange way to the user
    // (e.g. returning a "Ver...ing" instead of "Very long string") !
    if (newlabel == DoGetLabel())
        return;
    DoSetLabel(newlabel);
}

wxString wxStaticTextBase::GetEllipsizedLabel() const
{
    // this function should be used only by ports which do not support
    // ellipsis in static texts: we first remove markup (which cannot
    // be handled safely by Ellipsize()) and then ellipsize the result.

    wxString ret(m_labelOrig);

    if (IsEllipsized())
        ret = Ellipsize(ret);

    return ret;
}

wxString wxStaticTextBase::Ellipsize(const wxString& label) const
{
    wxSize sz(GetSize());
    if (sz.GetWidth() < 2 || sz.GetHeight() < 2)
    {
        // the size of this window is not valid (yet)
        return label;
    }

    wxClientDC dc(const_cast<wxStaticTextBase*>(this));
    dc.SetFont(GetFont());

    wxEllipsizeMode mode;
    if ( HasFlag(wxST_ELLIPSIZE_START) )
        mode = wxELLIPSIZE_START;
    else if ( HasFlag(wxST_ELLIPSIZE_MIDDLE) )
        mode = wxELLIPSIZE_MIDDLE;
    else if ( HasFlag(wxST_ELLIPSIZE_END) )
        mode = wxELLIPSIZE_END;
    else
    {
        wxFAIL_MSG( "should only be called if have one of wxST_ELLIPSIZE_XXX" );

        return label;
    }

    return wxControl::Ellipsize(label, dc, mode, sz.GetWidth());
}

#endif // wxUSE_STATTEXT
