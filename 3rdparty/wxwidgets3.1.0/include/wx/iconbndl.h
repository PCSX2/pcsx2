///////////////////////////////////////////////////////////////////////////////
// Name:        wx/iconbndl.h
// Purpose:     wxIconBundle
// Author:      Mattia barbon
// Modified by:
// Created:     23.03.02
// Copyright:   (c) Mattia Barbon
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ICONBNDL_H_
#define _WX_ICONBNDL_H_

#include "wx/gdiobj.h"
#include "wx/gdicmn.h"      // for wxSize
#include "wx/icon.h"

#include "wx/dynarray.h"

class WXDLLIMPEXP_FWD_BASE wxInputStream;

WX_DECLARE_EXPORTED_OBJARRAY(wxIcon, wxIconArray);

// this class can't load bitmaps of type wxBITMAP_TYPE_ICO_RESOURCE,
// if you need them, you have to load them manually and call
// wxIconCollection::AddIcon
class WXDLLIMPEXP_CORE wxIconBundle : public wxGDIObject
{
public:
    // Flags that determine what happens if GetIcon() doesn't find the icon of
    // exactly the requested size.
    enum
    {
        // Return invalid icon if exact size is not found.
        FALLBACK_NONE = 0,

        // Return the icon of the system icon size if exact size is not found.
        // May be combined with other non-NONE enum elements to determine what
        // happens if the system icon size is not found neither.
        FALLBACK_SYSTEM = 1,

        // Return the icon of closest larger size or, if there is no icon of
        // larger size in the bundle, the closest icon of smaller size.
        FALLBACK_NEAREST_LARGER = 2
    };

    // default constructor
    wxIconBundle();

    // initializes the bundle with the icon(s) found in the file
#if wxUSE_STREAMS && wxUSE_IMAGE
#if wxUSE_FFILE || wxUSE_FILE
    wxIconBundle(const wxString& file, wxBitmapType type = wxBITMAP_TYPE_ANY);
#endif // wxUSE_FFILE || wxUSE_FILE
    wxIconBundle(wxInputStream& stream, wxBitmapType type = wxBITMAP_TYPE_ANY);
#endif // wxUSE_STREAMS && wxUSE_IMAGE

    // initializes the bundle with a single icon
    wxIconBundle(const wxIcon& icon);

    // default copy ctor and assignment operator are OK

    // adds all the icons contained in the file to the collection,
    // if the collection already contains icons with the same
    // width and height, they are replaced
#if wxUSE_STREAMS && wxUSE_IMAGE
#if wxUSE_FFILE || wxUSE_FILE
    void AddIcon(const wxString& file, wxBitmapType type = wxBITMAP_TYPE_ANY);
#endif // wxUSE_FFILE || wxUSE_FILE
    void AddIcon(wxInputStream& stream, wxBitmapType type = wxBITMAP_TYPE_ANY);
#endif // wxUSE_STREAMS && wxUSE_IMAGE

    // adds the icon to the collection, if the collection already
    // contains an icon with the same width and height, it is
    // replaced
    void AddIcon(const wxIcon& icon);

    // returns the icon with the given size; if no such icon exists,
    // behavior is specified by the flags.
    wxIcon GetIcon(const wxSize& size, int flags = FALLBACK_SYSTEM) const;

    // equivalent to GetIcon(wxSize(size, size))
    wxIcon GetIcon(wxCoord size = wxDefaultCoord,
                   int flags = FALLBACK_SYSTEM) const
        { return GetIcon(wxSize(size, size), flags); }

    // returns the icon exactly of the specified size or wxNullIcon if no icon
    // of exactly given size are available
    wxIcon GetIconOfExactSize(const wxSize& size) const;
    wxIcon GetIconOfExactSize(wxCoord size) const
        { return GetIconOfExactSize(wxSize(size, size)); }

    // enumerate all icons in the bundle: don't use these functions if ti can
    // be avoided, using GetIcon() directly is better

    // return the number of available icons
    size_t GetIconCount() const;

    // return the icon at index (must be < GetIconCount())
    wxIcon GetIconByIndex(size_t n) const;

    // check if we have any icons at all
    bool IsEmpty() const { return GetIconCount() == 0; }

#if WXWIN_COMPATIBILITY_2_8
#if wxUSE_STREAMS && wxUSE_IMAGE && (wxUSE_FFILE || wxUSE_FILE)
    wxDEPRECATED( void AddIcon(const wxString& file, long type)
        {
            AddIcon(file, (wxBitmapType)type);
        }
    )

    wxDEPRECATED_CONSTRUCTOR( wxIconBundle (const wxString& file, long type)
        {
            AddIcon(file, (wxBitmapType)type);
        }
    )
#endif // wxUSE_STREAMS && wxUSE_IMAGE && (wxUSE_FFILE || wxUSE_FILE)
#endif // WXWIN_COMPATIBILITY_2_8

protected:
    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const wxOVERRIDE;

private:
    // delete all icons
    void DeleteIcons();

    wxDECLARE_DYNAMIC_CLASS(wxIconBundle);
};

#endif // _WX_ICONBNDL_H_
