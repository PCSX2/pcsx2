/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/minifram.h
// Purpose:     wxMiniFrame class
// Author:      Robert Roebling
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_MINIFRAME_H_
#define _WX_GTK_MINIFRAME_H_

#include "wx/bitmap.h"
#include "wx/frame.h"

//-----------------------------------------------------------------------------
// wxMiniFrame
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMiniFrame: public wxFrame
{
    DECLARE_DYNAMIC_CLASS(wxMiniFrame)

public:
    wxMiniFrame() {}
    wxMiniFrame(wxWindow *parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxCAPTION | wxRESIZE_BORDER,
            const wxString& name = wxFrameNameStr)
    {
        Create(parent, id, title, pos, size, style, name);
    }
    ~wxMiniFrame();

    bool Create(wxWindow *parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxCAPTION | wxRESIZE_BORDER,
            const wxString& name = wxFrameNameStr);

    virtual void SetTitle( const wxString &title );

protected:
    virtual void DoSetSizeHints( int minW, int minH,
                                 int maxW, int maxH,
                                 int incW, int incH );
    virtual void DoGetClientSize(int* width, int* height) const;

 // implementation
public:
    bool   m_isDragging;
    int    m_oldX,m_oldY;
    int    m_diffX,m_diffY;
    wxBitmap  m_closeButton;
    int m_miniEdge;
    int m_miniTitle;
};

#endif // _WX_GTK_MINIFRAME_H_
