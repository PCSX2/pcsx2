/////////////////////////////////////////////////////////////////////////////
// Name:        wx/imaggif.h
// Purpose:     wxImage GIF handler
// Author:      Vaclav Slavik, Guillermo Rodriguez Garcia, Gershon Elber, Troels K
// Copyright:   (c) 1999-2011 Vaclav Slavik, Guillermo Rodriguez Garcia, Gershon Elber, Troels K
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGGIF_H_
#define _WX_IMAGGIF_H_

#include "wx/image.h"


//-----------------------------------------------------------------------------
// wxGIFHandler
//-----------------------------------------------------------------------------

#if wxUSE_GIF

#define wxIMAGE_OPTION_GIF_COMMENT wxT("GifComment")

struct wxRGB;
struct GifHashTableType;
class WXDLLIMPEXP_FWD_CORE wxImageArray; // anidecod.h

class WXDLLIMPEXP_CORE wxGIFHandler : public wxImageHandler
{
public:
    inline wxGIFHandler()
    {
        m_name = wxT("GIF file");
        m_extension = wxT("gif");
        m_type = wxBITMAP_TYPE_GIF;
        m_mime = wxT("image/gif");
        m_hashTable = NULL;
    }

#if wxUSE_STREAMS
    virtual bool LoadFile(wxImage *image, wxInputStream& stream,
                          bool verbose = true, int index = -1) wxOVERRIDE;
    virtual bool SaveFile(wxImage *image, wxOutputStream& stream,
                          bool verbose=true) wxOVERRIDE;

    // Save animated gif
    bool SaveAnimation(const wxImageArray& images, wxOutputStream *stream,
        bool verbose = true, int delayMilliSecs = 1000);

protected:
    virtual int DoGetImageCount(wxInputStream& stream) wxOVERRIDE;
    virtual bool DoCanRead(wxInputStream& stream) wxOVERRIDE;

    bool DoSaveFile(const wxImage&, wxOutputStream *, bool verbose,
        bool first, int delayMilliSecs, bool loop,
        const wxRGB *pal, int palCount,
        int mask_index);
#endif // wxUSE_STREAMS
protected:

    // Declarations for saving

    unsigned long m_crntShiftDWord;   /* For bytes decomposition into codes. */
    int m_pixelCount;
    struct GifHashTableType *m_hashTable;
    wxInt16
      m_EOFCode,     /* The EOF LZ code. */
      m_clearCode,   /* The CLEAR LZ code. */
      m_runningCode, /* The next code algorithm can generate. */
      m_runningBits, /* The number of bits required to represent RunningCode. */
      m_maxCode1,    /* 1 bigger than max. possible code, in RunningBits bits. */
      m_crntCode,    /* Current algorithm code. */
      m_crntShiftState;    /* Number of bits in CrntShiftDWord. */
    wxUint8 m_LZBuf[256];   /* Compressed input is buffered here. */

    bool InitHashTable();
    void ClearHashTable();
    void InsertHashTable(unsigned long key, int code);
    int  ExistsHashTable(unsigned long key);

#if wxUSE_STREAMS
    bool CompressOutput(wxOutputStream *, int code);
    bool SetupCompress(wxOutputStream *, int bpp);
    bool CompressLine(wxOutputStream *, const wxUint8 *line, int lineLen);
#endif

private:
    wxDECLARE_DYNAMIC_CLASS(wxGIFHandler);
};

#endif // wxUSE_GIF

#endif // _WX_IMAGGIF_H_

