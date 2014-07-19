///////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/dataform.h
// Purpose:     declaration of the wxDataFormat class
// Author:      Robert Roebling
// Modified by:
// Created:     19.10.99 (extracted from motif/dataobj.h)
// Copyright:   (c) 1999 Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_X11_DATAFORM_H
#define _WX_X11_DATAFORM_H

class WXDLLIMPEXP_CORE wxDataFormat
{
public:
    // the clipboard formats under Xt are Atoms
    typedef Atom NativeFormat;

    wxDataFormat();
    wxDataFormat( wxDataFormatId type );
    wxDataFormat( const wxString &id );
    wxDataFormat( NativeFormat format );

    wxDataFormat& operator=(NativeFormat format)
        { SetId(format); return *this; }

    // comparison (must have both versions)
    bool operator==(NativeFormat format) const
        { return m_format == (NativeFormat)format; }
    bool operator!=(NativeFormat format) const
        { return m_format != (NativeFormat)format; }
    bool operator==(wxDataFormatId format) const
        { return m_type == (wxDataFormatId)format; }
    bool operator!=(wxDataFormatId format) const
        { return m_type != (wxDataFormatId)format; }

    // explicit and implicit conversions to NativeFormat which is one of
    // standard data types (implicit conversion is useful for preserving the
    // compatibility with old code)
    NativeFormat GetFormatId() const { return m_format; }
    operator NativeFormat() const { return m_format; }

    void SetId( NativeFormat format );

    // string ids are used for custom types - this SetId() must be used for
    // application-specific formats
    wxString GetId() const;
    void SetId( const wxString& id );

    // implementation
    wxDataFormatId GetType() const;

private:
    wxDataFormatId   m_type;
    NativeFormat     m_format;

    void PrepareFormats();
    void SetType( wxDataFormatId type );
};


#endif // _WX_X11_DATAFORM_H

