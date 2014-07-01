/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/tooltip.h
// Purpose:     wxToolTip class
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKTOOLTIP_H_
#define _WX_GTKTOOLTIP_H_

#include "wx/string.h"
#include "wx/object.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxWindow;

//-----------------------------------------------------------------------------
// wxToolTip
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxToolTip : public wxObject
{
public:
    wxToolTip( const wxString &tip );

    // globally change the tooltip parameters
    static void Enable( bool flag );
    static void SetDelay( long msecs );
        // set the delay after which the tooltip disappears or how long the tooltip remains visible
    static void SetAutoPop(long msecs);
        // set the delay between subsequent tooltips to appear
    static void SetReshow(long msecs);

    // get/set the tooltip text
    void SetTip( const wxString &tip );
    wxString GetTip() const { return m_text; }

    wxWindow *GetWindow() const { return m_window; }

    // Implementation
    void GTKSetWindow(wxWindow* win);
    static void GTKApply(GtkWidget* widget, const char* tip);

private:
    wxString     m_text;
    wxWindow    *m_window;

    DECLARE_ABSTRACT_CLASS(wxToolTip)
};

#endif // _WX_GTKTOOLTIP_H_
