/////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/bmpbuttn.h
// Purpose:     wxBitmapButton class for wxUniversal
// Author:      Vadim Zeitlin
// Modified by:
// Created:     25.08.00
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_BMPBUTTN_H_
#define _WX_UNIV_BMPBUTTN_H_

class WXDLLIMPEXP_CORE wxBitmapButton : public wxBitmapButtonBase
{
public:
    wxBitmapButton() { }

    wxBitmapButton(wxWindow *parent,
                   wxWindowID id,
                   const wxBitmap& bitmap,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, bitmap, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxBitmap& bitmap,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    virtual void SetMargins(int x, int y) wxOVERRIDE
    {
        SetBitmapMargins(x, y);

        wxBitmapButtonBase::SetMargins(x, y);
    }

    virtual bool Enable(bool enable = true) wxOVERRIDE;

    virtual bool SetCurrent(bool doit = true) wxOVERRIDE;

    virtual void Press() wxOVERRIDE;
    virtual void Release() wxOVERRIDE;

protected:
    void OnSetFocus(wxFocusEvent& event);
    void OnKillFocus(wxFocusEvent& event);

    // called when one of the bitmap is changed by user
    virtual void OnSetBitmap() wxOVERRIDE;

    // set bitmap to the given one if it's ok or to the normal bitmap and
    // return true if the bitmap really changed
    bool ChangeBitmap(const wxBitmap& bmp);

private:
    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxBitmapButton);
};

#endif // _WX_UNIV_BMPBUTTN_H_

