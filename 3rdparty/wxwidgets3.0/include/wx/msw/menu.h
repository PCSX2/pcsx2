/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/menu.h
// Purpose:     wxMenu, wxMenuBar classes
// Author:      Julian Smart
// Modified by: Vadim Zeitlin (wxMenuItem is now in separate file)
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MENU_H_
#define _WX_MENU_H_

#if wxUSE_ACCEL
    #include "wx/accel.h"
    #include "wx/dynarray.h"

    WX_DEFINE_EXPORTED_ARRAY_PTR(wxAcceleratorEntry *, wxAcceleratorArray);
#endif // wxUSE_ACCEL

class WXDLLIMPEXP_FWD_CORE wxFrame;

#if defined(__WXWINCE__) && wxUSE_TOOLBAR
class WXDLLIMPEXP_FWD_CORE wxToolBar;
#endif

class wxMenuRadioItemsData;

// Not using a combined wxToolBar/wxMenuBar? then use
// a commandbar in WinCE .NET to implement the
// menubar, since there is no ::SetMenu function.
#if defined(__WXWINCE__)
#   if ((_WIN32_WCE >= 400) && !defined(__POCKETPC__) && !defined(__SMARTPHONE__)) || \
        defined(__HANDHELDPC__)
#       define WINCE_WITH_COMMANDBAR
#   else
#       define WINCE_WITHOUT_COMMANDBAR
#   endif
#endif


#include "wx/arrstr.h"

// ----------------------------------------------------------------------------
// Menu
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenu : public wxMenuBase
{
public:
    // ctors & dtor
    wxMenu(const wxString& title, long style = 0)
        : wxMenuBase(title, style) { Init(); }

    wxMenu(long style = 0) : wxMenuBase(style) { Init(); }

    virtual ~wxMenu();

    virtual void Break();

    virtual void SetTitle(const wxString& title);

    // MSW-only methods
    // ----------------

    // Create a new menu from the given native HMENU. Takes ownership of the
    // menu handle and will delete it when this object is destroyed.
    static wxMenu *MSWNewFromHMENU(WXHMENU hMenu) { return new wxMenu(hMenu); }

#if wxABI_VERSION >= 30002
    // Detaches HMENU so that it isn't deleted when this object is destroyed.
    // Don't use this object after calling this method.
    WXHMENU MSWDetachHMENU() { WXHMENU m = m_hMenu; m_hMenu = NULL; return m; }
#endif

    // implementation only from now on
    // -------------------------------

    bool MSWCommand(WXUINT param, WXWORD id);

    // get the native menu handle
    WXHMENU GetHMenu() const { return m_hMenu; }

    // Return the start and end position of the radio group to which the item
    // at the given position belongs. Returns false if there is no radio group
    // containing this position.
    bool MSWGetRadioGroupRange(int pos, int *start, int *end) const;

#if wxUSE_ACCEL
    // called by wxMenuBar to build its accel table from the accels of all menus
    bool HasAccels() const { return !m_accels.empty(); }
    size_t GetAccelCount() const { return m_accels.size(); }
    size_t CopyAccels(wxAcceleratorEntry *accels) const;

    // called by wxMenuItem when its accels changes
    void UpdateAccel(wxMenuItem *item);

    // helper used by wxMenu itself (returns the index in m_accels)
    int FindAccel(int id) const;

    // used only by wxMDIParentFrame currently but could be useful elsewhere:
    // returns a new accelerator table with accelerators for just this menu
    // (shouldn't be called if we don't have any accelerators)
    wxAcceleratorTable *CreateAccelTable() const;
#endif // wxUSE_ACCEL

#if wxUSE_OWNER_DRAWN

    int GetMaxAccelWidth()
    {
        if (m_maxAccelWidth == -1)
            CalculateMaxAccelWidth();
        return m_maxAccelWidth;
    }

    void ResetMaxAccelWidth()
    {
        m_maxAccelWidth = -1;
    }

    // get the menu with given handle (recursively)
    wxMenu* MSWGetMenu(WXHMENU hMenu);

private:
    void CalculateMaxAccelWidth();

#endif // wxUSE_OWNER_DRAWN

protected:
    virtual wxMenuItem* DoAppend(wxMenuItem *item);
    virtual wxMenuItem* DoInsert(size_t pos, wxMenuItem *item);
    virtual wxMenuItem* DoRemove(wxMenuItem *item);

private:
    // This constructor is private, use MSWNewFromHMENU() to use it.
    wxMenu(WXHMENU hMenu);

    // Common part of all ctors, it doesn't create a new HMENU.
    void InitNoCreate();

    // Common part of all ctors except of the one above taking a native menu
    // handler: calls InitNoCreate() and also creates a new menu.
    void Init();

    // common part of Append/Insert (behaves as Append is pos == (size_t)-1)
    bool DoInsertOrAppend(wxMenuItem *item, size_t pos = (size_t)-1);


    // This variable contains the description of the radio item groups and
    // allows to find whether an item at the given position is part of the
    // group and also where its group starts and ends.
    //
    // It is initially NULL and only allocated if we have any radio items.
    wxMenuRadioItemsData *m_radioData;

    // if true, insert a breal before appending the next item
    bool m_doBreak;

    // the menu handle of this menu
    WXHMENU m_hMenu;

#if wxUSE_ACCEL
    // the accelerators for our menu items
    wxAcceleratorArray m_accels;
#endif // wxUSE_ACCEL

#if wxUSE_OWNER_DRAWN
    // true if the menu has any ownerdrawn items
    bool m_ownerDrawn;

    // the max width of menu items bitmaps
    int m_maxBitmapWidth;

    // the max width of menu items accels
    int m_maxAccelWidth;
#endif // wxUSE_OWNER_DRAWN

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMenu)
};

// ----------------------------------------------------------------------------
// Menu Bar (a la Windows)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenuBar : public wxMenuBarBase
{
public:
    // ctors & dtor
        // default constructor
    wxMenuBar();
        // unused under MSW
    wxMenuBar(long style);
        // menubar takes ownership of the menus arrays but copies the titles
    wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long style = 0);
    virtual ~wxMenuBar();

    // menubar construction
    virtual bool Append( wxMenu *menu, const wxString &title );
    virtual bool Insert(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Replace(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Remove(size_t pos);

    virtual void EnableTop( size_t pos, bool flag );
    virtual bool IsEnabledTop(size_t pos) const;
    virtual void SetMenuLabel( size_t pos, const wxString& label );
    virtual wxString GetMenuLabel( size_t pos ) const;

    // implementation from now on
    WXHMENU Create();
    virtual void Detach();
    virtual void Attach(wxFrame *frame);

#if defined(__WXWINCE__) && wxUSE_TOOLBAR
    // Under WinCE, a menubar is owned by the frame's toolbar
    void SetToolBar(wxToolBar* toolBar) { m_toolBar = toolBar; }
    wxToolBar* GetToolBar() const { return m_toolBar; }
#endif

#ifdef WINCE_WITH_COMMANDBAR
    WXHWND GetCommandBar() const { return m_commandBar; }
    bool AddAdornments(long style);
#endif

#if wxUSE_ACCEL
    // update the accel table (must be called after adding/deleting a menu)
    void RebuildAccelTable();
#endif // wxUSE_ACCEL

        // get the menu handle
    WXHMENU GetHMenu() const { return m_hMenu; }

    // if the menubar is modified, the display is not updated automatically,
    // call this function to update it (m_menuBarFrame should be !NULL)
    void Refresh();

    // To avoid compile warning
    void Refresh( bool eraseBackground,
                          const wxRect *rect = (const wxRect *) NULL ) { wxWindow::Refresh(eraseBackground, rect); }

    // get the menu with given handle (recursively)
    wxMenu* MSWGetMenu(WXHMENU hMenu);

protected:
    // common part of all ctors
    void Init();

    WXHMENU       m_hMenu;

    // Return the MSW position for a wxMenu which is sometimes different from
    // the wxWidgets position.
    int MSWPositionForWxMenu(wxMenu *menu, int wxpos);

#if defined(__WXWINCE__) && wxUSE_TOOLBAR
    wxToolBar*  m_toolBar;
#endif

#ifdef WINCE_WITH_COMMANDBAR
    WXHWND      m_commandBar;
    bool        m_adornmentsAdded;
#endif

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMenuBar)
};

#endif // _WX_MENU_H_
