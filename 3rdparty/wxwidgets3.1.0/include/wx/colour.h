/////////////////////////////////////////////////////////////////////////////
// Name:        wx/colour.h
// Purpose:     wxColourBase definition
// Author:      Julian Smart
// Modified by: Francesco Montorsi
// Created:
// Copyright:   Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLOUR_H_BASE_
#define _WX_COLOUR_H_BASE_

#include "wx/defs.h"
#include "wx/gdiobj.h"

class WXDLLIMPEXP_FWD_CORE wxColour;

// A macro to define the standard wxColour constructors:
//
// It avoids the need to repeat these lines across all colour.h files, since
// Set() is a virtual function and thus cannot be called by wxColourBase ctors
#define DEFINE_STD_WXCOLOUR_CONSTRUCTORS                                      \
    wxColour() { Init(); }                                                    \
    wxColour(ChannelType red,                                                 \
             ChannelType green,                                               \
             ChannelType blue,                                                \
             ChannelType alpha = wxALPHA_OPAQUE)                              \
        { Init(); Set(red, green, blue, alpha); }                             \
    wxColour(unsigned long colRGB) { Init(); Set(colRGB    ); }               \
    wxColour(const wxString& colourName) { Init(); Set(colourName); }         \
    wxColour(const char *colourName) { Init(); Set(colourName); }             \
    wxColour(const wchar_t *colourName) { Init(); Set(colourName); }


// flags for wxColour -> wxString conversion (see wxColour::GetAsString)
enum {
    wxC2S_NAME             = 1,   // return colour name, when possible
    wxC2S_CSS_SYNTAX       = 2,   // return colour in rgb(r,g,b) syntax
    wxC2S_HTML_SYNTAX      = 4    // return colour in #rrggbb syntax
};

const unsigned char wxALPHA_TRANSPARENT = 0;
const unsigned char wxALPHA_OPAQUE = 0xff;

// a valid but fully transparent colour
#define wxTransparentColour wxColour(0, 0, 0, wxALPHA_TRANSPARENT)
#define wxTransparentColor wxTransparentColour

// ----------------------------------------------------------------------------
// wxVariant support
// ----------------------------------------------------------------------------

#if wxUSE_VARIANT
#include "wx/variant.h"
DECLARE_VARIANT_OBJECT_EXPORTED(wxColour,WXDLLIMPEXP_CORE)
#endif

//-----------------------------------------------------------------------------
// wxColourBase: this class has no data members, just some functions to avoid
//               code redundancy in all native wxColour implementations
//-----------------------------------------------------------------------------

/*  Transition from wxGDIObject to wxObject is incomplete.  If your port does
    not need the wxGDIObject machinery to handle colors, please add it to the
    list of ports which do not need it.
 */
#if defined( __WXMAC__ ) || defined( __WXMSW__ )
#define wxCOLOUR_IS_GDIOBJECT 0
#else
#define wxCOLOUR_IS_GDIOBJECT 1
#endif

class WXDLLIMPEXP_CORE wxColourBase : public
#if wxCOLOUR_IS_GDIOBJECT
    wxGDIObject
#else
    wxObject
#endif
{
public:
    // type of a single colour component
    typedef unsigned char ChannelType;

    wxColourBase() {}
    virtual ~wxColourBase() {}


    // Set() functions
    // ---------------

    void Set(ChannelType red,
             ChannelType green,
             ChannelType blue,
             ChannelType alpha = wxALPHA_OPAQUE)
        { InitRGBA(red, green, blue, alpha); }

    // implemented in colourcmn.cpp
    bool Set(const wxString &str)
        { return FromString(str); }

    void Set(unsigned long colRGB)
    {
        // we don't need to know sizeof(long) here because we assume that the three
        // least significant bytes contain the R, G and B values
        Set((ChannelType)(0xFF & colRGB),
            (ChannelType)(0xFF & (colRGB >> 8)),
            (ChannelType)(0xFF & (colRGB >> 16)));
    }



    // accessors
    // ---------

    virtual ChannelType Red() const = 0;
    virtual ChannelType Green() const = 0;
    virtual ChannelType Blue() const = 0;
    virtual ChannelType Alpha() const
        { return wxALPHA_OPAQUE ; }

    // implemented in colourcmn.cpp
    virtual wxString GetAsString(long flags = wxC2S_NAME | wxC2S_CSS_SYNTAX) const;

    void SetRGB(wxUint32 colRGB)
    {
        Set((ChannelType)(0xFF & colRGB),
            (ChannelType)(0xFF & (colRGB >> 8)),
            (ChannelType)(0xFF & (colRGB >> 16)));
    }

    void SetRGBA(wxUint32 colRGBA)
    {
        Set((ChannelType)(0xFF & colRGBA),
            (ChannelType)(0xFF & (colRGBA >> 8)),
            (ChannelType)(0xFF & (colRGBA >> 16)),
            (ChannelType)(0xFF & (colRGBA >> 24)));
    }

    wxUint32 GetRGB() const
        { return Red() | (Green() << 8) | (Blue() << 16); }

    wxUint32 GetRGBA() const
        { return Red() | (Green() << 8) | (Blue() << 16) | (Alpha() << 24); }

#if !wxCOLOUR_IS_GDIOBJECT
    virtual bool IsOk() const= 0;

    // older version, for backwards compatibility only (but not deprecated
    // because it's still widely used)
    bool Ok() const { return IsOk(); }
#endif

    // manipulation
    // ------------

    // These methods are static because they are mostly used
    // within tight loops (where we don't want to instantiate wxColour's)

    static void          MakeMono    (unsigned char* r, unsigned char* g, unsigned char* b, bool on);
    static void          MakeDisabled(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char brightness = 255);
    static void          MakeGrey    (unsigned char* r, unsigned char* g, unsigned char* b); // integer version
    static void          MakeGrey    (unsigned char* r, unsigned char* g, unsigned char* b,
                                      double weight_r, double weight_g, double weight_b); // floating point version
    static unsigned char AlphaBlend  (unsigned char fg, unsigned char bg, double alpha);
    static void          ChangeLightness(unsigned char* r, unsigned char* g, unsigned char* b, int ialpha);

    wxColour ChangeLightness(int ialpha) const;
    wxColour& MakeDisabled(unsigned char brightness = 255);

protected:
    // Some ports need Init() and while we don't, provide a stub so that the
    // ports which don't need it are not forced to define it
    void Init() { }

    virtual void
    InitRGBA(ChannelType r, ChannelType g, ChannelType b, ChannelType a) = 0;

    virtual bool FromString(const wxString& s);

#if wxCOLOUR_IS_GDIOBJECT
    // wxColour doesn't use reference counted data (at least not in all ports)
    // so provide stubs for the functions which need to be defined if we do use
    // them
    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE
    {
        wxFAIL_MSG( "must be overridden if used" );

        return NULL;
    }

    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *WXUNUSED(data)) const wxOVERRIDE
    {
        wxFAIL_MSG( "must be overridden if used" );

        return NULL;
    }
#endif
};


// wxColour <-> wxString utilities, used by wxConfig, defined in colourcmn.cpp
WXDLLIMPEXP_CORE wxString wxToString(const wxColourBase& col);
WXDLLIMPEXP_CORE bool wxFromString(const wxString& str, wxColourBase* col);



#if defined(__WXMSW__)
    #include "wx/msw/colour.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/colour.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/colour.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/colour.h"
#elif defined(__WXDFB__)
    #include "wx/generic/colour.h"
#elif defined(__WXX11__)
    #include "wx/x11/colour.h"
#elif defined(__WXMAC__)
    #include "wx/osx/colour.h"
#elif defined(__WXQT__)
    #include "wx/qt/colour.h"
#endif

#define wxColor wxColour

#endif // _WX_COLOUR_H_BASE_
