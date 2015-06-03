/////////////////////////////////////////////////////////////////////////////
// Name:        wx/imagpnm.h
// Purpose:     wxImage PNM handler
// Author:      Sylvain Bougnoux
// Copyright:   (c) Sylvain Bougnoux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGPNM_H_
#define _WX_IMAGPNM_H_

#include "wx/image.h"

//-----------------------------------------------------------------------------
// wxPNMHandler
//-----------------------------------------------------------------------------

#if wxUSE_PNM
class WXDLLIMPEXP_CORE wxPNMHandler : public wxImageHandler
{
public:
    inline wxPNMHandler()
    {
        m_name = wxT("PNM file");
        m_extension = wxT("pnm");
        m_altExtensions.Add(wxT("ppm"));
        m_altExtensions.Add(wxT("pgm"));
        m_altExtensions.Add(wxT("pbm"));
        m_type = wxBITMAP_TYPE_PNM;
        m_mime = wxT("image/pnm");
    }

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=true, int index=-1 ) wxOVERRIDE;
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=true ) wxOVERRIDE;
protected:
    virtual bool DoCanRead( wxInputStream& stream ) wxOVERRIDE;
#endif

private:
    wxDECLARE_DYNAMIC_CLASS(wxPNMHandler);
};
#endif


#endif
  // _WX_IMAGPNM_H_

