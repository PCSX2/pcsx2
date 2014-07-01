///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dataform.h
// Purpose:     declaration of the wxDataFormat class
// Author:      Stefan Csomor
// Modified by:
// Created:     10/21/99
// Copyright:   (c) 1999 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_DATAFORM_H
#define _WX_OS2_DATAFORM_H

class wxDataFormat
{
public:
    wxDataFormat(unsigned int uFormat = wxDF_INVALID) { m_uFormat = uFormat; }
    wxDataFormat(const wxString& zFormat) { SetId(zFormat); }

    wxDataFormat& operator=(unsigned int uFormat) { m_uFormat = uFormat; return(*this); }
    wxDataFormat& operator=(const wxDataFormat& rFormat) {m_uFormat = rFormat.m_uFormat; return(*this); }

    //
    // Comparison (must have both versions)
    //
    bool operator==(wxDataFormatId eFormat) const { return (m_uFormat == (unsigned int)eFormat); }
    bool operator!=(wxDataFormatId eFormat) const { return (m_uFormat != (unsigned int)eFormat); }
    bool operator==(const wxDataFormat& rFormat) const { return (m_uFormat == rFormat.m_uFormat); }
    bool operator!=(const wxDataFormat& rFormat) const { return (m_uFormat != rFormat.m_uFormat); }
         operator unsigned int(void) const { return m_uFormat; }

    unsigned int GetFormatId(void) const { return (unsigned int)m_uFormat; }
    unsigned int GetType(void) const { return (unsigned int)m_uFormat; }

    bool IsStandard(void) const;

    void SetType(unsigned int uType){ m_uFormat = uType; }

    //
    // String ids are used for custom types - this SetId() must be used for
    // application-specific formats
    //
    wxString GetId(void) const;
    void     SetId(const wxString& pId);

private:
    unsigned int                    m_uFormat;
}; // end of CLASS wxDataFormat

#endif // _WX_GTK_DATAFORM_H
