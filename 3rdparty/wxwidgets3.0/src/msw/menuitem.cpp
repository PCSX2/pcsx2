///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/menuitem.cpp
// Purpose:     wxMenuItem implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.11.97
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MENUS

#include "wx/menuitem.h"
#include "wx/stockitem.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/dcmemory.h"
    #include "wx/font.h"
    #include "wx/bitmap.h"
    #include "wx/settings.h"
    #include "wx/window.h"
    #include "wx/accel.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/menu.h"
#endif

#if wxUSE_ACCEL
    #include "wx/accel.h"
#endif // wxUSE_ACCEL

#include "wx/msw/private.h"
#include "wx/msw/dc.h"

#ifdef __WXWINCE__
// Implemented in menu.cpp
UINT GetMenuState(HMENU hMenu, UINT id, UINT flags) ;
#endif

#if wxUSE_UXTHEME
    #include "wx/msw/uxtheme.h"
#endif

// ---------------------------------------------------------------------------
// macro
// ---------------------------------------------------------------------------

// hide the ugly cast
#define GetHMenuOf(menu)    ((HMENU)menu->GetHMenu())

// ----------------------------------------------------------------------------
// helper classes for temporarily changing HDC parameters
// ----------------------------------------------------------------------------

namespace
{

// This class just stores an HDC.
class HDCHandler
{
protected:
    HDCHandler(HDC hdc) : m_hdc(hdc) { }

    const HDC m_hdc;
};

class HDCTextColChanger : HDCHandler
{
public:
    HDCTextColChanger(HDC hdc, COLORREF col)
        : HDCHandler(hdc),
          m_colOld(::SetTextColor(hdc, col))
    {
    }

    ~HDCTextColChanger()
    {
        ::SetTextColor(m_hdc, m_colOld);
    }

private:
    COLORREF m_colOld;
};

class HDCBgColChanger : HDCHandler
{
public:
    HDCBgColChanger(HDC hdc, COLORREF col)
        : HDCHandler(hdc),
          m_colOld(::SetBkColor(hdc, col))
    {
    }

    ~HDCBgColChanger()
    {
        ::SetBkColor(m_hdc, m_colOld);
    }

private:
    COLORREF m_colOld;
};

class HDCBgModeChanger : HDCHandler
{
public:
    HDCBgModeChanger(HDC hdc, int mode)
        : HDCHandler(hdc),
          m_modeOld(::SetBkMode(hdc, mode))
    {
    }

    ~HDCBgModeChanger()
    {
        ::SetBkMode(m_hdc, m_modeOld);
    }

private:
    int m_modeOld;
};

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

#if wxUSE_OWNER_DRAWN

#include "wx/fontutil.h"
#include "wx/msw/private/metrics.h"

#ifndef SPI_GETKEYBOARDCUES
#define SPI_GETKEYBOARDCUES 0x100A
#endif

#if wxUSE_UXTHEME

enum MENUPARTS
{
    MENU_MENUITEM_TMSCHEMA = 1,
    MENU_SEPARATOR_TMSCHEMA = 6,
    MENU_POPUPBACKGROUND = 9,
    MENU_POPUPBORDERS = 10,
    MENU_POPUPCHECK = 11,
    MENU_POPUPCHECKBACKGROUND = 12,
    MENU_POPUPGUTTER = 13,
    MENU_POPUPITEM = 14,
    MENU_POPUPSEPARATOR = 15,
    MENU_POPUPSUBMENU = 16,
};


enum POPUPITEMSTATES
{
    MPI_NORMAL = 1,
    MPI_HOT = 2,
    MPI_DISABLED = 3,
    MPI_DISABLEDHOT = 4,
};

enum POPUPCHECKBACKGROUNDSTATES
{
    MCB_DISABLED = 1,
    MCB_NORMAL = 2,
    MCB_BITMAP = 3,
};

enum POPUPCHECKSTATES
{
    MC_CHECKMARKNORMAL = 1,
    MC_CHECKMARKDISABLED = 2,
    MC_BULLETNORMAL = 3,
    MC_BULLETDISABLED = 4,
};

const int TMT_MENUFONT       = 803;
const int TMT_BORDERSIZE     = 2403;
const int TMT_CONTENTMARGINS = 3602;
const int TMT_SIZINGMARGINS  = 3601;

#endif // wxUSE_UXTHEME

#endif // wxUSE_OWNER_DRAWN

// ----------------------------------------------------------------------------
// dynamic classes implementation
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxMenuItem
// ----------------------------------------------------------------------------

#if wxUSE_OWNER_DRAWN

namespace
{

// helper class to keep information about metrics and other stuff
// needed for measuring and drawing menu item
class MenuDrawData
{
public:
    // Wrapper around standard MARGINS structure providing some helper
    // functions and automatically initializing the margin fields to 0.
    struct Margins : MARGINS
    {
        Margins()
        {
            cxLeftWidth =
            cxRightWidth =
            cyTopHeight =
            cyBottomHeight = 0;
        }

        int GetTotalX() const { return cxLeftWidth + cxRightWidth; }
        int GetTotalY() const { return cyTopHeight + cyBottomHeight; }

        void ApplyTo(RECT& rect) const
        {
            rect.top += cyTopHeight;
            rect.left += cxLeftWidth;
            rect.right -= cyTopHeight;
            rect.bottom -= cyBottomHeight;
        }

        void UnapplyFrom(RECT& rect) const
        {
            rect.top -= cyTopHeight;
            rect.left -= cxLeftWidth;
            rect.right += cyTopHeight;
            rect.bottom += cyBottomHeight;
        }
    };

    Margins ItemMargin;         // popup item margins

    Margins CheckMargin;        // popup check margins
    Margins CheckBgMargin;      // popup check background margins

    Margins ArrowMargin;        // popup submenu arrow margins

    Margins SeparatorMargin;    // popup separator margins

    SIZE CheckSize;             // popup check size metric
    SIZE ArrowSize;             // popup submenu arrow size metric
    SIZE SeparatorSize;         // popup separator size metric

    int TextBorder;             // popup border space between
                                // item text and gutter

    int AccelBorder;            // popup border space between
                                // item text and accelerator

    int ArrowBorder;            // popup border space between
                                // item accelerator and submenu arrow

    int Offset;                 // system added space at the end of the menu,
                                // add this offset for remove the extra space

    wxFont Font;                // default menu font

    bool AlwaysShowCues;        // must keyboard cues always be shown?

    bool Theme;                 // is data initialized for FullTheme?

    static const MenuDrawData* Get()
    {
        // notice that s_menuData can't be created as a global variable because
        // it needs a window to initialize and no windows exist at the time of
        // globals initialization yet
        if ( !ms_instance )
        {
            static MenuDrawData s_menuData;
            ms_instance = &s_menuData;
        }

    #if wxUSE_UXTHEME
        bool theme = MenuLayout() == FullTheme;
        if ( ms_instance->Theme != theme )
            ms_instance->Init();
    #endif // wxUSE_UXTHEME
        return ms_instance;
    }

    MenuDrawData()
    {
        Init();
    }


    // get the theme engine or NULL if themes
    // are not available or not supported on menu
    static wxUxThemeEngine *GetUxThemeEngine()
    {
    #if wxUSE_UXTHEME
        if ( MenuLayout() == FullTheme )
            return wxUxThemeEngine::GetIfActive();
    #endif // wxUSE_UXTHEME
        return NULL;
    }


    enum MenuLayoutType
    {
        FullTheme,      // full menu themes (Vista or new)
        PseudoTheme,    // pseudo menu themes (on XP)
        Classic
    };

    static MenuLayoutType MenuLayout()
    {
        MenuLayoutType menu = Classic;
    #if wxUSE_UXTHEME
        if ( wxUxThemeEngine::GetIfActive() != NULL )
        {
            static wxWinVersion ver = wxGetWinVersion();
            if ( ver >= wxWinVersion_Vista )
                menu = FullTheme;
            else if ( ver == wxWinVersion_XP )
                menu = PseudoTheme;
        }
    #endif // wxUSE_UXTHEME
        return menu;
    }

private:
    void Init();

    static MenuDrawData* ms_instance;
};

MenuDrawData* MenuDrawData::ms_instance = NULL;

void MenuDrawData::Init()
{
#if wxUSE_UXTHEME
    wxUxThemeEngine* theme = GetUxThemeEngine();
    if ( theme )
    {
        wxWindow* window = static_cast<wxApp*>(wxApp::GetInstance())->GetTopWindow();
        wxUxThemeHandle hTheme(window, L"MENU");

        theme->GetThemeMargins(hTheme, NULL, MENU_POPUPITEM, 0,
                               TMT_CONTENTMARGINS, NULL,
                               &ItemMargin);

        theme->GetThemeMargins(hTheme, NULL, MENU_POPUPCHECK, 0,
                               TMT_CONTENTMARGINS, NULL,
                               &CheckMargin);
        theme->GetThemeMargins(hTheme, NULL, MENU_POPUPCHECKBACKGROUND, 0,
                               TMT_CONTENTMARGINS, NULL,
                               &CheckBgMargin);

        theme->GetThemeMargins(hTheme, NULL, MENU_POPUPSUBMENU, 0,
                               TMT_CONTENTMARGINS, NULL,
                               &ArrowMargin);

        theme->GetThemeMargins(hTheme, NULL, MENU_POPUPSEPARATOR, 0,
                               TMT_SIZINGMARGINS, NULL,
                               &SeparatorMargin);

        theme->GetThemePartSize(hTheme, NULL, MENU_POPUPCHECK, 0,
                                NULL, TS_TRUE, &CheckSize);

        theme->GetThemePartSize(hTheme, NULL, MENU_POPUPSUBMENU, 0,
                                NULL, TS_TRUE, &ArrowSize);

        theme->GetThemePartSize(hTheme, NULL, MENU_POPUPSEPARATOR, 0,
                                NULL, TS_TRUE, &SeparatorSize);

        theme->GetThemeInt(hTheme, MENU_POPUPBACKGROUND, 0, TMT_BORDERSIZE, &TextBorder);

        AccelBorder = 34;
        ArrowBorder = 0;

        Offset = -14;

        wxUxThemeFont themeFont;
        theme->GetThemeSysFont(hTheme, TMT_MENUFONT, themeFont.GetPtr());
        Font = wxFont(themeFont.GetLOGFONT());

        Theme = true;

        // native menu doesn't uses the vertical margins
        ItemMargin.cyTopHeight =
        ItemMargin.cyBottomHeight = 0;

        // native menu uses small top margin for separator
        if ( SeparatorMargin.cyTopHeight >= 2 )
            SeparatorMargin.cyTopHeight -= 2;
    }
    else
#endif // wxUSE_UXTHEME
    {
        const NONCLIENTMETRICS& metrics = wxMSWImpl::GetNonClientMetrics();

        CheckMargin.cxLeftWidth =
        CheckMargin.cxRightWidth  = ::GetSystemMetrics(SM_CXEDGE);
        CheckMargin.cyTopHeight =
        CheckMargin.cyBottomHeight = ::GetSystemMetrics(SM_CYEDGE);

        CheckSize.cx = ::GetSystemMetrics(SM_CXMENUCHECK);
        CheckSize.cy = ::GetSystemMetrics(SM_CYMENUCHECK);

        ArrowSize = CheckSize;

        // separator height with margins
        int sepFullSize = metrics.iMenuHeight / 2;

        SeparatorMargin.cxLeftWidth =
        SeparatorMargin.cxRightWidth = 1;
        SeparatorMargin.cyTopHeight =
        SeparatorMargin.cyBottomHeight = sepFullSize / 2 - 1;

        SeparatorSize.cx = 1;
        SeparatorSize.cy = sepFullSize - SeparatorMargin.GetTotalY();

        TextBorder = 0;
        AccelBorder = 8;
        ArrowBorder = 6;

        Offset = -12;

        Font = wxFont(wxNativeFontInfo(metrics.lfMenuFont));

        Theme = false;
    }

    int value;
    if ( ::SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &value, 0) == 0 )
    {
        // if it's not supported, we must be on an old Windows version
        // which always shows them
        value = 1;
    }

    AlwaysShowCues = value == 1;

}

} // anonymous namespace

#endif // wxUSE_OWNER_DRAWN


// ctor & dtor
// -----------

wxMenuItem::wxMenuItem(wxMenu *pParentMenu,
                       int id,
                       const wxString& text,
                       const wxString& strHelp,
                       wxItemKind kind,
                       wxMenu *pSubMenu)
          : wxMenuItemBase(pParentMenu, id, text, strHelp, kind, pSubMenu)
{
    Init();
}

#if WXWIN_COMPATIBILITY_2_8
wxMenuItem::wxMenuItem(wxMenu *parentMenu,
                       int id,
                       const wxString& text,
                       const wxString& help,
                       bool isCheckable,
                       wxMenu *subMenu)
          : wxMenuItemBase(parentMenu, id, text, help,
                           isCheckable ? wxITEM_CHECK : wxITEM_NORMAL, subMenu)
{
    Init();
}
#endif

void wxMenuItem::Init()
{
#if  wxUSE_OWNER_DRAWN

    // when the color is not valid, wxOwnerDraw takes the default ones.
    // If we set the colors here and they are changed by the user during
    // the execution, then the colors are not updated until the application
    // is restarted and our menus look bad
    SetTextColour(wxNullColour);
    SetBackgroundColour(wxNullColour);

    // setting default colors switched ownerdraw on: switch it off again
    SetOwnerDrawn(false);

    //  switch ownerdraw back on if using a non default margin
    if ( !IsSeparator() )
        SetMarginWidth(GetMarginWidth());

#endif // wxUSE_OWNER_DRAWN
}

wxMenuItem::~wxMenuItem()
{
}

// misc
// ----

// return the id for calling Win32 API functions
WXWPARAM wxMenuItem::GetMSWId() const
{
    // we must use ids in unsigned short range with Windows functions, if we
    // pass ids > USHRT_MAX to them they get very confused (e.g. start
    // generating WM_COMMAND messages with negative high word of wParam), so
    // use the cast to ensure the id is in range
    return m_subMenu ? wxPtrToUInt(m_subMenu->GetHMenu())
                     : static_cast<unsigned short>(GetId());
}

// get item state
// --------------

bool wxMenuItem::IsChecked() const
{
    // fix that RTTI is always getting the correct state (separators cannot be
    // checked, but the Windows call below returns true
    if ( IsSeparator() )
        return false;

    // the item might not be attached to a menu yet
    //
    // TODO: shouldn't we just always call the base class version? It seems
    //       like it ought to always be in sync
    if ( !m_parentMenu )
        return wxMenuItemBase::IsChecked();

    HMENU hmenu = GetHMenuOf(m_parentMenu);
    int flag = ::GetMenuState(hmenu, GetMSWId(), MF_BYCOMMAND);

    return (flag & MF_CHECKED) != 0;
}

// change item state
// -----------------

void wxMenuItem::Enable(bool enable)
{
    if ( m_isEnabled == enable )
        return;

    const int itemPos = MSGetMenuItemPos();
    if ( itemPos != -1 )
    {
        long rc = EnableMenuItem(GetHMenuOf(m_parentMenu),
                                 itemPos,
                                 MF_BYPOSITION |
                                 (enable ? MF_ENABLED : MF_GRAYED));

        if ( rc == -1 )
        {
            wxLogLastError(wxT("EnableMenuItem"));
        }
    }

    wxMenuItemBase::Enable(enable);
}

void wxMenuItem::Check(bool check)
{
    wxCHECK_RET( IsCheckable(), wxT("only checkable items may be checked") );

    if ( m_isChecked == check )
        return;

    if ( m_parentMenu )
    {
        int flags = check ? MF_CHECKED : MF_UNCHECKED;
        HMENU hmenu = GetHMenuOf(m_parentMenu);

        if ( GetKind() == wxITEM_RADIO )
        {
            // it doesn't make sense to uncheck a radio item -- what would this
            // do?
            if ( !check )
                return;

            // get the index of this item in the menu
            const wxMenuItemList& items = m_parentMenu->GetMenuItems();
            int pos = items.IndexOf(this);
            wxCHECK_RET( pos != wxNOT_FOUND,
                         wxT("menuitem not found in the menu items list?") );

            // get the radio group range
            int start,
                end;

            if ( !m_parentMenu->MSWGetRadioGroupRange(pos, &start, &end) )
            {
                wxFAIL_MSG( wxT("Menu radio item not part of radio group?") );
                return;
            }

#ifdef __WIN32__
            // calling CheckMenuRadioItem() with such parameters hangs my system
            // (NT4 SP6) and I suspect this could happen to the others as well,
            // so don't do it!
            wxCHECK_RET( start != -1 && end != -1,
                         wxT("invalid ::CheckMenuRadioItem() parameter(s)") );

            if ( !::CheckMenuRadioItem(hmenu,
                                       start,   // the first radio group item
                                       end,     // the last one
                                       pos,     // the one to check
                                       MF_BYPOSITION) )
            {
                wxLogLastError(wxT("CheckMenuRadioItem"));
            }
#endif // __WIN32__

            // also uncheck all the other items in this radio group
            wxMenuItemList::compatibility_iterator node = items.Item(start);
            for ( int n = start; n <= end && node; n++ )
            {
                if ( n != pos )
                {
                    node->GetData()->m_isChecked = false;
                }

                node = node->GetNext();
            }
        }
        else // check item
        {
            if ( ::CheckMenuItem(hmenu,
                                 GetMSWId(),
                                 MF_BYCOMMAND | flags) == (DWORD)-1 )
            {
                wxFAIL_MSG(wxT("CheckMenuItem() failed, item not in the menu?"));
            }
        }
    }

    wxMenuItemBase::Check(check);
}

void wxMenuItem::SetItemLabel(const wxString& txt)
{
    wxString text = txt;

    // don't do anything if label didn't change
    if ( m_text == txt )
        return;

    // wxMenuItemBase will do stock ID checks
    wxMenuItemBase::SetItemLabel(text);

    // the item can be not attached to any menu yet and SetItemLabel() is still
    // valid to call in this case and should do nothing else
    if ( !m_parentMenu )
        return;

#if wxUSE_ACCEL
    m_parentMenu->UpdateAccel(this);
#endif // wxUSE_ACCEL

    const int itemPos = MSGetMenuItemPos();
    if ( itemPos == -1 )
        return;

    HMENU hMenu = GetHMenuOf(m_parentMenu);

    // update the text of the native menu item
    WinStruct<MENUITEMINFO> info;

    // surprisingly, calling SetMenuItemInfo() with just MIIM_STRING doesn't
    // work as it resets the menu bitmap, so we need to first get the old item
    // state and then modify it
    const bool isLaterThanWin95 = wxGetWinVersion() > wxWinVersion_95;
    info.fMask = MIIM_STATE |
                 MIIM_ID |
                 MIIM_SUBMENU |
                 MIIM_CHECKMARKS |
                 MIIM_DATA;
    if ( isLaterThanWin95 )
        info.fMask |= MIIM_BITMAP | MIIM_FTYPE;
    else
        info.fMask |= MIIM_TYPE;
    if ( !::GetMenuItemInfo(hMenu, itemPos, TRUE, &info) )
    {
        wxLogLastError(wxT("GetMenuItemInfo"));
        return;
    }

#if wxUSE_OWNER_DRAWN
    // Don't set the text for the owner drawn items, they don't use it and even
    // though setting it doesn't seem to actually do any harm under Windows 7,
    // avoid doing this relatively nonsensical operation just in case it does
    // break something on other, past or future, Windows versions.
    //
    // Notice that we do need to call SetMenuItemInfo() even for the ownerdrawn
    // items however as otherwise their size wouldn't be recalculated as
    // WM_MEASUREITEM wouldn't be sent and this could result in display
    // problems if the length of the menu item changed significantly.
    //
    // Also notice that we shouldn't use our IsOwnerDrawn() because it can be
    // true because it was set by e.g. SetBitmap(), even if the item wasn't
    // made owner drawn at Windows level.
    if ( !(info.fState & MF_OWNERDRAW) )
#endif // wxUSE_OWNER_DRAWN
    {
        if ( isLaterThanWin95 )
            info.fMask |= MIIM_STRING;
        //else: MIIM_TYPE already specified
        info.dwTypeData = wxMSW_CONV_LPTSTR(m_text);
        info.cch = m_text.length();
    }

    if ( !::SetMenuItemInfo(hMenu, itemPos, TRUE, &info) )
    {
        wxLogLastError(wxT("SetMenuItemInfo"));
    }
}

#if wxUSE_OWNER_DRAWN

int wxMenuItem::MeasureAccelWidth() const
{
    wxString accel = GetItemLabel().AfterFirst(wxT('\t'));

    wxMemoryDC dc;
    wxFont font;
    GetFontToUse(font);
    dc.SetFont(font);

    wxCoord w;
    dc.GetTextExtent(accel, &w, NULL);

    return w;
}

wxString wxMenuItem::GetName() const
{
    return GetItemLabelText();
}

bool wxMenuItem::OnMeasureItem(size_t *width, size_t *height)
{
    const MenuDrawData* data = MenuDrawData::Get();

    if ( IsOwnerDrawn() )
    {
        *width  = data->ItemMargin.GetTotalX();
        *height = data->ItemMargin.GetTotalY();

        if ( IsSeparator() )
        {
            *width  += data->SeparatorSize.cx
                     + data->SeparatorMargin.GetTotalX();
            *height += data->SeparatorSize.cy
                     + data->SeparatorMargin.GetTotalY();
            return true;
        }

        wxString str = GetName();

        wxMemoryDC dc;
        wxFont font;
        GetFontToUse(font);
        dc.SetFont(font);

        wxCoord w, h;
        dc.GetTextExtent(str, &w, &h);

        *width = data->TextBorder + w + data->AccelBorder;
        *height = h;

        w = m_parentMenu->GetMaxAccelWidth();
        if ( w > 0 )
            *width += w + data->ArrowBorder;

        *width += data->Offset;
        *width += data->ArrowMargin.GetTotalX() + data->ArrowSize.cx;
    }
    else // don't draw the text, just the bitmap (if any)
    {
        *width = 0;
        *height = 0;
    }

    // bitmap

    if ( IsOwnerDrawn() )
    {
        // width of menu icon with margins in ownerdrawn menu
        // if any bitmap is not set, the width of space reserved for icon
        // image is equal to the width of std check mark,
        // if bitmap is set, then the width is set to the width of the widest
        // bitmap in menu (GetMarginWidth()) unless std check mark is wider,
        // then it's is set to std mark's width
        int imgWidth = wxMax(GetMarginWidth(), data->CheckSize.cx)
                     + data->CheckMargin.GetTotalX();

        *width += imgWidth + data->CheckBgMargin.GetTotalX();
    }

    if ( m_bmpChecked.IsOk() || m_bmpUnchecked.IsOk() )
    {
        // get size of bitmap always return valid value (0 for invalid bitmap),
        // so we don't needed check if bitmap is valid ;)
        size_t heightBmp = wxMax(m_bmpChecked.GetHeight(), m_bmpUnchecked.GetHeight());
        size_t widthBmp = wxMax(m_bmpChecked.GetWidth(),  m_bmpUnchecked.GetWidth());

        if ( IsOwnerDrawn() )
        {
            heightBmp += data->CheckMargin.GetTotalY();
        }
        else
        {
            // we must allocate enough space for the bitmap
            *width += widthBmp;
        }

        // Is BMP height larger than text height?
        if ( *height < heightBmp )
            *height = heightBmp;
    }

    // make sure that this item is at least as tall as the system menu height
    const size_t menuHeight = data->CheckMargin.GetTotalY()
                            + data->CheckSize.cy;
    if (*height < menuHeight)
        *height = menuHeight;

    return true;
}

bool wxMenuItem::OnDrawItem(wxDC& dc, const wxRect& rc,
                            wxODAction WXUNUSED(act), wxODStatus stat)
{
    const MenuDrawData* data = MenuDrawData::Get();

    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    HDC hdc = GetHdcOf(*impl);

    RECT rect;
    wxCopyRectToRECT(rc, rect);

    int imgWidth = wxMax(GetMarginWidth(), data->CheckSize.cx);

    if ( IsOwnerDrawn() )
    {
        // font and colors to use
        wxFont font;
        GetFontToUse(font);

        wxColour colText, colBack;
        GetColourToUse(stat, colText, colBack);

        // calculate metrics of item parts
        RECT rcSelection = rect;
        data->ItemMargin.ApplyTo(rcSelection);

        RECT rcSeparator = rcSelection;
        data->SeparatorMargin.ApplyTo(rcSeparator);

        RECT rcGutter = rcSelection;
        rcGutter.right = rcGutter.left
                       + data->ItemMargin.cxLeftWidth
                       + data->CheckBgMargin.cxLeftWidth
                       + data->CheckMargin.cxLeftWidth
                       + imgWidth
                       + data->CheckMargin.cxRightWidth
                       + data->CheckBgMargin.cxRightWidth;

        RECT rcText = rcSelection;
        rcText.left = rcGutter.right + data->TextBorder;

        // we draw the text label vertically centered, but this results in it
        // being 1px too low compared to native menus for some reason, fix it
        if ( data->MenuLayout() != MenuDrawData::FullTheme )
            rcText.top--;

#if wxUSE_UXTHEME
        // If a custom background colour is explicitly specified, we should use
        // it instead of the default theme background.
        wxUxThemeEngine* const theme = GetBackgroundColour().IsOk()
                                        ? NULL
                                        : MenuDrawData::GetUxThemeEngine();
        if ( theme )
        {
            POPUPITEMSTATES state;
            if ( stat & wxODDisabled )
            {
                state = (stat & wxODSelected) ? MPI_DISABLEDHOT
                                              : MPI_DISABLED;
            }
            else if ( stat & wxODSelected )
            {
                state = MPI_HOT;
            }
            else
            {
                state = MPI_NORMAL;
            }

            wxUxThemeHandle hTheme(GetMenu()->GetWindow(), L"MENU");

            if ( theme->IsThemeBackgroundPartiallyTransparent(hTheme,
                    MENU_POPUPITEM, state) )
            {
                theme->DrawThemeBackground(hTheme, hdc,
                                           MENU_POPUPBACKGROUND,
                                           0, &rect, NULL);
            }

            theme->DrawThemeBackground(hTheme, hdc, MENU_POPUPGUTTER,
                                       0, &rcGutter, NULL);

            if ( IsSeparator() )
            {
                rcSeparator.left = rcGutter.right;
                theme->DrawThemeBackground(hTheme, hdc, MENU_POPUPSEPARATOR,
                                           0, &rcSeparator, NULL);
                return true;
            }

            theme->DrawThemeBackground(hTheme, hdc, MENU_POPUPITEM,
                                       state, &rcSelection, NULL);

        }
        else
#endif // wxUSE_UXTHEME
        {
            if ( IsSeparator() )
            {
                DrawEdge(hdc, &rcSeparator, EDGE_ETCHED, BF_TOP);
                return true;
            }

            AutoHBRUSH hbr(colBack.GetPixel());
            SelectInHDC selBrush(hdc, hbr);
            ::FillRect(hdc, &rcSelection, hbr);
        }


        // draw text label
        // using native API because it recognizes '&'

        HDCTextColChanger changeTextCol(hdc, colText.GetPixel());
        HDCBgColChanger changeBgCol(hdc, colBack.GetPixel());
        HDCBgModeChanger changeBgMode(hdc, TRANSPARENT);

        SelectInHDC selFont(hdc, GetHfontOf(font));


        // item text name without mnemonic for calculating size
        wxString text = GetName();

        SIZE textSize;
        ::GetTextExtentPoint32(hdc, text.c_str(), text.length(), &textSize);

        // item text name with mnemonic
        text = GetItemLabel().BeforeFirst('\t');

        int flags = DST_PREFIXTEXT;
        // themes menu is using specified color for disabled labels
        if ( data->MenuLayout() == MenuDrawData::Classic &&
             (stat & wxODDisabled) && !(stat & wxODSelected) )
            flags |= DSS_DISABLED;

        if ( (stat & wxODHidePrefix) && !data->AlwaysShowCues )
            flags |= DSS_HIDEPREFIX;

        int x = rcText.left;
        int y = rcText.top + (rcText.bottom - rcText.top - textSize.cy) / 2;

        ::DrawState(hdc, NULL, NULL, wxMSW_CONV_LPARAM(text),
                    text.length(), x, y, 0, 0, flags);

        // ::SetTextAlign(hdc, TA_RIGHT) doesn't work with DSS_DISABLED or DSS_MONO
        // as the last parameter in DrawState() (at least with Windows98). So we have
        // to take care of right alignment ourselves.
        wxString accel = GetItemLabel().AfterFirst(wxT('\t'));
        if ( !accel.empty() )
        {
            SIZE accelSize;
            ::GetTextExtentPoint32(hdc, accel.c_str(), accel.length(), &accelSize);

            flags = DST_TEXT;
            // themes menu is using specified color for disabled labels
            if ( data->MenuLayout() == MenuDrawData::Classic &&
                 (stat & wxODDisabled) && !(stat & wxODSelected) )
                flags |= DSS_DISABLED;

            x = rcText.right - data->ArrowMargin.GetTotalX()
                                 - data->ArrowSize.cx
                                 - data->ArrowBorder;

            // right align accel on FullTheme menu, left otherwise
            if ( data->MenuLayout() == MenuDrawData::FullTheme)
                x -= accelSize.cx;
            else
                x -= m_parentMenu->GetMaxAccelWidth();

            y = rcText.top + (rcText.bottom - rcText.top - accelSize.cy) / 2;

            ::DrawState(hdc, NULL, NULL, wxMSW_CONV_LPARAM(accel),
                        accel.length(), x, y, 0, 0, flags);
        }
    }


    // draw the bitmap

    RECT rcImg;
    SetRect(&rcImg,
            rect.left   + data->ItemMargin.cxLeftWidth
                        + data->CheckBgMargin.cxLeftWidth
                        + data->CheckMargin.cxLeftWidth,
            rect.top    + data->ItemMargin.cyTopHeight
                        + data->CheckBgMargin.cyTopHeight
                        + data->CheckMargin.cyTopHeight,
            rect.left   + data->ItemMargin.cxLeftWidth
                        + data->CheckBgMargin.cxLeftWidth
                        + data->CheckMargin.cxLeftWidth
                        + imgWidth,
            rect.bottom - data->ItemMargin.cyBottomHeight
                        - data->CheckBgMargin.cyBottomHeight
                        - data->CheckMargin.cyBottomHeight);

    if ( IsCheckable() && !m_bmpChecked.IsOk() )
    {
        if ( stat & wxODChecked )
        {
            DrawStdCheckMark((WXHDC)hdc, &rcImg, stat);
        }
    }
    else
    {
        wxBitmap bmp;

        if ( stat & wxODDisabled )
        {
            bmp = GetDisabledBitmap();
        }

        if ( !bmp.IsOk() )
        {
            // for not checkable bitmaps we should always use unchecked one
            // because their checked bitmap is not set
            bmp = GetBitmap(!IsCheckable() || (stat & wxODChecked));

#if wxUSE_IMAGE
            if ( bmp.IsOk() && stat & wxODDisabled )
            {
                // we need to grey out the bitmap as we don't have any specific
                // disabled bitmap
                wxImage imgGrey = bmp.ConvertToImage().ConvertToGreyscale();
                if ( imgGrey.IsOk() )
                    bmp = wxBitmap(imgGrey);
            }
#endif // wxUSE_IMAGE
        }

        if ( bmp.IsOk() )
        {
            wxMemoryDC dcMem(&dc);
            dcMem.SelectObjectAsSource(bmp);

            // center bitmap
            int nBmpWidth  = bmp.GetWidth(),
                nBmpHeight = bmp.GetHeight();

            int x = rcImg.left + (imgWidth - nBmpWidth) / 2;
            int y = rcImg.top  + (rcImg.bottom - rcImg.top - nBmpHeight) / 2;
            dc.Blit(x, y, nBmpWidth, nBmpHeight, &dcMem, 0, 0, wxCOPY, true);
        }
    }

    return true;

}

namespace
{

// helper function for draw coloured check mark
void DrawColorCheckMark(HDC hdc, int x, int y, int cx, int cy, HDC hdcCheckMask, int idxColor)
{
    const COLORREF colBlack = RGB(0, 0, 0);
    const COLORREF colWhite = RGB(255, 255, 255);

    HDCTextColChanger changeTextCol(hdc, colBlack);
    HDCBgColChanger changeBgCol(hdc, colWhite);
    HDCBgModeChanger changeBgMode(hdc, TRANSPARENT);

    // memory DC for color bitmap
    MemoryHDC hdcMem(hdc);
    CompatibleBitmap hbmpMem(hdc, cx, cy);
    SelectInHDC selMem(hdcMem, hbmpMem);

    RECT rect = { 0, 0, cx, cy };
    ::FillRect(hdcMem, &rect, ::GetSysColorBrush(idxColor));

    const COLORREF colCheck = ::GetSysColor(idxColor);
    if ( colCheck == colWhite )
    {
        ::BitBlt(hdc, x, y, cx, cy, hdcCheckMask, 0, 0, MERGEPAINT);
        ::BitBlt(hdc, x, y, cx, cy, hdcMem, 0, 0, SRCAND);
    }
    else
    {
        if ( colCheck != colBlack )
        {
            const DWORD ROP_DSna = 0x00220326;  // dest = (NOT src) AND dest
            ::BitBlt(hdcMem, 0, 0, cx, cy, hdcCheckMask, 0, 0, ROP_DSna);
        }

        ::BitBlt(hdc, x, y, cx, cy, hdcCheckMask, 0, 0, SRCAND);
        ::BitBlt(hdc, x, y, cx, cy, hdcMem, 0, 0, SRCPAINT);
    }
}

} // anonymous namespace

void wxMenuItem::DrawStdCheckMark(WXHDC hdc_, const RECT* rc, wxODStatus stat)
{
    HDC hdc = (HDC)hdc_;

#if wxUSE_UXTHEME
    wxUxThemeEngine* theme = MenuDrawData::GetUxThemeEngine();
    if ( theme )
    {
        wxUxThemeHandle hTheme(GetMenu()->GetWindow(), L"MENU");

        const MenuDrawData* data = MenuDrawData::Get();

        // rect for background must be without check margins
        RECT rcBg = *rc;
        data->CheckMargin.UnapplyFrom(rcBg);

        POPUPCHECKBACKGROUNDSTATES stateCheckBg = (stat & wxODDisabled)
                                                    ? MCB_DISABLED
                                                    : MCB_NORMAL;

        theme->DrawThemeBackground(hTheme, hdc, MENU_POPUPCHECKBACKGROUND,
                                   stateCheckBg, &rcBg, NULL);

        POPUPCHECKSTATES stateCheck;
        if ( GetKind() == wxITEM_CHECK )
        {
            stateCheck = (stat & wxODDisabled) ? MC_CHECKMARKDISABLED
                                               : MC_CHECKMARKNORMAL;
        }
        else
        {
            stateCheck = (stat & wxODDisabled) ? MC_BULLETDISABLED
                                               : MC_BULLETNORMAL;
        }

        theme->DrawThemeBackground(hTheme, hdc, MENU_POPUPCHECK,
                                   stateCheck, rc, NULL);
    }
    else
#endif // wxUSE_UXTHEME
    {
        int cx = rc->right - rc->left;
        int cy = rc->bottom - rc->top;

        // first create mask of check mark
        MemoryHDC hdcMask(hdc);
        MonoBitmap hbmpMask(cx, cy);
        SelectInHDC selMask(hdcMask,hbmpMask);

        // then draw a check mark into it
        UINT stateCheck = (GetKind() == wxITEM_CHECK) ? DFCS_MENUCHECK
                                                      : DFCS_MENUBULLET;
        RECT rect = { 0, 0, cx, cy };
        ::DrawFrameControl(hdcMask, &rect, DFC_MENU, stateCheck);

        // first draw shadow if disabled
        if ( (stat & wxODDisabled) && !(stat & wxODSelected) )
        {
            DrawColorCheckMark(hdc, rc->left + 1, rc->top + 1,
                               cx, cy, hdcMask, COLOR_3DHILIGHT);
        }

        // then draw a check mark
        int color = COLOR_MENUTEXT;
        if ( stat & wxODDisabled )
            color = COLOR_BTNSHADOW;
        else if ( stat & wxODSelected )
            color = COLOR_HIGHLIGHTTEXT;

        DrawColorCheckMark(hdc, rc->left, rc->top, cx, cy, hdcMask, color);
    }
}

void wxMenuItem::GetFontToUse(wxFont& font) const
{
    font = GetFont();
    if ( !font.IsOk() )
        font = MenuDrawData::Get()->Font;
}

void wxMenuItem::GetColourToUse(wxODStatus stat, wxColour& colText, wxColour& colBack) const
{
#if wxUSE_UXTHEME
    wxUxThemeEngine* theme = MenuDrawData::GetUxThemeEngine();
    if ( theme )
    {
        wxUxThemeHandle hTheme(GetMenu()->GetWindow(), L"MENU");

        if ( stat & wxODDisabled)
        {
            wxRGBToColour(colText, theme->GetThemeSysColor(hTheme, COLOR_GRAYTEXT));
        }
        else
        {
            colText = GetTextColour();
            if ( !colText.IsOk() )
                wxRGBToColour(colText, theme->GetThemeSysColor(hTheme, COLOR_MENUTEXT));
        }

        if ( stat & wxODSelected )
        {
            wxRGBToColour(colBack, theme->GetThemeSysColor(hTheme, COLOR_HIGHLIGHT));
        }
        else
        {
            colBack = GetBackgroundColour();
            if ( !colBack.IsOk() )
                wxRGBToColour(colBack, theme->GetThemeSysColor(hTheme, COLOR_MENU));
        }
    }
    else
#endif // wxUSE_UXTHEME
    {
        wxOwnerDrawn::GetColourToUse(stat, colText, colBack);
    }
}
#endif // wxUSE_OWNER_DRAWN

int wxMenuItem::MSGetMenuItemPos() const
{
    if ( !m_parentMenu )
        return -1;

    const HMENU hMenu = GetHMenuOf(m_parentMenu);
    if ( !hMenu )
        return -1;

    const UINT id = GetMSWId();
    const int menuItems = ::GetMenuItemCount(hMenu);
    for ( int i = 0; i < menuItems; i++ )
    {
        const UINT state = ::GetMenuState(hMenu, i, MF_BYPOSITION);
        if ( state == (UINT)-1 )
        {
            // This indicates that the item at this position and is not
            // supposed to happen here, but test for it just in case.
            continue;
        }

        if ( state & MF_POPUP )
        {
            if ( ::GetSubMenu(hMenu, i) == (HMENU)wxUIntToPtr(id) )
                return i;
        }
        else if ( !(state & MF_SEPARATOR) )
        {
            if ( ::GetMenuItemID(hMenu, i) == id )
                return i;
        }
    }

    return -1;
}

// ----------------------------------------------------------------------------
// wxMenuItemBase
// ----------------------------------------------------------------------------

wxMenuItem *wxMenuItemBase::New(wxMenu *parentMenu,
                                int id,
                                const wxString& name,
                                const wxString& help,
                                wxItemKind kind,
                                wxMenu *subMenu)
{
    return new wxMenuItem(parentMenu, id, name, help, kind, subMenu);
}

#endif // wxUSE_MENUS
