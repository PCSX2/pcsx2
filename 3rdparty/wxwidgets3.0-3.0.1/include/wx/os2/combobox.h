/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/combobox.h
// Purpose:     wxComboBox class
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COMBOBOX_H_
#define _WX_COMBOBOX_H_

#include "wx/choice.h"
#include "wx/textentry.h"

#if wxUSE_COMBOBOX

// Combobox item
class WXDLLIMPEXP_CORE wxComboBox : public wxChoice,
                               public wxTextEntry
{

 public:
  inline wxComboBox() {}

  inline wxComboBox( wxWindow*          pParent
                    ,wxWindowID         vId
                    ,const wxString&    rsValue = wxEmptyString
                    ,const wxPoint&     rPos = wxDefaultPosition
                    ,const wxSize&      rSize = wxDefaultSize
                    ,int                n = 0
                    ,const wxString     asChoices[] = NULL
                    ,long               lStyle = 0
                    ,const wxValidator& rValidator = wxDefaultValidator
                    ,const wxString&    rsName = wxComboBoxNameStr
                   )
    {
        Create( pParent
               ,vId
               ,rsValue
               ,rPos
               ,rSize
               ,n
               ,asChoices
               ,lStyle
               ,rValidator
               ,rsName
              );
    }

  inline wxComboBox( wxWindow*            pParent
                    ,wxWindowID           vId
                    ,const wxString&      rsValue
                    ,const wxPoint&       rPos
                    ,const wxSize&        rSize
                    ,const wxArrayString& asChoices
                    ,long                 lStyle = 0
                    ,const wxValidator&   rValidator = wxDefaultValidator
                    ,const wxString&      rsName = wxComboBoxNameStr
                   )
    {
        Create( pParent
               ,vId
               ,rsValue
               ,rPos
               ,rSize
               ,asChoices
               ,lStyle
               ,rValidator
               ,rsName
              );
    }

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,const wxString&    rsValue = wxEmptyString
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,int                n = 0
                ,const wxString     asChoices[] = NULL
                ,long               lStyle = 0
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxComboBoxNameStr
               );

    bool Create( wxWindow*            pParent
                ,wxWindowID           vId
                ,const wxString&      rsValue
                ,const wxPoint&       rPos
                ,const wxSize&        rSize
                ,const wxArrayString& asChoices
                ,long                 lStyle = 0
                ,const wxValidator&   rValidator = wxDefaultValidator
                ,const wxString&      rsName = wxComboBoxNameStr
               );

    // See wxComboBoxBase discussion of IsEmpty().
    bool IsListEmpty() const { return wxItemContainer::IsEmpty(); }
    bool IsTextEmpty() const { return wxTextEntry::IsEmpty(); }

    // resolve ambiguities among virtual functions inherited from both base
    // classes
    virtual void Clear();
    virtual wxString GetValue() const;
    virtual void SetValue(const wxString& value);
    virtual wxString GetStringSelection() const
        { return wxChoice::GetStringSelection(); }

    inline virtual void SetSelection(int n) { wxChoice::SetSelection(n); }
    virtual void SetSelection(long from, long to)
        { wxTextEntry::SetSelection(from, to); }
    virtual int GetSelection() const { return wxChoice::GetSelection(); }
    virtual void GetSelection(long *from, long *to) const
        { wxTextEntry::GetSelection(from, to); }

    virtual bool IsEditable() const;

    virtual bool        OS2Command( WXUINT uParam
                                   ,WXWORD wId
                                  );
    bool                ProcessEditMsg( WXUINT   uMsg
                                       ,WXWPARAM wParam
                                       ,WXLPARAM lParam
                                      );

private:
    // implement wxTextEntry pure virtual methods
    virtual wxWindow *GetEditableWindow() { return this; }
    virtual WXHWND GetEditHWND() const { return m_hWnd; }

    DECLARE_DYNAMIC_CLASS(wxComboBox)
}; // end of CLASS wxComboBox

#endif // wxUSE_COMBOBOX
#endif
    // _WX_COMBOBOX_H_
