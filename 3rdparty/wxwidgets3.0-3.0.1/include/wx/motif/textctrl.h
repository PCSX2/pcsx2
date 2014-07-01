/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/textctrl.h
// Purpose:     wxTextCtrl class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTCTRL_H_
#define _WX_TEXTCTRL_H_

// Single-line text item
class WXDLLIMPEXP_CORE wxTextCtrl : public wxTextCtrlBase
{
public:
    // creation
    // --------

    wxTextCtrl();
    wxTextCtrl(wxWindow *parent,
               wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxTextCtrlNameStr)
    {
        Create(parent, id, value, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTextCtrlNameStr);

    // accessors
    // ---------
    virtual wxString GetValue() const;

    virtual int GetLineLength(long lineNo) const;
    virtual wxString GetLineText(long lineNo) const;
    virtual int GetNumberOfLines() const;

    // operations
    // ----------

    virtual void MarkDirty();
    virtual void DiscardEdits();
    virtual bool IsModified() const;

    virtual long XYToPosition(long x, long y) const;
    virtual bool PositionToXY(long pos, long *x, long *y) const;
    virtual void ShowPosition(long pos);

    // callbacks
    // ---------
    void OnDropFiles(wxDropFilesEvent& event);
    void OnChar(wxKeyEvent& event);
    //  void OnEraseBackground(wxEraseEvent& event);

    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);

    void OnUpdateCut(wxUpdateUIEvent& event);
    void OnUpdateCopy(wxUpdateUIEvent& event);
    void OnUpdatePaste(wxUpdateUIEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);

    virtual void Command(wxCommandEvent& event);

    // implementation from here to the end
    // -----------------------------------
    virtual void ChangeFont(bool keepOriginalSize = true);
    virtual void ChangeBackgroundColour();
    virtual void ChangeForegroundColour();
    void SetModified(bool mod) { m_modified = mod; }
    virtual WXWidget GetTopWidget() const;

    // send the CHAR and TEXT_UPDATED events
    void DoSendEvents(void /* XmTextVerifyCallbackStruct */ *cbs,
                      long keycode);

protected:
    virtual wxSize DoGetBestSize() const;

    virtual void DoSetValue(const wxString& value, int flags = 0);

    virtual WXWidget GetTextWidget() const { return m_mainWidget; }

public:
    // Motif-specific
    void*     m_tempCallbackStruct;
    bool      m_modified;
    wxString  m_value;            // Required for password text controls

    // Did we call wxTextCtrl::OnChar? If so, generate a command event.
    bool      m_processedDefault;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxTextCtrl)
};

#endif
// _WX_TEXTCTRL_H_
