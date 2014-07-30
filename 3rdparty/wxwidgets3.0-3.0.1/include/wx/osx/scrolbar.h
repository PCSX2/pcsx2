/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/scrolbar.h
// Purpose:     wxScrollBar class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SCROLBAR_H_
#define _WX_SCROLBAR_H_

// Scrollbar item
class WXDLLIMPEXP_CORE wxScrollBar : public wxScrollBarBase
{
public:
    wxScrollBar() { m_pageSize = 0; m_viewSize = 0; m_objectSize = 0; }
    virtual ~wxScrollBar();

    wxScrollBar(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSB_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxScrollBarNameStr)
    {
        Create(parent, id, pos, size, style, validator, name);
    }
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSB_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxScrollBarNameStr);

    virtual int GetThumbPosition() const ;
    virtual int GetThumbSize() const { return m_viewSize; }
    virtual int GetPageSize() const { return m_pageSize; }
    virtual int GetRange() const { return m_objectSize; }

    virtual void SetThumbPosition(int viewStart);
    virtual void SetScrollbar(int position, int thumbSize, int range,
            int pageSize, bool refresh = true);

    // needed for RTTI
    void SetThumbSize( int s ) { SetScrollbar( GetThumbPosition() , s , GetRange() , GetPageSize() , true ) ; }
    void SetPageSize( int s ) { SetScrollbar( GetThumbPosition() , GetThumbSize() , GetRange() , s , true ) ; }
    void SetRange( int s ) { SetScrollbar( GetThumbPosition() , GetThumbSize() , s , GetPageSize() , true ) ; }

        // implementation only from now on
    void Command(wxCommandEvent& event);
    virtual void TriggerScrollEvent( wxEventType scrollEvent ) ;
    virtual bool OSXHandleClicked( double timestampsec );
protected:
    virtual wxSize DoGetBestSize() const;

    int m_pageSize;
    int m_viewSize;
    int m_objectSize;

    DECLARE_DYNAMIC_CLASS(wxScrollBar)
    DECLARE_EVENT_TABLE()
};

#endif // _WX_SCROLBAR_H_
