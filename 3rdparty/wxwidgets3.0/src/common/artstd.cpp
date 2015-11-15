/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/artstd.cpp
// Purpose:     stock wxArtProvider instance with default wxWin art
// Author:      Vaclav Slavik
// Modified by:
// Created:     18/03/2002
// Copyright:   (c) Vaclav Slavik
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

#if wxUSE_ARTPROVIDER_STD

#ifndef WX_PRECOMP
    #include "wx/image.h"
#endif

#include "wx/artprov.h"

// ----------------------------------------------------------------------------
// wxDefaultArtProvider
// ----------------------------------------------------------------------------

class wxDefaultArtProvider : public wxArtProvider
{
protected:
    virtual wxBitmap CreateBitmap(const wxArtID& id, const wxArtClient& client,
                                  const wxSize& size);
};

// ----------------------------------------------------------------------------
// wxArtProvider::InitStdProvider
// ----------------------------------------------------------------------------

/*static*/ void wxArtProvider::InitStdProvider()
{
    wxArtProvider::PushBack(new wxDefaultArtProvider);
}

// ----------------------------------------------------------------------------
// helper macros
// ----------------------------------------------------------------------------

// Standard macro for getting a resource from XPM file:
#define ART(artId, xpmRc) \
    if ( id == artId ) return wxBitmap(xpmRc##_xpm);

// ----------------------------------------------------------------------------
// XPMs with the art
// ----------------------------------------------------------------------------

#ifndef __WXUNIVERSAL__
    #if defined(__WXGTK__)
        #include "../../art/gtk/info.xpm"
        #include "../../art/gtk/error.xpm"
        #include "../../art/gtk/warning.xpm"
        #include "../../art/gtk/question.xpm"
    #elif defined(__WXMOTIF__)
        #include "../../art/motif/info.xpm"
        #include "../../art/motif/error.xpm"
        #include "../../art/motif/warning.xpm"
        #include "../../art/motif/question.xpm"
    #endif
#endif // !__WXUNIVERSAL__

#if wxUSE_HTML
    #include "../../art/htmsidep.xpm"
    #include "../../art/htmoptns.xpm"
    #include "../../art/htmbook.xpm"
    #include "../../art/htmfoldr.xpm"
    #include "../../art/htmpage.xpm"
#endif // wxUSE_HTML

#include "../../art/missimg.xpm"
#include "../../art/addbookm.xpm"
#include "../../art/delbookm.xpm"
#include "../../art/back.xpm"
#include "../../art/forward.xpm"
#include "../../art/up.xpm"
#include "../../art/down.xpm"
#include "../../art/toparent.xpm"
#include "../../art/fileopen.xpm"
#include "../../art/print.xpm"
#include "../../art/helpicon.xpm"
#include "../../art/tipicon.xpm"
#include "../../art/home.xpm"
#include "../../art/first.xpm"
#include "../../art/last.xpm"
#include "../../art/repview.xpm"
#include "../../art/listview.xpm"
#include "../../art/new_dir.xpm"
#include "../../art/harddisk.xpm"
#include "../../art/cdrom.xpm"
#include "../../art/floppy.xpm"
#include "../../art/removable.xpm"
#include "../../art/folder.xpm"
#include "../../art/folder_open.xpm"
#include "../../art/dir_up.xpm"
#include "../../art/exefile.xpm"
#include "../../art/deffile.xpm"
#include "../../art/tick.xpm"
#include "../../art/cross.xpm"

#include "../../art/filesave.xpm"
#include "../../art/filesaveas.xpm"
#include "../../art/copy.xpm"
#include "../../art/cut.xpm"
#include "../../art/paste.xpm"
#include "../../art/delete.xpm"
#include "../../art/new.xpm"
#include "../../art/undo.xpm"
#include "../../art/redo.xpm"
#include "../../art/plus.xpm"
#include "../../art/minus.xpm"
#include "../../art/close.xpm"
#include "../../art/quit.xpm"
#include "../../art/find.xpm"
#include "../../art/findrepl.xpm"

wxBitmap wxDefaultArtProvider_CreateBitmap(const wxArtID& id)
{
#if !defined(__WXUNIVERSAL__) && (defined(__WXGTK__) || defined(__WXMOTIF__))
    // wxMessageBox icons:
    ART(wxART_ERROR,                               error)
    ART(wxART_INFORMATION,                         info)
    ART(wxART_WARNING,                             warning)
    ART(wxART_QUESTION,                            question)
#endif

    // standard icons:
#if wxUSE_HTML
    ART(wxART_HELP_SIDE_PANEL,                     htmsidep)
    ART(wxART_HELP_SETTINGS,                       htmoptns)
    ART(wxART_HELP_BOOK,                           htmbook)
    ART(wxART_HELP_FOLDER,                         htmfoldr)
    ART(wxART_HELP_PAGE,                           htmpage)
#endif // wxUSE_HTML
    ART(wxART_MISSING_IMAGE,                       missimg)
    ART(wxART_ADD_BOOKMARK,                        addbookm)
    ART(wxART_DEL_BOOKMARK,                        delbookm)
    ART(wxART_GO_BACK,                             back)
    ART(wxART_GO_FORWARD,                          forward)
    ART(wxART_GO_UP,                               up)
    ART(wxART_GO_DOWN,                             down)
    ART(wxART_GO_TO_PARENT,                        toparent)
    ART(wxART_GO_HOME,                             home)
    ART(wxART_GOTO_FIRST,                          first)
    ART(wxART_GOTO_LAST,                           last)
    ART(wxART_FILE_OPEN,                           fileopen)
    ART(wxART_PRINT,                               print)
    ART(wxART_HELP,                                helpicon)
    ART(wxART_TIP,                                 tipicon)
    ART(wxART_REPORT_VIEW,                         repview)
    ART(wxART_LIST_VIEW,                           listview)
    ART(wxART_NEW_DIR,                             new_dir)
    ART(wxART_HARDDISK,                            harddisk)
    ART(wxART_FLOPPY,                              floppy)
    ART(wxART_CDROM,                               cdrom)
    ART(wxART_REMOVABLE,                           removable)
    ART(wxART_FOLDER,                              folder)
    ART(wxART_FOLDER_OPEN,                         folder_open)
    ART(wxART_GO_DIR_UP,                           dir_up)
    ART(wxART_EXECUTABLE_FILE,                     exefile)
    ART(wxART_NORMAL_FILE,                         deffile)
    ART(wxART_TICK_MARK,                           tick)
    ART(wxART_CROSS_MARK,                          cross)

    ART(wxART_FILE_SAVE,                           filesave)
    ART(wxART_FILE_SAVE_AS,                        filesaveas)
    ART(wxART_COPY,                                copy)
    ART(wxART_CUT,                                 cut)
    ART(wxART_PASTE,                               paste)
    ART(wxART_DELETE,                              delete)
    ART(wxART_UNDO,                                undo)
    ART(wxART_REDO,                                redo)
    ART(wxART_PLUS,                                plus)
    ART(wxART_MINUS,                               minus)
    ART(wxART_CLOSE,                               close)
    ART(wxART_QUIT,                                quit)
    ART(wxART_FIND,                                find)
    ART(wxART_FIND_AND_REPLACE,                    findrepl)
    ART(wxART_NEW,                                 new)


    return wxNullBitmap;
}

// ----------------------------------------------------------------------------
// CreateBitmap routine
// ----------------------------------------------------------------------------

wxBitmap wxDefaultArtProvider::CreateBitmap(const wxArtID& id,
                                            const wxArtClient& client,
                                            const wxSize& reqSize)
{
    wxBitmap bmp = wxDefaultArtProvider_CreateBitmap(id);

#if wxUSE_IMAGE && (!defined(__WXMSW__) || wxUSE_WXDIB)
    if (bmp.IsOk())
    {
        // fit into transparent image with desired size hint from the client
        if (reqSize == wxDefaultSize)
        {
            // find out if there is a desired size for this client
            wxSize bestSize = GetSizeHint(client);
            if (bestSize != wxDefaultSize)
            {
                int bmp_w = bmp.GetWidth();
                int bmp_h = bmp.GetHeight();

                if (bmp_w == 16 && bmp_h == 15 && bestSize == wxSize(16, 16))
                {
                    // Do nothing in this special but quite common case, because scaling
                    // with only a pixel difference will look horrible.
                }
                else if ((bmp_h < bestSize.x) && (bmp_w < bestSize.y))
                {
                    // the caller wants default size, which is larger than
                    // the image we have; to avoid degrading it visually by
                    // scaling it up, paste it into transparent image instead:
                    wxPoint offset((bestSize.x - bmp_w)/2, (bestSize.y - bmp_h)/2);
                    wxImage img = bmp.ConvertToImage();
                    img.Resize(bestSize, offset);
                    bmp = wxBitmap(img);
                }
                else // scale (down or mixed, but not up)
                {
                    wxImage img = bmp.ConvertToImage();
                    bmp = wxBitmap
                          (
                              img.Scale(bestSize.x, bestSize.y,
                                        wxIMAGE_QUALITY_HIGH)
                          );
                }
            }
        }
    }
#else
    wxUnusedVar(client);
    wxUnusedVar(reqSize);
#endif // wxUSE_IMAGE

    return bmp;
}

#endif // wxUSE_ARTPROVIDER_STD
