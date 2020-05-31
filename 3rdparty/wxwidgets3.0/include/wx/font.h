/////////////////////////////////////////////////////////////////////////////
// Name:        wx/font.h
// Purpose:     wxFontBase class: the interface of wxFont
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.09.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONT_H_BASE_
#define _WX_FONT_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"        // for wxDEFAULT &c
#include "wx/fontenc.h"     // the font encoding constants
#include "wx/gdiobj.h"      // the base class
#include "wx/gdicmn.h"      // for wxGDIObjListBase

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxFont;

// ----------------------------------------------------------------------------
// font constants
// ----------------------------------------------------------------------------

// standard font families: these may be used only for the font creation, it
// doesn't make sense to query an existing font for its font family as,
// especially if the font had been created from a native font description, it
// may be unknown
enum wxFontFamily
{
    wxFONTFAMILY_DEFAULT = wxDEFAULT,
    wxFONTFAMILY_DECORATIVE = wxDECORATIVE,
    wxFONTFAMILY_ROMAN = wxROMAN,
    wxFONTFAMILY_SCRIPT = wxSCRIPT,
    wxFONTFAMILY_SWISS = wxSWISS,
    wxFONTFAMILY_MODERN = wxMODERN,
    wxFONTFAMILY_TELETYPE = wxTELETYPE,
    wxFONTFAMILY_MAX,
    wxFONTFAMILY_UNKNOWN = wxFONTFAMILY_MAX
};

// font styles
enum wxFontStyle
{
    wxFONTSTYLE_NORMAL = wxNORMAL,
    wxFONTSTYLE_ITALIC = wxITALIC,
    wxFONTSTYLE_SLANT = wxSLANT,
    wxFONTSTYLE_MAX
};

// font weights
enum wxFontWeight
{
    wxFONTWEIGHT_NORMAL = wxNORMAL,
    wxFONTWEIGHT_LIGHT = wxLIGHT,
    wxFONTWEIGHT_BOLD = wxBOLD,
    wxFONTWEIGHT_MAX
};

// Symbolic font sizes as defined in CSS specification.
enum wxFontSymbolicSize
{
    wxFONTSIZE_XX_SMALL = -3,
    wxFONTSIZE_X_SMALL,
    wxFONTSIZE_SMALL,
    wxFONTSIZE_MEDIUM,
    wxFONTSIZE_LARGE,
    wxFONTSIZE_X_LARGE,
    wxFONTSIZE_XX_LARGE
};

// the font flag bits for the new font ctor accepting one combined flags word
enum wxFontFlag
{
    // no special flags: font with default weight/slant/anti-aliasing
    wxFONTFLAG_DEFAULT          = 0,

    // slant flags (default: no slant)
    wxFONTFLAG_ITALIC           = 1 << 0,
    wxFONTFLAG_SLANT            = 1 << 1,

    // weight flags (default: medium)
    wxFONTFLAG_LIGHT            = 1 << 2,
    wxFONTFLAG_BOLD             = 1 << 3,

    // anti-aliasing flag: force on or off (default: the current system default)
    wxFONTFLAG_ANTIALIASED      = 1 << 4,
    wxFONTFLAG_NOT_ANTIALIASED  = 1 << 5,

    // underlined/strikethrough flags (default: no lines)
    wxFONTFLAG_UNDERLINED       = 1 << 6,
    wxFONTFLAG_STRIKETHROUGH    = 1 << 7,

    // the mask of all currently used flags
    wxFONTFLAG_MASK = wxFONTFLAG_ITALIC             |
                      wxFONTFLAG_SLANT              |
                      wxFONTFLAG_LIGHT              |
                      wxFONTFLAG_BOLD               |
                      wxFONTFLAG_ANTIALIASED        |
                      wxFONTFLAG_NOT_ANTIALIASED    |
                      wxFONTFLAG_UNDERLINED         |
                      wxFONTFLAG_STRIKETHROUGH
};

// ----------------------------------------------------------------------------
// wxFontInfo describes a wxFont
// ----------------------------------------------------------------------------

class wxFontInfo
{
public:
    // Default ctor uses the default font size appropriate for the current
    // platform.
    wxFontInfo()
        { InitPointSize(-1); }

    // These ctors specify the font size, either in points or in pixels.
    wxEXPLICIT wxFontInfo(int pointSize)
        { InitPointSize(pointSize); }
    wxEXPLICIT wxFontInfo(const wxSize& pixelSize) : m_pixelSize(pixelSize)
        { Init(); }

    // Setters for the various attributes. All of them return the object itself
    // so that the calls to them could be chained.
    wxFontInfo& Family(wxFontFamily family)
        { m_family = family; return *this; }
    wxFontInfo& FaceName(const wxString& faceName)
        { m_faceName = faceName; return *this; }

    wxFontInfo& Bold(bool bold = true)
        { SetFlag(wxFONTFLAG_BOLD, bold); return *this; }
    wxFontInfo& Light(bool light = true)
        { SetFlag(wxFONTFLAG_LIGHT, light); return *this; }

    wxFontInfo& Italic(bool italic = true)
        { SetFlag(wxFONTFLAG_ITALIC, italic); return *this; }
    wxFontInfo& Slant(bool slant = true)
        { SetFlag(wxFONTFLAG_SLANT, slant); return *this; }

    wxFontInfo& AntiAliased(bool antiAliased = true)
        { SetFlag(wxFONTFLAG_ANTIALIASED, antiAliased); return *this; }
    wxFontInfo& Underlined(bool underlined = true)
        { SetFlag(wxFONTFLAG_UNDERLINED, underlined); return *this; }
    wxFontInfo& Strikethrough(bool strikethrough = true)
        { SetFlag(wxFONTFLAG_STRIKETHROUGH, strikethrough); return *this; }

    wxFontInfo& Encoding(wxFontEncoding encoding)
        { m_encoding = encoding; return *this; }


    // Set all flags at once.
    wxFontInfo& AllFlags(int flags)
        { m_flags = flags; return *this; }


    // Accessors are mostly meant to be used by wxFont itself to extract the
    // various pieces of the font description.

    bool IsUsingSizeInPixels() const { return m_pixelSize != wxDefaultSize; }
    int GetPointSize() const { return m_pointSize; }
    wxSize GetPixelSize() const { return m_pixelSize; }
    wxFontFamily GetFamily() const { return m_family; }
    const wxString& GetFaceName() const { return m_faceName; }

    wxFontStyle GetStyle() const
    {
        return m_flags & wxFONTFLAG_ITALIC
                        ? wxFONTSTYLE_ITALIC
                        : m_flags & wxFONTFLAG_SLANT
                            ? wxFONTSTYLE_SLANT
                            : wxFONTSTYLE_NORMAL;
    }

    wxFontWeight GetWeight() const
    {
        return m_flags & wxFONTFLAG_LIGHT
                        ? wxFONTWEIGHT_LIGHT
                        : m_flags & wxFONTFLAG_BOLD
                            ? wxFONTWEIGHT_BOLD
                            : wxFONTWEIGHT_NORMAL;
    }

    bool IsAntiAliased() const
    {
        return (m_flags & wxFONTFLAG_ANTIALIASED) != 0;
    }

    bool IsUnderlined() const
    {
        return (m_flags & wxFONTFLAG_UNDERLINED) != 0;
    }

    bool IsStrikethrough() const
    {
        return (m_flags & wxFONTFLAG_STRIKETHROUGH) != 0;
    }

    wxFontEncoding GetEncoding() const { return m_encoding; }


    // Default copy ctor, assignment operator and dtor are OK.

private:
    // Common part of all ctor, initializing everything except the size (which
    // is initialized by the ctors themselves).
    void Init()
    {
        m_family = wxFONTFAMILY_DEFAULT;
        m_flags = wxFONTFLAG_DEFAULT;
        m_encoding = wxFONTENCODING_DEFAULT;
    }

    void InitPointSize(int pointSize)
    {
        Init();

        m_pointSize = pointSize;
        m_pixelSize = wxDefaultSize;
    }

    // Turn on or off the given bit in m_flags depending on the value of the
    // boolean argument.
    void SetFlag(int flag, bool on)
    {
        if ( on )
            m_flags |= flag;
        else
            m_flags &= ~flag;
    }

    // The size information: if m_pixelSize is valid (!= wxDefaultSize), then
    // it is used. Otherwise m_pointSize is used, taking into account that if
    // it is == -1, it means that the platform dependent font size should be
    // used.
    int m_pointSize;
    wxSize m_pixelSize;

    wxFontFamily m_family;
    wxString m_faceName;
    int m_flags;
    wxFontEncoding m_encoding;
};

// ----------------------------------------------------------------------------
// wxFontBase represents a font object
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxNativeFontInfo;

class WXDLLIMPEXP_CORE wxFontBase : public wxGDIObject
{
public:
    /*
        derived classes should provide the following ctors:

    wxFont();
    wxFont(const wxFontInfo& info);
    wxFont(const wxString& nativeFontInfoString);
    wxFont(const wxNativeFontInfo& info);
    wxFont(int size,
           wxFontFamily family,
           wxFontStyle style,
           wxFontWeight weight,
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT);
    wxFont(const wxSize& pixelSize,
           wxFontFamily family,
           wxFontStyle style,
           wxFontWeight weight,
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT);
    */

    // creator function
    virtual ~wxFontBase();


#if FUTURE_WXWIN_COMPATIBILITY_3_0
    // from the font components
    static wxFont *New(
        int pointSize,              // size of the font in points
        int family,                 // see wxFontFamily enum
        int style,                  // see wxFontStyle enum
        int weight,                 // see wxFontWeight enum
        bool underlined = false,    // not underlined by default
        const wxString& face = wxEmptyString,              // facename
        wxFontEncoding encoding = wxFONTENCODING_DEFAULT)  // ISO8859-X, ...
        { return New(pointSize, (wxFontFamily)family, (wxFontStyle)style,
                     (wxFontWeight)weight, underlined, face, encoding); }

    // from the font components
    static wxFont *New(
        const wxSize& pixelSize,    // size of the font in pixels
        int family,                 // see wxFontFamily enum
        int style,                  // see wxFontStyle enum
        int weight,                 // see wxFontWeight enum
        bool underlined = false,    // not underlined by default
        const wxString& face = wxEmptyString,              // facename
        wxFontEncoding encoding = wxFONTENCODING_DEFAULT)  // ISO8859-X, ...
        { return New(pixelSize, (wxFontFamily)family, (wxFontStyle)style,
                     (wxFontWeight)weight, underlined, face, encoding); }
#endif

    // from the font components
    static wxFont *New(
        int pointSize,              // size of the font in points
        wxFontFamily family,        // see wxFontFamily enum
        wxFontStyle style,          // see wxFontStyle enum
        wxFontWeight weight,        // see wxFontWeight enum
        bool underlined = false,    // not underlined by default
        const wxString& face = wxEmptyString,              // facename
        wxFontEncoding encoding = wxFONTENCODING_DEFAULT); // ISO8859-X, ...

    // from the font components
    static wxFont *New(
        const wxSize& pixelSize,    // size of the font in pixels
        wxFontFamily family,        // see wxFontFamily enum
        wxFontStyle style,          // see wxFontStyle enum
        wxFontWeight weight,        // see wxFontWeight enum
        bool underlined = false,    // not underlined by default
        const wxString& face = wxEmptyString,              // facename
        wxFontEncoding encoding = wxFONTENCODING_DEFAULT); // ISO8859-X, ...

    // from the font components but using the font flags instead of separate
    // parameters for each flag
    static wxFont *New(int pointSize,
                       wxFontFamily family,
                       int flags = wxFONTFLAG_DEFAULT,
                       const wxString& face = wxEmptyString,
                       wxFontEncoding encoding = wxFONTENCODING_DEFAULT);


    // from the font components but using the font flags instead of separate
    // parameters for each flag
    static wxFont *New(const wxSize& pixelSize,
                       wxFontFamily family,
                       int flags = wxFONTFLAG_DEFAULT,
                       const wxString& face = wxEmptyString,
                       wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    // from the (opaque) native font description object
    static wxFont *New(const wxNativeFontInfo& nativeFontDesc);

    // from the string representation of wxNativeFontInfo
    static wxFont *New(const wxString& strNativeFontDesc);

    // comparison
    bool operator==(const wxFont& font) const;
    bool operator!=(const wxFont& font) const { return !(*this == font); }

    // accessors: get the font characteristics
    virtual int GetPointSize() const = 0;
    virtual wxSize GetPixelSize() const;
    virtual bool IsUsingSizeInPixels() const;
    wxFontFamily GetFamily() const;
    virtual wxFontStyle GetStyle() const = 0;
    virtual wxFontWeight GetWeight() const = 0;
    virtual bool GetUnderlined() const = 0;
    virtual bool GetStrikethrough() const { return false; }
    virtual wxString GetFaceName() const = 0;
    virtual wxFontEncoding GetEncoding() const = 0;
    virtual const wxNativeFontInfo *GetNativeFontInfo() const = 0;

    virtual bool IsFixedWidth() const;

    wxString GetNativeFontInfoDesc() const;
    wxString GetNativeFontInfoUserDesc() const;

    // change the font characteristics
    virtual void SetPointSize( int pointSize ) = 0;
    virtual void SetPixelSize( const wxSize& pixelSize );
    virtual void SetFamily( wxFontFamily family ) = 0;
    virtual void SetStyle( wxFontStyle style ) = 0;
    virtual void SetWeight( wxFontWeight weight ) = 0;

    virtual void SetUnderlined( bool underlined ) = 0;
    virtual void SetStrikethrough( bool WXUNUSED(strikethrough) ) {}
    virtual void SetEncoding(wxFontEncoding encoding) = 0;
    virtual bool SetFaceName( const wxString& faceName );
    void SetNativeFontInfo(const wxNativeFontInfo& info)
        { DoSetNativeFontInfo(info); }

    bool SetNativeFontInfo(const wxString& info);
    bool SetNativeFontInfoUserDesc(const wxString& info);

    // Symbolic font sizes support: set the font size to "large" or "very
    // small" either absolutely (i.e. compared to the default font size) or
    // relatively to the given font size.
    void SetSymbolicSize(wxFontSymbolicSize size);
    void SetSymbolicSizeRelativeTo(wxFontSymbolicSize size, int base)
    {
        SetPointSize(AdjustToSymbolicSize(size, base));
    }

    // Adjust the base size in points according to symbolic size.
    static int AdjustToSymbolicSize(wxFontSymbolicSize size, int base);


    // translate the fonts into human-readable string (i.e. GetStyleString()
    // will return "wxITALIC" for an italic font, ...)
    wxString GetFamilyString() const;
    wxString GetStyleString() const;
    wxString GetWeightString() const;

    // the default encoding is used for creating all fonts with default
    // encoding parameter
    static wxFontEncoding GetDefaultEncoding() { return ms_encodingDefault; }
    static void SetDefaultEncoding(wxFontEncoding encoding);

    // this doesn't do anything and is kept for compatibility only
#if WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED_INLINE(void SetNoAntiAliasing(bool no = true), wxUnusedVar(no);)
    wxDEPRECATED_INLINE(bool GetNoAntiAliasing() const, return false;)
#endif // WXWIN_COMPATIBILITY_2_8

protected:
    // the function called by both overloads of SetNativeFontInfo()
    virtual void DoSetNativeFontInfo(const wxNativeFontInfo& info);

    // The function called by public GetFamily(): it can return
    // wxFONTFAMILY_UNKNOWN unlike the public method (see comment there).
    virtual wxFontFamily DoGetFamily() const = 0;


    // Helper functions to recover wxFONTSTYLE/wxFONTWEIGHT and underlined flag
    // values from flags containing a combination of wxFONTFLAG_XXX.
    static wxFontStyle GetStyleFromFlags(int flags)
    {
        return flags & wxFONTFLAG_ITALIC
                        ? wxFONTSTYLE_ITALIC
                        : flags & wxFONTFLAG_SLANT
                            ? wxFONTSTYLE_SLANT
                            : wxFONTSTYLE_NORMAL;
    }

    static wxFontWeight GetWeightFromFlags(int flags)
    {
        return flags & wxFONTFLAG_LIGHT
                        ? wxFONTWEIGHT_LIGHT
                        : flags & wxFONTFLAG_BOLD
                            ? wxFONTWEIGHT_BOLD
                            : wxFONTWEIGHT_NORMAL;
    }

    static bool GetUnderlinedFromFlags(int flags)
    {
        return (flags & wxFONTFLAG_UNDERLINED) != 0;
    }

    static bool GetStrikethroughFromFlags(int flags)
    {
        return (flags & wxFONTFLAG_STRIKETHROUGH) != 0;
    }

private:
    // the currently default encoding: by default, it's the default system
    // encoding, but may be changed by the application using
    // SetDefaultEncoding() to make all subsequent fonts created without
    // specifying encoding parameter using this encoding
    static wxFontEncoding ms_encodingDefault;
};

// wxFontBase <-> wxString utilities, used by wxConfig
WXDLLIMPEXP_CORE wxString wxToString(const wxFontBase& font);
WXDLLIMPEXP_CORE bool wxFromString(const wxString& str, wxFontBase* font);


#if FUTURE_WXWIN_COMPATIBILITY_3_0
#define wxDECLARE_FONT_COMPAT_SETTER   \
    wxDEPRECATED_FUTURE( void SetFamily(int family) ) \
        { SetFamily((wxFontFamily)family); } \
    wxDEPRECATED_FUTURE( void SetStyle(int style) ) \
        { SetStyle((wxFontStyle)style); } \
    wxDEPRECATED_FUTURE( void SetWeight(int weight) ) \
        { SetWeight((wxFontWeight)weight); } \
    wxDEPRECATED_FUTURE( void SetFamily(wxDeprecatedGUIConstants family) ) \
        { SetFamily((wxFontFamily)family); } \
    wxDEPRECATED_FUTURE( void SetStyle(wxDeprecatedGUIConstants style) ) \
        { SetStyle((wxFontStyle)style); } \
    wxDEPRECATED_FUTURE( void SetWeight(wxDeprecatedGUIConstants weight) ) \
        { SetWeight((wxFontWeight)weight); }
#else
#define wxDECLARE_FONT_COMPAT_SETTER  /*empty*/
#endif

// this macro must be used in all derived wxFont classes declarations
#define wxDECLARE_COMMON_FONT_METHODS() \
    wxDECLARE_FONT_COMPAT_SETTER \
 \
    /* functions for modifying font in place */ \
    wxFont& MakeBold(); \
    wxFont& MakeItalic(); \
    wxFont& MakeUnderlined(); \
    wxFont& MakeStrikethrough(); \
    wxFont& MakeLarger() { return Scale(1.2f); } \
    wxFont& MakeSmaller() { return Scale(1/1.2f); } \
    wxFont& Scale(float x); \
    /* functions for creating fonts based on this one */ \
    wxFont Bold() const; \
    wxFont Italic() const; \
    wxFont Underlined() const; \
    wxFont Strikethrough() const; \
    wxFont Larger() const { return Scaled(1.2f); } \
    wxFont Smaller() const { return Scaled(1/1.2f); } \
    wxFont Scaled(float x) const

// include the real class declaration
#if defined(__WXMSW__)
    #include "wx/msw/font.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/font.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/font.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/font.h"
#elif defined(__WXX11__)
    #include "wx/x11/font.h"
#elif defined(__WXDFB__)
    #include "wx/dfb/font.h"
#elif defined(__WXMAC__)
    #include "wx/osx/font.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/font.h"
#elif defined(__WXPM__)
    #include "wx/os2/font.h"
#endif

class WXDLLIMPEXP_CORE wxFontList: public wxGDIObjListBase
{
public:
    wxFont *FindOrCreateFont(int pointSize,
                             wxFontFamily family,
                             wxFontStyle style,
                             wxFontWeight weight,
                             bool underline = false,
                             const wxString& face = wxEmptyString,
                             wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

#if FUTURE_WXWIN_COMPATIBILITY_3_0
     wxFont *FindOrCreateFont(int pointSize, int family, int style, int weight,
                              bool underline = false,
                              const wxString& face = wxEmptyString,
                              wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
        { return FindOrCreateFont(pointSize, (wxFontFamily)family, (wxFontStyle)style,
                                  (wxFontWeight)weight, underline, face, encoding); }
#endif

#if WXWIN_COMPATIBILITY_2_6
    wxDEPRECATED( void AddFont(wxFont*) );
    wxDEPRECATED( void RemoveFont(wxFont*) );
#endif
};

extern WXDLLIMPEXP_DATA_CORE(wxFontList*)    wxTheFontList;


// provide comparison operators to allow code such as
//
//      if ( font.GetStyle() == wxFONTSTYLE_SLANT )
//
// to compile without warnings which it would otherwise provoke from some
// compilers as it compares elements of different enums
#if FUTURE_WXWIN_COMPATIBILITY_3_0

// Unfortunately some compilers have ambiguity issues when enum comparisons are
// overloaded so we have to disable the overloads in this case, see
// wxCOMPILER_NO_OVERLOAD_ON_ENUM definition in wx/platform.h for more details.
#ifndef wxCOMPILER_NO_OVERLOAD_ON_ENUM

inline bool operator==(wxFontFamily s, wxDeprecatedGUIConstants t)
{ return static_cast<int>(s) == static_cast<int>(t); }
inline bool operator!=(wxFontFamily s, wxDeprecatedGUIConstants t)
{ return !(s == t); }
inline bool operator==(wxFontStyle s, wxDeprecatedGUIConstants t)
{ return static_cast<int>(s) == static_cast<int>(t); }
inline bool operator!=(wxFontStyle s, wxDeprecatedGUIConstants t)
{ return !(s == t); }
inline bool operator==(wxFontWeight s, wxDeprecatedGUIConstants t)
{ return static_cast<int>(s) == static_cast<int>(t); }
inline bool operator!=(wxFontWeight s, wxDeprecatedGUIConstants t)
{ return !(s == t); }

#endif // // wxCOMPILER_NO_OVERLOAD_ON_ENUM

#endif // FUTURE_WXWIN_COMPATIBILITY_3_0

#endif
    // _WX_FONT_H_BASE_
