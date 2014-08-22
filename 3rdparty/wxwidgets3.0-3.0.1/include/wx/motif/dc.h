/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/dc.h
// Purpose:     wxMotifDCImpl class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DC_H_
#define _WX_DC_H_

#include "wx/dc.h"

// ----------------------------------------------------------------------------
// wxMotifDCImpl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMotifDCImpl : public wxDCImpl
{
public:
    wxMotifDCImpl(wxDC *owner);

    virtual wxSize GetPPI() const;

protected:
    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y);
    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
                              bool useMask = false);

    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
                                     wxCoord width, wxCoord height);
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetSizeMM(int* width, int* height) const;

public:
    // implementation
    wxCoord XDEV2LOG(wxCoord x) const       { return DeviceToLogicalX(x); }
    wxCoord XDEV2LOGREL(wxCoord x) const    { return DeviceToLogicalXRel(x); }
    wxCoord YDEV2LOG(wxCoord y) const       { return DeviceToLogicalY(y); }
    wxCoord YDEV2LOGREL(wxCoord y) const    { return DeviceToLogicalYRel(y); }
    wxCoord XLOG2DEV(wxCoord x) const       { return LogicalToDeviceX(x); }
    wxCoord XLOG2DEVREL(wxCoord x) const    { return LogicalToDeviceXRel(x); }
    wxCoord YLOG2DEV(wxCoord y) const       { return LogicalToDeviceY(y); }
    wxCoord YLOG2DEVREL(wxCoord y) const    { return LogicalToDeviceYRel(y); }

    // Without device translation, for backing pixmap purposes
    wxCoord XLOG2DEV_2(wxCoord x) const
    {
        return wxRound((double)(x - m_logicalOriginX) * m_scaleX) * m_signX;
    }
    wxCoord YLOG2DEV_2(wxCoord y) const
    {
        return wxRound((double)(y - m_logicalOriginY) * m_scaleY) * m_signY;
    }

    DECLARE_DYNAMIC_CLASS(wxMotifDCImpl)
};

#endif // _WX_DC_H_
