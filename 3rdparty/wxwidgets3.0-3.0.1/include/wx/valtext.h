/////////////////////////////////////////////////////////////////////////////
// Name:        wx/valtext.h
// Purpose:     wxTextValidator class
// Author:      Julian Smart
// Modified by: Francesco Montorsi
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_VALTEXT_H_
#define _WX_VALTEXT_H_

#include "wx/defs.h"

#if wxUSE_VALIDATORS && (wxUSE_TEXTCTRL || wxUSE_COMBOBOX)

class WXDLLIMPEXP_FWD_CORE wxTextEntry;

#include "wx/validate.h"

enum wxTextValidatorStyle
{
    wxFILTER_NONE = 0x0,
    wxFILTER_EMPTY = 0x1,
    wxFILTER_ASCII = 0x2,
    wxFILTER_ALPHA = 0x4,
    wxFILTER_ALPHANUMERIC = 0x8,
    wxFILTER_DIGITS = 0x10,
    wxFILTER_NUMERIC = 0x20,
    wxFILTER_INCLUDE_LIST = 0x40,
    wxFILTER_INCLUDE_CHAR_LIST = 0x80,
    wxFILTER_EXCLUDE_LIST = 0x100,
    wxFILTER_EXCLUDE_CHAR_LIST = 0x200
};

class WXDLLIMPEXP_CORE wxTextValidator: public wxValidator
{
public:
    wxTextValidator(long style = wxFILTER_NONE, wxString *val = NULL);
    wxTextValidator(const wxTextValidator& val);

    virtual ~wxTextValidator(){}

    // Make a clone of this validator (or return NULL) - currently necessary
    // if you're passing a reference to a validator.
    // Another possibility is to always pass a pointer to a new validator
    // (so the calling code can use a copy constructor of the relevant class).
    virtual wxObject *Clone() const { return new wxTextValidator(*this); }
    bool Copy(const wxTextValidator& val);

    // Called when the value in the window must be validated.
    // This function can pop up an error message.
    virtual bool Validate(wxWindow *parent);

    // Called to transfer data to the window
    virtual bool TransferToWindow();

    // Called to transfer data from the window
    virtual bool TransferFromWindow();

    // Filter keystrokes
    void OnChar(wxKeyEvent& event);

    // ACCESSORS
    inline long GetStyle() const { return m_validatorStyle; }
    void SetStyle(long style);

    wxTextEntry *GetTextEntry();

    void SetCharIncludes(const wxString& chars);
    void SetIncludes(const wxArrayString& includes) { m_includes = includes; }
    inline wxArrayString& GetIncludes() { return m_includes; }

    void SetCharExcludes(const wxString& chars);
    void SetExcludes(const wxArrayString& excludes) { m_excludes = excludes; }
    inline wxArrayString& GetExcludes() { return m_excludes; }

    bool HasFlag(wxTextValidatorStyle style) const
        { return (m_validatorStyle & style) != 0; }

protected:

    // returns true if all characters of the given string are present in m_includes
    bool ContainsOnlyIncludedCharacters(const wxString& val) const;

    // returns true if at least one character of the given string is present in m_excludes
    bool ContainsExcludedCharacters(const wxString& val) const;

    // returns the error message if the contents of 'val' are invalid
    virtual wxString IsValid(const wxString& val) const;

protected:
    long                 m_validatorStyle;
    wxString*            m_stringValue;
    wxArrayString        m_includes;
    wxArrayString        m_excludes;

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxTextValidator);
    DECLARE_DYNAMIC_CLASS(wxTextValidator)
    DECLARE_EVENT_TABLE()
};

#endif
  // wxUSE_VALIDATORS && (wxUSE_TEXTCTRL || wxUSE_COMBOBOX)

#endif // _WX_VALTEXT_H_
