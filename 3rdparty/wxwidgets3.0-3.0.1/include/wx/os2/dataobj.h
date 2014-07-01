///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dataobj.h
// Purpose:     declaration of the wxDataObject
// Author:      Stefan Csomor
// Modified by:
// Created:     10/21/99
// Copyright:   (c) 1998, 1999 Vadim Zeitlin, Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_DATAOBJ_H_
#define _WX_OS2_DATAOBJ_H_

// ----------------------------------------------------------------------------
// wxDataObject is the same as wxDataObjectBase under wxGTK
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDataObject : public wxDataObjectBase
{
public:
    wxDataObject();
    virtual ~wxDataObject();

    virtual bool IsSupportedFormat( const wxDataFormat& eFormat
                                   ,Direction           eDir = Get
                                  ) const
    {
        return(IsSupported( eFormat
                           ,eDir
                          ));
    }

    PDRAGITEM GetInterface(void) const {return m_pDataObject;}
private:
    PDRAGITEM                       m_pDataObject;
};

#endif // _WX_OS2_DATAOBJ_H_
