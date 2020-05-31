///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/headerctrl.h
// Purpose:     wxMSW native wxHeaderCtrl
// Author:      Vadim Zeitlin
// Created:     2008-12-01
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_HEADERCTRL_H_
#define _WX_MSW_HEADERCTRL_H_

class WXDLLIMPEXP_FWD_CORE wxImageList;

// ----------------------------------------------------------------------------
// wxHeaderCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxHeaderCtrl : public wxHeaderCtrlBase
{
public:
    wxHeaderCtrl()
    {
        Init();
    }

    wxHeaderCtrl(wxWindow *parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxHD_DEFAULT_STYLE,
                 const wxString& name = wxHeaderCtrlNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxHD_DEFAULT_STYLE,
                const wxString& name = wxHeaderCtrlNameStr);

    virtual ~wxHeaderCtrl();

    
protected:
    // override wxWindow methods which must be implemented by a new control
    virtual wxSize DoGetBestSize() const;
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
    
private:
    // implement base class pure virtuals
    virtual void DoSetCount(unsigned int count);
    virtual unsigned int DoGetCount() const;
    virtual void DoUpdate(unsigned int idx);

    virtual void DoScrollHorz(int dx);

    virtual void DoSetColumnsOrder(const wxArrayInt& order);
    virtual wxArrayInt DoGetColumnsOrder() const;

    // override MSW-specific methods needed for new control
    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

    // common part of all ctors
    void Init();

    // wrapper around Header_InsertItem(): insert the item using information
    // from the given column at the given index
    void DoInsertItem(const wxHeaderColumn& col, unsigned int idx);

    // get the number of currently visible items: this is also the total number
    // of items contained in the native control
    int GetShownColumnsCount() const;

    // due to the discrepancy for the hidden columns which we know about but
    // the native control does not, there can be a difference between the
    // column indices we use and the ones used by the native control; these
    // functions translate between them
    //
    // notice that MSWToNativeIdx() shouldn't be called for hidden columns and
    // MSWFromNativeIdx() always returns an index of a visible column
    int MSWToNativeIdx(int idx);
    int MSWFromNativeIdx(int item);

    // this is the same as above but for order, not index
    int MSWToNativeOrder(int order);
    int MSWFromNativeOrder(int order);

    // get the event type corresponding to a click or double click event
    // (depending on dblclk value) with the specified (using MSW convention)
    // mouse button
    wxEventType GetClickEventType(bool dblclk, int button);


    // the number of columns in the control, including the hidden ones (not
    // taken into account by the native control, see comment in DoGetCount())
    unsigned int m_numColumns;

    // this is a lookup table allowing us to check whether the column with the
    // given index is currently shown in the native control, in which case the
    // value of this array element with this index is 0, or hidden
    //
    // notice that this may be different from GetColumn(idx).IsHidden() and in
    // fact we need this array precisely because it will be different from it
    // in DoUpdate() when the column hidden flag gets toggled and we need it to
    // handle this transition correctly
    wxArrayInt m_isHidden;

    // the order of our columns: this array contains the index of the column
    // shown at the position n as the n-th element
    //
    // this is necessary only to handle the hidden columns: the native control
    // doesn't know about them and so we can't use Header_GetOrderArray()
    wxArrayInt m_colIndices;

    // the image list: initially NULL, created on demand
    wxImageList *m_imageList;

    // the offset of the window used to emulate scrolling it
    int m_scrollOffset;

    // actual column we are dragging or -1 if not dragging anything
    int m_colBeingDragged;

    wxDECLARE_NO_COPY_CLASS(wxHeaderCtrl);
};

#endif // _WX_MSW_HEADERCTRL_H_

