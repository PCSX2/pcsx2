/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/dcmemory.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKDCMEMORYH__
#define __GTKDCMEMORYH__

#include "wx/dcmemory.h"
#include "wx/gtk1/dcclient.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxMemoryDCImpl;

//-----------------------------------------------------------------------------
// wxMemoryDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMemoryDCImpl : public wxWindowDCImpl
{
public:
    wxMemoryDCImpl(wxMemoryDC *owner)
        : wxWindowDCImpl(owner)
    {
        Init();
    }

    wxMemoryDCImpl(wxMemoryDC *owner, wxBitmap& bitmap)
        : wxWindowDCImpl(owner)
    {
        Init();

        DoSelect(bitmap);
    }

    wxMemoryDCImpl(wxMemoryDC *owner, wxDC *dc);
    virtual ~wxMemoryDCImpl();

    virtual void DoSelect(const wxBitmap& bitmap);
    virtual void DoGetSize( int *width, int *height ) const;

    // these get reimplemented for mono-bitmaps to behave
    // more like their Win32 couterparts. They now interpret
    // wxWHITE, wxWHITE_BRUSH and wxWHITE_PEN as drawing 0
    // and everything else as drawing 1.
    virtual void SetPen( const wxPen &pen );
    virtual void SetBrush( const wxBrush &brush );
    virtual void SetBackground( const wxBrush &brush );
    virtual void SetTextForeground( const wxColour &col );
    virtual void SetTextBackground( const wxColour &col );

    // implementation
    wxBitmap  m_selected;

private:
    void Init();

    DECLARE_DYNAMIC_CLASS(wxMemoryDCImpl)
};

#endif // __GTKDCMEMORYH__

