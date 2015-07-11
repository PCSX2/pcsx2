///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/menucmn.cpp
// Purpose:     wxMenu and wxMenuBar methods common to all ports
// Author:      Vadim Zeitlin
// Modified by:
// Created:     26.10.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MENUS

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/menu.h"
    #include "wx/frame.h"
#endif

#include "wx/stockitem.h"

// ----------------------------------------------------------------------------
// template lists
// ----------------------------------------------------------------------------

#include "wx/listimpl.cpp"

WX_DEFINE_LIST(wxMenuList)
WX_DEFINE_LIST(wxMenuItemList)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// XTI for wxMenu(Bar)
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxMenuStyle )
wxBEGIN_FLAGS( wxMenuStyle )
wxFLAGS_MEMBER(wxMENU_TEAROFF)
wxEND_FLAGS( wxMenuStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxMenu, wxEvtHandler, "wx/menu.h")
wxCOLLECTION_TYPE_INFO( wxMenuItem *, wxMenuItemList ) ;

#if wxUSE_EXTENDED_RTTI    
template<> void wxCollectionToVariantArray( wxMenuItemList const &theList,
                                           wxAnyList &value)
{
    wxListCollectionToAnyList<wxMenuItemList::compatibility_iterator>( theList, value ) ;
}
#endif

wxBEGIN_PROPERTIES_TABLE(wxMenu)
wxEVENT_PROPERTY( Select, wxEVT_MENU, wxCommandEvent)

wxPROPERTY( Title, wxString, SetTitle, GetTitle, wxString(), \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )

wxREADONLY_PROPERTY_FLAGS( MenuStyle, wxMenuStyle, long, GetStyle, \
                          wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), \
                          wxT("group")) // style

wxPROPERTY_COLLECTION( MenuItems, wxMenuItemList, wxMenuItem*, Append, \
                      GetMenuItems, 0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxMenu)

wxDIRECT_CONSTRUCTOR_2( wxMenu, wxString, Title, long, MenuStyle  )

wxDEFINE_FLAGS( wxMenuBarStyle )

wxBEGIN_FLAGS( wxMenuBarStyle )
wxFLAGS_MEMBER(wxMB_DOCKABLE)
wxEND_FLAGS( wxMenuBarStyle )

#if wxUSE_EXTENDED_RTTI    
// the negative id would lead the window (its superclass !) to
// vetoe streaming out otherwise
bool wxMenuBarStreamingCallback( const wxObject *WXUNUSED(object), wxObjectWriter *,
                                wxObjectWriterCallback *, const wxStringToAnyHashMap & )
{
    return true;
}
#endif

wxIMPLEMENT_DYNAMIC_CLASS_XTI_CALLBACK(wxMenuBar, wxWindow, "wx/menu.h", \
                                       wxMenuBarStreamingCallback)


#if wxUSE_EXTENDED_RTTI    
WX_DEFINE_LIST( wxMenuInfoHelperList )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxMenuInfoHelper, wxObject, "wx/menu.h")

wxBEGIN_PROPERTIES_TABLE(wxMenuInfoHelper)
wxREADONLY_PROPERTY( Menu, wxMenu*, GetMenu, wxEMPTY_PARAMETER_VALUE, \
                    0 /*flags*/, wxT("Helpstring"), wxT("group"))

wxREADONLY_PROPERTY( Title, wxString, GetTitle, wxString(), \
                    0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxMenuInfoHelper)

wxCONSTRUCTOR_2( wxMenuInfoHelper, wxMenu*, Menu, wxString, Title )

wxCOLLECTION_TYPE_INFO( wxMenuInfoHelper *, wxMenuInfoHelperList ) ;

template<> void wxCollectionToVariantArray( wxMenuInfoHelperList const &theList, 
                                           wxAnyList &value)
{
    wxListCollectionToAnyList<wxMenuInfoHelperList::compatibility_iterator>( theList, value ) ;
}

#endif

wxBEGIN_PROPERTIES_TABLE(wxMenuBar)
wxPROPERTY_COLLECTION( MenuInfos, wxMenuInfoHelperList, wxMenuInfoHelper*, AppendMenuInfo, \
                      GetMenuInfos, 0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxMenuBar)

wxCONSTRUCTOR_DUMMY( wxMenuBar )

#if wxUSE_EXTENDED_RTTI    

const wxMenuInfoHelperList& wxMenuBarBase::GetMenuInfos() const
{
    wxMenuInfoHelperList* list = const_cast< wxMenuInfoHelperList* > (& m_menuInfos);
    WX_CLEAR_LIST( wxMenuInfoHelperList, *list);
    for (size_t i = 0 ; i < GetMenuCount(); ++i)
    {
        wxMenuInfoHelper* info = new wxMenuInfoHelper();
        info->Create( GetMenu(i), GetMenuLabel(i));
        list->Append(info);
    }
    return m_menuInfos;
}

#endif

// ----------------------------------------------------------------------------
// XTI for wxMenuItem
// ----------------------------------------------------------------------------

#if wxUSE_EXTENDED_RTTI

bool wxMenuItemStreamingCallback( const wxObject *object, wxObjectWriter *,
                                 wxObjectWriterCallback *, const wxStringToAnyHashMap & )
{
    const wxMenuItem * mitem = wx_dynamic_cast(const wxMenuItem*, object);
    if ( mitem->GetMenu() && !mitem->GetMenu()->GetTitle().empty() )
    {
        // we don't stream out the first two items for menus with a title,
        // they will be reconstructed
        if ( mitem->GetMenu()->FindItemByPosition(0) == mitem ||
            mitem->GetMenu()->FindItemByPosition(1) == mitem )
            return false;
    }
    return true;
}

#endif

wxBEGIN_ENUM( wxItemKind )
wxENUM_MEMBER( wxITEM_SEPARATOR )
wxENUM_MEMBER( wxITEM_NORMAL )
wxENUM_MEMBER( wxITEM_CHECK )
wxENUM_MEMBER( wxITEM_RADIO )
wxEND_ENUM( wxItemKind )

wxIMPLEMENT_DYNAMIC_CLASS_XTI_CALLBACK(wxMenuItem, wxObject, "wx/menuitem.h", \
                                       wxMenuItemStreamingCallback)

wxBEGIN_PROPERTIES_TABLE(wxMenuItem)
wxPROPERTY( Parent, wxMenu*, SetMenu, GetMenu, wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( Id, int, SetId, GetId, wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( ItemLabel, wxString, SetItemLabel, GetItemLabel, wxString(), \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( Help, wxString, SetHelp, GetHelp, wxString(), \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxREADONLY_PROPERTY( Kind, wxItemKind, GetKind, wxEMPTY_PARAMETER_VALUE, \
                    0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( SubMenu, wxMenu*, SetSubMenu, GetSubMenu, wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( Enabled, bool, Enable, IsEnabled, wxAny((bool)true), \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( Checked, bool, Check, IsChecked, wxAny((bool)false), \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( Checkable, bool, SetCheckable, IsCheckable, wxAny((bool)false), \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxMenuItem)

wxDIRECT_CONSTRUCTOR_6( wxMenuItem, wxMenu*, Parent, int, Id, wxString, \
                       Text, wxString, Help, wxItemKind, Kind, wxMenu*, SubMenu )

// ----------------------------------------------------------------------------
// wxMenuItemBase
// ----------------------------------------------------------------------------

wxMenuItemBase::wxMenuItemBase(wxMenu *parentMenu,
                               int itemid,
                               const wxString& text,
                               const wxString& help,
                               wxItemKind kind,
                               wxMenu *subMenu)
{
    switch ( itemid )
    {
        case wxID_ANY:
            m_id = wxWindow::NewControlId();
            break;

        case wxID_SEPARATOR:
            m_id = wxID_SEPARATOR;

            // there is a lot of existing code just doing Append(wxID_SEPARATOR)
            // and it makes sense to omit the following optional parameters,
            // including the kind one which doesn't default to wxITEM_SEPARATOR,
            // of course, so override it here
            kind = wxITEM_SEPARATOR;
            break;

        case wxID_NONE:
            // (popup) menu titles in wxMSW use this ID to indicate that
            // it's not a real menu item, so we don't want the check below to
            // apply to it
            m_id = itemid;
            break;

        default:
            // ids are limited to 16 bits under MSW so portable code shouldn't
            // use ids outside of this range (negative ids generated by wx are
            // fine though)
            wxASSERT_MSG( (itemid >= 0 && itemid < SHRT_MAX) ||
                            (itemid >= wxID_AUTO_LOWEST && itemid <= wxID_AUTO_HIGHEST),
                          wxS("invalid itemid value") );
            m_id = itemid;
    }

    // notice that parentMenu can be NULL: the item can be attached to the menu
    // later with SetMenu()

    m_parentMenu  = parentMenu;
    m_subMenu     = subMenu;
    m_isEnabled   = true;
    m_isChecked   = false;
    m_kind        = kind;

    SetItemLabel(text);
    SetHelp(help);
}

wxMenuItemBase::~wxMenuItemBase()
{
    delete m_subMenu;
}

#if wxUSE_ACCEL

wxAcceleratorEntry *wxMenuItemBase::GetAccel() const
{
    return wxAcceleratorEntry::Create(GetItemLabel());
}

void wxMenuItemBase::SetAccel(wxAcceleratorEntry *accel)
{
    wxString text = m_text.BeforeFirst(wxT('\t'));
    if ( accel )
    {
        text += wxT('\t');
        text += accel->ToString();
    }

    SetItemLabel(text);
}

#endif // wxUSE_ACCEL

void wxMenuItemBase::SetItemLabel(const wxString& str)
{
    m_text = str;

    if ( m_text.empty() && !IsSeparator() )
    {
        wxASSERT_MSG( wxIsStockID(GetId()),
                      wxT("A non-stock menu item with an empty label?") );
        m_text = wxGetStockLabel(GetId(), wxSTOCK_WITH_ACCELERATOR |
                                          wxSTOCK_WITH_MNEMONIC);
    }
}

void wxMenuItemBase::SetHelp(const wxString& str)
{
    m_help = str;

    if ( m_help.empty() && !IsSeparator() && wxIsStockID(GetId()) )
    {
        // get a stock help string
        m_help = wxGetStockHelpString(GetId());
    }
}

#ifndef __WXPM__
wxString wxMenuItemBase::GetLabelText(const wxString& text)
{
    return wxStripMenuCodes(text);
}
#endif

#if WXWIN_COMPATIBILITY_2_8
wxString wxMenuItemBase::GetLabelFromText(const wxString& text)
{
    return GetLabelText(text);
}
#endif

bool wxMenuBase::ms_locked = true;

// ----------------------------------------------------------------------------
// wxMenu ctor and dtor
// ----------------------------------------------------------------------------

void wxMenuBase::Init(long style)
{
    m_menuBar = NULL;
    m_menuParent = NULL;

    m_invokingWindow = NULL;
    m_style = style;
    m_clientData = NULL;
    m_eventHandler = this;
}

wxMenuBase::~wxMenuBase()
{
    WX_CLEAR_LIST(wxMenuItemList, m_items);
}

// ----------------------------------------------------------------------------
// wxMenu item adding/removing
// ----------------------------------------------------------------------------

void wxMenuBase::AddSubMenu(wxMenu *submenu)
{
    wxCHECK_RET( submenu, wxT("can't add a NULL submenu") );

    submenu->SetParent((wxMenu *)this);
}

wxMenuItem* wxMenuBase::DoAppend(wxMenuItem *item)
{
    wxCHECK_MSG( item, NULL, wxT("invalid item in wxMenu::Append()") );

    m_items.Append(item);
    item->SetMenu((wxMenu*)this);
    if ( item->IsSubMenu() )
    {
        AddSubMenu(item->GetSubMenu());
    }

    return item;
}

wxMenuItem* wxMenuBase::Insert(size_t pos, wxMenuItem *item)
{
    wxCHECK_MSG( item, NULL, wxT("invalid item in wxMenu::Insert") );

    if ( pos == GetMenuItemCount() )
    {
        return DoAppend(item);
    }
    else
    {
        wxCHECK_MSG( pos < GetMenuItemCount(), NULL,
                     wxT("invalid index in wxMenu::Insert") );

        return DoInsert(pos, item);
    }
}

wxMenuItem* wxMenuBase::DoInsert(size_t pos, wxMenuItem *item)
{
    wxCHECK_MSG( item, NULL, wxT("invalid item in wxMenu::Insert()") );

    wxMenuItemList::compatibility_iterator node = m_items.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("invalid index in wxMenu::Insert()") );

    m_items.Insert(node, item);
    item->SetMenu((wxMenu*)this);
    if ( item->IsSubMenu() )
    {
        AddSubMenu(item->GetSubMenu());
    }

    return item;
}

wxMenuItem *wxMenuBase::Remove(wxMenuItem *item)
{
    wxCHECK_MSG( item, NULL, wxT("invalid item in wxMenu::Remove") );

    return DoRemove(item);
}

wxMenuItem *wxMenuBase::DoRemove(wxMenuItem *item)
{
    wxMenuItemList::compatibility_iterator node = m_items.Find(item);

    // if we get here, the item is valid or one of Remove() functions is broken
    wxCHECK_MSG( node, NULL, wxT("bug in wxMenu::Remove logic") );

    // we detach the item, but we do delete the list node (i.e. don't call
    // DetachNode() here!)
    m_items.Erase(node);

    // item isn't attached to anything any more
    item->SetMenu(NULL);
    wxMenu *submenu = item->GetSubMenu();
    if ( submenu )
    {
        submenu->SetParent(NULL);
        if ( submenu->IsAttached() )
            submenu->Detach();
    }

    return item;
}

bool wxMenuBase::Delete(wxMenuItem *item)
{
    wxCHECK_MSG( item, false, wxT("invalid item in wxMenu::Delete") );

    return DoDelete(item);
}

bool wxMenuBase::DoDelete(wxMenuItem *item)
{
    wxMenuItem *item2 = DoRemove(item);
    wxCHECK_MSG( item2, false, wxT("failed to delete menu item") );

    // don't delete the submenu
    item2->SetSubMenu(NULL);

    delete item2;

    return true;
}

bool wxMenuBase::Destroy(wxMenuItem *item)
{
    wxCHECK_MSG( item, false, wxT("invalid item in wxMenu::Destroy") );

    return DoDestroy(item);
}

bool wxMenuBase::DoDestroy(wxMenuItem *item)
{
    wxMenuItem *item2 = DoRemove(item);
    wxCHECK_MSG( item2, false, wxT("failed to delete menu item") );

    delete item2;

    return true;
}

// ----------------------------------------------------------------------------
// wxMenu searching for items
// ----------------------------------------------------------------------------

// Finds the item id matching the given string, wxNOT_FOUND if not found.
int wxMenuBase::FindItem(const wxString& text) const
{
    wxString label = wxMenuItem::GetLabelText(text);
    for ( wxMenuItemList::compatibility_iterator node = m_items.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxMenuItem *item = node->GetData();
        if ( item->IsSubMenu() )
        {
            int rc = item->GetSubMenu()->FindItem(label);
            if ( rc != wxNOT_FOUND )
                return rc;
        }

        // we execute this code for submenus as well to alllow finding them by
        // name just like the ordinary items
        if ( !item->IsSeparator() )
        {
            if ( item->GetItemLabelText() == label )
                return item->GetId();
        }
    }

    return wxNOT_FOUND;
}

// recursive search for item by id
wxMenuItem *wxMenuBase::FindItem(int itemId, wxMenu **itemMenu) const
{
    if ( itemMenu )
        *itemMenu = NULL;

    wxMenuItem *item = NULL;
    for ( wxMenuItemList::compatibility_iterator node = m_items.GetFirst();
          node && !item;
          node = node->GetNext() )
    {
        item = node->GetData();

        if ( item->GetId() == itemId )
        {
            if ( itemMenu )
                *itemMenu = (wxMenu *)this;
        }
        else if ( item->IsSubMenu() )
        {
            item = item->GetSubMenu()->FindItem(itemId, itemMenu);
        }
        else
        {
            // don't exit the loop
            item = NULL;
        }
    }

    return item;
}

// non recursive search
wxMenuItem *wxMenuBase::FindChildItem(int itemid, size_t *ppos) const
{
    wxMenuItem *item = NULL;
    wxMenuItemList::compatibility_iterator node = GetMenuItems().GetFirst();

    size_t pos;
    for ( pos = 0; node; pos++ )
    {
        if ( node->GetData()->GetId() == itemid )
        {
            item = node->GetData();

            break;
        }

        node = node->GetNext();
    }

    if ( ppos )
    {
        *ppos = item ? pos : (size_t)wxNOT_FOUND;
    }

    return item;
}

// find by position
wxMenuItem* wxMenuBase::FindItemByPosition(size_t position) const
{
    wxCHECK_MSG( position < m_items.GetCount(), NULL,
                 wxT("wxMenu::FindItemByPosition(): invalid menu index") );

    return m_items.Item( position )->GetData();
}

// ----------------------------------------------------------------------------
// wxMenu helpers used by derived classes
// ----------------------------------------------------------------------------

// Update a menu and all submenus recursively. source is the object that has
// the update event handlers defined for it. If NULL, the menu or associated
// window will be used.
void wxMenuBase::UpdateUI(wxEvtHandler* source)
{
    wxWindow * const win = GetWindow();

    if ( !source && win )
        source = win->GetEventHandler();
    if ( !source )
        source = GetEventHandler();
    if ( !source )
        source = this;

    wxMenuItemList::compatibility_iterator  node = GetMenuItems().GetFirst();
    while ( node )
    {
        wxMenuItem* item = node->GetData();
        if ( !item->IsSeparator() )
        {
            wxWindowID itemid = item->GetId();
            wxUpdateUIEvent event(itemid);
            event.SetEventObject( this );

            if ( source->ProcessEvent(event) )
            {
                // if anything changed, update the changed attribute
                if (event.GetSetText())
                    SetLabel(itemid, event.GetText());
                if (event.GetSetChecked())
                    Check(itemid, event.GetChecked());
                if (event.GetSetEnabled())
                    Enable(itemid, event.GetEnabled());
            }

            // recurse to the submenus
            if ( item->GetSubMenu() )
                item->GetSubMenu()->UpdateUI(source);
        }
        //else: item is a separator (which doesn't process update UI events)

        node = node->GetNext();
    }
}

bool wxMenuBase::SendEvent(int itemid, int checked)
{
    wxCommandEvent event(wxEVT_MENU, itemid);
    event.SetEventObject(this);
    event.SetInt(checked);

    wxWindow* const win = GetWindow();
    wxMenuBar* const mb = GetMenuBar();

    // Try the menu's event handler first
    wxEvtHandler *handler = GetEventHandler();
    if ( handler )
    {
        // Indicate to the event processing code that we're going to pass this
        // event to another handler if it's not processed here to prevent it
        // from passing the event to wxTheApp: this will be done below if we do
        // have the associated window.
        if ( win || mb )
            event.SetWillBeProcessedAgain();

        if ( handler->SafelyProcessEvent(event) )
            return true;
    }

    // If this menu is part of the menu bar, process the event there: this will
    // also propagate it upwards to the window containing the menu bar.
    if ( mb )
        return mb->HandleWindowEvent(event);

    // Try the window the menu was popped up from.
    if ( win )
        return win->HandleWindowEvent(event);

    // Not processed.
    return false;
}

// ----------------------------------------------------------------------------
// wxMenu attaching/detaching to/from menu bar
// ----------------------------------------------------------------------------

wxMenuBar* wxMenuBase::GetMenuBar() const
{
    if(GetParent())
        return GetParent()->GetMenuBar();
    return m_menuBar;
}

void wxMenuBase::Attach(wxMenuBarBase *menubar)
{
    // use Detach() instead!
    wxASSERT_MSG( menubar, wxT("menu can't be attached to NULL menubar") );

    // use IsAttached() to prevent this from happening
    wxASSERT_MSG( !m_menuBar, wxT("attaching menu twice?") );

    m_menuBar = (wxMenuBar *)menubar;
}

void wxMenuBase::Detach()
{
    // use IsAttached() to prevent this from happening
    wxASSERT_MSG( m_menuBar, wxT("detaching unattached menu?") );

    m_menuBar = NULL;
}

// ----------------------------------------------------------------------------
// wxMenu invoking window handling
// ----------------------------------------------------------------------------

void wxMenuBase::SetInvokingWindow(wxWindow *win)
{
    wxASSERT_MSG( !GetParent(),
                    "should only be called for top level popup menus" );
    wxASSERT_MSG( !IsAttached(),
                    "menus attached to menu bar can't have invoking window" );

    m_invokingWindow = win;
}

wxWindow *wxMenuBase::GetWindow() const
{
    // only the top level menus have non-NULL invoking window or a pointer to
    // the menu bar so recurse upwards until we find it
    const wxMenuBase *menu = this;
    while ( menu->GetParent() )
    {
        menu = menu->GetParent();
    }

    return menu->GetMenuBar() ? menu->GetMenuBar()->GetFrame()
                              : menu->GetInvokingWindow();
}

// ----------------------------------------------------------------------------
// wxMenu functions forwarded to wxMenuItem
// ----------------------------------------------------------------------------

void wxMenuBase::Enable( int itemid, bool enable )
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("wxMenu::Enable: no such item") );

    item->Enable(enable);
}

bool wxMenuBase::IsEnabled( int itemid ) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, false, wxT("wxMenu::IsEnabled: no such item") );

    return item->IsEnabled();
}

void wxMenuBase::Check( int itemid, bool enable )
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("wxMenu::Check: no such item") );

    item->Check(enable);
}

bool wxMenuBase::IsChecked( int itemid ) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, false, wxT("wxMenu::IsChecked: no such item") );

    return item->IsChecked();
}

void wxMenuBase::SetLabel( int itemid, const wxString &label )
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("wxMenu::SetLabel: no such item") );

    item->SetItemLabel(label);
}

wxString wxMenuBase::GetLabel( int itemid ) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, wxEmptyString, wxT("wxMenu::GetLabel: no such item") );

    return item->GetItemLabel();
}

void wxMenuBase::SetHelpString( int itemid, const wxString& helpString )
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("wxMenu::SetHelpString: no such item") );

    item->SetHelp( helpString );
}

wxString wxMenuBase::GetHelpString( int itemid ) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, wxEmptyString, wxT("wxMenu::GetHelpString: no such item") );

    return item->GetHelp();
}

// ----------------------------------------------------------------------------
// wxMenuBarBase ctor and dtor
// ----------------------------------------------------------------------------

wxMenuBarBase::wxMenuBarBase()
{
    // not attached yet
    m_menuBarFrame = NULL;
}

wxMenuBarBase::~wxMenuBarBase()
{
    WX_CLEAR_LIST(wxMenuList, m_menus);
}

// ----------------------------------------------------------------------------
// wxMenuBar item access: the base class versions manage m_menus list, the
// derived class should reflect the changes in the real menubar
// ----------------------------------------------------------------------------

wxMenu *wxMenuBarBase::GetMenu(size_t pos) const
{
    wxMenuList::compatibility_iterator node = m_menus.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("bad index in wxMenuBar::GetMenu()") );

    return node->GetData();
}

bool wxMenuBarBase::Append(wxMenu *menu, const wxString& title)
{
    wxCHECK_MSG( menu, false, wxT("can't append NULL menu") );
    wxCHECK_MSG( !title.empty(), false, wxT("can't append menu with empty title") );

    m_menus.Append(menu);
    menu->Attach(this);

    return true;
}

bool wxMenuBarBase::Insert(size_t pos, wxMenu *menu,
                           const wxString& title)
{
    if ( pos == m_menus.GetCount() )
    {
        return wxMenuBarBase::Append(menu, title);
    }
    else // not at the end
    {
        wxCHECK_MSG( menu, false, wxT("can't insert NULL menu") );

        wxMenuList::compatibility_iterator node = m_menus.Item(pos);
        wxCHECK_MSG( node, false, wxT("bad index in wxMenuBar::Insert()") );

        m_menus.Insert(node, menu);
        menu->Attach(this);

        return true;
    }
}

wxMenu *wxMenuBarBase::Replace(size_t pos, wxMenu *menu,
                               const wxString& WXUNUSED(title))
{
    wxCHECK_MSG( menu, NULL, wxT("can't insert NULL menu") );

    wxMenuList::compatibility_iterator node = m_menus.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("bad index in wxMenuBar::Replace()") );

    wxMenu *menuOld = node->GetData();
    node->SetData(menu);

    menu->Attach(this);
    menuOld->Detach();

    return menuOld;
}

wxMenu *wxMenuBarBase::Remove(size_t pos)
{
    wxMenuList::compatibility_iterator node = m_menus.Item(pos);
    wxCHECK_MSG( node, NULL, wxT("bad index in wxMenuBar::Remove()") );

    wxMenu *menu = node->GetData();
    m_menus.Erase(node);
    menu->Detach();

    return menu;
}

int wxMenuBarBase::FindMenu(const wxString& title) const
{
    wxString label = wxMenuItem::GetLabelText(title);

    size_t count = GetMenuCount();
    for ( size_t i = 0; i < count; i++ )
    {
        wxString title2 = GetMenuLabel(i);
        if ( (title2 == title) ||
             (wxMenuItem::GetLabelText(title2) == label) )
        {
            // found
            return (int)i;
        }
    }

    return wxNOT_FOUND;

}

// ----------------------------------------------------------------------------
// wxMenuBar attaching/detaching to/from the frame
// ----------------------------------------------------------------------------

void wxMenuBarBase::Attach(wxFrame *frame)
{
    wxASSERT_MSG( !IsAttached(), wxT("menubar already attached!") );

    SetParent(frame);
    m_menuBarFrame = frame;
}

void wxMenuBarBase::Detach()
{
    wxASSERT_MSG( IsAttached(), wxT("detaching unattached menubar") );

    m_menuBarFrame = NULL;
    SetParent(NULL);
}

// ----------------------------------------------------------------------------
// wxMenuBar searching for items
// ----------------------------------------------------------------------------

wxMenuItem *wxMenuBarBase::FindItem(int itemid, wxMenu **menu) const
{
    if ( menu )
        *menu = NULL;

    wxMenuItem *item = NULL;
    size_t count = GetMenuCount(), i;
    wxMenuList::const_iterator it;
    for ( i = 0, it = m_menus.begin(); !item && (i < count); i++, it++ )
    {
        item = (*it)->FindItem(itemid, menu);
    }

    return item;
}

int wxMenuBarBase::FindMenuItem(const wxString& menu, const wxString& item) const
{
    wxString label = wxMenuItem::GetLabelText(menu);

    int i = 0;
    wxMenuList::compatibility_iterator node;
    for ( node = m_menus.GetFirst(); node; node = node->GetNext(), i++ )
    {
        if ( label == wxMenuItem::GetLabelText(GetMenuLabel(i)) )
            return node->GetData()->FindItem(item);
    }

    return wxNOT_FOUND;
}

// ---------------------------------------------------------------------------
// wxMenuBar functions forwarded to wxMenuItem
// ---------------------------------------------------------------------------

void wxMenuBarBase::Enable(int itemid, bool enable)
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("attempt to enable an item which doesn't exist") );

    item->Enable(enable);
}

void wxMenuBarBase::Check(int itemid, bool check)
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("attempt to check an item which doesn't exist") );
    wxCHECK_RET( item->IsCheckable(), wxT("attempt to check an uncheckable item") );

    item->Check(check);
}

bool wxMenuBarBase::IsChecked(int itemid) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, false, wxT("wxMenuBar::IsChecked(): no such item") );

    return item->IsChecked();
}

bool wxMenuBarBase::IsEnabled(int itemid) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, false, wxT("wxMenuBar::IsEnabled(): no such item") );

    return item->IsEnabled();
}

void wxMenuBarBase::SetLabel(int itemid, const wxString& label)
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("wxMenuBar::SetLabel(): no such item") );

    item->SetItemLabel(label);
}

wxString wxMenuBarBase::GetLabel(int itemid) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, wxEmptyString,
                 wxT("wxMenuBar::GetLabel(): no such item") );

    return item->GetItemLabel();
}

void wxMenuBarBase::SetHelpString(int itemid, const wxString& helpString)
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_RET( item, wxT("wxMenuBar::SetHelpString(): no such item") );

    item->SetHelp(helpString);
}

wxString wxMenuBarBase::GetHelpString(int itemid) const
{
    wxMenuItem *item = FindItem(itemid);

    wxCHECK_MSG( item, wxEmptyString,
                 wxT("wxMenuBar::GetHelpString(): no such item") );

    return item->GetHelp();
}

void wxMenuBarBase::UpdateMenus()
{
    wxMenu* menu;
    int nCount = GetMenuCount();
    for (int n = 0; n < nCount; n++)
    {
        menu = GetMenu( n );
        if (menu != NULL)
            menu->UpdateUI( NULL );
    }
}

#if WXWIN_COMPATIBILITY_2_8
// get or change the label of the menu at given position
void wxMenuBarBase::SetLabelTop(size_t pos, const wxString& label)
{
    SetMenuLabel(pos, label);
}

wxString wxMenuBarBase::GetLabelTop(size_t pos) const
{
    return GetMenuLabelText(pos);
}
#endif

#endif // wxUSE_MENUS
