///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dataform.h
// Purpose:     declaration of the wxDataFormat class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.10.99 (extracted from gtk/dataobj.h)
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_DATAFORM_H
#define _WX_GTK_DATAFORM_H

class WXDLLIMPEXP_CORE wxDataFormat
{
public:
    // the clipboard formats under GDK are GdkAtoms
    typedef GdkAtom NativeFormat;

    wxDataFormat();
    wxDataFormat( wxDataFormatId type );
    wxDataFormat( NativeFormat format );

    // we have to provide all the overloads to allow using strings instead of
    // data formats (as a lot of existing code does)
    wxDataFormat( const wxString& id ) { InitFromString(id); }
    wxDataFormat( const char *id ) { InitFromString(id); }
    wxDataFormat( const wchar_t *id ) { InitFromString(id); }
    wxDataFormat( const wxCStrData& id ) { InitFromString(id); }

    wxDataFormat& operator=(const wxDataFormat& format)
    {
        if (&format != this)
        {
            m_type = format.m_type;
            m_format = format.m_format;
        }
        return *this;
    }
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
    void SetType( wxDataFormatId type );

private:
    // common part of ctors from format name
    void InitFromString(const wxString& id);

    wxDataFormatId   m_type;
    NativeFormat     m_format;

    void PrepareFormats();
};

#endif // _WX_GTK_DATAFORM_H
