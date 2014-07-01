///////////////////////////////////////////////////////////////////////////////
// Name:        wx/ribbon/gallery.h
// Purpose:     Ribbon control which displays a gallery of items to choose from
// Author:      Peter Cawley
// Modified by:
// Created:     2009-07-22
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////
#ifndef _WX_RIBBON_GALLERY_H_
#define _WX_RIBBON_GALLERY_H_

#include "wx/defs.h"

#if wxUSE_RIBBON

#include "wx/ribbon/art.h"
#include "wx/ribbon/control.h"

class wxRibbonGalleryItem;

WX_DEFINE_USER_EXPORTED_ARRAY_PTR(wxRibbonGalleryItem*, wxArrayRibbonGalleryItem, class WXDLLIMPEXP_RIBBON);

class WXDLLIMPEXP_RIBBON wxRibbonGallery : public wxRibbonControl
{
public:
    wxRibbonGallery();

    wxRibbonGallery(wxWindow* parent,
                  wxWindowID id = wxID_ANY,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = 0);

    virtual ~wxRibbonGallery();

    bool Create(wxWindow* parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0);

    void Clear();

    bool IsEmpty() const;
    unsigned int GetCount() const;
    wxRibbonGalleryItem* GetItem(unsigned int n);
    wxRibbonGalleryItem* Append(const wxBitmap& bitmap, int id);
    wxRibbonGalleryItem* Append(const wxBitmap& bitmap, int id, void* clientData);
    wxRibbonGalleryItem* Append(const wxBitmap& bitmap, int id, wxClientData* clientData);

    void SetItemClientObject(wxRibbonGalleryItem* item, wxClientData* data);
    wxClientData* GetItemClientObject(const wxRibbonGalleryItem* item) const;
    void SetItemClientData(wxRibbonGalleryItem* item, void* data);
    void* GetItemClientData(const wxRibbonGalleryItem* item) const;

    void SetSelection(wxRibbonGalleryItem* item);
    wxRibbonGalleryItem* GetSelection() const;
    wxRibbonGalleryItem* GetHoveredItem() const;
    wxRibbonGalleryItem* GetActiveItem() const;
    wxRibbonGalleryButtonState GetUpButtonState() const;
    wxRibbonGalleryButtonState GetDownButtonState() const;
    wxRibbonGalleryButtonState GetExtensionButtonState() const;

    bool IsHovered() const;
    virtual bool IsSizingContinuous() const;
    virtual bool Realize();
    virtual bool Layout();

    virtual bool ScrollLines(int lines);
    bool ScrollPixels(int pixels);
    void EnsureVisible(const wxRibbonGalleryItem* item);

protected:
    wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }
    void CommonInit(long style);
    void CalculateMinSize();
    bool TestButtonHover(const wxRect& rect, wxPoint pos,
        wxRibbonGalleryButtonState* state);

    void OnEraseBackground(wxEraseEvent& evt);
    void OnMouseEnter(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseLeave(wxMouseEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);
    void OnMouseDClick(wxMouseEvent& evt);
    void OnPaint(wxPaintEvent& evt);
    void OnSize(wxSizeEvent& evt);
    int GetScrollLineSize() const;

    virtual wxSize DoGetBestSize() const;
    virtual wxSize DoGetNextSmallerSize(wxOrientation direction,
                                        wxSize relative_to) const;
    virtual wxSize DoGetNextLargerSize(wxOrientation direction,
                                       wxSize relative_to) const;

    wxArrayRibbonGalleryItem m_items;
    wxRibbonGalleryItem* m_selected_item;
    wxRibbonGalleryItem* m_hovered_item;
    wxRibbonGalleryItem* m_active_item;
    wxSize m_bitmap_size;
    wxSize m_bitmap_padded_size;
    wxSize m_best_size;
    wxRect m_client_rect;
    wxRect m_scroll_up_button_rect;
    wxRect m_scroll_down_button_rect;
    wxRect m_extension_button_rect;
    const wxRect* m_mouse_active_rect;
    int m_item_separation_x;
    int m_item_separation_y;
    int m_scroll_amount;
    int m_scroll_limit;
    wxRibbonGalleryButtonState m_up_button_state;
    wxRibbonGalleryButtonState m_down_button_state;
    wxRibbonGalleryButtonState m_extension_button_state;
    bool m_hovered;

#ifndef SWIG
    DECLARE_CLASS(wxRibbonGallery)
    DECLARE_EVENT_TABLE()
#endif
};

class WXDLLIMPEXP_RIBBON wxRibbonGalleryEvent : public wxCommandEvent
{
public:
    wxRibbonGalleryEvent(wxEventType command_type = wxEVT_NULL,
                       int win_id = 0,
                       wxRibbonGallery* gallery = NULL,
                       wxRibbonGalleryItem* item = NULL)
        : wxCommandEvent(command_type, win_id)
        , m_gallery(gallery), m_item(item)
    {
    }
#ifndef SWIG
    wxRibbonGalleryEvent(const wxRibbonGalleryEvent& e) : wxCommandEvent(e)
    {
        m_gallery = e.m_gallery;
        m_item = e.m_item;
    }
#endif
    wxEvent *Clone() const { return new wxRibbonGalleryEvent(*this); }

    wxRibbonGallery* GetGallery() {return m_gallery;}
    wxRibbonGalleryItem* GetGalleryItem() {return m_item;}
    void SetGallery(wxRibbonGallery* gallery) {m_gallery = gallery;}
    void SetGalleryItem(wxRibbonGalleryItem* item) {m_item = item;}

protected:
    wxRibbonGallery* m_gallery;
    wxRibbonGalleryItem* m_item;

#ifndef SWIG
private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxRibbonGalleryEvent)
#endif
};

#ifndef SWIG

wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONGALLERY_HOVER_CHANGED, wxRibbonGalleryEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONGALLERY_SELECTED, wxRibbonGalleryEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_RIBBON, wxEVT_RIBBONGALLERY_CLICKED, wxRibbonGalleryEvent);

typedef void (wxEvtHandler::*wxRibbonGalleryEventFunction)(wxRibbonGalleryEvent&);

#define wxRibbonGalleryEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxRibbonGalleryEventFunction, func)

#define EVT_RIBBONGALLERY_HOVER_CHANGED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONGALLERY_HOVER_CHANGED, winid, wxRibbonGalleryEventHandler(fn))
#define EVT_RIBBONGALLERY_SELECTED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONGALLERY_SELECTED, winid, wxRibbonGalleryEventHandler(fn))
#define EVT_RIBBONGALLERY_CLICKED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_RIBBONGALLERY_CLICKED, winid, wxRibbonGalleryEventHandler(fn))
#else

// wxpython/swig event work
%constant wxEventType wxEVT_RIBBONGALLERY_HOVER_CHANGED;
%constant wxEventType wxEVT_RIBBONGALLERY_SELECTED;

%pythoncode {
    EVT_RIBBONGALLERY_HOVER_CHANGED = wx.PyEventBinder( wxEVT_RIBBONGALLERY_HOVER_CHANGED, 1 )
    EVT_RIBBONGALLERY_SELECTED = wx.PyEventBinder( wxEVT_RIBBONGALLERY_SELECTED, 1 )
}
#endif // SWIG

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_RIBBONGALLERY_HOVER_CHANGED   wxEVT_RIBBONGALLERY_HOVER_CHANGED
#define wxEVT_COMMAND_RIBBONGALLERY_SELECTED        wxEVT_RIBBONGALLERY_SELECTED
#define wxEVT_COMMAND_RIBBONGALLERY_CLICKED         wxEVT_RIBBONGALLERY_CLICKED

#endif // wxUSE_RIBBON

#endif // _WX_RIBBON_GALLERY_H_
