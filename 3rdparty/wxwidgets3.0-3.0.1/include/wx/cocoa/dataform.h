///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/dataform.h
// Purpose:     declaration of the wxDataFormat class
// Author:      David Elliott <dfe@cox.net>
// Modified by:
// Created:     2003/07/23
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_DATAFORM_H__
#define __WX_COCOA_DATAFORM_H__

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
    void     SetId(const wxString& WXUNUSED(pId)) { /* TODO */ }

private:
    unsigned int                    m_uFormat;
}; // end of CLASS wxDataFormat

#endif // __WX_COCOA_DATAFORM_H__
