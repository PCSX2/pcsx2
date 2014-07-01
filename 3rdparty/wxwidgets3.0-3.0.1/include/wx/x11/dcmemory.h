/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/dcmemory.h
// Purpose:     wxMemoryDC class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCMEMORY_H_
#define _WX_DCMEMORY_H_

#include "wx/dc.h"
#include "wx/dcmemory.h"
#include "wx/x11/dcclient.h"

class WXDLLIMPEXP_CORE wxMemoryDCImpl : public wxWindowDCImpl
{
public:
    wxMemoryDCImpl( wxDC* owner );
    wxMemoryDCImpl( wxDC* owner, wxBitmap& bitmap);
    wxMemoryDCImpl( wxDC* owner, wxDC *dc );
    virtual ~wxMemoryDCImpl();

    virtual const wxBitmap& GetSelectedBitmap() const;
    virtual wxBitmap& GetSelectedBitmap();

    // implementation
    wxBitmap  m_selected;

protected:
    virtual void DoGetSize( int *width, int *height ) const;
    virtual void DoSelect(const wxBitmap& bitmap);

private:
    void Init();

private:
    DECLARE_CLASS(wxMemoryDCImpl)
};

#endif
// _WX_DCMEMORY_H_
