/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/toolbar.h
// Purpose:     wxToolBar class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOOLBAR_H_
#define _WX_TOOLBAR_H_

#if wxUSE_TOOLBAR

#include "wx/tbarbase.h"
#include "wx/dynarray.h"

class WXDLLIMPEXP_CORE wxToolBar: public wxToolBarBase
{
  DECLARE_DYNAMIC_CLASS(wxToolBar)
 public:
  /*
   * Public interface
   */

   wxToolBar() { Init(); }

  inline wxToolBar(wxWindow *parent, wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                   long style = wxTB_HORIZONTAL,
                   const wxString& name = wxToolBarNameStr)
  {
    Init();
    Create(parent, id, pos, size, style, name);
  }
  virtual ~wxToolBar();

  bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
            long style = wxTB_HORIZONTAL,
            const wxString& name = wxToolBarNameStr);

    virtual void SetWindowStyleFlag(long style);

    virtual bool Destroy();

    // override/implement base class virtuals
    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const;

#ifndef __WXOSX_IPHONE__
    virtual bool Show(bool show = true);
    virtual bool IsShown() const;
#endif
    virtual bool Realize();

    virtual void SetToolBitmapSize(const wxSize& size);
    virtual wxSize GetToolSize() const;

    virtual void SetRows(int nRows);

    virtual void SetToolNormalBitmap(int id, const wxBitmap& bitmap);
    virtual void SetToolDisabledBitmap(int id, const wxBitmap& bitmap);

#ifndef __WXOSX_IPHONE__
    // Add all the buttons

    virtual wxString MacGetToolTipString( wxPoint &where ) ;
    void OnPaint(wxPaintEvent& event) ;
    void OnMouse(wxMouseEvent& event) ;
    virtual void MacSuperChangedPosition() ;
#endif

#if wxOSX_USE_NATIVE_TOOLBAR
    // make all tools selectable
    void OSXSetSelectableTools(bool set);
    void OSXSelectTool(int toolId);

    bool MacInstallNativeToolbar(bool usesNative);
    void MacUninstallNativeToolbar();
    bool MacWantsNativeToolbar();
    bool MacTopLevelHasNativeToolbar(bool *ownToolbarInstalled) const;
#endif

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bmpNormal,
                                          const wxBitmap& bmpDisabled = wxNullBitmap,
                                          wxItemKind kind = wxITEM_NORMAL,
                                          wxObject *clientData = NULL,
                                          const wxString& shortHelp = wxEmptyString,
                                          const wxString& longHelp = wxEmptyString);
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label);

protected:
    // common part of all ctors
    void Init();
    
    void DoLayout();
    
    void DoSetSize(int x, int y, int width, int height, int sizeFlags);

#ifndef __WXOSX_IPHONE__
    virtual void DoGetSize(int *width, int *height) const;
    virtual wxSize DoGetBestSize() const;
#endif
#ifdef __WXOSX_COCOA__
    virtual void DoGetPosition(int*x, int *y) const;
#endif
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool);
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool);

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable);
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle);
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle);

    DECLARE_EVENT_TABLE()
#if wxOSX_USE_NATIVE_TOOLBAR
    bool m_macUsesNativeToolbar ;
    void* m_macToolbar ;
#endif
#ifdef __WXOSX_IPHONE__
    WX_UIView m_macToolbar;
#endif
};

#endif // wxUSE_TOOLBAR

#endif
    // _WX_TOOLBAR_H_
