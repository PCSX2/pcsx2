///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/toolbar.h
// Purpose:     wxToolBar declaration
// Author:      Robert Roebling
// Modified by:
// Created:     10.09.00
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_TOOLBAR_H_
#define _WX_UNIV_TOOLBAR_H_

#include "wx/button.h"      // for wxStdButtonInputHandler

class WXDLLIMPEXP_FWD_CORE wxToolBarTool;

// ----------------------------------------------------------------------------
// the actions supported by this control
// ----------------------------------------------------------------------------

#define wxACTION_TOOLBAR_TOGGLE  wxACTION_BUTTON_TOGGLE
#define wxACTION_TOOLBAR_PRESS   wxACTION_BUTTON_PRESS
#define wxACTION_TOOLBAR_RELEASE wxACTION_BUTTON_RELEASE
#define wxACTION_TOOLBAR_CLICK   wxACTION_BUTTON_CLICK
#define wxACTION_TOOLBAR_ENTER   wxT("enter")     // highlight the tool
#define wxACTION_TOOLBAR_LEAVE   wxT("leave")     // unhighlight the tool

// ----------------------------------------------------------------------------
// wxToolBar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxToolBar : public wxToolBarBase
{
public:
    // construction/destruction
    wxToolBar() { Init(); }
    wxToolBar(wxWindow *parent,
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = wxToolBarNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxString& name = wxToolBarNameStr );

    virtual ~wxToolBar();

    virtual bool Realize() wxOVERRIDE;

    virtual void SetWindowStyleFlag( long style ) wxOVERRIDE;

    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const wxOVERRIDE;

    virtual void SetToolShortHelp(int id, const wxString& helpString) wxOVERRIDE;

    virtual void SetMargins(int x, int y) wxOVERRIDE;
    void SetMargins(const wxSize& size)
        { SetMargins((int) size.x, (int) size.y); }

    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = -1,
                               const wxString& strArg = wxEmptyString) wxOVERRIDE;
    static wxInputHandler *GetStdInputHandler(wxInputHandler *handlerDef);
    virtual wxInputHandler *DoGetStdInputHandler(wxInputHandler *handlerDef) wxOVERRIDE
    {
        return GetStdInputHandler(handlerDef);
    }

protected:
    // common part of all ctors
    void Init();

    // implement base class pure virtuals
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable) wxOVERRIDE;
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bmpNormal,
                                          const wxBitmap& bmpDisabled,
                                          wxItemKind kind,
                                          wxObject *clientData,
                                          const wxString& shortHelp,
                                          const wxString& longHelp) wxOVERRIDE;
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label) wxOVERRIDE;

    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;
    virtual void DoDraw(wxControlRenderer *renderer) wxOVERRIDE;

    // get the bounding rect for the given tool
    wxRect GetToolRect(wxToolBarToolBase *tool) const;

    // redraw the given tool
    void RefreshTool(wxToolBarToolBase *tool);

    // (re)calculate the tool positions, should only be called if it is
    // necessary to do it, i.e. m_needsLayout == true
    void DoLayout();

    // get the rect limits depending on the orientation: top/bottom for a
    // vertical toolbar, left/right for a horizontal one
    void GetRectLimits(const wxRect& rect, wxCoord *start, wxCoord *end) const;

private:
    // have we calculated the positions of our tools?
    bool m_needsLayout;

    // the width of a separator
    wxCoord m_widthSeparator;

    // the total size of all toolbar elements
    wxCoord m_maxWidth,
            m_maxHeight;

private:
    wxDECLARE_DYNAMIC_CLASS(wxToolBar);
};

#endif // _WX_UNIV_TOOLBAR_H_
