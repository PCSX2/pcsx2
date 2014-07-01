/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dcmemory.h
// Purpose:     wxMemoryDC class
// Author:      David Webster
// Modified by:
// Created:     09/09/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCMEMORY_H_
#define _WX_DCMEMORY_H_

#include "wx/dcmemory.h"
#include "wx/os2/dc.h"

class WXDLLIMPEXP_CORE wxMemoryDCImpl: public wxPMDCImpl
{
public:
    wxMemoryDCImpl( wxMemoryDC *owner );
    wxMemoryDCImpl( wxMemoryDC *owner, wxBitmap& bitmap );
    wxMemoryDCImpl( wxMemoryDC *owner, wxDC* pDC); // Create compatible DC

    // override some base class virtuals
    virtual void DoGetSize(int* pWidth, int* pHeight) const;
    virtual void DoSelect(const wxBitmap& bitmap);

    virtual wxBitmap DoGetAsBitmap(const wxRect* subrect) const
    { return subrect == NULL ? GetSelectedBitmap() : GetSelectedBitmap().GetSubBitmap(*subrect);}

protected:
    // create DC compatible with the given one or screen if dc == NULL
    bool CreateCompatible(wxDC* pDC);

    // initialize the newly created DC
    void Init(void);
private:
    DECLARE_CLASS(wxMemoryDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxMemoryDCImpl);
}; // end of CLASS wxMemoryDCImpl

#endif
    // _WX_DCMEMORY_H_
