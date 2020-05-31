/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/artmsw.cpp
// Purpose:     stock wxArtProvider instance with native MSW stock icons
// Author:      Vaclav Slavik
// Modified by:
// Created:     2008-10-15
// Copyright:   (c) Vaclav Slavik, 2008
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/artprov.h"
#include "wx/image.h"
#include "wx/dynlib.h"
#include "wx/volume.h"
#include "wx/msw/private.h"
#include "wx/msw/wrapwin.h"

#ifdef SHGSI_ICON
    #define wxHAS_SHGetStockIconInfo
#endif

namespace
{

#ifdef wxHAS_SHGetStockIconInfo

SHSTOCKICONID MSWGetStockIconIdForArtProviderId(const wxArtID& art_id)
{
    // try to find an equivalent MSW stock icon id for wxArtID
    if ( art_id == wxART_ERROR)             return SIID_ERROR;
    else if ( art_id == wxART_QUESTION )    return SIID_HELP;
    else if ( art_id == wxART_WARNING )     return SIID_WARNING;
    else if ( art_id == wxART_INFORMATION ) return SIID_INFO;
    else if ( art_id == wxART_HELP )        return SIID_HELP;
    else if ( art_id == wxART_FOLDER )      return SIID_FOLDER;
    else if ( art_id == wxART_FOLDER_OPEN ) return SIID_FOLDEROPEN;
    else if ( art_id == wxART_DELETE )      return SIID_DELETE;
    else if ( art_id == wxART_FIND )        return SIID_FIND;
    else if ( art_id == wxART_HARDDISK )    return SIID_DRIVEFIXED;
    else if ( art_id == wxART_FLOPPY )      return SIID_DRIVE35;
    else if ( art_id == wxART_CDROM )       return SIID_DRIVECD;
    else if ( art_id == wxART_REMOVABLE )   return SIID_DRIVEREMOVE;

    return SIID_INVALID;
};


// try to load SHGetStockIconInfo dynamically, so this code runs
// even on pre-Vista Windows versions
HRESULT
MSW_SHGetStockIconInfo(SHSTOCKICONID siid,
                       UINT uFlags,
                       SHSTOCKICONINFO *psii)
{
    typedef HRESULT (WINAPI *PSHGETSTOCKICONINFO)(SHSTOCKICONID, UINT, SHSTOCKICONINFO *);
    static PSHGETSTOCKICONINFO pSHGetStockIconInfo = (PSHGETSTOCKICONINFO)-1;

    if ( pSHGetStockIconInfo == (PSHGETSTOCKICONINFO)-1 )
    {
        wxDynamicLibrary shell32(wxT("shell32.dll"));

        pSHGetStockIconInfo = (PSHGETSTOCKICONINFO)shell32.RawGetSymbol( wxT("SHGetStockIconInfo") );
    }

    if ( !pSHGetStockIconInfo )
        return E_FAIL;

    return pSHGetStockIconInfo(siid, uFlags, psii);
}

#endif // #ifdef wxHAS_SHGetStockIconInfo

wxBitmap
MSWGetBitmapForPath(const wxString& path, const wxSize& size, DWORD uFlags = 0)
{
    SHFILEINFO fi;
    wxZeroMemory(fi);

    uFlags |= SHGFI_USEFILEATTRIBUTES | SHGFI_ICON;
    if ( size != wxDefaultSize )
    {
        if ( size.x <= 16 )
            uFlags |= SHGFI_SMALLICON;
        else if ( size.x >= 64 )
            uFlags |= SHGFI_LARGEICON;
    }

    if ( !SHGetFileInfo(path.t_str(), FILE_ATTRIBUTE_DIRECTORY,
                        &fi, sizeof(SHFILEINFO), uFlags) )
        return wxNullBitmap;

    wxIcon icon;
    icon.CreateFromHICON((WXHICON)fi.hIcon);

    wxBitmap bitmap(icon);
    ::DestroyIcon(fi.hIcon);

    return bitmap;
}

#if wxUSE_FSVOLUME

wxBitmap
GetDriveBitmapForVolumeType(const wxFSVolumeKind& volKind, const wxSize& size)
{
    // get all volumes and try to find one with a matching type
    wxArrayString volumes = wxFSVolume::GetVolumes();
    for ( size_t i = 0; i < volumes.Count(); i++ )
    {
        wxFSVolume vol( volumes[i] );
        if ( vol.GetKind() == volKind )
        {
            return MSWGetBitmapForPath(volumes[i], size);
        }
    }

    return wxNullBitmap;
}

#endif // wxUSE_FSVOLUME

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxWindowsArtProvider
// ----------------------------------------------------------------------------

class wxWindowsArtProvider : public wxArtProvider
{
protected:
    virtual wxBitmap CreateBitmap(const wxArtID& id, const wxArtClient& client,
                                  const wxSize& size);
};

static wxBitmap CreateFromStdIcon(const char *iconName,
                                  const wxArtClient& client)
{
    wxIcon icon(iconName);
    wxBitmap bmp;
    bmp.CopyFromIcon(icon);

#if wxUSE_IMAGE
    // The standard native message box icons are in message box size (32x32).
    // If they are requested in any size other than the default or message
    // box size, they must be rescaled first.
    if ( client != wxART_MESSAGE_BOX && client != wxART_OTHER )
    {
        const wxSize size = wxArtProvider::GetNativeSizeHint(client);
        if ( size != wxDefaultSize )
        {
            wxImage img = bmp.ConvertToImage();
            img.Rescale(size.x, size.y);
            bmp = wxBitmap(img);
        }
    }
#endif // wxUSE_IMAGE

    return bmp;
}

wxBitmap wxWindowsArtProvider::CreateBitmap(const wxArtID& id,
                                            const wxArtClient& client,
                                            const wxSize& size)
{
    wxBitmap bitmap;

#ifdef wxHAS_SHGetStockIconInfo
    // first try to use SHGetStockIconInfo, available only on Vista and higher
    SHSTOCKICONID stockIconId = MSWGetStockIconIdForArtProviderId( id );
    if ( stockIconId != SIID_INVALID )
    {
        WinStruct<SHSTOCKICONINFO> sii;

        UINT uFlags = SHGSI_ICON;
        if ( size != wxDefaultSize )
        {
            if ( size.x <= 16 )
                uFlags |= SHGSI_SMALLICON;
            else if ( size.x >= 64 )
                uFlags |= SHGSI_LARGEICON;
        }

        HRESULT res = MSW_SHGetStockIconInfo(stockIconId, uFlags, &sii);
        if ( res == S_OK )
        {
            wxIcon icon;
            icon.CreateFromHICON( (WXHICON)sii.hIcon );

            bitmap = wxBitmap(icon);
            ::DestroyIcon(sii.hIcon);

            if ( bitmap.IsOk() )
                return bitmap;
        }
    }
#endif // wxHAS_SHGetStockIconInfo


#if wxUSE_FSVOLUME
    // now try SHGetFileInfo
    wxFSVolumeKind volKind = wxFS_VOL_OTHER;
    if ( id == wxART_HARDDISK )
        volKind = wxFS_VOL_DISK;
    else if ( id == wxART_FLOPPY )
        volKind = wxFS_VOL_FLOPPY;
    else if ( id == wxART_CDROM )
        volKind = wxFS_VOL_CDROM;

    if ( volKind != wxFS_VOL_OTHER )
    {
        bitmap = GetDriveBitmapForVolumeType(volKind, size);
        if ( bitmap.IsOk() )
            return bitmap;
    }
#endif // wxUSE_FSVOLUME

    // notice that the directory used here doesn't need to exist
    if ( id == wxART_FOLDER )
        bitmap = MSWGetBitmapForPath("C:\\wxdummydir\\", size );
    else if ( id == wxART_FOLDER_OPEN )
        bitmap = MSWGetBitmapForPath("C:\\wxdummydir\\", size, SHGFI_OPENICON );

    if ( !bitmap.IsOk() )
    {
        // handle message box icons specially (wxIcon ctor treat these names
        // as special cases via wxICOResourceHandler::LoadIcon):
        const char *name = NULL;
        if ( id == wxART_ERROR )
            name = "wxICON_ERROR";
        else if ( id == wxART_INFORMATION )
            name = "wxICON_INFORMATION";
        else if ( id == wxART_WARNING )
            name = "wxICON_WARNING";
        else if ( id == wxART_QUESTION )
            name = "wxICON_QUESTION";

        if ( name )
            return CreateFromStdIcon(name, client);
    }

    // for anything else, fall back to generic provider:
    return bitmap;
}

// ----------------------------------------------------------------------------
// wxArtProvider::InitNativeProvider()
// ----------------------------------------------------------------------------

/*static*/ void wxArtProvider::InitNativeProvider()
{
    PushBack(new wxWindowsArtProvider);
}

// ----------------------------------------------------------------------------
// wxArtProvider::GetNativeSizeHint()
// ----------------------------------------------------------------------------

/*static*/
wxSize wxArtProvider::GetNativeSizeHint(const wxArtClient& client)
{
    if ( client == wxART_TOOLBAR )
    {
        return wxSize(24, 24);
    }
    else if ( client == wxART_MENU )
    {
        return wxSize(16, 16);
    }
    else if ( client == wxART_FRAME_ICON )
    {
        return wxSize(::GetSystemMetrics(SM_CXSMICON),
                      ::GetSystemMetrics(SM_CYSMICON));
    }
    else if ( client == wxART_CMN_DIALOG ||
              client == wxART_MESSAGE_BOX )
    {
        return wxSize(::GetSystemMetrics(SM_CXICON),
                      ::GetSystemMetrics(SM_CYICON));
    }
    else if (client == wxART_BUTTON)
    {
        return wxSize(16, 16);
    }
    else if (client == wxART_LIST)
    {
        return wxSize(16, 16);
    }

    return wxDefaultSize;
}
