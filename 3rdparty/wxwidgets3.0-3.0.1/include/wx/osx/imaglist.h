/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/imaglist.h
// Purpose:
// Author:      Robert Roebling, Stefan Csomor
// Created:     01/02/97
// Id:
// Copyright:   (c) 1998 Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_IMAGLIST_H_
#define _WX_IMAGLIST_H_

#include "wx/defs.h"
#include "wx/list.h"
#include "wx/icon.h"

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxColour;

class WXDLLIMPEXP_CORE wxImageList: public wxObject
{
public:
    wxImageList() { m_width = m_height = 0; }
    wxImageList( int width, int height, bool mask = true, int initialCount = 1 );
    virtual ~wxImageList();
    bool Create( int width, int height, bool mask = true, int initialCount = 1 );
    bool Create();

    virtual int GetImageCount() const;
    virtual bool GetSize( int index, int &width, int &height ) const;

    int Add( const wxIcon& bitmap );
    int Add( const wxBitmap& bitmap );
    int Add( const wxBitmap& bitmap, const wxBitmap& mask );
    int Add( const wxBitmap& bitmap, const wxColour& maskColour );
    wxBitmap GetBitmap(int index) const;
    wxIcon GetIcon(int index) const;
    bool Replace( int index, const wxIcon &bitmap );
    bool Replace( int index, const wxBitmap &bitmap );
    bool Replace( int index, const wxBitmap &bitmap, const wxBitmap &mask );
    bool Remove( int index );
    bool RemoveAll();

    virtual bool Draw(int index, wxDC& dc, int x, int y,
                      int flags = wxIMAGELIST_DRAW_NORMAL,
                      bool solidBackground = false);

private:
    wxList  m_images;

    int     m_width;
    int     m_height;

    DECLARE_DYNAMIC_CLASS(wxImageList)
};

#endif  // _WX_IMAGLIST_H_

