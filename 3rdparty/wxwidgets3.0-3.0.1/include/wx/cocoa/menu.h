///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/menu.h
// Purpose:     wxMenu and wxMenuBar classes
// Author:      David Elliott
// Modified by:
// Created:     2002/12/09
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_MENU_H__
#define __WX_COCOA_MENU_H__

#include "wx/cocoa/NSMenu.h"

#if wxUSE_ACCEL
    #include "wx/accel.h"
#endif // wxUSE_ACCEL

// ========================================================================
// wxMenu
// ========================================================================

class WXDLLIMPEXP_CORE wxMenu : public wxMenuBase, public wxCocoaNSMenu
{
public:
    // ctors and dtor
    wxMenu(const wxString& title, long style = 0)
    :   wxMenuBase(title, style)
    ,   m_cocoaDeletes(false)
    {   Create(title,style); }
    bool Create(const wxString& title, long style = 0);

    wxMenu(long style = 0) : wxMenuBase(style) { Create(wxEmptyString, style); }

    virtual ~wxMenu();

// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
public:
    inline WX_NSMenu GetNSMenu() { return m_cocoaNSMenu; }
    void SetCocoaDeletes(bool cocoaDeletes);
    virtual void Cocoa_dealloc();
protected:
    WX_NSMenu m_cocoaNSMenu;
    bool m_cocoaDeletes;
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
protected:
    // implement base class virtuals
    virtual wxMenuItem* DoAppend(wxMenuItem *item);
    virtual wxMenuItem* DoInsert(size_t pos, wxMenuItem *item);
    virtual wxMenuItem* DoRemove(wxMenuItem *item);

#if wxUSE_ACCEL
    // add/remove accel for the given menu item
    void AddAccelFor(wxMenuItem *item);
    void RemoveAccelFor(wxMenuItem *item);
#endif // wxUSE_ACCEL

private:
#if wxUSE_ACCEL
    // the accel table for this menu
    wxAcceleratorTable m_accelTable;
#endif // wxUSE_ACCEL

    DECLARE_DYNAMIC_CLASS(wxMenu)
};

// ========================================================================
// wxMenuBar
// ========================================================================
class WXDLLIMPEXP_CORE wxMenuBar : public wxMenuBarBase
{
public:
    // ctors and dtor
    wxMenuBar(long style = 0) { Create(style); }
    wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long style = 0);
    bool Create(long style = 0);
    virtual ~wxMenuBar();

// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
public:
    inline WX_NSMenu GetNSMenu() { return m_cocoaNSMenu; }
protected:
    WX_NSMenu m_cocoaNSMenu;
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // implement base class virtuals
    virtual bool Append(wxMenu *menu, const wxString &title);
    virtual bool Insert(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Replace(size_t pos, wxMenu *menu, const wxString& title);
    virtual wxMenu *Remove(size_t pos);

    virtual void EnableTop(size_t pos, bool enable);
    virtual bool IsEnabledTop(size_t pos) const;

    virtual void SetMenuLabel(size_t pos, const wxString& label);
    virtual wxString GetMenuLabel(size_t pos) const;

    virtual void Attach(wxFrame *frame);
    virtual void Detach();

    // get the next item for the givan accel letter (used by wxFrame), return
    // -1 if none
    //
    // if unique is not NULL, filled with TRUE if there is only one item with
    // this accel, FALSE if two or more
    int FindNextItemForAccel(int idxStart,
                             int keycode,
                             bool *unique = NULL) const;

    // called by wxFrame to set focus to or open the given menu
    void SelectMenu(size_t pos);

#if wxUSE_ACCEL
    // find the item for the given accel and generate an event if found
    bool ProcessAccelEvent(const wxKeyEvent& event);
#endif // wxUSE_ACCEL

protected:
    // event handlers
    void OnLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKillFocus(wxFocusEvent& event);

    // process the mouse move event, return TRUE if we did, FALSE to continue
    // processing as usual
    //
    // the coordinates are client coordinates of menubar, convert if necessary
    bool ProcessMouseEvent(const wxPoint& pt);

    // menubar geometry
    virtual wxSize DoGetBestClientSize() const;

    // has the menubar been created already?
    bool IsCreated() const { return m_frameLast != NULL; }

    // get the (total) width of the specified menu
    wxCoord GetItemWidth(size_t pos) const;

    // get the rect of the item
    wxRect GetItemRect(size_t pos) const;

    // get the menu from the given point or -1 if none
    int GetMenuFromPoint(const wxPoint& pos) const;

    // refresh the given item
    void RefreshItem(size_t pos);

    // refresh all items after this one (including it)
    void RefreshAllItemsAfter(size_t pos);

    // do we show a menu currently?
    bool IsShowingMenu() const { return m_menuShown != 0; }

    // we don't want to have focus except while selecting from menu
    void GiveAwayFocus();

    // the current item (only used when menubar has focus)
    int m_current;

private:
    // the last frame to which we were attached, NULL initially
    wxFrame *m_frameLast;

    // the currently shown menu or NULL
    wxMenu *m_menuShown;

    // should be showing the menu? this is subtly different from m_menuShown !=
    // NULL as the menu which should be shown may be disabled in which case we
    // don't show it - but will do as soon as the focus shifts to another menu
    bool m_shouldShowMenu;

    DECLARE_DYNAMIC_CLASS(wxMenuBar)
};

#endif // _WX_COCOA_MENU_H_
