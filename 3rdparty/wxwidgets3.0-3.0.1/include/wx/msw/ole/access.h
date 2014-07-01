///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ole/access.h
// Purpose:     declaration of the wxAccessible class
// Author:      Julian Smart
// Modified by:
// Created:     2003-02-12
// Copyright:   (c) 2003 Julian Smart
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _WX_ACCESS_H_
#define   _WX_ACCESS_H_

#if wxUSE_ACCESSIBILITY

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class wxIAccessible;
class WXDLLIMPEXP_FWD_CORE wxWindow;

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxAccessible implements accessibility behaviour.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxAccessible : public wxAccessibleBase
{
public:
    wxAccessible(wxWindow *win = NULL);
    virtual ~wxAccessible();

// Overridables

// Accessors

    // Returns the wxIAccessible pointer
    wxIAccessible* GetIAccessible() { return m_pIAccessible; }

    // Returns the IAccessible standard interface pointer
    void* GetIAccessibleStd() ;

// Operations

    // Sends an event when something changes in an accessible object.
    static void NotifyEvent(int eventType, wxWindow* window, wxAccObject objectType,
                            int objectId);

protected:
    void Init();

private:
    wxIAccessible * m_pIAccessible;  // the pointer to COM interface
    void*           m_pIAccessibleStd;  // the pointer to the standard COM interface,
                                        // for default processing

    wxDECLARE_NO_COPY_CLASS(wxAccessible);
};

#endif  //wxUSE_ACCESSIBILITY

#endif  //_WX_ACCESS_H_

