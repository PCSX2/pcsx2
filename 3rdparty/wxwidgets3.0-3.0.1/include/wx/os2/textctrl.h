/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/textctrl.h
// Purpose:     wxTextCtrl class
// Author:      David Webster
// Modified by:
// Created:     10/17/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTCTRL_H_
#define _WX_TEXTCTRL_H_

typedef int (wxCALLBACK *wxTreeCtrlCompare)(long lItem1, long lItem2, long lSortData);

class WXDLLIMPEXP_CORE wxTextCtrl : public wxTextCtrlBase
{
public:
    wxTextCtrl();
    wxTextCtrl( wxWindow*          pParent
               ,wxWindowID         vId
               ,const wxString&    rsValue = wxEmptyString
               ,const wxPoint&     rPos = wxDefaultPosition
               ,const wxSize&      rSize = wxDefaultSize
               ,long               lStyle = 0
               ,const wxValidator& rValidator = wxDefaultValidator
               ,const wxString&    rsName = wxTextCtrlNameStr
              )
    {
        Create(pParent, vId, rsValue, rPos, rSize, lStyle, rValidator, rsName);
    }
    virtual ~wxTextCtrl();

    bool Create( wxWindow*          pParent
                ,wxWindowID         vId
                ,const wxString&    rsValue = wxEmptyString
                ,const wxPoint&     rPos = wxDefaultPosition
                ,const wxSize&      rSize = wxDefaultSize
                ,long               lStyle = 0
                ,const wxValidator& rValidator = wxDefaultValidator
                ,const wxString&    rsName = wxTextCtrlNameStr
               );

    //
    // Implement base class pure virtuals
    // ----------------------------------
    //
    virtual      wxString GetValue(void) const;

    virtual int      GetLineLength(long nLineNo) const;
    virtual wxString GetLineText(long nLineNo) const;
    virtual int      GetNumberOfLines(void) const;

    virtual bool IsModified(void) const;
    virtual bool IsEditable(void) const;

    virtual void GetSelection( long* pFrom
                              ,long* pTo
                             ) const;
    //
    // Operations
    // ----------
    //
    virtual void Clear(void);
    virtual void Replace( long            lFrom
                         ,long            lTo
                         ,const wxString& rsValue
                        );
    virtual void Remove( long lFrom
                        ,long lTo
                       );

    virtual bool DoLoadFile(const wxString& rsFile, int fileType);

    virtual void MarkDirty();
    virtual void DiscardEdits(void);

    virtual void WriteText(const wxString& rsText);
    virtual void AppendText(const wxString& rsText);
    virtual bool EmulateKeyPress(const wxKeyEvent& rEvent);

    virtual bool SetStyle( long              lStart
                          ,long              lEnd
                          ,const wxTextAttr& rStyle
                         );
    virtual long XYToPosition( long lX
                              ,long lY
                             ) const;
    virtual bool PositionToXY( long  lPos
                              ,long* plX
                              ,long* plY
                             ) const;

    virtual void ShowPosition(long lPos);

    virtual void Copy(void);
    virtual void Cut(void);
    virtual void Paste(void);

    virtual bool CanCopy(void) const;
    virtual bool CanCut(void) const;
    virtual bool CanPaste(void) const;

    virtual void Undo(void);
    virtual void Redo(void);

    virtual bool CanUndo(void) const;
    virtual bool CanRedo(void) const;

    virtual void SetInsertionPoint(long lPos);
    virtual void SetInsertionPointEnd(void);
    virtual long GetInsertionPoint(void) const;
    virtual wxTextPos GetLastPosition(void) const;

    virtual void SetSelection( long lFrom
                              ,long lTo
                             );
    virtual void SetEditable(bool bEditable);
    virtual void SetWindowStyleFlag(long lStyle);

    //
    // Implementation from now on
    // --------------------------
    //
    virtual void Command(wxCommandEvent& rEvent);
    virtual bool OS2Command( WXUINT uParam
                            ,WXWORD wId
                           );

    virtual WXHBRUSH OnCtlColor( WXHDC    hDC
                                ,WXHWND   pWnd
                                ,WXUINT   nCtlColor
                                ,WXUINT   message
                                ,WXWPARAM wParam
                                ,WXLPARAM lParam
                               );

    virtual bool SetBackgroundColour(const wxColour& colour);
    virtual bool SetForegroundColour(const wxColour& colour);

    virtual void AdoptAttributesFromHWND(void);
    virtual void SetupColours(void);

    virtual bool AcceptsFocus(void) const;

    // callbacks
    void OnDropFiles(wxDropFilesEvent& rEvent);
    void OnChar(wxKeyEvent& rEvent); // Process 'enter' if required

    void OnCut(wxCommandEvent& rEvent);
    void OnCopy(wxCommandEvent& rEvent);
    void OnPaste(wxCommandEvent& rEvent);
    void OnUndo(wxCommandEvent& rEvent);
    void OnRedo(wxCommandEvent& rEvent);
    void OnDelete(wxCommandEvent& rEvent);
    void OnSelectAll(wxCommandEvent& rEvent);

    void OnUpdateCut(wxUpdateUIEvent& rEvent);
    void OnUpdateCopy(wxUpdateUIEvent& rEvent);
    void OnUpdatePaste(wxUpdateUIEvent& rEvent);
    void OnUpdateUndo(wxUpdateUIEvent& rEvent);
    void OnUpdateRedo(wxUpdateUIEvent& rEvent);
    void OnUpdateDelete(wxUpdateUIEvent& rEvent);
    void OnUpdateSelectAll(wxUpdateUIEvent& rEvent);

    inline bool IsMLE(void) {return m_bIsMLE;}
    inline void SetMLE(bool bIsMLE) {m_bIsMLE = bIsMLE;}

protected:
    //
    // call this to increase the size limit (will do nothing if the current
    // limit is big enough)
    //
    void           AdjustSpaceLimit(void);
    virtual wxSize DoGetBestSize(void) const;
    virtual bool   OS2ShouldPreProcessMessage(WXMSG* pMsg);

    virtual WXDWORD OS2GetStyle( long     lStyle
                                ,WXDWORD* dwExstyle
                               ) const;

    virtual void DoSetValue(const wxString &value, int flags = 0);

    bool m_bSkipUpdate;

private:
    // implement wxTextEntry pure virtual: it implements all the operations for
    // the simple EDIT controls
    virtual WXHWND GetEditHWND() const { return m_hWnd; }

    bool                            m_bIsMLE;
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxTextCtrl)
}; // end of CLASS wxTextCtrl

#endif
    // _WX_TEXTCTRL_H_
