///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dnd.h
// Purpose:     declaration of the wxDropTarget class
// Author:      Robert Roebling
// Copyright:   (c) 1998 Vadim Zeitlin, Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_DND_H_
#define _WX_GTK_DND_H_

#include "wx/icon.h"

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// this macro may be used instead for wxDropSource ctor arguments: it will use
// the icon 'name' from an XPM file under GTK, but will expand to something
// else under MSW. If you don't use it, you will have to use #ifdef in the
// application code.
#define wxDROP_ICON(name)   wxICON(name)

//-------------------------------------------------------------------------
// wxDropTarget
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDropTarget: public wxDropTargetBase
{
public:
    wxDropTarget(wxDataObject *dataObject = NULL );

    virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def);
    virtual bool OnDrop(wxCoord x, wxCoord y);
    virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def);
    virtual bool GetData();

    // Can only be called during OnXXX methods.
    wxDataFormat GetMatchingPair();

    // implementation

    GdkAtom GTKGetMatchingPair(bool quiet = false);
    wxDragResult GTKFigureOutSuggestedAction();

    void GtkRegisterWidget( GtkWidget *widget );
    void GtkUnregisterWidget( GtkWidget *widget );

    GdkDragContext     *m_dragContext;
    GtkWidget          *m_dragWidget;
    GtkSelectionData   *m_dragData;
    unsigned            m_dragTime;
    bool                m_firstMotion;     // gdk has no "gdk_drag_enter" event

    void GTKSetDragContext( GdkDragContext *dc ) { m_dragContext = dc; }
    void GTKSetDragWidget( GtkWidget *w ) { m_dragWidget = w; }
    void GTKSetDragData( GtkSelectionData *sd ) { m_dragData = sd; }
    void GTKSetDragTime(unsigned time) { m_dragTime = time; }
};

//-------------------------------------------------------------------------
// wxDropSource
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDropSource: public wxDropSourceBase
{
public:
    // constructor. set data later with SetData()
    wxDropSource( wxWindow *win = NULL,
                  const wxIcon &copy = wxNullIcon,
                  const wxIcon &move = wxNullIcon,
                  const wxIcon &none = wxNullIcon);

    // constructor for setting one data object
    wxDropSource( wxDataObject& data,
                  wxWindow *win,
                  const wxIcon &copy = wxNullIcon,
                  const wxIcon &move = wxNullIcon,
                  const wxIcon &none = wxNullIcon);

    virtual ~wxDropSource();

    // set the icon corresponding to given drag result
    void SetIcon(wxDragResult res, const wxIcon& icon)
    {
        if ( res == wxDragCopy )
            m_iconCopy = icon;
        else if ( res == wxDragMove )
            m_iconMove = icon;
        else
            m_iconNone = icon;
    }

    // start drag action
    virtual wxDragResult DoDragDrop(int flags = wxDrag_CopyOnly);

    void PrepareIcon( int action, GdkDragContext *context );

    GtkWidget       *m_widget;
    GtkWidget       *m_iconWindow;
    GdkDragContext  *m_dragContext;
    wxWindow        *m_window;

    wxDragResult     m_retValue;
    wxIcon           m_iconCopy,
                     m_iconMove,
                     m_iconNone;

    bool             m_waiting;

private:
    // common part of both ctors
    void SetIcons(const wxIcon& copy,
                  const wxIcon& move,
                  const wxIcon& none);

    // GTK implementation
    void GTKConnectDragSignals();
    void GTKDisconnectDragSignals();

};

#endif // _WX_GTK_DND_H_

