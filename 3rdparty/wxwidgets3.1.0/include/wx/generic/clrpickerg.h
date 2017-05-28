/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/clrpickerg.h
// Purpose:     wxGenericColourButton header
// Author:      Francesco Montorsi (based on Vadim Zeitlin's code)
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Vadim Zeitlin, Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CLRPICKER_H_
#define _WX_CLRPICKER_H_

#include "wx/button.h"
#include "wx/bmpbuttn.h"
#include "wx/colourdata.h"

//-----------------------------------------------------------------------------
// wxGenericColourButton: a button which brings up a wxColourDialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericColourButton : public wxBitmapButton,
                                               public wxColourPickerWidgetBase
{
public:
    wxGenericColourButton() {}
    wxGenericColourButton(wxWindow *parent,
                          wxWindowID id,
                          const wxColour& col = *wxBLACK,
                          const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxDefaultSize,
                          long style = wxCLRBTN_DEFAULT_STYLE,
                          const wxValidator& validator = wxDefaultValidator,
                          const wxString& name = wxColourPickerWidgetNameStr)
    {
        Create(parent, id, col, pos, size, style, validator, name);
    }

    virtual ~wxGenericColourButton() {}


public:     // API extensions specific for wxGenericColourButton

    // user can override this to init colour data in a different way
    virtual void InitColourData();

    // returns the colour data shown in wxColourDialog
    wxColourData *GetColourData() { return &ms_data; }


public:

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxColour& col = *wxBLACK,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxCLRBTN_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxColourPickerWidgetNameStr);

    void OnButtonClick(wxCommandEvent &);


protected:
    wxBitmap    m_bitmap;

    wxSize DoGetBestSize() const;

    void UpdateColour();

    // the colour data shown in wxColourPickerCtrlGeneric
    // controls. This member is static so that all colour pickers
    // in the program share the same set of custom colours.
    static wxColourData ms_data;

private:
    wxDECLARE_DYNAMIC_CLASS(wxGenericColourButton);
};


#endif // _WX_CLRPICKER_H_
