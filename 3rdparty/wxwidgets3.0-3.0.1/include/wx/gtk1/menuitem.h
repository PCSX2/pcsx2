///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/menuitem.h
// Purpose:     wxMenuItem class
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __GTKMENUITEMH__
#define __GTKMENUITEMH__

#include "wx/bitmap.h"

//-----------------------------------------------------------------------------
// wxMenuItem
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenuItem : public wxMenuItemBase
{
public:
    wxMenuItem(wxMenu *parentMenu = NULL,
               int id = wxID_SEPARATOR,
               const wxString& text = wxEmptyString,
               const wxString& help = wxEmptyString,
               wxItemKind kind = wxITEM_NORMAL,
               wxMenu *subMenu = NULL);
    virtual ~wxMenuItem();

    // implement base class virtuals
    virtual void SetItemLabel( const wxString& str );
    virtual wxString GetItemLabel() const;
    virtual void Enable( bool enable = TRUE );
    virtual void Check( bool check = TRUE );
    virtual bool IsChecked() const;
    virtual void SetBitmap(const wxBitmap& bitmap) { m_bitmap = bitmap; }
    virtual const wxBitmap& GetBitmap() const { return m_bitmap; }

#if wxUSE_ACCEL
    virtual wxAcceleratorEntry *GetAccel() const;
#endif // wxUSE_ACCEL

    // implementation
    void SetMenuItem(GtkWidget *menuItem) { m_menuItem = menuItem; }
    GtkWidget *GetMenuItem() const { return m_menuItem; }
    GtkWidget *GetLabelWidget() const { return m_labelWidget; }
    void SetLabelWidget(GtkWidget *labelWidget) { m_labelWidget = labelWidget; }
    wxString GetFactoryPath() const;

    wxString GetHotKey() const { return m_hotKey; }

    // compatibility only, don't use in new code
    wxMenuItem(wxMenu *parentMenu,
               int id,
               const wxString& text,
               const wxString& help,
               bool isCheckable,
               wxMenu *subMenu = NULL);

private:
    // common part of all ctors
    void Init();

    // DoSetText() transforms the accel mnemonics in our label from MSW/wxWin
    // style to GTK+ and is called from ctor and SetText()
    void DoSetText(const wxString& text);

    wxString  m_hotKey;
    wxBitmap  m_bitmap; // Bitmap for menuitem, if any

    GtkWidget *m_menuItem;  // GtkMenuItem
    GtkWidget* m_labelWidget; // Label widget

    DECLARE_DYNAMIC_CLASS(wxMenuItem)
};

#endif
        //__GTKMENUITEMH__
