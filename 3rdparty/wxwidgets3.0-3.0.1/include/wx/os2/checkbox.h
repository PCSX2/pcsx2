/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/checkbox.h
// Purpose:     wxCheckBox class
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKBOX_H_
#define _WX_CHECKBOX_H_

#include "wx/control.h"

// Checkbox item (single checkbox)
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_CORE wxCheckBox : public wxCheckBoxBase
{
 public:
    inline wxCheckBox() { }
    inline wxCheckBox( wxWindow*          pParent
                      ,wxWindowID         vId
                      ,const wxString&    rsLabel
                      ,const wxPoint&     rPos = wxDefaultPosition
                      ,const wxSize&      rSize = wxDefaultSize
                      ,long lStyle = 0
                      ,const wxValidator& rValidator = wxDefaultValidator
                      ,const wxString&    rsName = wxCheckBoxNameStr
                     )
    {
        Create( pParent
               ,vId
               ,rsLabel
               ,rPos
               ,rSize
               ,lStyle
               ,rValidator
               ,rsName
              );
    }

    bool Create( wxWindow* pParent
                ,wxWindowID         vId
                ,const wxString&    rsLabel
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long lStyle = 0
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxCheckBoxNameStr
               );

  virtual void SetValue(bool bValue);
  virtual bool GetValue(void) const ;

  virtual bool OS2Command( WXUINT uParam
                          ,WXWORD wId
                         );
  virtual void SetLabel(const wxString& rsLabel);
  virtual void Command(wxCommandEvent& rEvent);

protected:
  virtual wxSize DoGetBestSize(void) const;
private:
  DECLARE_DYNAMIC_CLASS(wxCheckBox)
};

class WXDLLIMPEXP_CORE wxBitmapCheckBox: public wxCheckBox
{
 public:

    inline wxBitmapCheckBox() { m_nCheckWidth = -1; m_nCheckHeight = -1; }
    inline wxBitmapCheckBox( wxWindow*          pParent
                            ,wxWindowID         vId
                            ,const wxBitmap*    pLabel
                            ,const wxPoint&     rPos = wxDefaultPosition
                            ,const wxSize&      rSize = wxDefaultSize
                            ,long               lStyle = 0
                            ,const wxValidator& rValidator = wxDefaultValidator
                            ,const wxString&    rsName = wxCheckBoxNameStr
                           )
    {
        Create( pParent
               ,vId
               ,pLabel
               ,rPos
               ,rSize
               ,lStyle
               ,rValidator
               ,rsName
              );
    }

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,const wxBitmap*    pLabel
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = 0
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxCheckBoxNameStr
               );

    virtual void SetLabel(const wxBitmap& rBitmap);

    int                             m_nCheckWidth;
    int                             m_nCheckHeight;

private:

    virtual void SetLabel(const wxString& rsString)
    { wxCheckBox::SetLabel(rsString); }
    DECLARE_DYNAMIC_CLASS(wxBitmapCheckBox)
};
#endif
    // _WX_CHECKBOX_H_
