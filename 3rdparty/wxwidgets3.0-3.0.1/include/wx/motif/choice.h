/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/choice.h
// Purpose:     wxChoice class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHOICE_H_
#define _WX_CHOICE_H_

#include "wx/clntdata.h"

#ifndef wxWIDGET_ARRAY_DEFINED
    #define wxWIDGET_ARRAY_DEFINED

    #include "wx/dynarray.h"
    WX_DEFINE_ARRAY_PTR(WXWidget, wxWidgetArray);
#endif

// Choice item
class WXDLLIMPEXP_CORE wxChoice: public wxChoiceBase
{
    DECLARE_DYNAMIC_CLASS(wxChoice)

public:
    wxChoice();
    virtual ~wxChoice();

    wxChoice(wxWindow *parent, wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        int n = 0, const wxString choices[] = NULL,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxChoiceNameStr)
    {
        Init();
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }

    wxChoice(wxWindow *parent, wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        const wxArrayString& choices,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxChoiceNameStr)
    {
        Init();
        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        int n = 0, const wxString choices[] = NULL,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxChoiceNameStr);

    bool Create(wxWindow *parent, wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        const wxArrayString& choices,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxChoiceNameStr);

    // implementation of wxControlWithItems
    virtual unsigned int GetCount() const;
    virtual int GetSelection() const;
    virtual void DoDeleteOneItem(unsigned int n);
    virtual void DoClear();
    virtual void SetString(unsigned int n, const wxString& s);
    virtual wxString GetString(unsigned int n) const;

    // implementation of wxChoiceBase
    virtual void SetSelection(int n);
    virtual void SetColumns(int n = 1 );
    virtual int GetColumns() const ;

    // Original API
    virtual void Command(wxCommandEvent& event);

    void SetFocus();

    // Implementation
    virtual void ChangeFont(bool keepOriginalSize = true);
    virtual void ChangeBackgroundColour();
    virtual void ChangeForegroundColour();
    WXWidget GetTopWidget() const { return m_formWidget; }
    WXWidget GetMainWidget() const { return m_buttonWidget; }

    virtual wxSize DoGetBestSize() const;

    // implementation, for wxChoiceCallback
    const wxWidgetArray& GetWidgets() const { return m_widgetArray; }
    const wxArrayString&  GetStrings() const { return m_stringArray; }
protected:
    // minimum size for the text ctrl
    wxSize GetItemsSize() const;
    // common part of all contructors
    void Init();

    WXWidget      m_menuWidget;
    WXWidget      m_buttonWidget;
    wxWidgetArray m_widgetArray;
    WXWidget      m_formWidget;
    wxArrayString m_stringArray;

    virtual void DoSetSize(int x, int y,
        int width, int height,
        int sizeFlags = wxSIZE_AUTO);

    // implementation of wxControlWithItems
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);
};

#endif // _WX_CHOICE_H_
