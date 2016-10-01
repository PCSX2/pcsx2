/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/font.cpp
// Purpose:     wxFont class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) wxWidgets team
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

#include "wx/font.h"

#ifndef WX_PRECOMP
    #include "wx/list.h"
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/msw/private.h"
#endif // WX_PRECOMP

#include "wx/encinfo.h"
#include "wx/fontutil.h"
#include "wx/fontmap.h"

#ifndef __WXWINCE__
    #include "wx/sysopt.h"
#endif

#include "wx/scopeguard.h"
#include "wx/tokenzr.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the mask used to extract the pitch from LOGFONT::lfPitchAndFamily field
static const int PITCH_MASK = FIXED_PITCH | VARIABLE_PITCH;

// ----------------------------------------------------------------------------
// wxFontRefData - the internal description of the font
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxFontRefData: public wxGDIRefData
{
public:
    // constructors
    wxFontRefData()
    {
        Init(-1, wxSize(0,0), false, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
             wxFONTWEIGHT_NORMAL, false, false, wxEmptyString,
             wxFONTENCODING_DEFAULT);
    }

    wxFontRefData(int size,
                  const wxSize& pixelSize,
                  bool sizeUsingPixels,
                  wxFontFamily family,
                  wxFontStyle style,
                  wxFontWeight weight,
                  bool underlined,
                  bool strikethrough,
                  const wxString& faceName,
                  wxFontEncoding encoding)
    {
        Init(size, pixelSize, sizeUsingPixels, family, style, weight,
             underlined, strikethrough, faceName, encoding);
    }

    wxFontRefData(const wxNativeFontInfo& info, WXHFONT hFont = 0)
    {
        Init(info, hFont);
    }

    wxFontRefData(const wxFontRefData& data) : wxGDIRefData()
    {
        Init(data.m_nativeFontInfo);
    }

    virtual ~wxFontRefData();

    // operations
    bool Alloc();

    void Free();

    // all wxFont accessors
    int GetPointSize() const
    {
        return m_nativeFontInfo.GetPointSize();
    }

    wxSize GetPixelSize() const
    {
        return m_nativeFontInfo.GetPixelSize();
    }

    bool IsUsingSizeInPixels() const
    {
        return m_sizeUsingPixels;
    }

    wxFontFamily GetFamily() const
    {
        return m_nativeFontInfo.GetFamily();
    }

    wxFontStyle GetStyle() const
    {
        return m_nativeFontInfo.GetStyle();
    }

    wxFontWeight GetWeight() const
    {
        return m_nativeFontInfo.GetWeight();
    }

    bool GetUnderlined() const
    {
        return m_nativeFontInfo.GetUnderlined();
    }

    bool GetStrikethrough() const
    {
        return m_nativeFontInfo.GetStrikethrough();
    }

    wxString GetFaceName() const
    {
        wxString facename = m_nativeFontInfo.GetFaceName();
        if ( facename.empty() )
        {
            facename = GetMSWFaceName();
            if ( !facename.empty() )
            {
                // cache the face name, it shouldn't change unless the family
                // does and wxNativeFontInfo::SetFamily() resets the face name
                // Don't call this->SetFaceName(), because it deletes the HFONT.
                const_cast<wxNativeFontInfo &>(m_nativeFontInfo).SetFaceName(facename);
            }
        }

        return facename;
    }

    wxFontEncoding GetEncoding() const
    {
        return m_nativeFontInfo.GetEncoding();
    }

    WXHFONT GetHFONT() const
    {
        AllocIfNeeded();

        return (WXHFONT)m_hFont;
    }

    bool HasHFONT() const
    {
        return m_hFont != 0;
    }

    // ... and setters: notice that all of them invalidate the currently
    // allocated HFONT, if any, so that the next call to GetHFONT() recreates a
    // new one
    void SetPointSize(int pointSize)
    {
        Free();

        m_nativeFontInfo.SetPointSize(pointSize);
        m_sizeUsingPixels = false;
    }

    void SetPixelSize(const wxSize& pixelSize)
    {
        wxCHECK_RET( pixelSize.GetWidth() >= 0, "negative font width" );
        wxCHECK_RET( pixelSize.GetHeight() != 0, "zero font height" );

        Free();

        m_nativeFontInfo.SetPixelSize(pixelSize);
        m_sizeUsingPixels = true;
    }

    void SetFamily(wxFontFamily family)
    {
        Free();

        m_nativeFontInfo.SetFamily(family);
    }

    void SetStyle(wxFontStyle style)
    {
        Free();

        m_nativeFontInfo.SetStyle(style);
    }

    void SetWeight(wxFontWeight weight)
    {
        Free();

        m_nativeFontInfo.SetWeight(weight);
    }

    bool SetFaceName(const wxString& faceName)
    {
        Free();

        return m_nativeFontInfo.SetFaceName(faceName);
    }

    void SetUnderlined(bool underlined)
    {
        Free();

        m_nativeFontInfo.SetUnderlined(underlined);
    }

    void SetStrikethrough(bool strikethrough)
    {
        Free();

        m_nativeFontInfo.SetStrikethrough(strikethrough);
    }

    void SetEncoding(wxFontEncoding encoding)
    {
        Free();

        m_nativeFontInfo.SetEncoding(encoding);
    }

    const wxNativeFontInfo& GetNativeFontInfo() const
    {
        // we need to create the font now to get the corresponding LOGFONT if
        // it hadn't been done yet
        AllocIfNeeded();

        // ensure that we have a valid face name in our font information:
        // GetFaceName() will try to retrieve it from our HFONT and save it if
        // it was successful
        (void)GetFaceName();

        return m_nativeFontInfo;
    }

    void SetNativeFontInfo(const wxNativeFontInfo& nativeFontInfo)
    {
        Free();

        m_nativeFontInfo = nativeFontInfo;
    }

protected:
    // common part of all ctors
    void Init(int size,
              const wxSize& pixelSize,
              bool sizeUsingPixels,
              wxFontFamily family,
              wxFontStyle style,
              wxFontWeight weight,
              bool underlined,
              bool strikethrough,
              const wxString& faceName,
              wxFontEncoding encoding);

    void Init(const wxNativeFontInfo& info, WXHFONT hFont = 0);

    void AllocIfNeeded() const
    {
        if ( !m_hFont )
            const_cast<wxFontRefData *>(this)->Alloc();
    }

    // retrieve the face name really being used by the font: this is used to
    // get the face name selected by the system when we don't specify it (but
    // use just the family for example)
    wxString GetMSWFaceName() const
    {
        ScreenHDC hdc;
        SelectInHDC selectFont(hdc, (HFONT)GetHFONT());

        UINT otmSize = GetOutlineTextMetrics(hdc, 0, NULL);
        if ( !otmSize )
        {
            wxLogLastError("GetOutlineTextMetrics(NULL)");
            return wxString();
        }

        OUTLINETEXTMETRIC * const
            otm = static_cast<OUTLINETEXTMETRIC *>(malloc(otmSize));
        wxON_BLOCK_EXIT1( free, otm );

        otm->otmSize = otmSize;
        if ( !GetOutlineTextMetrics(hdc, otmSize, otm) )
        {
            wxLogLastError("GetOutlineTextMetrics()");
            return wxString();
        }

        // in spite of its type, the otmpFamilyName field of OUTLINETEXTMETRIC
        // gives an offset in _bytes_ of the face (not family!) name from the
        // struct start while the name itself is an array of TCHARs
        //
        // FWIW otmpFaceName contains the same thing as otmpFamilyName followed
        // by a possible " Italic" or " Bold" or something else suffix
        return reinterpret_cast<wxChar *>(otm) +
                    wxPtrToUInt(otm->otmpFamilyName)/sizeof(wxChar);
    }

    // are we using m_nativeFontInfo.lf.lfHeight for point size or pixel size?
    bool             m_sizeUsingPixels;

    // Windows font handle, created on demand in GetHFONT()
    HFONT            m_hFont;

    // Native font info
    wxNativeFontInfo m_nativeFontInfo;
};

#define M_FONTDATA ((wxFontRefData*)m_refData)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFontRefData
// ----------------------------------------------------------------------------

void wxFontRefData::Init(int pointSize,
                         const wxSize& pixelSize,
                         bool sizeUsingPixels,
                         wxFontFamily family,
                         wxFontStyle style,
                         wxFontWeight weight,
                         bool underlined,
                         bool strikethrough,
                         const wxString& faceName,
                         wxFontEncoding encoding)
{
    m_hFont = NULL;

    m_sizeUsingPixels = sizeUsingPixels;
    if ( m_sizeUsingPixels )
        SetPixelSize(pixelSize);
    else
        SetPointSize(pointSize == -1 ? wxNORMAL_FONT->GetPointSize() : pointSize);

    SetStyle(style);
    SetWeight(weight);
    SetUnderlined(underlined);
    SetStrikethrough(strikethrough);

    // set the family/facename
    SetFamily(family);
    if ( !faceName.empty() )
        SetFaceName(faceName);

    // deal with encoding now (it may override the font family and facename
    // so do it after setting them)
    SetEncoding(encoding);
}

void wxFontRefData::Init(const wxNativeFontInfo& info, WXHFONT hFont)
{
    // hFont may be zero, or it be passed in case we really want to
    // use the exact font created in the underlying system
    // (for example where we can't guarantee conversion from HFONT
    // to LOGFONT back to HFONT)
    m_hFont = (HFONT)hFont;
    m_nativeFontInfo = info;

    // size of native fonts is expressed in pixels
    m_sizeUsingPixels = true;
}

wxFontRefData::~wxFontRefData()
{
    Free();
}

bool wxFontRefData::Alloc()
{
    m_hFont = ::CreateFontIndirect(&m_nativeFontInfo.lf);
    if ( !m_hFont )
    {
        wxLogLastError(wxT("CreateFont"));
        return false;
    }

    return true;
}

void wxFontRefData::Free()
{
    if ( m_hFont )
    {
        if ( !::DeleteObject(m_hFont) )
        {
            wxLogLastError(wxT("DeleteObject(font)"));
        }

        m_hFont = 0;
    }
}

// ----------------------------------------------------------------------------
// wxNativeFontInfo
// ----------------------------------------------------------------------------

void wxNativeFontInfo::Init()
{
    wxZeroMemory(lf);

    // we get better font quality if we use PROOF_QUALITY instead of
    // DEFAULT_QUALITY but some fonts (e.g. "Terminal 6pt") are not available
    // then so we allow to set a global option to choose between quality and
    // wider font selection
#ifdef __WXWINCE__
    lf.lfQuality = CLEARTYPE_QUALITY;
#else
    lf.lfQuality = wxSystemOptions::GetOptionInt("msw.font.no-proof-quality")
                    ? DEFAULT_QUALITY
                    : PROOF_QUALITY;
#endif
}

int wxNativeFontInfo::GetPointSize() const
{
    // FIXME: using the screen here results in incorrect font size calculation
    //        for printing!
    const int ppInch = ::GetDeviceCaps(ScreenHDC(), LOGPIXELSY);

    // BC++ 2007 doesn't provide abs(long) overload, hence the cast
    return (int) (((72.0*abs((int)lf.lfHeight)) / (double) ppInch) + 0.5);
}

wxSize wxNativeFontInfo::GetPixelSize() const
{
    wxSize ret;
    ret.SetHeight(abs((int)lf.lfHeight));
    ret.SetWidth(lf.lfWidth);
    return ret;
}

wxFontStyle wxNativeFontInfo::GetStyle() const
{
    return lf.lfItalic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;
}

wxFontWeight wxNativeFontInfo::GetWeight() const
{
    if ( lf.lfWeight <= 300 )
        return wxFONTWEIGHT_LIGHT;

    if ( lf.lfWeight >= 600 )
        return wxFONTWEIGHT_BOLD;

    return wxFONTWEIGHT_NORMAL;
}

bool wxNativeFontInfo::GetUnderlined() const
{
    return lf.lfUnderline != 0;
}

bool wxNativeFontInfo::GetStrikethrough() const
{
    return lf.lfStrikeOut != 0;
}

wxString wxNativeFontInfo::GetFaceName() const
{
    return lf.lfFaceName;
}

wxFontFamily wxNativeFontInfo::GetFamily() const
{
    wxFontFamily family;

    // extract family from pitch-and-family
    switch ( lf.lfPitchAndFamily & ~PITCH_MASK )
    {
        case 0:
            family = wxFONTFAMILY_UNKNOWN;
            break;

        case FF_ROMAN:
            family = wxFONTFAMILY_ROMAN;
            break;

        case FF_SWISS:
            family = wxFONTFAMILY_SWISS;
            break;

        case FF_SCRIPT:
            family = wxFONTFAMILY_SCRIPT;
            break;

        case FF_MODERN:
            family = wxFONTFAMILY_MODERN;
            break;

        case FF_DECORATIVE:
            family = wxFONTFAMILY_DECORATIVE;
            break;

        default:
            wxFAIL_MSG( "unknown LOGFONT::lfFamily value" );
            family = wxFONTFAMILY_UNKNOWN;
                // just to avoid a warning
    }

    return family;
}

wxFontEncoding wxNativeFontInfo::GetEncoding() const
{
    return wxGetFontEncFromCharSet(lf.lfCharSet);
}

void wxNativeFontInfo::SetPointSize(int pointsize)
{
    // FIXME: using the screen here results in incorrect font size calculation
    //        for printing!
    const int ppInch = ::GetDeviceCaps(ScreenHDC(), LOGPIXELSY);

    lf.lfHeight = -(int)((pointsize*((double)ppInch)/72.0) + 0.5);
}

void wxNativeFontInfo::SetPixelSize(const wxSize& pixelSize)
{
    // MSW accepts both positive and negative heights here but they mean
    // different things: positive specifies the cell height while negative
    // specifies the character height. We used to just pass the value to MSW
    // unchanged but changed the behaviour for positive values in 2.9.1 to
    // match other ports and, more importantly, the expected behaviour. So now
    // passing the negative height doesn't make sense at all any more but we
    // still accept it for compatibility with the existing code which worked
    // around the wrong interpretation of the height argument in older wxMSW
    // versions by passing a negative value explicitly itself.
    lf.lfHeight = -abs(pixelSize.GetHeight());
    lf.lfWidth = pixelSize.GetWidth();
}

void wxNativeFontInfo::SetStyle(wxFontStyle style)
{
    switch ( style )
    {
        default:
            wxFAIL_MSG( "unknown font style" );
            // fall through

        case wxFONTSTYLE_NORMAL:
            lf.lfItalic = FALSE;
            break;

        case wxFONTSTYLE_ITALIC:
        case wxFONTSTYLE_SLANT:
            lf.lfItalic = TRUE;
            break;
    }
}

void wxNativeFontInfo::SetWeight(wxFontWeight weight)
{
    switch ( weight )
    {
        default:
            wxFAIL_MSG( "unknown font weight" );
            // fall through

        case wxFONTWEIGHT_NORMAL:
            lf.lfWeight = FW_NORMAL;
            break;

        case wxFONTWEIGHT_LIGHT:
            lf.lfWeight = FW_LIGHT;
            break;

        case wxFONTWEIGHT_BOLD:
            lf.lfWeight = FW_BOLD;
            break;
    }
}

void wxNativeFontInfo::SetUnderlined(bool underlined)
{
    lf.lfUnderline = underlined;
}

void wxNativeFontInfo::SetStrikethrough(bool strikethrough)
{
    lf.lfStrikeOut = strikethrough;
}

bool wxNativeFontInfo::SetFaceName(const wxString& facename)
{
    wxStrlcpy(lf.lfFaceName, facename.c_str(), WXSIZEOF(lf.lfFaceName));
    return true;
}

void wxNativeFontInfo::SetFamily(wxFontFamily family)
{
    BYTE ff_family = FF_DONTCARE;

    switch ( family )
    {
        case wxFONTFAMILY_SCRIPT:
            ff_family = FF_SCRIPT;
            break;

        case wxFONTFAMILY_DECORATIVE:
            ff_family = FF_DECORATIVE;
            break;

        case wxFONTFAMILY_ROMAN:
            ff_family = FF_ROMAN;
            break;

        case wxFONTFAMILY_TELETYPE:
        case wxFONTFAMILY_MODERN:
            ff_family = FF_MODERN;
            break;

        case wxFONTFAMILY_SWISS:
        case wxFONTFAMILY_DEFAULT:
            ff_family = FF_SWISS;
            break;

        case wxFONTFAMILY_UNKNOWN:
            wxFAIL_MSG( "invalid font family" );
            return;
    }

    wxCHECK_RET( ff_family != FF_DONTCARE, "unknown wxFontFamily" );

    lf.lfPitchAndFamily = (BYTE)(DEFAULT_PITCH) | ff_family;

    // reset the facename so that CreateFontIndirect() will automatically choose a
    // face name based only on the font family.
    lf.lfFaceName[0] = '\0';
}

void wxNativeFontInfo::SetEncoding(wxFontEncoding encoding)
{
    wxNativeEncodingInfo info;
    if ( !wxGetNativeFontEncoding(encoding, &info) )
    {
#if wxUSE_FONTMAP
        if ( wxFontMapper::Get()->GetAltForEncoding(encoding, &info) )
        {
            if ( !info.facename.empty() )
            {
                // if we have this encoding only in some particular facename, use
                // the facename - it is better to show the correct characters in a
                // wrong facename than unreadable text in a correct one
                SetFaceName(info.facename);
            }
        }
        else
#endif // wxUSE_FONTMAP
        {
            // unsupported encoding, replace with the default
            info.charset = DEFAULT_CHARSET;
        }
    }

    lf.lfCharSet = (BYTE)info.charset;
}

bool wxNativeFontInfo::FromString(const wxString& s)
{
    long l;

    wxStringTokenizer tokenizer(s, wxS(";"), wxTOKEN_RET_EMPTY_ALL);

    // first the version
    wxString token = tokenizer.GetNextToken();
    if ( token != wxS('0') )
        return false;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfHeight = l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfWidth = l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfEscapement = l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfOrientation = l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfWeight = l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfItalic = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfUnderline = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfStrikeOut = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfCharSet = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfOutPrecision = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfClipPrecision = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfQuality = (BYTE)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return false;
    lf.lfPitchAndFamily = (BYTE)l;

    if ( !tokenizer.HasMoreTokens() )
        return false;

    // the face name may be empty
    SetFaceName(tokenizer.GetNextToken());

    return true;
}

wxString wxNativeFontInfo::ToString() const
{
    wxString s;

    s.Printf(wxS("%d;%ld;%ld;%ld;%ld;%ld;%d;%d;%d;%d;%d;%d;%d;%d;%s"),
             0, // version, in case we want to change the format later
             lf.lfHeight,
             lf.lfWidth,
             lf.lfEscapement,
             lf.lfOrientation,
             lf.lfWeight,
             lf.lfItalic,
             lf.lfUnderline,
             lf.lfStrikeOut,
             lf.lfCharSet,
             lf.lfOutPrecision,
             lf.lfClipPrecision,
             lf.lfQuality,
             lf.lfPitchAndFamily,
             lf.lfFaceName);

    return s;
}

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

wxFont::wxFont(const wxString& fontdesc)
{
    wxNativeFontInfo info;
    if ( info.FromString(fontdesc) )
        (void)Create(info);
}

wxFont::wxFont(const wxFontInfo& info)
{
    m_refData = new wxFontRefData(info.GetPointSize(),
                                  info.GetPixelSize(),
                                  info.IsUsingSizeInPixels(),
                                  info.GetFamily(),
                                  info.GetStyle(),
                                  info.GetWeight(),
                                  info.IsUnderlined(),
                                  info.IsStrikethrough(),
                                  info.GetFaceName(),
                                  info.GetEncoding());
}

bool wxFont::Create(const wxNativeFontInfo& info, WXHFONT hFont)
{
    UnRef();

    m_refData = new wxFontRefData(info, hFont);

    return RealizeResource();
}

bool wxFont::DoCreate(int pointSize,
                      const wxSize& pixelSize,
                      bool sizeUsingPixels,
                      wxFontFamily family,
                      wxFontStyle style,
                      wxFontWeight weight,
                      bool underlined,
                      const wxString& faceName,
                      wxFontEncoding encoding)
{
    UnRef();

    // wxDEFAULT is a valid value for the font size too so we must treat it
    // specially here (otherwise the size would be 70 == wxDEFAULT value)
    if ( pointSize == wxDEFAULT )
    {
        pointSize = wxNORMAL_FONT->GetPointSize();
    }

    m_refData = new wxFontRefData(pointSize, pixelSize, sizeUsingPixels,
                                  family, style, weight,
                                  underlined, false, faceName, encoding);

    return RealizeResource();
}

wxFont::~wxFont()
{
}

// ----------------------------------------------------------------------------
// real implementation
// ----------------------------------------------------------------------------

wxGDIRefData *wxFont::CreateGDIRefData() const
{
    return new wxFontRefData();
}

wxGDIRefData *wxFont::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxFontRefData(*static_cast<const wxFontRefData *>(data));
}

bool wxFont::RealizeResource()
{
    // NOTE: the GetHFONT() call automatically triggers a reallocation of
    //       the HFONT if necessary (will do nothing if we already have the resource);
    //       it returns NULL only if there is a failure in wxFontRefData::Alloc()...
    return GetHFONT() != NULL;
}

bool wxFont::FreeResource(bool WXUNUSED(force))
{
    if ( !M_FONTDATA )
        return false;

    M_FONTDATA->Free();

    return true;
}

WXHANDLE wxFont::GetResourceHandle() const
{
    return (WXHANDLE)GetHFONT();
}

WXHFONT wxFont::GetHFONT() const
{
    // NOTE: wxFontRefData::GetHFONT() will automatically call
    //       wxFontRefData::Alloc() if necessary
    return M_FONTDATA ? M_FONTDATA->GetHFONT() : 0;
}

bool wxFont::IsFree() const
{
    return M_FONTDATA && !M_FONTDATA->HasHFONT();
}

// ----------------------------------------------------------------------------
// change font attribute: we recreate font when doing it
// ----------------------------------------------------------------------------

void wxFont::SetPointSize(int pointSize)
{
    AllocExclusive();

    M_FONTDATA->Free();
    M_FONTDATA->SetPointSize(pointSize);
}

void wxFont::SetPixelSize(const wxSize& pixelSize)
{
    AllocExclusive();

    M_FONTDATA->SetPixelSize(pixelSize);
}

void wxFont::SetFamily(wxFontFamily family)
{
    AllocExclusive();

    M_FONTDATA->SetFamily(family);
}

void wxFont::SetStyle(wxFontStyle style)
{
    AllocExclusive();

    M_FONTDATA->SetStyle(style);
}

void wxFont::SetWeight(wxFontWeight weight)
{
    AllocExclusive();

    M_FONTDATA->SetWeight(weight);
}

bool wxFont::SetFaceName(const wxString& faceName)
{
    AllocExclusive();

    if ( !M_FONTDATA->SetFaceName(faceName) )
        return false;

    // NB: using win32's GetObject() API on M_FONTDATA->GetHFONT()
    //     to retrieve a LOGFONT and then compare lf.lfFaceName
    //     with given facename is not reliable at all:
    //     Windows copies the facename given to ::CreateFontIndirect()
    //     without any validity check.
    //     Thus we use wxFontBase::SetFaceName to check if facename
    //     is valid...
    return wxFontBase::SetFaceName(faceName);
}

void wxFont::SetUnderlined(bool underlined)
{
    AllocExclusive();

    M_FONTDATA->SetUnderlined(underlined);
}

void wxFont::SetStrikethrough(bool strikethrough)
{
    AllocExclusive();

    M_FONTDATA->SetStrikethrough(strikethrough);
}

void wxFont::SetEncoding(wxFontEncoding encoding)
{
    AllocExclusive();

    M_FONTDATA->SetEncoding(encoding);
}

void wxFont::DoSetNativeFontInfo(const wxNativeFontInfo& info)
{
    AllocExclusive();

    M_FONTDATA->SetNativeFontInfo(info);
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

int wxFont::GetPointSize() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid font") );

    return M_FONTDATA->GetPointSize();
}

wxSize wxFont::GetPixelSize() const
{
    wxCHECK_MSG( IsOk(), wxDefaultSize, wxT("invalid font") );

    return M_FONTDATA->GetPixelSize();
}

bool wxFont::IsUsingSizeInPixels() const
{
    wxCHECK_MSG( IsOk(), 0, wxT("invalid font") );

    return M_FONTDATA->IsUsingSizeInPixels();
}

wxFontFamily wxFont::DoGetFamily() const
{
    return M_FONTDATA->GetFamily();
}

wxFontStyle wxFont::GetStyle() const
{
    wxCHECK_MSG( IsOk(), wxFONTSTYLE_MAX, wxT("invalid font") );

    return M_FONTDATA->GetStyle();
}

wxFontWeight wxFont::GetWeight() const
{
    wxCHECK_MSG( IsOk(), wxFONTWEIGHT_MAX, wxT("invalid font") );

    return M_FONTDATA->GetWeight();
}

bool wxFont::GetUnderlined() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid font") );

    return M_FONTDATA->GetUnderlined();
}

bool wxFont::GetStrikethrough() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid font") );

    return M_FONTDATA->GetStrikethrough();
}

wxString wxFont::GetFaceName() const
{
    wxCHECK_MSG( IsOk(), wxEmptyString, wxT("invalid font") );

    return M_FONTDATA->GetFaceName();
}

wxFontEncoding wxFont::GetEncoding() const
{
    wxCHECK_MSG( IsOk(), wxFONTENCODING_DEFAULT, wxT("invalid font") );

    return M_FONTDATA->GetEncoding();
}

const wxNativeFontInfo *wxFont::GetNativeFontInfo() const
{
    return IsOk() ? &(M_FONTDATA->GetNativeFontInfo()) : NULL;
}

bool wxFont::IsFixedWidth() const
{
    wxCHECK_MSG( IsOk(), false, wxT("invalid font") );

    // LOGFONT doesn't contain the correct pitch information so we need to call
    // GetTextMetrics() to get it
    ScreenHDC hdc;
    SelectInHDC selectFont(hdc, M_FONTDATA->GetHFONT());

    TEXTMETRIC tm;
    if ( !::GetTextMetrics(hdc, &tm) )
    {
        wxLogLastError(wxT("GetTextMetrics"));
        return false;
    }

    // Quoting MSDN description of TMPF_FIXED_PITCH: "Note very carefully that
    // those meanings are the opposite of what the constant name implies."
    return !(tm.tmPitchAndFamily & TMPF_FIXED_PITCH);
}
