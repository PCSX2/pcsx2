///////////////////////////////////////////////////////////////////////////////
// Name:        wx/itemid.h
// Purpose:     wxItemId class declaration.
// Author:      Vadim Zeitlin
// Created:     2011-08-17
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ITEMID_H_
#define _WX_ITEMID_H_

// ----------------------------------------------------------------------------
// wxItemId: an opaque item identifier used with wx{Tree,TreeList,DataView}Ctrl.
// ----------------------------------------------------------------------------

// The template argument T is typically a pointer to some opaque type. While
// wxTreeItemId and wxDataViewItem use a pointer to void, this is dangerous and
// not recommended for the new item id classes.
template <typename T>
class wxItemId
{
public:
    typedef T Type;

    // This ctor is implicit which is fine for non-void* types, but if you use
    // this class with void* you're strongly advised to make the derived class
    // ctor explicit as implicitly converting from any pointer is simply too
    // dangerous.
    wxItemId(Type item = NULL) : m_pItem(item) { }

    // Default copy ctor, assignment operator and dtor are ok.

    bool IsOk() const { return m_pItem != NULL; }
    Type GetID() const { return m_pItem; }
    operator const Type() const { return m_pItem; }

    // This is used for implementation purposes only.
    Type operator->() const { return m_pItem; }

    void Unset() { m_pItem = NULL; }

    // This field is public *only* for compatibility with the old wxTreeItemId
    // implementation and must not be used in any new code.
//private:
    Type m_pItem;
};

template <typename T>
bool operator==(const wxItemId<T>& left, const wxItemId<T>& right)
{
    return left.GetID() == right.GetID();
}

template <typename T>
bool operator!=(const wxItemId<T>& left, const wxItemId<T>& right)
{
    return !(left == right);
}

#endif // _WX_ITEMID_H_
