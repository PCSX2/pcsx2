/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/scrolbar.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKSCROLLBARH__
#define __GTKSCROLLBARH__

#include "wx/defs.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxScrollBar;

//-----------------------------------------------------------------------------
// wxScrollBar
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxScrollBar: public wxScrollBarBase
{
public:
    wxScrollBar()
       { m_adjust = NULL; m_oldPos = 0.0; }
    inline wxScrollBar( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = wxSB_HORIZONTAL,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxScrollBarNameStr )
    {
        Create( parent, id, pos, size, style, validator, name );
    }
    bool Create( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = wxSB_HORIZONTAL,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxScrollBarNameStr );
    virtual ~wxScrollBar();
    int GetThumbPosition() const;
    int GetThumbSize() const;
    int GetPageSize() const;
    int GetRange() const;
    virtual void SetThumbPosition( int viewStart );
    virtual void SetScrollbar( int position, int thumbSize, int range, int pageSize,
      bool refresh = TRUE );

    // Backward compatibility
    // ----------------------

    int GetValue(void) const;
    void SetValue( int viewStart );
    void GetValues( int *viewStart, int *viewLength, int *objectLength, int *pageLength) const;
    int GetViewLength() const;
    int GetObjectLength() const;
    void SetPageSize( int pageLength );
    void SetObjectLength( int objectLength );
    void SetViewLength( int viewLength );

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    // --------------

    bool IsOwnGtkWindow( GdkWindow *window );

    GtkAdjustment  *m_adjust;
    float           m_oldPos;

protected:
    virtual wxSize DoGetBestSize() const;

private:
    DECLARE_DYNAMIC_CLASS(wxScrollBar)
};

#endif
    // __GTKSCROLLBARH__
