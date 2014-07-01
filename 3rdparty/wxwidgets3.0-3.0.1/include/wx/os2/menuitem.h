///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/menuitem.h
// Purpose:     wxMenuItem class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.11.97
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _MENUITEM_H
#define   _MENUITEM_H

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"
#include "wx/os2/private.h"     // for MENUITEM

// an exception to the general rule that a normal header doesn't include other
// headers - only because ownerdrw.h is not always included and I don't want
// to write #ifdef's everywhere...
#if wxUSE_OWNER_DRAWN
    #include "wx/ownerdrw.h"
    #include "wx/bitmap.h"
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxMenuItem: an item in the menu, optionally implements owner-drawn behaviour
// ----------------------------------------------------------------------------
class WXDLLIMPEXP_CORE wxMenuItem: public wxMenuItemBase
#if wxUSE_OWNER_DRAWN
                            , public wxOwnerDrawn
#endif
{
public:
    //
    // ctor & dtor
    //
    wxMenuItem( wxMenu*         pParentMenu = NULL
               ,int             nId = wxID_SEPARATOR
               ,const wxString& rStrName = wxEmptyString
               ,const wxString& rWxHelp = wxEmptyString
               ,wxItemKind      eKind = wxITEM_NORMAL
               ,wxMenu*         pSubMenu = NULL
              );

    //
    // Depricated, do not use in new code
    //
    wxMenuItem( wxMenu*         pParentMenu
               ,int             vId
               ,const wxString& rsText
               ,const wxString& rsHelp
               ,bool            bIsCheckable
               ,wxMenu*         pSubMenu = NULL
              );
    virtual ~wxMenuItem();

    //
    // Override base class virtuals
    //
    virtual void SetItemLabel(const wxString& rStrName);

    virtual void Enable(bool bDoEnable = true);
    virtual void Check(bool bDoCheck = true);
    virtual bool IsChecked(void) const;

    //
    // Unfortunately needed to resolve ambiguity between
    // wxMenuItemBase::IsCheckable() and wxOwnerDrawn::IsCheckable()
    //
    bool IsCheckable(void) const { return wxMenuItemBase::IsCheckable(); }

    //
    // The id for a popup menu is really its menu handle (as required by
    // ::AppendMenu() API), so this function will return either the id or the
    // menu handle depending on what we're
    //
    int GetRealId(void) const;

    //
    // Mark item as belonging to the given radio group
    //
    void SetAsRadioGroupStart(void);
    void SetRadioGroupStart(int nStart);
    void SetRadioGroupEnd(int nEnd);

    //
    // All OS/2PM Submenus and menus have one of these
    //
    MENUITEM                        m_vMenuData;

#if wxUSE_OWNER_DRAWN

    void SetBitmaps(const wxBitmap& bmpChecked,
                    const wxBitmap& bmpUnchecked = wxNullBitmap)
    {
        m_bmpChecked = bmpChecked;
        m_bmpUnchecked = bmpUnchecked;
        SetOwnerDrawn(true);
    }

    void SetBitmap(const wxBitmap& bmp, bool bChecked = true)
    {
        if ( bChecked )
            m_bmpChecked = bmp;
        else
            m_bmpUnchecked = bmp;
        SetOwnerDrawn(true);
    }

    void SetDisabledBitmap(const wxBitmap& bmpDisabled)
    {
        m_bmpDisabled = bmpDisabled;
        SetOwnerDrawn(true);
    }

    const wxBitmap& GetBitmap(bool bChecked = true) const
        { return (bChecked ? m_bmpChecked : m_bmpUnchecked); }

    const wxBitmap& GetDisabledBitmap() const
        { return m_bmpDisabled; }


    // override wxOwnerDrawn base class virtuals
    virtual wxString GetName() const;
    virtual bool OnMeasureItem(size_t *pwidth, size_t *pheight);
    virtual bool OnDrawItem(wxDC& dc, const wxRect& rc, wxODAction act, wxODStatus stat);

protected:
    virtual void GetFontToUse(wxFont& font) const;

#endif // wxUSE_OWNER_DRAWN

private:
    void Init();

    //
    // The positions of the first and last items of the radio group this item
    // belongs to or -1: start is the radio group start and is valid for all
    // but first radio group items (m_isRadioGroupStart == FALSE), end is valid
    // only for the first one
    //
    union
    {
        int m_nStart;
        int m_nEnd;
    } m_vRadioGroup;

    //
    // Does this item start a radio group?
    //
    bool                            m_bIsRadioGroupStart;

#if wxUSE_OWNER_DRAWN
    // item bitmaps
    wxBitmap m_bmpChecked,     // bitmap to put near the item
             m_bmpUnchecked,   // (checked is used also for 'uncheckable' items)
             m_bmpDisabled;
#endif // wxUSE_OWNER_DRAWN

    DECLARE_DYNAMIC_CLASS(wxMenuItem)
}; // end of CLASS wxMenuItem

#endif  //_MENUITEM_H
