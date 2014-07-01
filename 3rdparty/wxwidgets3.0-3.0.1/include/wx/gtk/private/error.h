///////////////////////////////////////////////////////////////////////////////
// Name:        gtk/private/error.h
// Purpose:     Wrapper around GError.
// Author:      Vadim Zeitlin
// Created:     2012-07-25
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_ERROR_H_
#define _WX_GTK_PRIVATE_ERROR_H_

// ----------------------------------------------------------------------------
// wxGtkError wraps GError and releases it automatically.
// ----------------------------------------------------------------------------

// Create an object of this class and pass the result of its Out() method to a
// function taking "GError**", then use GetMessage() if the function returned
// false.
class wxGtkError
{
public:
    wxGtkError() { m_error = NULL; }
    ~wxGtkError() { if ( m_error ) g_error_free(m_error); }

    GError** Out()
    {
        // This would result in a GError leak.
        wxASSERT_MSG( !m_error, wxS("Can't reuse the same object.") );

        return &m_error;
    }

    wxString GetMessage() const
    {
        return wxString::FromUTF8(m_error->message);
    }

private:
    GError* m_error;

    wxDECLARE_NO_COPY_CLASS(wxGtkError);
};

#endif // _WX_GTK_PRIVATE_ERROR_H_
