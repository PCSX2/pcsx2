/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/pnghand.h
// Purpose:     PNG bitmap handler
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PNGHAND_H_
#define _WX_PNGHAND_H_

#include "wx/defs.h"

#if wxUSE_LIBPNG

class WXDLLIMPEXP_CORE wxPNGFileHandler: public wxBitmapHandler
{
  DECLARE_DYNAMIC_CLASS(wxPNGFileHandler)
public:
  inline wxPNGFileHandler(void)
  {
    SetName(wxT("PNG bitmap file"));
    SetExtension(wxT("bmp"));
    SetType(wxBITMAP_TYPE_PNG);
  }

  virtual bool LoadFile(wxBitmap *bitmap, const wxString& name, long flags,
      int desiredWidth, int desiredHeight);
  virtual bool SaveFile(const wxBitmap *bitmap, const wxString& name, int type, const wxPalette *palette = NULL);
};

#endif //wxUSE_LIBPNG

#endif
  // _WX_PNGHAND_H_

