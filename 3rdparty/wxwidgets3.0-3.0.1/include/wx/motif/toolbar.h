/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/toolbar.h
// Purpose:     wxToolBar class
// Author:      Julian Smart
// Modified by: 13.12.99 by VZ during toolbar classes reorganization
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOOLBAR_H_
#define _WX_TOOLBAR_H_

class WXDLLIMPEXP_CORE wxToolBar : public wxToolBarBase
{
public:
    // ctors and dtor
    wxToolBar() { Init(); }

    wxToolBar(wxWindow *parent,
        wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTB_HORIZONTAL,
        const wxString& name = wxToolBarNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
        wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTB_HORIZONTAL,
        const wxString& name = wxToolBarNameStr);

    virtual ~wxToolBar();

    // override/implement base class virtuals
    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const;

    virtual bool Realize();

    // implementation from now on

    // find tool by widget
    wxToolBarToolBase *FindToolByWidget(WXWidget w) const;

private:
    // common part of all ctors
    void Init();

    // implement base class pure virtuals
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool);
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool);

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable);
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle);
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle);

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bmpNormal,
                                          const wxBitmap& bmpDisabled,
                                          wxItemKind kind,
                                          wxObject *clientData,
                                          const wxString& shortHelp,
                                          const wxString& longHelp);
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label);

    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
private:
    DECLARE_DYNAMIC_CLASS(wxToolBar)
};

#endif
    // _WX_TOOLBAR_H_
