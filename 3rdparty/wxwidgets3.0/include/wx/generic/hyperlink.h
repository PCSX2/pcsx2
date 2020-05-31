/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/hyperlink.h
// Purpose:     Hyperlink control
// Author:      David Norris <danorris@gmail.com>, Otto Wyss
// Modified by: Ryan Norton, Francesco Montorsi
// Created:     04/02/2005
// Copyright:   (c) 2005 David Norris
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERICHYPERLINKCTRL_H_
#define _WX_GENERICHYPERLINKCTRL_H_

// ----------------------------------------------------------------------------
// wxGenericHyperlinkCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGenericHyperlinkCtrl : public wxHyperlinkCtrlBase
{
public:
    // Default constructor (for two-step construction).
    wxGenericHyperlinkCtrl() { Init(); }

    // Constructor.
    wxGenericHyperlinkCtrl(wxWindow *parent,
                            wxWindowID id,
                            const wxString& label, const wxString& url,
                            const wxPoint& pos = wxDefaultPosition,
                            const wxSize& size = wxDefaultSize,
                            long style = wxHL_DEFAULT_STYLE,
                            const wxString& name = wxHyperlinkCtrlNameStr)
    {
        Init();
        (void) Create(parent, id, label, url, pos, size, style, name);
    }

    // Creation function (for two-step construction).
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label, const wxString& url,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxHL_DEFAULT_STYLE,
                const wxString& name = wxHyperlinkCtrlNameStr);


    // get/set
    wxColour GetHoverColour() const { return m_hoverColour; }
    void SetHoverColour(const wxColour &colour) { m_hoverColour = colour; }

    wxColour GetNormalColour() const { return m_normalColour; }
    void SetNormalColour(const wxColour &colour);

    wxColour GetVisitedColour() const { return m_visitedColour; }
    void SetVisitedColour(const wxColour &colour);

    wxString GetURL() const { return m_url; }
    void SetURL (const wxString &url) { m_url=url; }

    void SetVisited(bool visited = true) { m_visited=visited; }
    bool GetVisited() const { return m_visited; }

    // NOTE: also wxWindow::Set/GetLabel, wxWindow::Set/GetBackgroundColour,
    //       wxWindow::Get/SetFont, wxWindow::Get/SetCursor are important !


protected:
    // Helper used by this class itself and native MSW implementation that
    // connects OnRightUp() and OnPopUpCopy() handlers.
    void ConnectMenuHandlers();

    // event handlers

    // Renders the hyperlink.
    void OnPaint(wxPaintEvent& event);

    // Handle set/kill focus events (invalidate for painting focus rect)
    void OnFocus(wxFocusEvent& event);

    // Fire a HyperlinkEvent on space
    void OnChar(wxKeyEvent& event);

    // Returns the wxRect of the label of this hyperlink.
    // This is different from the clientsize's rectangle when
    // clientsize != bestsize and this rectangle is influenced
    // by the alignment of the label (wxHL_ALIGN_*).
    wxRect GetLabelRect() const;

    // If the click originates inside the bounding box of the label,
    // a flag is set so that an event will be fired when the left
    // button is released.
    void OnLeftDown(wxMouseEvent& event);

    // If the click both originated and finished inside the bounding box
    // of the label, a HyperlinkEvent is fired.
    void OnLeftUp(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);

    // Changes the cursor to a hand, if the mouse is inside the label's
    // bounding box.
    void OnMotion(wxMouseEvent& event);

    // Changes the cursor back to the default, if necessary.
    void OnLeaveWindow(wxMouseEvent& event);

    // handles "Copy URL" menuitem
    void OnPopUpCopy(wxCommandEvent& event);

    // overridden base class virtuals

    // Returns the best size for the window, which is the size needed
    // to display the text label.
    virtual wxSize DoGetBestClientSize() const;

    // creates a context menu with "Copy URL" menuitem
    virtual void DoContextMenu(const wxPoint &);

private:
    // Common part of all ctors.
    void Init();

    // URL associated with the link. This is transmitted inside
    // the HyperlinkEvent fired when the user clicks on the label.
    wxString m_url;

    // Foreground colours for various link types.
    // NOTE: wxWindow::m_backgroundColour is used for background,
    //       wxWindow::m_foregroundColour is used to render non-visited links
    wxColour m_hoverColour;
    wxColour m_normalColour;
    wxColour m_visitedColour;

    // True if the mouse cursor is inside the label's bounding box.
    bool m_rollover;

    // True if the link has been clicked before.
    bool m_visited;

    // True if a click is in progress (left button down) and the click
    // originated inside the label's bounding box.
    bool m_clicking;
};

#endif // _WX_GENERICHYPERLINKCTRL_H_
