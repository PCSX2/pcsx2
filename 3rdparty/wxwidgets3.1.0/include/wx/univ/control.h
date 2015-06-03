/////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/control.h
// Purpose:     universal wxControl: adds handling of mnemonics
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.08.00
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_CONTROL_H_
#define _WX_UNIV_CONTROL_H_

class WXDLLIMPEXP_FWD_CORE wxControlRenderer;
class WXDLLIMPEXP_FWD_CORE wxInputHandler;
class WXDLLIMPEXP_FWD_CORE wxRenderer;

// we must include it as most/all control classes derive their handlers from
// it
#include "wx/univ/inphand.h"

#include "wx/univ/inpcons.h"

// ----------------------------------------------------------------------------
// wxControlAction: the action is currently just a string which identifies it,
// later it might become an atom (i.e. an opaque handler to string).
// ----------------------------------------------------------------------------

typedef wxString wxControlAction;

// the list of actions which apply to all controls (other actions are defined
// in the controls headers)

#define wxACTION_NONE    wxT("")           // no action to perform

// ----------------------------------------------------------------------------
// wxControl: the base class for all GUI controls
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControl : public wxControlBase, public wxInputConsumer
{
public:
    wxControl() { Init(); }

    wxControl(wxWindow *parent,
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize, long style = 0,
              const wxValidator& validator = wxDefaultValidator,
              const wxString& name = wxControlNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxControlNameStr);

    // this function will filter out '&' characters and will put the
    // accelerator char (the one immediately after '&') into m_chAccel
    virtual void SetLabel(const wxString& label) wxOVERRIDE;

    // return the current label
    virtual wxString GetLabel() const wxOVERRIDE { return wxControlBase::GetLabel(); }

    // wxUniversal-specific methods

    // return the index of the accel char in the label or -1 if none
    int GetAccelIndex() const { return m_indexAccel; }

    // return the accel char itself or 0 if none
    wxChar GetAccelChar() const
    {
        return m_indexAccel == -1 ? wxT('\0') : (wxChar)m_label[m_indexAccel];
    }

    virtual wxWindow *GetInputWindow() const wxOVERRIDE { return (wxWindow*)this; }

protected:
    // common part of all ctors
    void Init();

    // set m_label and m_indexAccel and refresh the control to show the new
    // label (but, unlike SetLabel(), don't call the base class SetLabel() thus
    // avoiding to change wxControlBase::m_labelOrig)
    void UnivDoSetLabel(const wxString& label);

private:
    // label and accel info
    wxString   m_label;
    int        m_indexAccel;

    wxDECLARE_DYNAMIC_CLASS(wxControl);
    wxDECLARE_EVENT_TABLE();
    WX_DECLARE_INPUT_CONSUMER()
};

#endif // _WX_UNIV_CONTROL_H_
