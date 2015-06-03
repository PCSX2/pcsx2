/////////////////////////////////////////////////////////////////////////////
// Name:        wx/imagtiff.h
// Purpose:     wxImage TIFF handler
// Author:      Robert Roebling
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGTIFF_H_
#define _WX_IMAGTIFF_H_

#include "wx/defs.h"

//-----------------------------------------------------------------------------
// wxTIFFHandler
//-----------------------------------------------------------------------------

#if wxUSE_LIBTIFF

#include "wx/image.h"
#include "wx/versioninfo.h"

// defines for wxImage::SetOption
#define wxIMAGE_OPTION_TIFF_BITSPERSAMPLE               wxString(wxT("BitsPerSample"))
#define wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL             wxString(wxT("SamplesPerPixel"))
#define wxIMAGE_OPTION_TIFF_COMPRESSION                 wxString(wxT("Compression"))
#define wxIMAGE_OPTION_TIFF_PHOTOMETRIC                 wxString(wxT("Photometric"))
#define wxIMAGE_OPTION_TIFF_IMAGEDESCRIPTOR             wxString(wxT("ImageDescriptor"))

// for backwards compatibility
#define wxIMAGE_OPTION_BITSPERSAMPLE               wxIMAGE_OPTION_TIFF_BITSPERSAMPLE
#define wxIMAGE_OPTION_SAMPLESPERPIXEL             wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL
#define wxIMAGE_OPTION_COMPRESSION                 wxIMAGE_OPTION_TIFF_COMPRESSION
#define wxIMAGE_OPTION_IMAGEDESCRIPTOR             wxIMAGE_OPTION_TIFF_IMAGEDESCRIPTOR

class WXDLLIMPEXP_CORE wxTIFFHandler: public wxImageHandler
{
public:
    wxTIFFHandler();

    static wxVersionInfo GetLibraryVersionInfo();

#if wxUSE_STREAMS
    virtual bool LoadFile( wxImage *image, wxInputStream& stream, bool verbose=true, int index=-1 ) wxOVERRIDE;
    virtual bool SaveFile( wxImage *image, wxOutputStream& stream, bool verbose=true ) wxOVERRIDE;

protected:
    virtual int DoGetImageCount( wxInputStream& stream ) wxOVERRIDE;
    virtual bool DoCanRead( wxInputStream& stream ) wxOVERRIDE;
#endif

private:
    wxDECLARE_DYNAMIC_CLASS(wxTIFFHandler);
};

#endif // wxUSE_LIBTIFF

#endif // _WX_IMAGTIFF_H_

