/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/progdlg.h
// Purpose:     wxProgressDialog
// Author:      Rickard Westerlund
// Created:     2010-07-22
// Copyright:   (c) 2010 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PROGDLG_H_
#define _WX_PROGDLG_H_

class wxProgressDialogTaskRunner;
class wxProgressDialogSharedData;

class WXDLLIMPEXP_CORE wxProgressDialog : public wxGenericProgressDialog
{
public:
    wxProgressDialog(const wxString& title, const wxString& message,
                     int maximum = 100,
                     wxWindow *parent = NULL,
                     int style = wxPD_APP_MODAL | wxPD_AUTO_HIDE);

    virtual ~wxProgressDialog();

    virtual bool Update(int value, const wxString& newmsg = wxEmptyString, bool *skip = NULL);
    virtual bool Pulse(const wxString& newmsg = wxEmptyString, bool *skip = NULL);

    void Resume();

    int GetValue() const;
    wxString GetMessage() const;

    void SetRange(int maximum);

    // Return whether "Cancel" or "Skip" button was pressed, always return
    // false if the corresponding button is not shown.
    bool WasSkipped() const;
    bool WasCancelled() const;

    virtual void SetTitle(const wxString& title);
    virtual wxString GetTitle() const;

    virtual bool Show( bool show = true );

    // Must provide overload to avoid hiding it (and warnings about it)
    virtual void Update() { wxGenericProgressDialog::Update(); }

    virtual WXWidget GetHandle() const;

private:
    // Performs common routines to Update() and Pulse(). Requires the
    // shared object to have been entered.
    bool DoNativeBeforeUpdate(bool *skip);

    // Updates the various timing informations for both determinate
    // and indeterminate modes. Requires the shared object to have
    // been entered.
    void UpdateExpandedInformation(int value);

    wxProgressDialogTaskRunner *m_taskDialogRunner;

    wxProgressDialogSharedData *m_sharedData;

    // Store the message and title we currently use to be able to return it
    // from Get{Message,Title}()
    wxString m_message,
             m_title;

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxProgressDialog);
};

#endif // _WX_PROGDLG_H_
