/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gifdecod.h
// Purpose:     wxGIFDecoder, GIF reader for wxImage and wxAnimation
// Author:      Guillermo Rodriguez Garcia <guille@iies.es>
// Version:     3.02
// Copyright:   (c) 1999 Guillermo Rodriguez Garcia
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GIFDECOD_H_
#define _WX_GIFDECOD_H_

#include "wx/defs.h"

#if wxUSE_STREAMS && wxUSE_GIF

#include "wx/stream.h"
#include "wx/image.h"
#include "wx/animdecod.h"
#include "wx/dynarray.h"

// internal utility used to store a frame in 8bit-per-pixel format
class GIFImage;


// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------

// Error codes:
//  Note that the error code wxGIF_TRUNCATED means that the image itself
//  is most probably OK, but the decoder didn't reach the end of the data
//  stream; this means that if it was not reading directly from file,
//  the stream will not be correctly positioned.
//
enum wxGIFErrorCode
{
    wxGIF_OK = 0,                   // everything was OK
    wxGIF_INVFORMAT,                // error in GIF header
    wxGIF_MEMERR,                   // error allocating memory
    wxGIF_TRUNCATED                 // file appears to be truncated
};

// --------------------------------------------------------------------------
// wxGIFDecoder class
// --------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGIFDecoder : public wxAnimationDecoder
{
public:
    // constructor, destructor, etc.
    wxGIFDecoder();
    ~wxGIFDecoder();

    // get data of current frame
    unsigned char* GetData(unsigned int frame) const;
    unsigned char* GetPalette(unsigned int frame) const;
    unsigned int GetNcolours(unsigned int frame) const;
    int GetTransparentColourIndex(unsigned int frame) const;
    wxColour GetTransparentColour(unsigned int frame) const wxOVERRIDE;

    virtual wxSize GetFrameSize(unsigned int frame) const wxOVERRIDE;
    virtual wxPoint GetFramePosition(unsigned int frame) const wxOVERRIDE;
    virtual wxAnimationDisposal GetDisposalMethod(unsigned int frame) const wxOVERRIDE;
    virtual long GetDelay(unsigned int frame) const wxOVERRIDE;

    // GIFs can contain both static images and animations
    bool IsAnimation() const
        { return m_nFrames > 1; }

    // load function which returns more info than just Load():
    wxGIFErrorCode LoadGIF( wxInputStream& stream );

    // free all internal frames
    void Destroy();

    // implementation of wxAnimationDecoder's pure virtuals
    virtual bool Load( wxInputStream& stream ) wxOVERRIDE
        { return LoadGIF(stream) == wxGIF_OK; }

    bool ConvertToImage(unsigned int frame, wxImage *image) const wxOVERRIDE;

    wxAnimationDecoder *Clone() const wxOVERRIDE
        { return new wxGIFDecoder; }
    wxAnimationType GetType() const wxOVERRIDE
        { return wxANIMATION_TYPE_GIF; }

private:
    // wxAnimationDecoder pure virtual
    virtual bool DoCanRead( wxInputStream& stream ) const wxOVERRIDE;
        // modifies current stream position (see wxAnimationDecoder::CanRead)

    int getcode(wxInputStream& stream, int bits, int abfin);
    wxGIFErrorCode dgif(wxInputStream& stream,
                        GIFImage *img, int interl, int bits);


    // array of all frames
    wxArrayPtrVoid m_frames;

    // decoder state vars
    int           m_restbits;       // remaining valid bits
    unsigned int  m_restbyte;       // remaining bytes in this block
    unsigned int  m_lastbyte;       // last byte read
    unsigned char m_buffer[256];    // buffer for reading
    unsigned char *m_bufp;          // pointer to next byte in buffer

    wxDECLARE_NO_COPY_CLASS(wxGIFDecoder);
};

#endif // wxUSE_STREAMS && wxUSE_GIF

#endif // _WX_GIFDECOD_H_
