/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/statbmp.h
// Purpose:     wxStaticBitmap class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATBMP_H_
#define _WX_STATBMP_H_

#include "wx/icon.h"

class WXDLLIMPEXP_CORE wxStaticBitmap: public wxStaticBitmapBase
{
  DECLARE_DYNAMIC_CLASS(wxStaticBitmap)
 public:
  wxStaticBitmap() { }

  wxStaticBitmap(wxWindow *parent, wxWindowID id,
           const wxBitmap& label,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxStaticBitmapNameStr)
  {
    Create(parent, id, label, pos, size, style, name);
  }

  bool Create(wxWindow *parent, wxWindowID id,
           const wxBitmap& label,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxStaticBitmapNameStr);

  virtual void SetBitmap(const wxBitmap& bitmap);

  virtual void Command(wxCommandEvent& WXUNUSED(event)) {}
  virtual void ProcessCommand(wxCommandEvent& WXUNUSED(event)) {}
  void         OnPaint( wxPaintEvent &event ) ;

  wxBitmap GetBitmap() const { return m_bitmap; }
  wxIcon GetIcon() const
      {
      // icons and bitmaps are really the same thing in wxMac
      return (const wxIcon &)m_bitmap;
      }
  void  SetIcon(const wxIcon& icon) { SetBitmap( (const wxBitmap &)icon ) ; }

  // overridden base class virtuals
  virtual bool AcceptsFocus() const { return false; }

 protected:
    virtual wxSize DoGetBestSize() const;

    wxBitmap m_bitmap;
    DECLARE_EVENT_TABLE()
};

#endif
    // _WX_STATBMP_H_
