///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/arttango.cpp
// Purpose:     art provider using embedded PNG versions of Tango icons
// Author:      Vadim Zeitlin
// Created:     2010-12-27
// Copyright:   (c) 2010 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ARTPROVIDER_TANGO

#ifndef WX_PRECOMP
    #include "wx/image.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/artprov.h"

#include "wx/mstream.h"

// ----------------------------------------------------------------------------
// image data
// ----------------------------------------------------------------------------

// All files in art/tango in alphabetical order:
#include "../../art/tango/application_x_executable.h"
#include "../../art/tango/dialog_error.h"
#include "../../art/tango/dialog_information.h"
#include "../../art/tango/dialog_warning.h"
#include "../../art/tango/document_new.h"
#include "../../art/tango/document_open.h"
#include "../../art/tango/document_print.h"
#include "../../art/tango/document_save.h"
#include "../../art/tango/document_save_as.h"
#include "../../art/tango/drive_harddisk.h"
#include "../../art/tango/drive_optical.h"
#include "../../art/tango/drive_removable_media.h"
#include "../../art/tango/edit_copy.h"
#include "../../art/tango/edit_cut.h"
#include "../../art/tango/edit_delete.h"
#include "../../art/tango/edit_find.h"
#include "../../art/tango/edit_find_replace.h"
#include "../../art/tango/edit_paste.h"
#include "../../art/tango/edit_redo.h"
#include "../../art/tango/edit_undo.h"
#include "../../art/tango/folder.h"
#include "../../art/tango/folder_new.h"
#include "../../art/tango/folder_open.h"
#include "../../art/tango/go_down.h"
#include "../../art/tango/go_first.h"
#include "../../art/tango/go_home.h"
#include "../../art/tango/go_last.h"
#include "../../art/tango/go_next.h"
#include "../../art/tango/go_previous.h"
#include "../../art/tango/go_up.h"
#include "../../art/tango/image_missing.h"
#include "../../art/tango/text_x_generic.h"
#include "../../art/tango/list_add.h"
#include "../../art/tango/list_remove.h"

// ----------------------------------------------------------------------------
// art provider class
// ----------------------------------------------------------------------------

namespace
{

class wxTangoArtProvider : public wxArtProvider
{
public:
    wxTangoArtProvider()
    {
        m_imageHandledAdded = false;
    }

protected:
    virtual wxBitmap CreateBitmap(const wxArtID& id,
                                  const wxArtClient& client,
                                  const wxSize& size);

private:
    bool m_imageHandledAdded;

    wxDECLARE_NO_COPY_CLASS(wxTangoArtProvider);
};

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

wxBitmap
wxTangoArtProvider::CreateBitmap(const wxArtID& id,
                                 const wxArtClient& client,
                                 const wxSize& sizeHint)
{
    // Array indexed by the id names with pointers to image data in 16 and 32
    // pixel sizes as values. The order of the elements in this array is the
    // same as the definition order in wx/artprov.h. While it's not very
    // logical, this should make it simpler to add new icons later. Notice that
    // most elements without Tango equivalents are simply omitted.

    // To avoid repetition use BITMAP_DATA to only specify the image name once
    // (this is especially important if we decide to add more image sizes
    // later).
    #define BITMAP_ARRAY_NAME(name, size) \
        name ## _ ## size ## x ## size ## _png
    #define BITMAP_DATA_FOR_SIZE(name, size) \
        BITMAP_ARRAY_NAME(name, size), sizeof(BITMAP_ARRAY_NAME(name, size))
    #define BITMAP_DATA(name) \
        BITMAP_DATA_FOR_SIZE(name, 16), BITMAP_DATA_FOR_SIZE(name, 24)

    static const struct BitmapEntry
    {
        const char *id;
        const unsigned char *data16;
        size_t len16;
        const unsigned char *data24;
        size_t len24;
    } s_allBitmaps[] =
    {
        // Tango does have bookmark-new but no matching bookmark-delete and
        // using mismatching icons would be ugly so we don't provide this one
        // neither, we should add both of them if Tango ever adds the other one.
        //{ wxART_ADD_BOOKMARK,       BITMAP_DATA(bookmark_new)},
        //{ wxART_DEL_BOOKMARK,       BITMAP_DATA() },

        { wxART_GO_BACK,            BITMAP_DATA(go_previous)                },
        { wxART_GO_FORWARD,         BITMAP_DATA(go_next)                    },
        { wxART_GO_UP,              BITMAP_DATA(go_up)                      },
        { wxART_GO_DOWN,            BITMAP_DATA(go_down)                    },
        // wxART_GO_TO_PARENT doesn't seem to exist in Tango
        { wxART_GO_HOME,            BITMAP_DATA(go_home)                    },
        { wxART_GOTO_FIRST,         BITMAP_DATA(go_first)                   },
        { wxART_GOTO_LAST,          BITMAP_DATA(go_last)                    },

        { wxART_FILE_OPEN,          BITMAP_DATA(document_open)              },
        { wxART_FILE_SAVE,          BITMAP_DATA(document_save)              },
        { wxART_FILE_SAVE_AS,       BITMAP_DATA(document_save_as)           },
        { wxART_PRINT,              BITMAP_DATA(document_print)             },

        // Should we use help-browser for wxART_HELP?

        { wxART_NEW_DIR,            BITMAP_DATA(folder_new)                 },
        { wxART_HARDDISK,           BITMAP_DATA(drive_harddisk)             },
        // drive-removable-media seems to be better than media-floppy
        { wxART_FLOPPY,             BITMAP_DATA(drive_removable_media)      },
        { wxART_CDROM,              BITMAP_DATA(drive_optical)              },
        { wxART_REMOVABLE,          BITMAP_DATA(drive_removable_media)      },

        { wxART_FOLDER,             BITMAP_DATA(folder)                     },
        { wxART_FOLDER_OPEN,        BITMAP_DATA(folder_open)                },
        // wxART_GO_DIR_UP doesn't seem to exist in Tango

        { wxART_EXECUTABLE_FILE,    BITMAP_DATA(application_x_executable)   },
        { wxART_NORMAL_FILE,        BITMAP_DATA(text_x_generic)             },

        // There is no dialog-question in Tango so use the information icon
        // too, this is better for consistency and we do have a precedent for
        // doing this as Windows Vista/7 does the same thing natively.
        { wxART_ERROR,              BITMAP_DATA(dialog_error)               },
        { wxART_QUESTION,           BITMAP_DATA(dialog_information)         },
        { wxART_WARNING,            BITMAP_DATA(dialog_warning)             },
        { wxART_INFORMATION,        BITMAP_DATA(dialog_information)         },

        { wxART_MISSING_IMAGE,      BITMAP_DATA(image_missing)              },

        { wxART_COPY,               BITMAP_DATA(edit_copy)                  },
        { wxART_CUT,                BITMAP_DATA(edit_cut)                   },
        { wxART_PASTE,              BITMAP_DATA(edit_paste)                 },
        { wxART_DELETE,             BITMAP_DATA(edit_delete)                },
        { wxART_NEW,                BITMAP_DATA(document_new)               },
        { wxART_UNDO,               BITMAP_DATA(edit_undo)                  },
        { wxART_REDO,               BITMAP_DATA(edit_redo)                  },

        { wxART_PLUS,               BITMAP_DATA(list_add)                   },
        { wxART_MINUS,              BITMAP_DATA(list_remove)                },

        // Surprisingly Tango doesn't seem to have neither wxART_CLOSE nor
        // wxART_QUIT. We could use system-log-out for the latter but it
        // doesn't seem quite right.

        { wxART_FIND,               BITMAP_DATA(edit_find)                  },
        { wxART_FIND_AND_REPLACE,   BITMAP_DATA(edit_find_replace)          },
    };

    #undef BITMAP_ARRAY_NAME
    #undef BITMAP_DATA_FOR_SIZE
    #undef BITMAP_DATA

    for ( unsigned n = 0; n < WXSIZEOF(s_allBitmaps); n++ )
    {
        const BitmapEntry& entry = s_allBitmaps[n];
        if ( entry.id != id )
            continue;

        // This is one of the bitmaps that we have, determine in which size we
        // should return it.

        wxSize size;
        bool sizeIsAHint;
        if ( sizeHint == wxDefaultSize )
        {
            // Use the normal platform-specific icon size.
            size = GetNativeSizeHint(client);

            if ( size == wxDefaultSize )
            {
                // If we failed to get it, determine the best size more or less
                // arbitrarily. This definitely won't look good but then it
                // shouldn't normally happen, all platforms should implement
                // GetNativeSizeHint() properly.
                if ( client == wxART_MENU || client == wxART_BUTTON )
                    size = wxSize(16, 16);
                else
                    size = wxSize(24, 24);
            }

            // We should return the icon of exactly this size so it's more than
            // just a hint.
            sizeIsAHint = false;
        }
        else // We have a size hint
        {
            // Use it for determining the version of the icon to return.
            size = sizeHint;

            // But we don't need to return the image of exactly the same size
            // as the hint, after all it's just that, a hint.
            sizeIsAHint = true;
        }

        enum
        {
            TangoSize_16,
            TangoSize_24
        } tangoSize;

        // We prefer to downscale the image rather than upscale it if possible
        // so use the smaller one if we can, otherwise the large one.
        if ( size.x <= 16 && size.y <= 16 )
            tangoSize = TangoSize_16;
        else
            tangoSize = TangoSize_24;

        const unsigned char *data;
        size_t len;
        switch ( tangoSize )
        {
            default:
                wxFAIL_MSG( "Unsupported Tango bitmap size" );
                // fall through

            case TangoSize_16:
                data = entry.data16;
                len = entry.len16;
                break;

            case TangoSize_24:
                data = entry.data24;
                len = entry.len24;
                break;
        }

        wxMemoryInputStream is(data, len);

        // Before reading the image data from the stream for the first time,
        // add the handler for PNG images: we do it here and not in, say,
        // InitTangoProvider() to do it as lately as possible and so to avoid
        // the asserts about adding an already added handler if the user code
        // adds the handler itself.
        if ( !m_imageHandledAdded )
        {
            // Of course, if the user code did add it already, we have nothing
            // to do.
            if ( !wxImage::FindHandler(wxBITMAP_TYPE_PNG) )
                wxImage::AddHandler(new wxPNGHandler);

            // In any case, no need to do it again.
            m_imageHandledAdded = true;
        }

        wxImage image(is, wxBITMAP_TYPE_PNG);
        if ( !image.IsOk() )
        {
            // This should normally never happen as all the embedded images are
            // well-formed.
            wxLogDebug("Failed to load embedded PNG image for \"%s\"", id);
            return wxNullBitmap;
        }

        if ( !sizeIsAHint )
        {
            // Notice that this won't do anything if the size is already right.
            image.Rescale(size.x, size.y, wxIMAGE_QUALITY_HIGH);
        }

        return image;
    }

    // Not one of the bitmaps that we support.
    return wxNullBitmap;
}

/* static */
void wxArtProvider::InitTangoProvider()
{
    wxArtProvider::PushBack(new wxTangoArtProvider);
}

#endif // wxUSE_ARTPROVIDER_TANGO
