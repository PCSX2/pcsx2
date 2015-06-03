///////////////////////////////////////////////////////////////////////////////
// Name:        wx/selstore.h
// Purpose:     wxSelectionStore stores selected items in a control
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.06.03 (extracted from src/generic/listctrl.cpp)
// Copyright:   (c) 2000-2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SELSTORE_H_
#define _WX_SELSTORE_H_

#include "wx/dynarray.h"

// ----------------------------------------------------------------------------
// wxSelectedIndices is just a sorted array of indices
// ----------------------------------------------------------------------------

inline int CMPFUNC_CONV wxUIntCmp(unsigned n1, unsigned n2)
{
    return (int)(n1 - n2);
}

WX_DEFINE_SORTED_EXPORTED_ARRAY_CMP_INT(unsigned, wxUIntCmp, wxSelectedIndices);

// ----------------------------------------------------------------------------
// wxSelectionStore is used to store the selected items in the virtual
// controls, i.e. it is well suited for storing even when the control contains
// a huge (practically infinite) number of items.
//
// Of course, internally it still has to store the selected items somehow (as
// an array currently) but the advantage is that it can handle the selection
// of all items (common operation) efficiently and that it could be made even
// smarter in the future (e.g. store the selections as an array of ranges +
// individual items) without changing its API.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSelectionStore
{
public:
    wxSelectionStore() : m_itemsSel(wxUIntCmp) { Init(); }

    // set the total number of items we handle
    void SetItemCount(unsigned count);

    // special case of SetItemCount(0)
    void Clear() { m_itemsSel.Clear(); m_count = 0; m_defaultState = false; }

    // must be called when new items are inserted/added
    void OnItemsInserted(unsigned item, unsigned numItems);

    // must be called when an items is deleted
    void OnItemDelete(unsigned item);

    // more efficient version for notifying the selection about deleting
    // several items at once, return true if any of them were selected
    bool OnItemsDeleted(unsigned item, unsigned numItems);

    // select one item, use SelectRange() insted if possible!
    //
    // returns true if the items selection really changed
    bool SelectItem(unsigned item, bool select = true);

    // select the range of items (inclusive)
    //
    // return true and fill the itemsChanged array with the indices of items
    // which have changed state if "few" of them did, otherwise return false
    // (meaning that too many items changed state to bother counting them
    // individually)
    bool SelectRange(unsigned itemFrom, unsigned itemTo,
                     bool select = true,
                     wxArrayInt *itemsChanged = NULL);

    // return true if the given item is selected
    bool IsSelected(unsigned item) const;

    // return true if no items are currently selected
    bool IsEmpty() const
    {
        return m_defaultState ? m_itemsSel.size() == m_count
                              : m_itemsSel.empty();
    }

    // return the total number of selected items
    unsigned GetSelectedCount() const
    {
        return m_defaultState ? m_count - m_itemsSel.GetCount()
                              : m_itemsSel.GetCount();
    }

    // type of a "cookie" used to preserve the iteration state, this is an
    // opaque type, don't rely on its current representation
    typedef size_t IterationState;

    // constant representing absence of selection and hence end of iteration
    static const unsigned NO_SELECTION;

    // get the first selected item in index order, return NO_SELECTION if none
    unsigned GetFirstSelectedItem(IterationState& cookie) const;

    // get the next selected item, return NO_SELECTION if no more
    unsigned GetNextSelectedItem(IterationState& cookie) const;

private:
    // (re)init
    void Init() { m_count = 0; m_defaultState = false; }

    // the total number of items we handle
    unsigned m_count;

    // the default state: normally, false (i.e. off) but maybe set to true if
    // there are more selected items than non selected ones - this allows to
    // handle selection of all items efficiently
    bool m_defaultState;

    // the array of items whose selection state is different from default
    wxSelectedIndices m_itemsSel;

    wxDECLARE_NO_COPY_CLASS(wxSelectionStore);
};

#endif // _WX_SELSTORE_H_

