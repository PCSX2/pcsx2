///////////////////////////////////////////////////////////////////////////////
// Name:        wx/menuitem.h
// Purpose:     wxMenuItem class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     25.10.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MENUITEM_H_BASE_
#define _WX_MENUITEM_H_BASE_

#include "wx/defs.h"

#if wxUSE_MENUS

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/object.h"  // base class

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxAcceleratorEntry;
class WXDLLIMPEXP_FWD_CORE wxMenuItem;
class WXDLLIMPEXP_FWD_CORE wxMenu;

// ----------------------------------------------------------------------------
// wxMenuItem is an item in the menu which may be either a normal item, a sub
// menu or a separator
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenuItemBase : public wxObject
{
public:
    // creation
    static wxMenuItem *New(wxMenu *parentMenu = NULL,
                           int itemid = wxID_SEPARATOR,
                           const wxString& text = wxEmptyString,
                           const wxString& help = wxEmptyString,
                           wxItemKind kind = wxITEM_NORMAL,
                           wxMenu *subMenu = NULL);

    // destruction: wxMenuItem will delete its submenu
    virtual ~wxMenuItemBase();

    // the menu we're in
    wxMenu *GetMenu() const { return m_parentMenu; }
    void SetMenu(wxMenu* menu) { m_parentMenu = menu; }

    // get/set id
    void SetId(int itemid) { m_id = itemid; }
    int  GetId() const { return m_id; }

    // the item's text (or name)
    //
    // NB: the item's label includes the accelerators and mnemonics info (if
    //     any), i.e. it may contain '&' or '_' or "\t..." and thus is
    //     different from the item's text which only contains the text shown
    //     in the menu. This used to be called SetText.
    virtual void SetItemLabel(const wxString& str);

    // return the item label including any mnemonics and accelerators.
    // This used to be called GetText.
    virtual wxString GetItemLabel() const { return m_text; }

    // return just the text of the item label, without any mnemonics
    // This used to be called GetLabel.
    virtual wxString GetItemLabelText() const { return GetLabelText(m_text); }

    // return just the text part of the given label (implemented in platform-specific code)
    // This used to be called GetLabelFromText.
    static wxString GetLabelText(const wxString& label);

    // what kind of menu item we are
    wxItemKind GetKind() const { return m_kind; }
    void SetKind(wxItemKind kind) { m_kind = kind; }
    bool IsSeparator() const { return m_kind == wxITEM_SEPARATOR; }

    bool IsCheck() const { return m_kind == wxITEM_CHECK; }
    bool IsRadio() const { return m_kind == wxITEM_RADIO; }

    virtual void SetCheckable(bool checkable)
        { m_kind = checkable ? wxITEM_CHECK : wxITEM_NORMAL; }

    // Notice that this doesn't quite match SetCheckable().
    bool IsCheckable() const
        { return m_kind == wxITEM_CHECK || m_kind == wxITEM_RADIO; }

    bool IsSubMenu() const { return m_subMenu != NULL; }
    void SetSubMenu(wxMenu *menu) { m_subMenu = menu; }
    wxMenu *GetSubMenu() const { return m_subMenu; }

    // state
    virtual void Enable(bool enable = true) { m_isEnabled = enable; }
    virtual bool IsEnabled() const { return m_isEnabled; }

    virtual void Check(bool check = true) { m_isChecked = check; }
    virtual bool IsChecked() const { return m_isChecked; }
    void Toggle() { Check(!m_isChecked); }

    // help string (displayed in the status bar by default)
    void SetHelp(const wxString& str);
    const wxString& GetHelp() const { return m_help; }

#if wxUSE_ACCEL
    // extract the accelerator from the given menu string, return NULL if none
    // found
    static wxAcceleratorEntry *GetAccelFromString(const wxString& label);

    // get our accelerator or NULL (caller must delete the pointer)
    virtual wxAcceleratorEntry *GetAccel() const;

    // set the accel for this item - this may also be done indirectly with
    // SetText()
    virtual void SetAccel(wxAcceleratorEntry *accel);
#endif // wxUSE_ACCEL

#if WXWIN_COMPATIBILITY_2_8
    // compatibility only, use new functions in the new code
    wxDEPRECATED( void SetName(const wxString& str) );
    wxDEPRECATED( wxString GetName() const );

    // Now use GetItemLabelText
    wxDEPRECATED( wxString GetLabel() const ) ;

    // Now use GetItemLabel
    wxDEPRECATED( const wxString& GetText() const );

    // Now use GetLabelText to strip the accelerators
    static wxDEPRECATED( wxString GetLabelFromText(const wxString& text) );

    // Now use SetItemLabel
    wxDEPRECATED( virtual void SetText(const wxString& str) );
#endif // WXWIN_COMPATIBILITY_2_8

    static wxMenuItem *New(wxMenu *parentMenu,
                           int itemid,
                           const wxString& text,
                           const wxString& help,
                           bool isCheckable,
                           wxMenu *subMenu = NULL)
    {
        return New(parentMenu, itemid, text, help,
                   isCheckable ? wxITEM_CHECK : wxITEM_NORMAL, subMenu);
    }

protected:
    wxWindowIDRef m_id;             // numeric id of the item >= 0 or wxID_ANY or wxID_SEPARATOR
    wxMenu       *m_parentMenu,     // the menu we belong to
                 *m_subMenu;        // our sub menu or NULL
    wxString      m_text,           // label of the item
                  m_help;           // the help string for the item
    wxItemKind    m_kind;           // separator/normal/check/radio item?
    bool          m_isChecked;      // is checked?
    bool          m_isEnabled;      // is enabled?

    // this ctor is for the derived classes only, we're never created directly
    wxMenuItemBase(wxMenu *parentMenu = NULL,
                   int itemid = wxID_SEPARATOR,
                   const wxString& text = wxEmptyString,
                   const wxString& help = wxEmptyString,
                   wxItemKind kind = wxITEM_NORMAL,
                   wxMenu *subMenu = NULL);

private:
    // and, if we have one ctor, compiler won't generate a default copy one, so
    // declare them ourselves - but don't implement as they shouldn't be used
    wxMenuItemBase(const wxMenuItemBase& item);
    wxMenuItemBase& operator=(const wxMenuItemBase& item);
};

#if WXWIN_COMPATIBILITY_2_8
inline void wxMenuItemBase::SetName(const wxString &str)
    { SetItemLabel(str); }
inline wxString wxMenuItemBase::GetName() const
    { return GetItemLabel(); }
inline wxString wxMenuItemBase::GetLabel() const
    { return GetLabelText(m_text); }
inline const wxString& wxMenuItemBase::GetText() const { return m_text; }
inline void wxMenuItemBase::SetText(const wxString& text) { SetItemLabel(text); }
#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// include the real class declaration
// ----------------------------------------------------------------------------

#ifdef wxUSE_BASE_CLASSES_ONLY
    #define wxMenuItem wxMenuItemBase
#else // !wxUSE_BASE_CLASSES_ONLY
#if defined(__WXUNIVERSAL__)
    #include "wx/univ/menuitem.h"
#elif defined(__WXMSW__)
    #include "wx/msw/menuitem.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/menuitem.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/menuitem.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/menuitem.h"
#elif defined(__WXMAC__)
    #include "wx/osx/menuitem.h"
#elif defined(__WXQT__)
    #include "wx/qt/menuitem.h"
#endif
#endif // wxUSE_BASE_CLASSES_ONLY/!wxUSE_BASE_CLASSES_ONLY

#endif // wxUSE_MENUS

#endif
    // _WX_MENUITEM_H_BASE_
