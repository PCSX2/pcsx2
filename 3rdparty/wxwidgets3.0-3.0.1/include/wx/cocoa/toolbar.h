/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/toolbar.h
// Purpose:     wxToolBar
// Author:      David Elliott
// Modified by:
// Created:     2003/08/17
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_TOOLBAR_H__
#define __WX_COCOA_TOOLBAR_H__

#if wxUSE_TOOLBAR

// ========================================================================
// wxToolBar
// ========================================================================
#if defined(__LP64__) || defined(NS_BUILD_32_LIKE_64)
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif

class wxToolBarTool;

class WXDLLIMPEXP_CORE wxToolBar : public wxToolBarBase
{
    DECLARE_DYNAMIC_CLASS(wxToolBar)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxToolBar() { Init(); }
    wxToolBar( wxWindow *parent,
               wxWindowID toolid,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxToolBarNameStr )
    {
        Init();

        Create(parent, toolid, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID toolid,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxString& name = wxToolBarNameStr );

    virtual ~wxToolBar();

protected:
    // common part of all ctors
    void Init();

// ------------------------------------------------------------------------
// Cocoa
// ------------------------------------------------------------------------
protected:
    virtual bool Cocoa_acceptsFirstMouse(bool &acceptsFirstMouse, WX_NSEvent theEvent);
    virtual bool Cocoa_drawRect(const NSRect &rect);
    virtual bool Cocoa_mouseDown(WX_NSEvent theEvent);
    virtual bool Cocoa_mouseDragged(WX_NSEvent theEvent);
    wxToolBarTool *CocoaFindToolForPosition(const NSPoint& pos) const;
    void CocoaToolClickEnded();
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // override base class virtuals
    virtual void SetMargins(int x, int y);
    virtual void SetToolSeparation(int separation);

    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const;

    virtual void SetToolShortHelp(int toolid, const wxString& helpString);

    virtual void SetWindowStyleFlag( long style );

    // implementation from now on
    // --------------------------

    void OnInternalIdle();
    virtual bool Realize();
    virtual wxSize DoGetBestSize() const;

    void SetOwningFrame(wxFrame *owningFrame)
    {   m_owningFrame = owningFrame; }
protected:
    // implement base class pure virtuals
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool);
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool);

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable);
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle);
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle);

    virtual wxToolBarToolBase *CreateTool(int toolid,
                                          const wxString& label,
                                          const wxBitmap& bitmap1,
                                          const wxBitmap& bitmap2,
                                          wxItemKind kind,
                                          wxObject *clientData,
                                          const wxString& shortHelpString,
                                          const wxString& longHelpString);
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label);

    wxSize m_bestSize;
    wxFrame *m_owningFrame;
    wxToolBarTool *m_mouseDownTool;
};

#endif // wxUSE_TOOLBAR

#endif // __WX_COCOA_TOOLBAR_H__
