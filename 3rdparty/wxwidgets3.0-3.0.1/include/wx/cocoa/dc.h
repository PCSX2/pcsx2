/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/dc.h
// Purpose:     wxDC
// Author:      David Elliott
// Modified by:
// Created:     2003/04/01
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_DC_H__
#define __WX_COCOA_DC_H__

DECLARE_WXCOCOA_OBJC_CLASS(NSAffineTransform);

#include "wx/dc.h"

class WXDLLIMPEXP_FWD_CORE wxCocoaDCImpl;
WX_DECLARE_LIST(wxCocoaDCImpl, wxCocoaDCStack);

//=========================================================================
// wxDC
//=========================================================================
class WXDLLIMPEXP_CORE wxCocoaDCImpl: public wxDCImpl
{
    DECLARE_ABSTRACT_CLASS(wxCocoaDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxCocoaDCImpl);
//-------------------------------------------------------------------------
// Initialization
//-------------------------------------------------------------------------
public:
    wxCocoaDCImpl(wxDC *owner);
    virtual ~wxCocoaDCImpl();

//-------------------------------------------------------------------------
// wxCocoa specifics
//-------------------------------------------------------------------------
public:
    static void CocoaInitializeTextSystem();
    static void CocoaShutdownTextSystem();
    static WX_NSTextStorage sm_cocoaNSTextStorage;
    static WX_NSLayoutManager sm_cocoaNSLayoutManager;
    static WX_NSTextContainer sm_cocoaNSTextContainer;
    // Create a simple Wx to Bounds transform (just flip the coordinate system)
    static WX_NSAffineTransform CocoaGetWxToBoundsTransform(bool isFlipped, float height);
protected:
// DC stack
    static wxCocoaDCStack sm_cocoaDCStack;
    virtual bool CocoaLockFocus();
    virtual bool CocoaUnlockFocus();
    bool CocoaUnwindStackAndTakeFocus();
    inline bool CocoaTakeFocus()
    {
        wxCocoaDCStack::compatibility_iterator node = sm_cocoaDCStack.GetFirst();
        if(node && (node->GetData() == this))
            return true;
        return CocoaUnwindStackAndTakeFocus();
    }
    void CocoaUnwindStackAndLoseFocus();
// DC flipping/transformation
    void CocoaApplyTransformations();
    void CocoaUnapplyTransformations();
    WX_NSAffineTransform m_cocoaWxToBoundsTransform;
// Get bounds rect (for Clear())
    // note: we use void * to mean NSRect * so that we can avoid
    // putting NSRect in the headers.
    virtual bool CocoaGetBounds(void *rectData);
// Blitting
    virtual bool CocoaDoBlitOnFocusedDC(wxCoord xdest, wxCoord ydest,
        wxCoord width, wxCoord height, wxCoord xsrc, wxCoord ysrc,
        wxRasterOperationMode logicalFunc, bool useMask, wxCoord xsrcMask, wxCoord ysrcMask);
//-------------------------------------------------------------------------
// Implementation
//-------------------------------------------------------------------------
public:
    // implement base class pure virtuals
    // ----------------------------------

    virtual void Clear();

    virtual bool StartDoc( const wxString& WXUNUSED(message) ) { return true; }
    virtual void EndDoc(void) {}

    virtual void StartPage(void) {}
    virtual void EndPage(void) {}

    virtual void SetFont(const wxFont& font);
    virtual void SetPen(const wxPen& pen);
    virtual void SetBrush(const wxBrush& brush);
    virtual void SetBackground(const wxBrush& brush);
    virtual void SetBackgroundMode(int mode) { m_backgroundMode = mode; }
    virtual void SetPalette(const wxPalette& palette);

    virtual void DestroyClippingRegion();

    virtual wxCoord GetCharHeight() const;
    virtual wxCoord GetCharWidth() const;
    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *x, wxCoord *y,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const;

    virtual bool CanDrawBitmap() const;
    virtual bool CanGetTextExtent() const;
    virtual int GetDepth() const;
    virtual wxSize GetPPI() const;

    virtual void SetMapMode(wxMappingMode mode);
    virtual void SetUserScale(double x, double y);

    virtual void SetLogicalScale(double x, double y);
    virtual void SetLogicalOrigin(wxCoord x, wxCoord y);
    virtual void SetDeviceOrigin(wxCoord x, wxCoord y);
    virtual void SetAxisOrientation(bool xLeftRight, bool yBottomUp);
    virtual void SetLogicalFunction(wxRasterOperationMode function);

    virtual void SetTextForeground(const wxColour& colour) ;
    virtual void SetTextBackground(const wxColour& colour) ;

    virtual void ComputeScaleAndOrigin();
protected:
    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE);

    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const;

    virtual void DoDrawPoint(wxCoord x, wxCoord y);
    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);

    virtual void DoDrawArc(wxCoord x1, wxCoord y1,
                           wxCoord x2, wxCoord y2,
                           wxCoord xc, wxCoord yc);

    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                   double sa, double ea);

    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
                                        wxCoord width, wxCoord height,
                                        double radius);
    virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);

    virtual void DoCrossHair(wxCoord x, wxCoord y);

    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y);
    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
                              bool useMask = false);

    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y);
    virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
                                   double angle);

    virtual bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                        wxDC *source, wxCoord xsrc, wxCoord ysrc,
                        int rop = wxCOPY, bool useMask = false, wxCoord xsrcMask = -1, wxCoord ysrcMask = -1);

    // this is gnarly - we can't even call this function DoSetClippingRegion()
    // because of virtual function hiding
    virtual void DoSetDeviceClippingRegion(const wxRegion& region);
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
                                     wxCoord width, wxCoord height);

    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetSizeMM(int* width, int* height) const;

    virtual void DoDrawLines(int n, const wxPoint points[],
                             wxCoord xoffset, wxCoord yoffset);
    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE);
};

#endif // __WX_COCOA_DC_H__
