/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/private/scrollview.h
// Purpose:     wxWindowCocoaScrollView
// Author:      David Elliott
// Modified by:
// Created:     2008/02/14
// Copyright:   (c) 2003- David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_SCROLLVIEW_H__
#define _WX_COCOA_SCROLLVIEW_H__

@class NSScroller;

// ========================================================================
// wxWindowCocoaScrollView
// ========================================================================
class wxWindowCocoaScrollView: protected wxCocoaNSView
{
    wxDECLARE_NO_COPY_CLASS(wxWindowCocoaScrollView);
public:
    wxWindowCocoaScrollView(wxWindow *owner);
    virtual ~wxWindowCocoaScrollView();
    inline WX_NSScrollView GetNSScrollView() { return m_cocoaNSScrollView; }
    void ClientSizeToSize(int &width, int &height);
    void DoGetClientSize(int *x, int *y) const;
    void Encapsulate();
    void Unencapsulate();

    // wxWindow calls this to do the work.  Note that we don't have the refresh parameter
    // because wxWindow handles that itself.
    void SetScrollbar(int orientation, int position, int thumbSize, int range);
    int GetScrollPos(wxOrientation orient);
    void SetScrollPos(wxOrientation orient, int position);
    int GetScrollRange(wxOrientation orient);
    int GetScrollThumb(wxOrientation orient);
    void ScrollWindow(int dx, int dy, const wxRect*);
    void UpdateSizes();

    void _wx_doScroller(NSScroller *sender);

protected:
    wxWindowCocoa *m_owner;
    WX_NSScrollView m_cocoaNSScrollView;
    virtual void Cocoa_FrameChanged(void);
    virtual void Cocoa_synthesizeMouseMoved(void) {}
    /*!
        Flag as to whether we're scrolling for a native view or a custom
        wxWindow.  This controls the scrolling behaviour.  When providing
        scrolling for a native view we don't catch scroller action messages
        and thus don't send scroll events and we don't actually scroll the
        window when the application calls ScrollWindow.

        When providing scrolling for a custom wxWindow, we make the NSScroller
        send their action messages to us which we in turn package as wx window
        scrolling events.  At this point, the window will not physically be
        scrolled.  The application will most likely handle the event by calling
        ScrollWindow which will do the real scrolling.  On the other hand,
        the application may instead not call ScrollWindow until some threshold
        is reached.  This causes the window to only scroll in steps which is
        what, for instance, wxScrolledWindow does.
     */
    bool m_isNativeView;
    /*!
        The range as the application code wishes to see it.  That is, the
        range from the last SetScrollbar call for the appropriate dimension.
        The horizontal dimension is the first [0] element and the vertical
        dimension the second [1] element.

        In wxMSW, a SCROLLINFO with nMin=0 and nMax=range-1 is used which
        gives exactly range possible positions so long as nPage (which is
        the thumb size) is less than or equal to 1.
     */
    int m_scrollRange[2];
    /*!
        The thumb size is intended to reflect the size of the visible portion
        of the scrolled document.  As the document size increases, the thumb
        visible thumb size decreases.  As document size decreases, the visible
        thumb size increases.  However, the thumb size on wx is defined in
        terms of scroll units (which are effectively defined by the scroll
        range) and so increasing the number of scroll units to reflect increased
        document size will have the effect of decreasing the visible thumb
        size even though the number doesn't change.

        It's also important to note that subtracting the thumb size from the
        full range gives you the real range that can be used.  Microsoft
        defines nPos (the current scrolling position) to be within the range
        from nMin to nMax - max(nPage - 1, 0).  We know that wxMSW code always
        sets nMin = 0 and nMax = range -1.  So let's algebraically reduce the
        definition of the maximum allowed position:

        Begin:
        = nMax - max(nPage - 1, 0)
        Substitute (range - 1) for nMax and thumbSize for nPage:
        = range - 1 - max(thumbSize - 1, 0)
        Add one inside the max conditional and subtract one outside of it:
        = range - 1 - (max(thumbSize - 1 + 1, 1) - 1)
        Reduce some constants:
        = range - 1 - (max(thumbSize, 1) - 1)
        Distribute the negative across the parenthesis:
        = range - 1 - max(thumbSize, 1) + 1
        Reduce the constants:
        = range - max(thumbSize, 1)

        Also keep in mind that thumbSize may never be greater than range but
        can be equal to it.  Thus for the smallest possible thumbSize there
        are exactly range possible scroll positions (numbered from 0 to
        range - 1) and for the largest possible thumbSize there is exactly
        one possible scroll position (numbered 0).
     */
    int m_scrollThumb[2];

    /*!
        The origin of the virtual coordinate space expressed in terms of client
        coordinates.  Starts at (0,0) and each call to ScrollWindow accumulates
        into it.  Thus if the user scrolls the window right (thus causing the
        contents to move left with respect to the client origin, the
        application code (typically wxScrolledWindow) will be called with
        dx of -something, for example -20.  This is added to m_virtualOrigin
        and thus m_virtualOrigin will be (-20,0) in this example.
     */
    wxPoint m_virtualOrigin;
private:
    wxWindowCocoaScrollView();
};

#endif //ndef _WX_COCOA_SCROLLVIEW_H__
