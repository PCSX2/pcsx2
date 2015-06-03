///////////////////////////////////////////////////////////////////////////////
// Name:        wx/textwrapper.h
// Purpose:     declaration of wxTextWrapper class
// Author:      Vadim Zeitlin
// Created:     2009-05-31 (extracted from dlgcmn.cpp via wx/private/stattext.h)
// Copyright:   (c) 1999, 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTWRAPPER_H_
#define _WX_TEXTWRAPPER_H_

#include "wx/window.h"

// ----------------------------------------------------------------------------
// wxTextWrapper
// ----------------------------------------------------------------------------

// this class is used to wrap the text on word boundary: wrapping is done by
// calling OnStartLine() and OnOutputLine() functions
class WXDLLIMPEXP_CORE wxTextWrapper
{
public:
    wxTextWrapper() { m_eol = false; }

    // win is used for getting the font, text is the text to wrap, width is the
    // max line width or -1 to disable wrapping
    void Wrap(wxWindow *win, const wxString& text, int widthMax);

    // we don't need it, but just to avoid compiler warnings
    virtual ~wxTextWrapper() { }

protected:
    // line may be empty
    virtual void OnOutputLine(const wxString& line) = 0;

    // called at the start of every new line (except the very first one)
    virtual void OnNewLine() { }

private:
    // call OnOutputLine() and set m_eol to true
    void DoOutputLine(const wxString& line)
    {
        OnOutputLine(line);

        m_eol = true;
    }

    // this function is a destructive inspector: when it returns true it also
    // resets the flag to false so calling it again wouldn't return true any
    // more
    bool IsStartOfNewLine()
    {
        if ( !m_eol )
            return false;

        m_eol = false;

        return true;
    }


    bool m_eol;

    wxDECLARE_NO_COPY_CLASS(wxTextWrapper);
};

#if wxUSE_STATTEXT

#include "wx/sizer.h"
#include "wx/stattext.h"

// A class creating a sizer with one static text per line of text. Creation of
// the controls used for each line can be customized by overriding
// OnCreateLine() function.
//
// This class is currently private to wxWidgets and used only by wxDialog
// itself. We may make it public later if there is sufficient interest.
class wxTextSizerWrapper : public wxTextWrapper
{
public:
    wxTextSizerWrapper(wxWindow *win)
    {
        m_win = win;
        m_hLine = 0;
    }

    wxSizer *CreateSizer(const wxString& text, int widthMax)
    {
        m_sizer = new wxBoxSizer(wxVERTICAL);
        Wrap(m_win, text, widthMax);
        return m_sizer;
    }

    wxWindow *GetParent() const { return m_win; }

protected:
    virtual wxWindow *OnCreateLine(const wxString& line)
    {
        return new wxStaticText(m_win, wxID_ANY,
                                wxControl::EscapeMnemonics(line));
    }

    virtual void OnOutputLine(const wxString& line) wxOVERRIDE
    {
        if ( !line.empty() )
        {
            m_sizer->Add(OnCreateLine(line));
        }
        else // empty line, no need to create a control for it
        {
            if ( !m_hLine )
                m_hLine = m_win->GetCharHeight();

            m_sizer->Add(5, m_hLine);
        }
    }

private:
    wxWindow *m_win;
    wxSizer *m_sizer;
    int m_hLine;
};

#endif // wxUSE_STATTEXT

#endif // _WX_TEXTWRAPPER_H_

