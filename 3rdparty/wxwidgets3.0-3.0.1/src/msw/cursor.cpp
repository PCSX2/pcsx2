/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/cursor.cpp
// Purpose:     wxCursor class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) 1997-2003 Julian Smart and Vadim Zeitlin
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

#include "wx/cursor.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/bitmap.h"
    #include "wx/icon.h"
    #include "wx/settings.h"
    #include "wx/intl.h"
    #include "wx/image.h"
    #include "wx/module.h"
#endif

#include "wx/msw/private.h"
#include "wx/msw/missing.h" // IDC_HAND

// define functions missing in MicroWin
#ifdef __WXMICROWIN__
    static inline void DestroyCursor(HCURSOR) { }
    static inline void SetCursor(HCURSOR) { }
#endif // __WXMICROWIN__

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxCursorRefData : public wxGDIImageRefData
{
public:
    // the second parameter is used to tell us to delete the cursor when we're
    // done with it (normally we shouldn't call DestroyCursor() this is why it
    // doesn't happen by default)
    wxCursorRefData(HCURSOR hcursor = 0, bool takeOwnership = false);

    virtual ~wxCursorRefData() { Free(); }

    virtual void Free();


    // return the size of the standard cursor: notice that the system only
    // supports the cursors of this size
    static wxCoord GetStandardWidth();
    static wxCoord GetStandardHeight();

private:
    bool m_destroyCursor;

    // standard cursor size, computed on first use
    static wxSize ms_sizeStd;
};

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxCursor, wxGDIObject)

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

// Current cursor, in order to hang on to cursor handle when setting the cursor
// globally
static wxCursor *gs_globalCursor = NULL;

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

class wxCursorModule : public wxModule
{
public:
    virtual bool OnInit()
    {
        gs_globalCursor = new wxCursor;

        return true;
    }

    virtual void OnExit()
    {
        wxDELETE(gs_globalCursor);
    }
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxCursorRefData
// ----------------------------------------------------------------------------

wxSize wxCursorRefData::ms_sizeStd;

wxCoord wxCursorRefData::GetStandardWidth()
{
    if ( !ms_sizeStd.x )
        ms_sizeStd.x = wxSystemSettings::GetMetric(wxSYS_CURSOR_X);

    return ms_sizeStd.x;
}

wxCoord wxCursorRefData::GetStandardHeight()
{
    if ( !ms_sizeStd.y )
        ms_sizeStd.y = wxSystemSettings::GetMetric(wxSYS_CURSOR_Y);

    return ms_sizeStd.y;
}

wxCursorRefData::wxCursorRefData(HCURSOR hcursor, bool destroy)
{
    m_hCursor = (WXHCURSOR)hcursor;

    if ( m_hCursor )
    {
        m_width = GetStandardWidth();
        m_height = GetStandardHeight();
    }

    m_destroyCursor = destroy;
}

void wxCursorRefData::Free()
{
    if ( m_hCursor )
    {
#ifndef __WXWINCE__
        if ( m_destroyCursor )
            ::DestroyCursor((HCURSOR)m_hCursor);
#endif

        m_hCursor = 0;
    }
}

// ----------------------------------------------------------------------------
// Cursors
// ----------------------------------------------------------------------------

wxCursor::wxCursor()
{
}

#if wxUSE_IMAGE
wxCursor::wxCursor(const wxImage& image)
{
    // image has to be of the standard cursor size, otherwise we won't be able
    // to create it
    const int w = wxCursorRefData::GetStandardWidth();
    const int h = wxCursorRefData::GetStandardHeight();

    int hotSpotX = image.GetOptionInt(wxIMAGE_OPTION_CUR_HOTSPOT_X);
    int hotSpotY = image.GetOptionInt(wxIMAGE_OPTION_CUR_HOTSPOT_Y);
    int image_w = image.GetWidth();
    int image_h = image.GetHeight();

    wxASSERT_MSG( hotSpotX >= 0 && hotSpotX < image_w &&
                  hotSpotY >= 0 && hotSpotY < image_h,
                  wxT("invalid cursor hot spot coordinates") );

    wxImage imageSized(image); // final image of correct size

    // if image is too small then place it in the center, resize it if too big
    if ((w > image_w) && (h > image_h))
    {
        wxPoint offset((w - image_w)/2, (h - image_h)/2);
        hotSpotX = hotSpotX + offset.x;
        hotSpotY = hotSpotY + offset.y;

        imageSized = image.Size(wxSize(w, h), offset);
    }
    else if ((w != image_w) || (h != image_h))
    {
        hotSpotX = int(hotSpotX * double(w) / double(image_w));
        hotSpotY = int(hotSpotY * double(h) / double(image_h));

        imageSized = image.Scale(w, h);
    }

    HCURSOR hcursor = wxBitmapToHCURSOR( wxBitmap(imageSized),
                                         hotSpotX, hotSpotY );

    if ( !hcursor )
    {
        wxLogWarning(_("Failed to create cursor."));
        return;
    }

    m_refData = new wxCursorRefData(hcursor, true /* delete it later */);
}
#endif // wxUSE_IMAGE

// MicroWin doesn't have support needed for the other ctors
#ifdef __WXMICROWIN__

wxCursor::InitFromStock(wxStockCursor WXUNUSED(cursor_type))
{
}

#else // !__WXMICROWIN__

wxCursor::wxCursor(const wxString& filename,
                   wxBitmapType kind,
                   int hotSpotX,
                   int hotSpotY)
{
    HCURSOR hcursor;
    switch ( kind )
    {
        case wxBITMAP_TYPE_CUR_RESOURCE:
            hcursor = ::LoadCursor(wxGetInstance(), filename.t_str());
            break;

#ifndef __WXWINCE__
        case wxBITMAP_TYPE_ANI:
        case wxBITMAP_TYPE_CUR:
            hcursor = ::LoadCursorFromFile(filename.t_str());
            break;
#endif

        case wxBITMAP_TYPE_ICO:
            hcursor = wxBitmapToHCURSOR
                      (
                       wxIcon(filename, wxBITMAP_TYPE_ICO),
                       hotSpotX,
                       hotSpotY
                      );
            break;

        case wxBITMAP_TYPE_BMP:
            hcursor = wxBitmapToHCURSOR
                      (
                       wxBitmap(filename, wxBITMAP_TYPE_BMP),
                       hotSpotX,
                       hotSpotY
                      );
            break;

        default:
            wxLogError( wxT("unknown cursor resource type '%d'"), kind );

            hcursor = NULL;
    }

    if ( hcursor )
    {
        m_refData = new wxCursorRefData(hcursor, true /* delete it later */);
    }
}

namespace
{

void ReverseBitmap(HBITMAP bitmap, int width, int height)
{
    MemoryHDC hdc;
    SelectInHDC selBitmap(hdc, bitmap);
    ::StretchBlt(hdc, width - 1, 0, -width, height,
                 hdc, 0, 0, width, height, SRCCOPY);
}

HCURSOR CreateReverseCursor(HCURSOR cursor)
{
    ICONINFO info;
    if ( !::GetIconInfo(cursor, &info) )
        return NULL;

    HCURSOR cursorRev = NULL;

    BITMAP bmp;
    if ( ::GetObject(info.hbmMask, sizeof(bmp), &bmp) )
    {
        ReverseBitmap(info.hbmMask, bmp.bmWidth, bmp.bmHeight);
        if ( info.hbmColor )
            ReverseBitmap(info.hbmColor, bmp.bmWidth, bmp.bmHeight);
        info.xHotspot = (DWORD)bmp.bmWidth - 1 - info.xHotspot;

        cursorRev = ::CreateIconIndirect(&info);
    }

    ::DeleteObject(info.hbmMask);
    if ( info.hbmColor )
        ::DeleteObject(info.hbmColor);

    return cursorRev;
}

} // anonymous namespace

// Cursors by stock number
void wxCursor::InitFromStock(wxStockCursor idCursor)
{
    // all wxWidgets standard cursors
    static const struct StdCursor
    {
        // is this a standard Windows cursor?
        bool isStd;

        // the cursor name or id
        LPCTSTR name;
    } stdCursors[] =
    {
        {  true, NULL                        }, // wxCURSOR_NONE
        {  true, IDC_ARROW                   }, // wxCURSOR_ARROW
        { false, wxT("WXCURSOR_RIGHT_ARROW")  }, // wxCURSOR_RIGHT_ARROW
        { false, wxT("WXCURSOR_BULLSEYE")     }, // wxCURSOR_BULLSEYE
        {  true, IDC_ARROW                   }, // WXCURSOR_CHAR
        {  true, IDC_CROSS                   }, // WXCURSOR_CROSS
        {  true, IDC_HAND                    }, // wxCURSOR_HAND
        {  true, IDC_IBEAM                   }, // WXCURSOR_IBEAM
        {  true, IDC_ARROW                   }, // WXCURSOR_LEFT_BUTTON
        { false, wxT("WXCURSOR_MAGNIFIER")    }, // wxCURSOR_MAGNIFIER
        {  true, IDC_ARROW                   }, // WXCURSOR_MIDDLE_BUTTON
        {  true, IDC_NO                      }, // WXCURSOR_NO_ENTRY
        { false, wxT("WXCURSOR_PBRUSH")       }, // wxCURSOR_PAINT_BRUSH
        { false, wxT("WXCURSOR_PENCIL")       }, // wxCURSOR_PENCIL
        { false, wxT("WXCURSOR_PLEFT")        }, // wxCURSOR_POINT_LEFT
        { false, wxT("WXCURSOR_PRIGHT")       }, // wxCURSOR_POINT_RIGHT
        {  true, IDC_HELP                    }, // WXCURSOR_QUESTION_ARROW
        {  true, IDC_ARROW                   }, // WXCURSOR_RIGHT_BUTTON
        {  true, IDC_SIZENESW                }, // WXCURSOR_SIZENESW
        {  true, IDC_SIZENS                  }, // WXCURSOR_SIZENS
        {  true, IDC_SIZENWSE                }, // WXCURSOR_SIZENWSE
        {  true, IDC_SIZEWE                  }, // WXCURSOR_SIZEWE
        {  true, IDC_SIZEALL                 }, // WXCURSOR_SIZING
        { false, wxT("WXCURSOR_PBRUSH")       }, // wxCURSOR_SPRAYCAN
        {  true, IDC_WAIT                    }, // WXCURSOR_WAIT
        {  true, IDC_WAIT                    }, // WXCURSOR_WATCH
        { false, wxT("WXCURSOR_BLANK")        }, // wxCURSOR_BLANK
        {  true, IDC_APPSTARTING             }, // wxCURSOR_ARROWWAIT

        // no entry for wxCURSOR_MAX
    };

    wxCOMPILE_TIME_ASSERT( WXSIZEOF(stdCursors) == wxCURSOR_MAX,
                           CursorsIdArrayMismatch );

    wxCHECK_RET( idCursor > 0 && (size_t)idCursor < WXSIZEOF(stdCursors),
                 wxT("invalid cursor id in wxCursor() ctor") );

    const StdCursor& stdCursor = stdCursors[idCursor];
    bool deleteLater = !stdCursor.isStd;

    HCURSOR hcursor = ::LoadCursor(stdCursor.isStd ? NULL : wxGetInstance(),
                                   stdCursor.name);

    // IDC_HAND may not be available on some versions of Windows.
    if ( !hcursor && idCursor == wxCURSOR_HAND)
    {
        hcursor = ::LoadCursor(wxGetInstance(), wxT("WXCURSOR_HAND"));
        deleteLater = true;
    }

    if ( !hcursor && idCursor == wxCURSOR_RIGHT_ARROW)
    {
        hcursor = ::LoadCursor(NULL, IDC_ARROW);
        if ( hcursor )
        {
            hcursor = CreateReverseCursor(hcursor);
            deleteLater = true;
        }
    }

    if ( !hcursor )
    {
        if ( !stdCursor.isStd )
        {
            // it may be not obvious to the programmer why did loading fail,
            // try to help by pointing to the by far the most probable reason
            wxFAIL_MSG(wxT("Loading a cursor defined by wxWidgets failed, ")
                       wxT("did you include include/wx/msw/wx.rc file from ")
                       wxT("your resource file?"));
        }

        wxLogLastError(wxT("LoadCursor"));
    }
    else
    {
        m_refData = new wxCursorRefData(hcursor, deleteLater);
    }
}

#endif // __WXMICROWIN__/!__WXMICROWIN__

wxCursor::~wxCursor()
{
}

// ----------------------------------------------------------------------------
// other wxCursor functions
// ----------------------------------------------------------------------------

wxGDIImageRefData *wxCursor::CreateData() const
{
    return new wxCursorRefData;
}

// ----------------------------------------------------------------------------
// Global cursor setting
// ----------------------------------------------------------------------------

const wxCursor *wxGetGlobalCursor()
{
    return gs_globalCursor;
}

void wxSetCursor(const wxCursor& cursor)
{
    if ( cursor.IsOk() )
    {
        ::SetCursor(GetHcursorOf(cursor));

        if ( gs_globalCursor )
            *gs_globalCursor = cursor;
    }
}
