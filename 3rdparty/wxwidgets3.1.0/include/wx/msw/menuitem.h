///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/menuitem.h
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

#include "wx/bitmap.h"

#if wxUSE_OWNER_DRAWN
    #include "wx/ownerdrw.h"

    struct tagRECT;
#endif

// ----------------------------------------------------------------------------
// wxMenuItem: an item in the menu, optionally implements owner-drawn behaviour
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMenuItem : public wxMenuItemBase
#if wxUSE_OWNER_DRAWN
                             , public wxOwnerDrawn
#endif
{
public:
    // ctor & dtor
    wxMenuItem(wxMenu *parentMenu = NULL,
               int id = wxID_SEPARATOR,
               const wxString& name = wxEmptyString,
               const wxString& help = wxEmptyString,
               wxItemKind kind = wxITEM_NORMAL,
               wxMenu *subMenu = NULL);
    virtual ~wxMenuItem();

    // override base class virtuals
    virtual void SetItemLabel(const wxString& strName);

    virtual void Enable(bool bDoEnable = true);
    virtual void Check(bool bDoCheck = true);
    virtual bool IsChecked() const;

    // unfortunately needed to resolve ambiguity between
    // wxMenuItemBase::IsCheckable() and wxOwnerDrawn::IsCheckable()
    bool IsCheckable() const { return wxMenuItemBase::IsCheckable(); }

    // the id for a popup menu is really its menu handle (as required by
    // ::AppendMenu() API), so this function will return either the id or the
    // menu handle depending on what we are
    //
    // notice that it also returns the id as an unsigned int, as required by
    // Win32 API
    WXWPARAM GetMSWId() const;

#if WXWIN_COMPATIBILITY_2_8
    // compatibility only, don't use in new code
    wxDEPRECATED(
    wxMenuItem(wxMenu *parentMenu,
               int id,
               const wxString& text,
               const wxString& help,
               bool isCheckable,
               wxMenu *subMenu = NULL)
    );
#endif

    void SetBitmaps(const wxBitmap& bmpChecked,
                    const wxBitmap& bmpUnchecked = wxNullBitmap)
    {
        DoSetBitmap(bmpChecked, true);
        DoSetBitmap(bmpUnchecked, false);
    }

    void SetBitmap(const wxBitmap& bmp, bool bChecked = true)
    {
        DoSetBitmap(bmp, bChecked);
    }

    const wxBitmap& GetBitmap(bool bChecked = true) const
        { return (bChecked ? m_bmpChecked : m_bmpUnchecked); }

#if wxUSE_OWNER_DRAWN
    void SetDisabledBitmap(const wxBitmap& bmpDisabled)
    {
        m_bmpDisabled = bmpDisabled;
        SetOwnerDrawn(true);
    }

    const wxBitmap& GetDisabledBitmap() const
        { return m_bmpDisabled; }

    int MeasureAccelWidth() const;

    // override wxOwnerDrawn base class virtuals
    virtual wxString GetName() const;
    virtual bool OnMeasureItem(size_t *pwidth, size_t *pheight);
    virtual bool OnDrawItem(wxDC& dc, const wxRect& rc, wxODAction act, wxODStatus stat);

protected:
    virtual void GetFontToUse(wxFont& font) const;
    virtual void GetColourToUse(wxODStatus stat, wxColour& colText, wxColour& colBack) const;

private:
    // helper function for draw std menu check mark
    void DrawStdCheckMark(WXHDC hdc, const tagRECT* rc, wxODStatus stat);

    // helper function to determine if the item must be owner-drawn
    bool MSWMustUseOwnerDrawn();
#endif // wxUSE_OWNER_DRAWN

    enum BitmapKind
    {
        Normal,
        Checked,
        Unchecked
    };

    // helper function to get a handle for bitmap associated with item
    WXHBITMAP GetHBitmapForMenu(BitmapKind kind) const;

    // helper function to set/change the bitmap
    void DoSetBitmap(const wxBitmap& bmp, bool bChecked);

private:
    // common part of all ctors
    void Init();

    // Return the item position in the menu containing it.
    //
    // Returns -1 if the item is not attached to a menu or if we can't find its
    // position (which is not really supposed to ever happen).
    int MSGetMenuItemPos() const;

    // item bitmaps
    wxBitmap m_bmpChecked,     // bitmap to put near the item
             m_bmpUnchecked;   // (checked is used also for 'uncheckable' items)
#if wxUSE_OWNER_DRAWN
    wxBitmap m_bmpDisabled;
#endif // wxUSE_OWNER_DRAWN

    // Give wxMenu access to our MSWMustUseOwnerDrawn() and GetHBitmapForMenu().
    friend class wxMenu;

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxMenuItem);
};

#endif  //_MENUITEM_H
