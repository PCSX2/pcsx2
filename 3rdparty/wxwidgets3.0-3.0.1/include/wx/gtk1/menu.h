/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/menu.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKMENUH__
#define __GTKMENUH__

//-----------------------------------------------------------------------------
// wxMenuBar
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenuBar : public wxMenuBarBase
{
public:
    // ctors
    wxMenuBar();
    wxMenuBar(long style);
    wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long style = 0);
    virtual ~wxMenuBar();

    // implement base class (pure) virtuals
    virtual bool Append( wxMenu *menu, const wxString &title );
    virtual bool Insert(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Replace(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Remove(size_t pos);

    virtual int FindMenuItem(const wxString& menuString,
                             const wxString& itemString) const;
    virtual wxMenuItem* FindItem( int id, wxMenu **menu = NULL ) const;

    virtual void EnableTop( size_t pos, bool flag );
    virtual void SetMenuLabel( size_t pos, const wxString& label );
    virtual wxString GetMenuLabel( size_t pos ) const;

    // common part of Append and Insert
    bool GtkAppend(wxMenu *menu, const wxString& title, int pos=-1);

    virtual void Attach(wxFrame *frame);
    virtual void Detach();

    GtkAccelGroup   *m_accel;
    GtkWidget       *m_menubar;
    long             m_style;

private:
    void Init(size_t n, wxMenu *menus[], const wxString titles[], long style);

    DECLARE_DYNAMIC_CLASS(wxMenuBar)
};

//-----------------------------------------------------------------------------
// wxMenu
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenu : public wxMenuBase
{
public:
    // ctors & dtor
    wxMenu(const wxString& title, long style = 0)
        : wxMenuBase(title, style) { Init(); }

    wxMenu(long style = 0) : wxMenuBase(style) { Init(); }

    virtual ~wxMenu();

    // implement base class virtuals
    virtual wxMenuItem* DoAppend(wxMenuItem *item);
    virtual wxMenuItem* DoInsert(size_t pos, wxMenuItem *item);
    virtual wxMenuItem* DoRemove(wxMenuItem *item);

    // Returns the title, with mnemonics translated to wx format
    wxString GetTitle() const;

    // TODO: virtual void SetTitle(const wxString& title);

    // implementation
    int FindMenuIdByMenuItem( GtkWidget *menuItem ) const;

    // implementation GTK only
    GtkWidget       *m_menu;  // GtkMenu
    GtkWidget       *m_owner;
    GtkAccelGroup   *m_accel;

private:
    // common code for all constructors:
    void Init();

    // common part of Append (if pos == -1)  and Insert
    bool GtkAppend(wxMenuItem *item, int pos=-1);

    GtkWidget *m_prevRadio;

    DECLARE_DYNAMIC_CLASS(wxMenu)
};

#endif
    // __GTKMENUH__
