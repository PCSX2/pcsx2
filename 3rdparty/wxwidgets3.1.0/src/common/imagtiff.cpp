/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/imagtiff.cpp
// Purpose:     wxImage TIFF handler
// Author:      Robert Roebling
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_IMAGE && wxUSE_LIBTIFF

#include "wx/imagtiff.h"
#include "wx/versioninfo.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/bitmap.h"
    #include "wx/module.h"
    #include "wx/wxcrtvararg.h"
#endif

extern "C"
{
    #include "tiff.h"
    #include "tiffio.h"
}
#include "wx/filefn.h"
#include "wx/wfstream.h"

#ifndef TIFFLINKAGEMODE
    #define TIFFLINKAGEMODE LINKAGEMODE
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// TIFF library error/warning handlers
// ----------------------------------------------------------------------------

static wxString
FormatTiffMessage(const char *module, const char *fmt, va_list ap)
{
    char buf[512];
    if ( wxCRT_VsnprintfA(buf, WXSIZEOF(buf), fmt, ap) <= 0 )
    {
        // this isn't supposed to happen, but if it does, it's better
        // than nothing
        strcpy(buf, "Incorrectly formatted TIFF message");
    }
    buf[WXSIZEOF(buf)-1] = 0; // make sure it is always NULL-terminated

    wxString msg(buf);
    if ( module )
        msg += wxString::Format(_(" (in module \"%s\")"), module);

    return msg;
}

extern "C"
{

static void
TIFFwxWarningHandler(const char* module, const char *fmt, va_list ap)
{
    wxLogWarning("%s", FormatTiffMessage(module, fmt, ap));
}

static void
TIFFwxErrorHandler(const char* module, const char *fmt, va_list ap)
{
    wxLogError("%s", FormatTiffMessage(module, fmt, ap));
}

} // extern "C"

//-----------------------------------------------------------------------------
// wxTIFFHandler
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxTIFFHandler,wxImageHandler);

wxTIFFHandler::wxTIFFHandler()
{
    m_name = wxT("TIFF file");
    m_extension = wxT("tif");
    m_altExtensions.Add(wxT("tiff"));
    m_type = wxBITMAP_TYPE_TIFF;
    m_mime = wxT("image/tiff");
    TIFFSetWarningHandler((TIFFErrorHandler) TIFFwxWarningHandler);
    TIFFSetErrorHandler((TIFFErrorHandler) TIFFwxErrorHandler);
}

#if wxUSE_STREAMS

// helper to translate our, possibly 64 bit, wxFileOffset to TIFF, always 32
// bit, toff_t
static toff_t wxFileOffsetToTIFF(wxFileOffset ofs)
{
    if ( ofs == wxInvalidOffset )
        return (toff_t)-1;

    toff_t tofs = wx_truncate_cast(toff_t, ofs);
    wxCHECK_MSG( (wxFileOffset)tofs == ofs, (toff_t)-1,
                    wxT("TIFF library doesn't support large files") );

    return tofs;
}

// another helper to convert standard seek mode to our
static wxSeekMode wxSeekModeFromTIFF(int whence)
{
    switch ( whence )
    {
        case SEEK_SET:
            return wxFromStart;

        case SEEK_CUR:
            return wxFromCurrent;

        case SEEK_END:
            return wxFromEnd;

        default:
            return wxFromCurrent;
    }
}

extern "C"
{

tsize_t TIFFLINKAGEMODE
wxTIFFNullProc(thandle_t WXUNUSED(handle),
          tdata_t WXUNUSED(buf),
          tsize_t WXUNUSED(size))
{
    return (tsize_t) -1;
}

tsize_t TIFFLINKAGEMODE
wxTIFFReadProc(thandle_t handle, tdata_t buf, tsize_t size)
{
    wxInputStream *stream = (wxInputStream*) handle;
    stream->Read( (void*) buf, (size_t) size );
    return wx_truncate_cast(tsize_t, stream->LastRead());
}

tsize_t TIFFLINKAGEMODE
wxTIFFWriteProc(thandle_t handle, tdata_t buf, tsize_t size)
{
    wxOutputStream *stream = (wxOutputStream*) handle;
    stream->Write( (void*) buf, (size_t) size );
    return wx_truncate_cast(tsize_t, stream->LastWrite());
}

toff_t TIFFLINKAGEMODE
wxTIFFSeekIProc(thandle_t handle, toff_t off, int whence)
{
    wxInputStream *stream = (wxInputStream*) handle;

    return wxFileOffsetToTIFF(stream->SeekI((wxFileOffset)off,
                                            wxSeekModeFromTIFF(whence)));
}

toff_t TIFFLINKAGEMODE
wxTIFFSeekOProc(thandle_t handle, toff_t off, int whence)
{
    wxOutputStream *stream = (wxOutputStream*) handle;

    toff_t offset = wxFileOffsetToTIFF(
        stream->SeekO((wxFileOffset)off, wxSeekModeFromTIFF(whence)) );

    if (offset != (toff_t) -1 || whence != SEEK_SET)
    {
        return offset;
    }


    /*
    Try to workaround problems with libtiff seeking past the end of streams.

    This occurs when libtiff is writing tag entries past the end of a
    stream but hasn't written the directory yet (which will be placed
    before the tags and contain offsets to the just written tags).
    The behaviour for seeking past the end of a stream is not consistent
    and doesn't work with for example wxMemoryOutputStream. When this type
    of seeking fails (with SEEK_SET), fill in the gap with zeroes and try
    again.
    */

    wxFileOffset streamLength = stream->GetLength();
    if (streamLength != wxInvalidOffset && (wxFileOffset) off > streamLength)
    {
       if (stream->SeekO(streamLength, wxFromStart) == wxInvalidOffset)
       {
           return (toff_t) -1;
       }

       for (wxFileOffset i = 0; i < (wxFileOffset) off - streamLength; ++i)
       {
           stream->PutC(0);
       }
    }

    return wxFileOffsetToTIFF( stream->TellO() );
}

int TIFFLINKAGEMODE
wxTIFFCloseIProc(thandle_t WXUNUSED(handle))
{
    // there is no need to close the input stream
    return 0;
}

int TIFFLINKAGEMODE
wxTIFFCloseOProc(thandle_t handle)
{
    wxOutputStream *stream = (wxOutputStream*) handle;

    return stream->Close() ? 0 : -1;
}

toff_t TIFFLINKAGEMODE
wxTIFFSizeProc(thandle_t handle)
{
    wxStreamBase *stream = (wxStreamBase*) handle;
    return (toff_t) stream->GetSize();
}

int TIFFLINKAGEMODE
wxTIFFMapProc(thandle_t WXUNUSED(handle),
             tdata_t* WXUNUSED(pbase),
             toff_t* WXUNUSED(psize))
{
    return 0;
}

void TIFFLINKAGEMODE
wxTIFFUnmapProc(thandle_t WXUNUSED(handle),
               tdata_t WXUNUSED(base),
               toff_t WXUNUSED(size))
{
}

} // extern "C"

TIFF*
TIFFwxOpen(wxInputStream &stream, const char* name, const char* mode)
{
    TIFF* tif = TIFFClientOpen(name, mode,
        (thandle_t) &stream,
        wxTIFFReadProc, wxTIFFNullProc,
        wxTIFFSeekIProc, wxTIFFCloseIProc, wxTIFFSizeProc,
        wxTIFFMapProc, wxTIFFUnmapProc);

    return tif;
}

TIFF*
TIFFwxOpen(wxOutputStream &stream, const char* name, const char* mode)
{
    TIFF* tif = TIFFClientOpen(name, mode,
        (thandle_t) &stream,
        wxTIFFNullProc, wxTIFFWriteProc,
        wxTIFFSeekOProc, wxTIFFCloseOProc, wxTIFFSizeProc,
        wxTIFFMapProc, wxTIFFUnmapProc);

    return tif;
}

bool wxTIFFHandler::LoadFile( wxImage *image, wxInputStream& stream, bool verbose, int index )
{
    if (index == -1)
        index = 0;

    image->Destroy();

    TIFF *tif = TIFFwxOpen( stream, "image", "r" );

    if (!tif)
    {
        if (verbose)
        {
            wxLogError( _("TIFF: Error loading image.") );
        }

        return false;
    }

    if (!TIFFSetDirectory( tif, (tdir_t)index ))
    {
        if (verbose)
        {
            wxLogError( _("Invalid TIFF image index.") );
        }

        TIFFClose( tif );

        return false;
    }

    uint32 w, h;
    uint32 *raster;

    TIFFGetField( tif, TIFFTAG_IMAGEWIDTH, &w );
    TIFFGetField( tif, TIFFTAG_IMAGELENGTH, &h );

    uint16 samplesPerPixel = 0;
    (void) TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);

    uint16 bitsPerSample = 0;
    (void) TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

    uint16 extraSamples;
    uint16* samplesInfo;
    TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
                          &extraSamples, &samplesInfo);

    uint16 photometric;
    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric))
    {
        photometric = PHOTOMETRIC_MINISWHITE;
    }
    const bool hasAlpha = (extraSamples >= 1
        && ((samplesInfo[0] == EXTRASAMPLE_UNSPECIFIED)
            || samplesInfo[0] == EXTRASAMPLE_ASSOCALPHA
            || samplesInfo[0] == EXTRASAMPLE_UNASSALPHA))
        || (extraSamples == 0 && samplesPerPixel == 4
            && photometric == PHOTOMETRIC_RGB);

    // guard against integer overflow during multiplication which could result
    // in allocating a too small buffer and then overflowing it
    const double bytesNeeded = (double)w * (double)h * sizeof(uint32);
    if ( bytesNeeded >= wxUINT32_MAX )
    {
        if ( verbose )
        {
            wxLogError( _("TIFF: Image size is abnormally big.") );
        }

        TIFFClose(tif);

        return false;
    }

    raster = (uint32*) _TIFFmalloc( (uint32)bytesNeeded );

    if (!raster)
    {
        if (verbose)
        {
            wxLogError( _("TIFF: Couldn't allocate memory.") );
        }

        TIFFClose( tif );

        return false;
    }

    image->Create( (int)w, (int)h );
    if (!image->IsOk())
    {
        if (verbose)
        {
            wxLogError( _("TIFF: Couldn't allocate memory.") );
        }

        _TIFFfree( raster );
        TIFFClose( tif );

        return false;
    }

    if ( hasAlpha )
        image->SetAlpha();

    uint16 planarConfig = PLANARCONFIG_CONTIG;
    (void) TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);

    bool ok = true;
    char msg[1024] = "";
    if
    (
        (planarConfig == PLANARCONFIG_CONTIG && samplesPerPixel == 2
            && extraSamples == 1)
        &&
        (
            ( !TIFFRGBAImageOK(tif, msg) )
            || (bitsPerSample == 8)
        )
    )
    {
        const bool isGreyScale = (bitsPerSample == 8);
        unsigned char *buf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(tif));
        uint32 pos = 0;
        const bool minIsWhite = (photometric == PHOTOMETRIC_MINISWHITE);
        const int minValue =  minIsWhite ? 255 : 0;
        const int maxValue = 255 - minValue;

        /*
        Decode to ABGR format as that is what the code, that converts to
        wxImage, later on expects (normally TIFFReadRGBAImageOriented is
        used to decode which uses an ABGR layout).
        */
        for (uint32 y = 0; y < h; ++y)
        {
            if (TIFFReadScanline(tif, buf, y, 0) != 1)
            {
                ok = false;
                break;
            }

            if (isGreyScale)
            {
                for (uint32 x = 0; x < w; ++x)
                {
                    uint8 val = minIsWhite ? 255 - buf[x*2] : buf[x*2];
                    uint8 alpha = minIsWhite ? 255 - buf[x*2+1] : buf[x*2+1];
                    raster[pos] = val + (val << 8) + (val << 16)
                        + (alpha << 24);
                    pos++;
                }
            }
            else
            {
                for (uint32 x = 0; x < w; ++x)
                {
                    int mask = buf[x*2/8] << ((x*2)%8);

                    uint8 val = mask & 128 ? maxValue : minValue;
                    raster[pos] = val + (val << 8) + (val << 16)
                        + ((mask & 64 ? maxValue : minValue) << 24);
                    pos++;
                }
            }
        }

        _TIFFfree(buf);
    }
    else
    {
        ok = TIFFReadRGBAImageOriented( tif, w, h, raster,
            ORIENTATION_TOPLEFT, 0 ) != 0;
    }


    if (!ok)
    {
        if (verbose)
        {
            wxLogError( _("TIFF: Error reading image.") );
        }

        _TIFFfree( raster );
        image->Destroy();
        TIFFClose( tif );

        return false;
    }

    unsigned char *ptr = image->GetData();

    unsigned char *alpha = image->GetAlpha();

    uint32 pos = 0;

    for (uint32 i = 0; i < h; i++)
    {
        for (uint32 j = 0; j < w; j++)
        {
            *(ptr++) = (unsigned char)TIFFGetR(raster[pos]);
            *(ptr++) = (unsigned char)TIFFGetG(raster[pos]);
            *(ptr++) = (unsigned char)TIFFGetB(raster[pos]);
            if ( hasAlpha )
                *(alpha++) = (unsigned char)TIFFGetA(raster[pos]);

            pos++;
        }
    }


    image->SetOption(wxIMAGE_OPTION_TIFF_PHOTOMETRIC, photometric);

    uint16 compression;
    /*
    Copy some baseline TIFF tags which helps when re-saving a TIFF
    to be similar to the original image.
    */
    if (samplesPerPixel)
    {
        image->SetOption(wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL, samplesPerPixel);
    }

    if (bitsPerSample)
    {
        image->SetOption(wxIMAGE_OPTION_TIFF_BITSPERSAMPLE, bitsPerSample);
    }

    if ( TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &compression) )
    {
        image->SetOption(wxIMAGE_OPTION_TIFF_COMPRESSION, compression);
    }

    // Set the resolution unit.
    wxImageResolution resUnit = wxIMAGE_RESOLUTION_NONE;
    uint16 tiffRes;
    if ( TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &tiffRes) )
    {
        switch (tiffRes)
        {
            default:
                wxLogWarning(_("Unknown TIFF resolution unit %d ignored"),
                    tiffRes);
                wxFALLTHROUGH;

            case RESUNIT_NONE:
                resUnit = wxIMAGE_RESOLUTION_NONE;
                break;

            case RESUNIT_INCH:
                resUnit = wxIMAGE_RESOLUTION_INCHES;
                break;

            case RESUNIT_CENTIMETER:
                resUnit = wxIMAGE_RESOLUTION_CM;
                break;
        }
    }

    image->SetOption(wxIMAGE_OPTION_RESOLUTIONUNIT, resUnit);

    /*
    Set the image resolution if it's available. Resolution tag is not
    dependent on RESOLUTIONUNIT != RESUNIT_NONE (according to TIFF spec).
    */
    float resX, resY;

    if ( TIFFGetField(tif, TIFFTAG_XRESOLUTION, &resX) )
    {
        /*
        Use a string value to not lose precision.
        rounding to int as cm and then converting to inch may
        result in whole integer rounding error, eg. 201 instead of 200 dpi.
        If an app wants an int, GetOptionInt will convert and round down.
        */
        image->SetOption(wxIMAGE_OPTION_RESOLUTIONX,
            wxString::FromCDouble((double) resX));
    }

    if ( TIFFGetField(tif, TIFFTAG_YRESOLUTION, &resY) )
    {
        image->SetOption(wxIMAGE_OPTION_RESOLUTIONY,
            wxString::FromCDouble((double) resY));
    }

    _TIFFfree( raster );

    TIFFClose( tif );

    return true;
}

int wxTIFFHandler::DoGetImageCount( wxInputStream& stream )
{
    TIFF *tif = TIFFwxOpen( stream, "image", "r" );

    if (!tif)
        return 0;

    int dircount = 0;  // according to the libtiff docs, dircount should be set to 1 here???
    do {
        dircount++;
    } while (TIFFReadDirectory(tif));

    TIFFClose( tif );

    // NOTE: this function modifies the current stream position but it's ok
    //       (see wxImageHandler::GetImageCount)

    return dircount;
}

bool wxTIFFHandler::SaveFile( wxImage *image, wxOutputStream& stream, bool verbose )
{
    TIFF *tif = TIFFwxOpen( stream, "image", "w" );

    if (!tif)
    {
        if (verbose)
        {
            wxLogError( _("TIFF: Error saving image.") );
        }

        return false;
    }

    const int imageWidth = image->GetWidth();
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,  (uint32) imageWidth);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32)image->GetHeight());
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    // save the image resolution if we have it
    int xres, yres;
    const wxImageResolution res = GetResolutionFromOptions(*image, &xres, &yres);
    uint16 tiffRes;
    switch ( res )
    {
        default:
            wxFAIL_MSG( wxT("unknown image resolution units") );
            wxFALLTHROUGH;

        case wxIMAGE_RESOLUTION_NONE:
            tiffRes = RESUNIT_NONE;
            break;

        case wxIMAGE_RESOLUTION_INCHES:
            tiffRes = RESUNIT_INCH;
            break;

        case wxIMAGE_RESOLUTION_CM:
            tiffRes = RESUNIT_CENTIMETER;
            break;
    }

    if ( tiffRes != RESUNIT_NONE )
    {
        TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, tiffRes);
        TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)xres);
        TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)yres);
    }


    int spp = image->GetOptionInt(wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL);
    if ( !spp )
        spp = 3;

    int bps = image->GetOptionInt(wxIMAGE_OPTION_TIFF_BITSPERSAMPLE);
    if ( !bps )
    {
        bps = 8;
    }
    else if (bps == 1)
    {
        // One bit per sample combined with 3 samples per pixel is
        // not allowed and crashes libtiff.
        spp = 1;
    }

    int photometric = PHOTOMETRIC_RGB;

    if ( image->HasOption(wxIMAGE_OPTION_TIFF_PHOTOMETRIC) )
    {
        photometric = image->GetOptionInt(wxIMAGE_OPTION_TIFF_PHOTOMETRIC);
        if (photometric == PHOTOMETRIC_MINISWHITE
            || photometric == PHOTOMETRIC_MINISBLACK)
        {
            // either b/w or greyscale
            spp = 1;
        }
    }
    else if (spp <= 2)
    {
        photometric = PHOTOMETRIC_MINISWHITE;
    }

    const bool hasAlpha = image->HasAlpha();

    int compression = image->GetOptionInt(wxIMAGE_OPTION_TIFF_COMPRESSION);
    if ( !compression || (compression == COMPRESSION_JPEG && hasAlpha) )
    {
        // We can't use COMPRESSION_LZW because current version of libtiff
        // doesn't implement it ("no longer implemented due to Unisys patent
        // enforcement") and other compression methods are lossy so we
        // shouldn't use them by default -- and the only remaining one is none.
        // Also JPEG compression for alpha images is not a good idea (viewers
        // not opening the image properly).
        compression = COMPRESSION_NONE;
    }

    if
    (
        (photometric == PHOTOMETRIC_RGB && spp == 4)
        || (photometric <= PHOTOMETRIC_MINISBLACK && spp == 2)
    )
    {
        // Compensate for user passing a SamplesPerPixel that includes
        // the alpha channel.
        spp--;
    }


    int extraSamples = hasAlpha ? 1 : 0;

    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, spp + extraSamples);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, photometric);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    if (extraSamples)
    {
        uint16 extra[] = { EXTRASAMPLE_UNSPECIFIED };
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, (long) 1, &extra);
    }

    // scanlinesize is determined by spp+extraSamples and bps
    const tsize_t linebytes =
        (tsize_t)((imageWidth * (spp + extraSamples) * bps + 7) / 8);

    unsigned char *buf;

    const bool isColouredImage = (spp > 1)
        && (photometric != PHOTOMETRIC_MINISWHITE)
        && (photometric != PHOTOMETRIC_MINISBLACK);


    if (TIFFScanlineSize(tif) > linebytes || !isColouredImage || hasAlpha)
    {
        buf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(tif));
        if (!buf)
        {
            if (verbose)
            {
                wxLogError( _("TIFF: Couldn't allocate memory.") );
            }

            TIFFClose( tif );

            return false;
        }
    }
    else
    {
        buf = NULL;
    }

    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,TIFFDefaultStripSize(tif, (uint32) -1));

    const int bitsPerPixel = (spp + extraSamples) * bps;
    const int bytesPerPixel = (bitsPerPixel + 7) / 8;
    const int pixelsPerByte = 8 / bitsPerPixel;
    int remainingPixelCount = 0;

    if (pixelsPerByte)
    {
        // How many pixels to write in the last byte column?
        remainingPixelCount = imageWidth % pixelsPerByte;
        if (!remainingPixelCount) remainingPixelCount = pixelsPerByte;
    }

    const bool minIsWhite = (photometric == PHOTOMETRIC_MINISWHITE);
    unsigned char *ptr = image->GetData();
    for ( int row = 0; row < image->GetHeight(); row++ )
    {
        if ( buf )
        {
            if (isColouredImage)
            {
                // colour image
                if (hasAlpha)
                {
                    for ( int column = 0; column < imageWidth; column++ )
                    {
                        buf[column*4    ] = ptr[column*3    ];
                        buf[column*4 + 1] = ptr[column*3 + 1];
                        buf[column*4 + 2] = ptr[column*3 + 2];
                        buf[column*4 + 3] = image->GetAlpha(column, row);
                    }
                }
                else
                {
                    memcpy(buf, ptr, imageWidth * 3);
                }
            }
            else if (spp * bps == 8) // greyscale image
            {
                for ( int column = 0; column < imageWidth; column++ )
                {
                    uint8 value = ptr[column*3 + 1];
                    if (minIsWhite)
                    {
                        value = 255 - value;
                    }

                    buf[column * bytesPerPixel] = value;

                    if (hasAlpha)
                    {
                        value = image->GetAlpha(column, row);
                        buf[column*bytesPerPixel+1]
                            = minIsWhite ? 255 - value : value;
                    }
                }
            }
            else // black and white image
            {
                for ( int column = 0; column < linebytes; column++ )
                {
                    uint8 reverse = 0;
                    int pixelsPerByteCount = (column + 1 != linebytes)
                        ? pixelsPerByte
                        : remainingPixelCount;
                    for ( int bp = 0; bp < pixelsPerByteCount; bp++ )
                    {
                        if ( (ptr[column * 3 * pixelsPerByte + bp*3 + 1] <=127)
                            == minIsWhite )
                        {
                            // check only green as this is sufficient
                            reverse |= (uint8) (128 >> (bp * bitsPerPixel));
                        }

                        if (hasAlpha
                            && (image->GetAlpha(column * pixelsPerByte + bp,
                                    row) <= 127) == minIsWhite)
                        {
                            reverse |= (uint8) (64 >> (bp * bitsPerPixel));
                        }
                    }

                    buf[column] = reverse;
                }
            }
        }

        if ( TIFFWriteScanline(tif, buf ? buf : ptr, (uint32)row, 0) < 0 )
        {
            if (verbose)
            {
                wxLogError( _("TIFF: Error writing image.") );
            }

            TIFFClose( tif );
            if (buf)
                _TIFFfree(buf);

            return false;
        }

        ptr += imageWidth * 3;
    }

    (void) TIFFClose(tif);

    if (buf)
        _TIFFfree(buf);

    return true;
}

bool wxTIFFHandler::DoCanRead( wxInputStream& stream )
{
    unsigned char hdr[2];

    if ( !stream.Read(&hdr[0], WXSIZEOF(hdr)) )     // it's ok to modify the stream position here
        return false;

    return (hdr[0] == 'I' && hdr[1] == 'I') ||
           (hdr[0] == 'M' && hdr[1] == 'M');
}

#endif  // wxUSE_STREAMS

/*static*/ wxVersionInfo wxTIFFHandler::GetLibraryVersionInfo()
{
    int major,
        minor,
        micro;

    const wxString ver(::TIFFGetVersion());
    if ( wxSscanf(ver, "LIBTIFF, Version %d.%d.%d", &major, &minor, &micro) != 3 )
    {
        wxLogDebug("Unrecognized libtiff version string \"%s\"", ver);

        major =
        minor =
        micro = 0;
    }

    wxString copyright;
    const wxString desc = ver.BeforeFirst('\n', &copyright);
    copyright.Replace("\n", "");

    return wxVersionInfo("libtiff", major, minor, micro, desc, copyright);
}

#endif  // wxUSE_LIBTIFF
