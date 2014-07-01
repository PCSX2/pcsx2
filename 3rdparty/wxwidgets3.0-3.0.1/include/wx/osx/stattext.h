/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/stattext.h
// Purpose:     wxStaticText class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATTEXT_H_
#define _WX_STATTEXT_H_

class WXDLLIMPEXP_CORE wxStaticText: public wxStaticTextBase
{
public:
    wxStaticText() { }

    wxStaticText(wxWindow *parent, wxWindowID id,
           const wxString& label,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxStaticTextNameStr)
  {
    Create(parent, id, label, pos, size, style, name);
  }

  bool Create(wxWindow *parent, wxWindowID id,
           const wxString& label,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxStaticTextNameStr);

  // accessors
  void SetLabel( const wxString &str ) ;
  bool SetFont( const wxFont &font );

    virtual bool AcceptsFocus() const { return false; }

protected :

    virtual wxString DoGetLabel() const;
    virtual void DoSetLabel(const wxString& str);

  virtual wxSize DoGetBestSize() const ;

#if wxUSE_MARKUP && wxOSX_USE_COCOA
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP && wxOSX_USE_COCOA

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxStaticText)
};

#endif
    // _WX_STATTEXT_H_
