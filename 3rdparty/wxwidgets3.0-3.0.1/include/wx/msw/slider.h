/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/slider.h
// Purpose:     wxSlider class implementation using trackbar control
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SLIDER_H_
#define _WX_SLIDER_H_

class WXDLLIMPEXP_FWD_CORE wxSubwindows;

// Slider
class WXDLLIMPEXP_CORE wxSlider : public wxSliderBase
{
public:
    wxSlider() { Init(); }

    wxSlider(wxWindow *parent,
             wxWindowID id,
             int value,
             int minValue,
             int maxValue,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxSL_HORIZONTAL,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxSliderNameStr)
    {
        Init();

        (void)Create(parent, id, value, minValue, maxValue,
                     pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                int value,
                int minValue, int maxValue,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSL_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSliderNameStr);

    virtual ~wxSlider();

    // slider methods
    virtual int GetValue() const;
    virtual void SetValue(int);

    void SetRange(int minValue, int maxValue);

    int GetMin() const { return m_rangeMin; }
    int GetMax() const { return m_rangeMax; }

    // Win32-specific slider methods
    int GetTickFreq() const { return m_tickFreq; }
    void SetPageSize(int pageSize);
    int GetPageSize() const;
    void ClearSel();
    void ClearTicks();
    void SetLineSize(int lineSize);
    int GetLineSize() const;
    int GetSelEnd() const;
    int GetSelStart() const;
    void SetSelection(int minPos, int maxPos);
    void SetThumbLength(int len);
    int GetThumbLength() const;
    void SetTick(int tickPos);

    // implementation only from now on
    WXHWND GetStaticMin() const;
    WXHWND GetStaticMax() const;
    WXHWND GetEditValue() const;
    virtual bool ContainsHWND(WXHWND hWnd) const;

    // we should let background show through the slider (and its labels)
    virtual bool HasTransparentBackground() { return true; }

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

    void Command(wxCommandEvent& event);
    virtual bool MSWOnScroll(int orientation, WXWORD wParam,
                             WXWORD pos, WXHWND control);

    virtual bool Show(bool show = true);
    virtual bool Enable(bool show = true);
    virtual bool SetFont(const wxFont& font);

    virtual WXDWORD MSWGetStyle(long flags, WXDWORD *exstyle = NULL) const;

protected:
    // common part of all ctors
    void Init();

    // format an integer value as string
    static wxString Format(int n) { return wxString::Format(wxT("%d"), n); }

    // get the boundig box for the slider and possible labels
    wxRect GetBoundingBox() const;

    // Get the height and, if the pointers are non NULL, widths of both labels.
    //
    // Notice that the return value will be 0 if we don't have wxSL_LABELS
    // style but we do fill widthMin and widthMax even if we don't have
    // wxSL_MIN_MAX_LABELS style set so the caller should account for it.
    int GetLabelsSize(int *widthMin = NULL, int *widthMax = NULL) const;


    // overridden base class virtuals
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual wxSize DoGetBestSize() const;

    // the labels windows, if any
    wxSubwindows  *m_labels;

    int           m_rangeMin;
    int           m_rangeMax;
    int           m_pageSize;
    int           m_lineSize;
    int           m_tickFreq;

    // flag needed to detect whether we're getting THUMBRELEASE event because
    // of dragging the thumb or scrolling the mouse wheel
    bool m_isDragging;

    // Platform-specific implementation of SetTickFreq
    virtual void DoSetTickFreq(int freq);

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxSlider)
};

#endif // _WX_SLIDER_H_

