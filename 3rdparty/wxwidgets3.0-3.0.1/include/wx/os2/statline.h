/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/statline.h
// Purpose:     MSW version of wxStaticLine class
// Author:      Vadim Zeitlin
// Created:     28.06.99
// Copyright:   (c) 1998 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_STATLINE_H_
#define _WX_OS2_STATLINE_H_

// ----------------------------------------------------------------------------
// wxStaticLine
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStaticLine : public wxStaticLineBase
{

public:
    // constructors and pseudo-constructors
    wxStaticLine() { }
    wxStaticLine( wxWindow*       pParent
                 ,wxWindowID      vId = wxID_ANY
                 ,const wxPoint&  rPos = wxDefaultPosition
                 ,const wxSize&   rSize = wxDefaultSize
                 ,long            lStyle = wxLI_HORIZONTAL
                 ,const wxString& rsName = wxStaticLineNameStr
                )
    {
        Create(pParent, vId, rPos, rSize, lStyle, rsName);
    }

    bool Create(  wxWindow*       pParent
                 ,wxWindowID      vId = wxID_ANY
                 ,const wxPoint&  rPos = wxDefaultPosition
                 ,const wxSize&   rSize = wxDefaultSize
                 ,long            lStyle = wxLI_HORIZONTAL
                 ,const wxString& rsName = wxStaticLineNameStr
                );

    inline bool          IsVertical(void) const { return((GetWindowStyleFlag() & wxLI_VERTICAL) != 0); }
    inline static int    GetDefaultSize(void) { return 2; }

    //
    // Overridden base class virtuals
    //
    inline virtual bool AcceptsFocus(void) const {return FALSE;}

protected:
    inline wxSize AdjustSize(const wxSize& rSize) const
    {
        wxSize                      vSizeReal( rSize.x
                                              ,rSize.y
                                             );

        if (IsVertical())
        {
            if (rSize.x == -1 )
                vSizeReal.x = GetDefaultSize();
        }
        else
        {
            if (rSize.y == -1)
                vSizeReal.y = GetDefaultSize();
        }
        return vSizeReal;
    }

    inline wxSize DoGetBestSize(void) const { return (AdjustSize(wxDefaultSize)); }

    //
    // Usually overridden base class virtuals
    //
    virtual WXDWORD OS2GetStyle( long     lStyle
                                ,WXDWORD* pdwExstyle
                               ) const;

private:
    DECLARE_DYNAMIC_CLASS(wxStaticLine)
}; // end of CLASS wxStaticLine

#endif // _WX_OS2_STATLINE_H_


