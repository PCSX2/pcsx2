/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/dcmemory.h
// Purpose:     wxMemoryDCImpl class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCMEMORY_H_
#define _WX_DCMEMORY_H_

#include "wx/motif/dcclient.h"

class WXDLLIMPEXP_CORE wxMemoryDCImpl : public wxWindowDCImpl
{
public:
    wxMemoryDCImpl(wxMemoryDC *owner) : wxWindowDCImpl(owner) { Init(); }
    wxMemoryDCImpl(wxMemoryDC *owner, wxBitmap& bitmap)
        : wxWindowDCImpl(owner)
    {
        Init();
        DoSelect(bitmap);
    }

    wxMemoryDCImpl(wxMemoryDC *owner, wxDC *dc);
    virtual ~wxMemoryDCImpl();

    virtual void DoGetSize( int *width, int *height ) const;
    virtual void DoSelect(const wxBitmap& bitmap);

private:
    friend class wxPaintDC;

    void Init();

    wxBitmap m_bitmap;

    DECLARE_DYNAMIC_CLASS(wxMemoryDCImpl)
};

#endif
// _WX_DCMEMORY_H_
