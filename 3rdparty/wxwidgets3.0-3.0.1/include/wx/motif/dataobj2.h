///////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/dataobj2.h
// Purpose:     declaration of standard wxDataObjectSimple-derived classes
// Author:      Mattia Barbon
// Created:     27.04.03
// Copyright:   (c) 2003 Mattia Barbon
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MOTIF_DATAOBJ2_H_
#define _WX_MOTIF_DATAOBJ2_H_

// ----------------------------------------------------------------------------
// wxBitmapDataObject is a specialization of wxDataObject for bitmaps
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmapDataObject : public wxBitmapDataObjectBase
{
public:
    // ctors
    wxBitmapDataObject()
        : wxBitmapDataObjectBase() { }
    wxBitmapDataObject(const wxBitmap& bitmap)
        : wxBitmapDataObjectBase(bitmap) { }

    // implement base class pure virtuals
    // ----------------------------------
    virtual size_t GetDataSize() const;
    virtual bool GetDataHere(void *buf) const;
    virtual bool SetData(size_t len, const void *buf);

    // unhide base class virtual functions
    virtual size_t GetDataSize(const wxDataFormat& WXUNUSED(format)) const
        { return GetDataSize(); }
    virtual bool GetDataHere(const wxDataFormat& WXUNUSED(format),
                             void *buf) const
        { return GetDataHere(buf); }
    virtual bool SetData(const wxDataFormat& WXUNUSED(format),
                         size_t len, const void *buf)
        { return SetData(len, buf); }
};

#endif // _WX_MOTIF_DATAOBJ2_H_
