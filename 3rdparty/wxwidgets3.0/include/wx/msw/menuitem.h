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

#if wxUSE_OWNER_DRAWN
    #include "wx/ownerdrw.h"
    #include "wx/bitmap.h"

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

#else // !wxUSE_OWNER_DRAWN
    // Provide stubs for the public functions above to ensure that the code
    // still compiles without wxUSE_OWNER_DRAWN -- it makes sense to just drop
    // the bitmaps then instead of failing compilation.
    void SetBitmaps(const wxBitmap& WXUNUSED(bmpChecked),
                    const wxBitmap& WXUNUSED(bmpUnchecked) = wxNullBitmap) { }
    void SetBitmap(const wxBitmap& WXUNUSED(bmp),
                   bool WXUNUSED(bChecked) = true) { }
    const wxBitmap& GetBitmap() const { return wxNullBitmap; }
#endif // wxUSE_OWNER_DRAWN/!wxUSE_OWNER_DRAWN

private:
    // common part of all ctors
    void Init();

    // Return the item position in the menu containing it.
    //
    // Returns -1 if the item is not attached to a menu or if we can't find its
    // position (which is not really supposed to ever happen).
    int MSGetMenuItemPos() const;

#if wxUSE_OWNER_DRAWN
    // item bitmaps
    wxBitmap m_bmpChecked,     // bitmap to put near the item
             m_bmpUnchecked,   // (checked is used also for 'uncheckable' items)
             m_bmpDisabled;
#endif // wxUSE_OWNER_DRAWN

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMenuItem)
};

#endif  //_MENUITEM_H
