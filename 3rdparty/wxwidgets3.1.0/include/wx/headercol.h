///////////////////////////////////////////////////////////////////////////////
// Name:        wx/headercol.h
// Purpose:     declaration of wxHeaderColumn class
// Author:      Vadim Zeitlin
// Created:     2008-12-02
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HEADERCOL_H_
#define _WX_HEADERCOL_H_

#include "wx/bitmap.h"

#if wxUSE_HEADERCTRL

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

enum
{
    // special value for column width meaning unspecified/default
    wxCOL_WIDTH_DEFAULT = -1,

    // size the column automatically to fit all values
    wxCOL_WIDTH_AUTOSIZE = -2
};

// bit masks for the various column attributes
enum
{
    // column can be resized (included in default flags)
    wxCOL_RESIZABLE   = 1,

    // column can be clicked to toggle the sort order by its contents
    wxCOL_SORTABLE    = 2,

    // column can be dragged to change its order (included in default)
    wxCOL_REORDERABLE = 4,

    // column is not shown at all
    wxCOL_HIDDEN      = 8,

    // default flags for wxHeaderColumn ctor
    wxCOL_DEFAULT_FLAGS = wxCOL_RESIZABLE | wxCOL_REORDERABLE
};

// ----------------------------------------------------------------------------
// wxHeaderColumn: interface for a column in a header of controls such as
//                 wxListCtrl, wxDataViewCtrl or wxGrid
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxHeaderColumn
{
public:
    // ctors and dtor
    // --------------

    /*
       Derived classes must provide ctors with the following signatures
       (notice that they shouldn't be explicit to allow passing strings/bitmaps
       directly to methods such wxHeaderCtrl::AppendColumn()):
    wxHeaderColumn(const wxString& title,
                   int width = wxCOL_WIDTH_DEFAULT,
                   wxAlignment align = wxALIGN_NOT,
                   int flags = wxCOL_DEFAULT_FLAGS);
    wxHeaderColumn(const wxBitmap &bitmap,
                   int width = wxDVC_DEFAULT_WIDTH,
                   wxAlignment align = wxALIGN_CENTER,
                   int flags = wxCOL_DEFAULT_FLAGS);
    */

    // virtual dtor for the base class to avoid gcc warnings even though we
    // don't normally delete the objects of this class via a pointer to
    // wxHeaderColumn so it's not necessary, strictly speaking
    virtual ~wxHeaderColumn() { }

    // getters for various attributes
    // ------------------------------

    // notice that wxHeaderColumn only provides getters as this is all the
    // wxHeaderCtrl needs, various derived class must also provide some way to
    // change these attributes but this can be done either at the column level
    // (in which case they should inherit from wxSettableHeaderColumn) or via
    // the methods of the main control in which case you don't need setters in
    // the column class at all

    // title is the string shown for this column
    virtual wxString GetTitle() const = 0;

    // bitmap shown (instead of text) in the column header
    virtual wxBitmap GetBitmap() const = 0;                                   \

    // width of the column in pixels, can be set to wxCOL_WIDTH_DEFAULT meaning
    // unspecified/default
    virtual int GetWidth() const = 0;

    // minimal width can be set for resizable columns to forbid resizing them
    // below the specified size (set to 0 to remove)
    virtual int GetMinWidth() const = 0;

    // alignment of the text: wxALIGN_CENTRE, wxALIGN_LEFT or wxALIGN_RIGHT
    virtual wxAlignment GetAlignment() const = 0;


    // flags manipulations:
    // --------------------

    // notice that while we make GetFlags() pure virtual here and implement the
    // individual flags access in terms of it, for some derived classes it is
    // more natural to implement access to each flag individually, in which
    // case they can use our GetFromIndividualFlags() helper below to implement
    // GetFlags()

    // retrieve all column flags at once: combination of wxCOL_XXX values above
    virtual int GetFlags() const = 0;

    bool HasFlag(int flag) const { return (GetFlags() & flag) != 0; }


    // wxCOL_RESIZABLE
    virtual bool IsResizeable() const
        { return HasFlag(wxCOL_RESIZABLE); }

    // wxCOL_SORTABLE
    virtual bool IsSortable() const
        { return HasFlag(wxCOL_SORTABLE); }

    // wxCOL_REORDERABLE
    virtual bool IsReorderable() const
        { return HasFlag(wxCOL_REORDERABLE); }

    // wxCOL_HIDDEN
    virtual bool IsHidden() const
        { return HasFlag(wxCOL_HIDDEN); }
    bool IsShown() const
        { return !IsHidden(); }


    // sorting
    // -------

    // return true if the column is the one currently used for sorting
    virtual bool IsSortKey() const = 0;

    // for sortable columns indicate whether we should sort in ascending or
    // descending order (this should only be taken into account if IsSortKey())
    virtual bool IsSortOrderAscending() const = 0;

protected:
    // helper for the class overriding IsXXX()
    int GetFromIndividualFlags() const;
};

// ----------------------------------------------------------------------------
// wxSettableHeaderColumn: column which allows to change its fields too
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSettableHeaderColumn : public wxHeaderColumn
{
public:
    virtual void SetTitle(const wxString& title) = 0;
    virtual void SetBitmap(const wxBitmap& bitmap) = 0;
    virtual void SetWidth(int width) = 0;
    virtual void SetMinWidth(int minWidth) = 0;
    virtual void SetAlignment(wxAlignment align) = 0;

    // see comment for wxHeaderColumn::GetFlags() about the relationship
    // between SetFlags() and Set{Sortable,Reorderable,...}

    // change, set, clear, toggle or test for any individual flag
    virtual void SetFlags(int flags) = 0;
    void ChangeFlag(int flag, bool set);
    void SetFlag(int flag);
    void ClearFlag(int flag);
    void ToggleFlag(int flag);

    virtual void SetResizeable(bool resizable)
        { ChangeFlag(wxCOL_RESIZABLE, resizable); }
    virtual void SetSortable(bool sortable)
        { ChangeFlag(wxCOL_SORTABLE, sortable); }
    virtual void SetReorderable(bool reorderable)
        { ChangeFlag(wxCOL_REORDERABLE, reorderable); }
    virtual void SetHidden(bool hidden)
        { ChangeFlag(wxCOL_HIDDEN, hidden); }

    // This function can be called to indicate that this column is not used for
    // sorting any more. Under some platforms it's not necessary to do anything
    // in this case as just setting another column as a sort key takes care of
    // everything but under MSW we currently need to call this explicitly to
    // reset the sort indicator displayed on the column.
    virtual void UnsetAsSortKey() { }

    virtual void SetSortOrder(bool ascending) = 0;
    void ToggleSortOrder() { SetSortOrder(!IsSortOrderAscending()); }

protected:
    // helper for the class overriding individual SetXXX() methods instead of
    // overriding SetFlags()
    void SetIndividualFlags(int flags);
};

// ----------------------------------------------------------------------------
// wxHeaderColumnSimple: trivial generic implementation of wxHeaderColumn
// ----------------------------------------------------------------------------

class wxHeaderColumnSimple : public wxSettableHeaderColumn
{
public:
    // ctors and dtor
    wxHeaderColumnSimple(const wxString& title,
                         int width = wxCOL_WIDTH_DEFAULT,
                         wxAlignment align = wxALIGN_NOT,
                         int flags = wxCOL_DEFAULT_FLAGS)
        : m_title(title),
          m_width(width),
          m_align(align),
          m_flags(flags)
    {
        Init();
    }

    wxHeaderColumnSimple(const wxBitmap& bitmap,
                         int width = wxCOL_WIDTH_DEFAULT,
                         wxAlignment align = wxALIGN_CENTER,
                         int flags = wxCOL_DEFAULT_FLAGS)
        : m_bitmap(bitmap),
          m_width(width),
          m_align(align),
          m_flags(flags)
    {
        Init();
    }

    // implement base class pure virtuals
    virtual void SetTitle(const wxString& title) wxOVERRIDE { m_title = title; }
    virtual wxString GetTitle() const wxOVERRIDE { return m_title; }

    virtual void SetBitmap(const wxBitmap& bitmap) wxOVERRIDE { m_bitmap = bitmap; }
    wxBitmap GetBitmap() const wxOVERRIDE { return m_bitmap; }

    virtual void SetWidth(int width) wxOVERRIDE { m_width = width; }
    virtual int GetWidth() const wxOVERRIDE { return m_width; }

    virtual void SetMinWidth(int minWidth) wxOVERRIDE { m_minWidth = minWidth; }
    virtual int GetMinWidth() const wxOVERRIDE { return m_minWidth; }

    virtual void SetAlignment(wxAlignment align) wxOVERRIDE { m_align = align; }
    virtual wxAlignment GetAlignment() const wxOVERRIDE { return m_align; }

    virtual void SetFlags(int flags) wxOVERRIDE { m_flags = flags; }
    virtual int GetFlags() const wxOVERRIDE { return m_flags; }

    virtual bool IsSortKey() const wxOVERRIDE { return m_sort; }
    virtual void UnsetAsSortKey() wxOVERRIDE { m_sort = false; }

    virtual void SetSortOrder(bool ascending) wxOVERRIDE
    {
        m_sort = true;
        m_sortAscending = ascending;
    }

    virtual bool IsSortOrderAscending() const wxOVERRIDE { return m_sortAscending; }

private:
    // common part of all ctors
    void Init()
    {
        m_minWidth = 0;
        m_sort = false;
        m_sortAscending = true;
    }

    wxString m_title;
    wxBitmap m_bitmap;
    int m_width,
        m_minWidth;
    wxAlignment m_align;
    int m_flags;
    bool m_sort,
         m_sortAscending;
};

#endif // wxUSE_HEADERCTRL

#endif // _WX_HEADERCOL_H_

