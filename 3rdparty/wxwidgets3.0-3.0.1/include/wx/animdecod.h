/////////////////////////////////////////////////////////////////////////////
// Name:        wx/animdecod.h
// Purpose:     wxAnimationDecoder
// Author:      Francesco Montorsi
// Copyright:   (c) 2006 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ANIMDECOD_H
#define _WX_ANIMDECOD_H

#include "wx/defs.h"

#if wxUSE_STREAMS

#include "wx/colour.h"
#include "wx/gdicmn.h"
#include "wx/log.h"
#include "wx/stream.h"

class WXDLLIMPEXP_FWD_CORE wxImage;

/*

 Differences between a wxAnimationDecoder and a wxImageHandler:

 1) wxImageHandlers always load an input stream directly into a given wxImage
    object converting from the format-specific data representation to the
    wxImage native format (RGB24).
    wxAnimationDecoders always load an input stream using some optimized format
    to store it which is format-depedent. This allows to store a (possibly big)
    animation using a format which is a good compromise between required memory
    and time required to blit it on the screen.

 2) wxAnimationDecoders contain the animation data in some internal variable.
    That's why they derive from wxObjectRefData: they are data which can be shared.

 3) wxAnimationDecoders can be used by a wxImageHandler to retrieve a frame
    in wxImage format; the viceversa cannot be done.

 4) wxAnimationDecoders are decoders only, thus they do not support save features.

 5) wxAnimationDecoders are directly used by wxAnimation (generic implementation)
    as wxObjectRefData while they need to be 'wrapped' by a wxImageHandler for
    wxImage uses.

*/


// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------

// NB: the values of these enum items are not casual but coincide with the
//     GIF disposal codes. Do not change them !!
enum wxAnimationDisposal
{
    // No disposal specified. The decoder is not required to take any action.
    wxANIM_UNSPECIFIED = -1,

    // Do not dispose. The graphic is to be left in place.
    wxANIM_DONOTREMOVE = 0,

    // Restore to background color. The area used by the graphic must be
    // restored to the background color.
    wxANIM_TOBACKGROUND = 1,

    // Restore to previous. The decoder is required to restore the area
    // overwritten by the graphic with what was there prior to rendering the graphic.
    wxANIM_TOPREVIOUS = 2
};

enum wxAnimationType
{
    wxANIMATION_TYPE_INVALID,
    wxANIMATION_TYPE_GIF,
    wxANIMATION_TYPE_ANI,

    wxANIMATION_TYPE_ANY
};


// --------------------------------------------------------------------------
// wxAnimationDecoder class
// --------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxAnimationDecoder : public wxObjectRefData
{
public:
    wxAnimationDecoder()
    {
        m_nFrames = 0;
    }

    virtual bool Load( wxInputStream& stream ) = 0;

    bool CanRead( wxInputStream& stream ) const
    {
        // NOTE: this code is the same of wxImageHandler::CallDoCanRead

        if ( !stream.IsSeekable() )
            return false;        // can't test unseekable stream

        wxFileOffset posOld = stream.TellI();
        bool ok = DoCanRead(stream);

        // restore the old position to be able to test other formats and so on
        if ( stream.SeekI(posOld) == wxInvalidOffset )
        {
            wxLogDebug(wxT("Failed to rewind the stream in wxAnimationDecoder!"));

            // reading would fail anyhow as we're not at the right position
            return false;
        }

        return ok;
    }

    virtual wxAnimationDecoder *Clone() const = 0;
    virtual wxAnimationType GetType() const = 0;

    // convert given frame to wxImage
    virtual bool ConvertToImage(unsigned int frame, wxImage *image) const = 0;


    // frame specific data getters

    // not all frames may be of the same size; e.g. GIF allows to
    // specify that between two frames only a smaller portion of the
    // entire animation has changed.
    virtual wxSize GetFrameSize(unsigned int frame) const = 0;

    // the position of this frame in case it's not as big as m_szAnimation
    // or wxPoint(0,0) otherwise.
    virtual wxPoint GetFramePosition(unsigned int frame) const = 0;

    // what should be done after displaying this frame.
    virtual wxAnimationDisposal GetDisposalMethod(unsigned int frame) const = 0;

    // the number of milliseconds this frame should be displayed.
    // if returns -1 then the frame must be displayed forever.
    virtual long GetDelay(unsigned int frame) const = 0;

    // the transparent colour for this frame if any or wxNullColour.
    virtual wxColour GetTransparentColour(unsigned int frame) const = 0;

    // get global data
    wxSize GetAnimationSize() const { return m_szAnimation; }
    wxColour GetBackgroundColour() const { return m_background; }
    unsigned int GetFrameCount() const { return m_nFrames; }

protected:
    // checks the signature of the data in the given stream and returns true if it
    // appears to be a valid animation format recognized by the animation decoder;
    // this function should modify the stream current position without taking care
    // of restoring it since CanRead() will do it.
    virtual bool DoCanRead(wxInputStream& stream) const = 0;

    wxSize m_szAnimation;
    unsigned int m_nFrames;

    // this is the colour to use for the wxANIM_TOBACKGROUND disposal.
    // if not specified by the animation, it's set to wxNullColour
    wxColour m_background;
};

#endif  // wxUSE_STREAMS

#endif  // _WX_ANIMDECOD_H

