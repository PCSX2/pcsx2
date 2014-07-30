/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dcmemory.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_DCMEMORY_H_
#define _WX_GTK_DCMEMORY_H_

#include "wx/dcmemory.h"
#include "wx/gtk/dcclient.h"

//-----------------------------------------------------------------------------
// wxMemoryDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMemoryDCImpl : public wxWindowDCImpl
{
public:
    wxMemoryDCImpl( wxMemoryDC *owner );
    wxMemoryDCImpl( wxMemoryDC *owner, wxBitmap& bitmap );
    wxMemoryDCImpl( wxMemoryDC *owner, wxDC *dc );

    virtual ~wxMemoryDCImpl();

    // these get reimplemented for mono-bitmaps to behave
    // more like their Win32 couterparts. They now interpret
    // wxWHITE, wxWHITE_BRUSH and wxWHITE_PEN as drawing 0
    // and everything else as drawing 1.
    virtual void SetPen( const wxPen &pen );
    virtual void SetBrush( const wxBrush &brush );
    virtual void SetBackground( const wxBrush &brush );
    virtual void SetTextForeground( const wxColour &col );
    virtual void SetTextBackground( const wxColour &col );

    // overridden from wxDCImpl
    virtual void DoGetSize( int *width, int *height ) const;
    virtual wxBitmap DoGetAsBitmap(const wxRect *subrect) const;
    virtual void* GetHandle() const;
    
    // overridden for wxMemoryDC Impl
    virtual void DoSelect(const wxBitmap& bitmap);

    virtual const wxBitmap& GetSelectedBitmap() const;
    virtual wxBitmap& GetSelectedBitmap();

private:
    wxBitmap  m_selected;

    void Init();

    DECLARE_ABSTRACT_CLASS(wxMemoryDCImpl)
};

#endif
    // _WX_GTK_DCMEMORY_H_

