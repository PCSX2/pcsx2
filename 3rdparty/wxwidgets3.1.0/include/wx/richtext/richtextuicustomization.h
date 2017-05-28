/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextuicustomization.h
// Purpose:     UI customization base class for wxRTC
// Author:      Julian Smart
// Modified by:
// Created:     2010-11-14
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RICHTEXTUICUSTOMIZATION_H_
#define _WX_RICHTEXTUICUSTOMIZATION_H_

#if wxUSE_RICHTEXT

#include "wx/window.h"

/**
    @class wxRichTextUICustomization
    The base class for functionality to plug in to various rich text control dialogs,
    currently allowing the application to respond to Help button clicks without the
    need to derive new dialog classes.

    The application will typically have calls like this in its initialisation:

    wxRichTextFormattingDialog::GetHelpInfo().SetHelpId(ID_HELP_FORMATTINGDIALOG);
    wxRichTextFormattingDialog::GetHelpInfo().SetUICustomization(& wxGetApp().GetRichTextUICustomization());
    wxRichTextBordersPage::GetHelpInfo().SetHelpId(ID_HELP_BORDERSPAGE);
    
    Only the wxRichTextFormattingDialog class needs to have its customization object and help id set,
    though the application set them for individual pages if it wants.
 **/

class WXDLLIMPEXP_RICHTEXT wxRichTextUICustomization
{
public:
    wxRichTextUICustomization() {}
    virtual ~wxRichTextUICustomization() {}

    /// Show the help given the current active window, and a help topic id.
    virtual bool ShowHelp(wxWindow* win, long id) = 0;
};

/**
    @class wxRichTextHelpInfo
    This class is used as a static member of dialogs, to store the help topic for the dialog
    and also the customization object that will allow help to be shown appropriately for the application.
 **/
  
class WXDLLIMPEXP_RICHTEXT wxRichTextHelpInfo
{
public:
    wxRichTextHelpInfo()
    {
        m_helpTopic = -1;
        m_uiCustomization = NULL;        
    }
    virtual ~wxRichTextHelpInfo() {}
        
    virtual bool ShowHelp(wxWindow* win)
    {
        if ( !m_uiCustomization || m_helpTopic == -1 )
            return false;

        return m_uiCustomization->ShowHelp(win, m_helpTopic);
    }

    /// Get the help topic identifier.
    long GetHelpId() const { return m_helpTopic; }

    /// Set the help topic identifier.
    void SetHelpId(long id) { m_helpTopic = id; }

    /// Get the UI customization object.
    wxRichTextUICustomization* GetUICustomization() const { return m_uiCustomization; }

    /// Set the UI customization object.
    void SetUICustomization(wxRichTextUICustomization* customization) { m_uiCustomization = customization; }

    /// Is there a valid help topic id?
    bool HasHelpId() const { return m_helpTopic != -1; }

    /// Is there a valid customization object?
    bool HasUICustomization() const { return m_uiCustomization != NULL; }

protected:
    wxRichTextUICustomization*  m_uiCustomization;
    long                        m_helpTopic;
};

/// Add this to the base class of dialogs

#define DECLARE_BASE_CLASS_HELP_PROVISION() \
    virtual long GetHelpId() const = 0; \
    virtual wxRichTextUICustomization* GetUICustomization() const = 0; \
    virtual bool ShowHelp(wxWindow* win) = 0;

/// A macro to make it easy to add help topic provision and UI customization
/// to a class. Optionally, add virtual functions to a base class
/// using DECLARE_BASE_CLASS_HELP_PROVISION. This means that the formatting dialog
/// can obtain help topics from its individual pages without needing
/// to know in advance what page classes are being used, allowing for extension
/// of the formatting dialog.

#define DECLARE_HELP_PROVISION() \
    virtual long GetHelpId() const { return sm_helpInfo.GetHelpId(); } \
    virtual void SetHelpId(long id) { sm_helpInfo.SetHelpId(id); } \
    virtual wxRichTextUICustomization* GetUICustomization() const { return sm_helpInfo.GetUICustomization(); } \
    virtual void SetUICustomization(wxRichTextUICustomization* customization) { sm_helpInfo.SetUICustomization(customization); } \
    virtual bool ShowHelp(wxWindow* win) { return sm_helpInfo.ShowHelp(win); } \
public: \
    static wxRichTextHelpInfo& GetHelpInfo() { return sm_helpInfo; }\
protected: \
    static wxRichTextHelpInfo sm_helpInfo; \
public:

/// Add this to the implementation file for each dialog that needs help provision.

#define IMPLEMENT_HELP_PROVISION(theClass) \
    wxRichTextHelpInfo theClass::sm_helpInfo;

#endif
    // wxUSE_RICHTEXT

#endif
    // _WX_RICHTEXTUICUSTOMIZATION_H_
