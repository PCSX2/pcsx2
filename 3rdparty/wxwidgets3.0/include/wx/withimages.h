///////////////////////////////////////////////////////////////////////////////
// Name:        wx/withimages.h
// Purpose:     Declaration of a simple wxWithImages class.
// Author:      Vadim Zeitlin
// Created:     2011-08-17
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WITHIMAGES_H_
#define _WX_WITHIMAGES_H_

#include "wx/defs.h"
#include "wx/icon.h"
#include "wx/imaglist.h"

// ----------------------------------------------------------------------------
// wxWithImages: mix-in class providing access to wxImageList.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWithImages
{
public:
    enum
    {
        NO_IMAGE = -1
    };

    wxWithImages()
    {
        m_imageList = NULL;
        m_ownsImageList = false;
    }

    virtual ~wxWithImages()
    {
        FreeIfNeeded();
    }

    // Sets the image list to use, it is *not* deleted by the control.
    virtual void SetImageList(wxImageList* imageList)
    {
        FreeIfNeeded();
        m_imageList = imageList;
    }

    // As SetImageList() but we will delete the image list ourselves.
    void AssignImageList(wxImageList* imageList)
    {
        SetImageList(imageList);
        m_ownsImageList = true;
    }

    // Get pointer (may be NULL) to the associated image list.
    wxImageList* GetImageList() const { return m_imageList; }

protected:
    // Return true if we have a valid image list.
    bool HasImageList() const { return m_imageList != NULL; }

    // Return the image with the given index from the image list.
    //
    // If there is no image list or if index == NO_IMAGE, silently returns
    // wxNullIcon.
    wxIcon GetImage(int iconIndex) const
    {
        return m_imageList && iconIndex != NO_IMAGE
                    ? m_imageList->GetIcon(iconIndex)
                    : wxNullIcon;
    }

private:
    // Free the image list if necessary, i.e. if we own it.
    void FreeIfNeeded()
    {
        if ( m_ownsImageList )
        {
            delete m_imageList;
            m_imageList = NULL;

            // We don't own it any more.
            m_ownsImageList = false;
        }
    }


    // The associated image list or NULL.
    wxImageList* m_imageList;

    // False by default, if true then we delete m_imageList.
    bool m_ownsImageList;

    wxDECLARE_NO_COPY_CLASS(wxWithImages);
};

#endif // _WX_WITHIMAGES_H_
