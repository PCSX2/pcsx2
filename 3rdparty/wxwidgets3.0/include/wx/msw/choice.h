/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/choice.h
// Purpose:     wxChoice class
// Author:      Julian Smart
// Modified by: Vadim Zeitlin to derive from wxChoiceBase
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHOICE_H_
#define _WX_CHOICE_H_

struct tagCOMBOBOXINFO;

// ----------------------------------------------------------------------------
// Choice item
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxChoice : public wxChoiceBase
{
public:
    // ctors
    wxChoice() { Init(); }
    virtual ~wxChoice();

    wxChoice(wxWindow *parent,
             wxWindowID id,
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

    wxChoice(wxWindow *parent,
             wxWindowID id,
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

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxChoiceNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxChoiceNameStr);

    virtual bool Show(bool show = true);

    virtual void SetLabel(const wxString& label);

    virtual unsigned int GetCount() const;
    virtual int GetSelection() const;
    virtual int GetCurrentSelection() const;
    virtual void SetSelection(int n);

    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& s);

    virtual wxVisualAttributes GetDefaultAttributes() const
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // MSW only
    virtual bool MSWCommand(WXUINT param, WXWORD id);
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
    virtual WXHBRUSH MSWControlColor(WXHDC hDC, WXHWND hWnd);
    virtual bool MSWShouldPreProcessMessage(WXMSG *pMsg);
    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    // common part of all ctors
    void Init()
    {
        m_lastAcceptedSelection =
        m_pendingSelection = wxID_NONE;
        m_heightOwn = wxDefaultCoord;
    }

    virtual void DoDeleteOneItem(unsigned int n);
    virtual void DoClear();

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;

    // MSW implementation
    virtual wxSize DoGetBestSize() const;
    virtual void DoGetSize(int *w, int *h) const;
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const;

    // Show or hide the popup part of the control.
    void MSWDoPopupOrDismiss(bool show);

    // update the height of the drop down list to fit the number of items we
    // have (without changing the visible height)
    void MSWUpdateDropDownHeight();

    // set the height of the visible part of the control to m_heightOwn
    void MSWUpdateVisibleHeight();

    // Call GetComboBoxInfo() and return false if it's not supported by this
    // system. Notice that the caller must initialize info.cbSize.
    bool MSWGetComboBoxInfo(tagCOMBOBOXINFO* info) const;

    // create and initialize the control
    bool CreateAndInit(wxWindow *parent, wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       int n, const wxString choices[],
                       long style,
                       const wxValidator& validator,
                       const wxString& name);

    // free all memory we have (used by Clear() and dtor)
    void Free();

    // set the height for simple combo box
    int SetHeightSimpleComboBox(int nItems) const;

#if wxUSE_DEFERRED_SIZING
    virtual void MSWEndDeferWindowPos();
#endif // wxUSE_DEFERRED_SIZING

    // These variables are only used while the drop down is opened.
    //
    // The first one contains the item that had been originally selected before
    // the drop down was opened and the second one the item we should select
    // when the drop down is closed again.
    int m_lastAcceptedSelection,
        m_pendingSelection;

    // the height of the control itself if it was set explicitly or
    // wxDefaultCoord if it hadn't
    int m_heightOwn;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxChoice)
};

#endif // _WX_CHOICE_H_
