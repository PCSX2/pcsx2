/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/toolbar.h
// Purpose:     GTK toolbar
// Author:      Robert Roebling
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TOOLBAR_H_
#define _WX_GTK_TOOLBAR_H_

#if wxUSE_TOOLBAR

typedef struct _GtkTooltips GtkTooltips;

// ----------------------------------------------------------------------------
// wxToolBar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxToolBar : public wxToolBarBase
{
public:
    // construction/destruction
    wxToolBar() { Init(); }
    wxToolBar( wxWindow *parent,
               wxWindowID id,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTB_HORIZONTAL,
               const wxString& name = wxToolBarNameStr )
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxTB_HORIZONTAL,
                 const wxString& name = wxToolBarNameStr );

    virtual ~wxToolBar();

    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const;

    virtual void SetToolShortHelp(int id, const wxString& helpString);

    virtual void SetWindowStyleFlag( long style );

    virtual void SetToolNormalBitmap(int id, const wxBitmap& bitmap);
    virtual void SetToolDisabledBitmap(int id, const wxBitmap& bitmap);

    virtual bool Realize();

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bitmap1,
                                          const wxBitmap& bitmap2 = wxNullBitmap,
                                          wxItemKind kind = wxITEM_NORMAL,
                                          wxObject *clientData = NULL,
                                          const wxString& shortHelpString = wxEmptyString,
                                          const wxString& longHelpString = wxEmptyString);
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label);

    // implementation from now on
    // --------------------------

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_DEFAULT; }

    virtual wxSize DoGetBestSize() const;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    // implement base class pure virtuals
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool);
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool);

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable);
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle);
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle);

private:
    void Init();
    void GtkSetStyle();
    GSList* GetRadioGroup(size_t pos);
    virtual void AddChildGTK(wxWindowGTK* child);

    GtkToolbar* m_toolbar;
    GtkTooltips* m_tooltips;

    DECLARE_DYNAMIC_CLASS(wxToolBar)
};

#endif // wxUSE_TOOLBAR

#endif
    // _WX_GTK_TOOLBAR_H_
