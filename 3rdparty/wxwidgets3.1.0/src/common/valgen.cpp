/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/valgen.cpp
// Purpose:     wxGenericValidator class
// Author:      Kevin Smith
// Modified by:
// Created:     Jan 22 1999
// Copyright:   (c) 1999 Kevin Smith
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_VALIDATORS

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/choice.h"
    #include "wx/combobox.h"
    #include "wx/radiobox.h"
    #include "wx/radiobut.h"
    #include "wx/checkbox.h"
    #include "wx/scrolbar.h"
    #include "wx/gauge.h"
    #include "wx/stattext.h"
    #include "wx/textctrl.h"
    #include "wx/button.h"
    #include "wx/listbox.h"
    #include "wx/slider.h"
    #include "wx/checklst.h"
#endif

#include "wx/spinctrl.h"
// #include "wx/datectrl.h" -- can't use it in this (core) file for now

#if wxUSE_SPINBTN
    #include "wx/spinbutt.h"
#endif
#if wxUSE_TOGGLEBTN
    #include "wx/tglbtn.h"
#endif
#include "wx/filename.h"

#include "wx/valgen.h"

wxIMPLEMENT_CLASS(wxGenericValidator, wxValidator);

wxGenericValidator::wxGenericValidator(bool *val)
{
    Initialize();
    m_pBool = val;
}

wxGenericValidator::wxGenericValidator(int *val)
{
    Initialize();
    m_pInt = val;
}

wxGenericValidator::wxGenericValidator(wxString *val)
{
    Initialize();
    m_pString = val;
}

wxGenericValidator::wxGenericValidator(wxArrayInt *val)
{
    Initialize();
    m_pArrayInt = val;
}

#if wxUSE_DATETIME

wxGenericValidator::wxGenericValidator(wxDateTime *val)
{
    Initialize();
    m_pDateTime = val;
}

#endif // wxUSE_DATETIME

wxGenericValidator::wxGenericValidator(wxFileName *val)
{
    Initialize();
    m_pFileName = val;
}

wxGenericValidator::wxGenericValidator(float *val)
{
    Initialize();
    m_pFloat = val;
}

wxGenericValidator::wxGenericValidator(double *val)
{
    Initialize();
    m_pDouble = val;
}

wxGenericValidator::wxGenericValidator(const wxGenericValidator& val)
    : wxValidator()
{
    Copy(val);
}

bool wxGenericValidator::Copy(const wxGenericValidator& val)
{
    wxValidator::Copy(val);

    m_pBool = val.m_pBool;
    m_pInt = val.m_pInt;
    m_pString = val.m_pString;
    m_pArrayInt = val.m_pArrayInt;
#if wxUSE_DATETIME
    m_pDateTime = val.m_pDateTime;
#endif // wxUSE_DATETIME
    m_pFileName = val.m_pFileName;
    m_pFloat = val.m_pFloat;
    m_pDouble = val.m_pDouble;

    return true;
}

// Called to transfer data to the window
bool wxGenericValidator::TransferToWindow(void)
{
    if ( !m_validatorWindow )
        return false;

    // bool controls
#if wxUSE_CHECKBOX
    if (wxDynamicCast(m_validatorWindow, wxCheckBox))
    {
        wxCheckBox* pControl = (wxCheckBox*) m_validatorWindow;
        if (m_pBool)
        {
            pControl->SetValue(*m_pBool);
            return true;
        }
    } else
#endif
#if wxUSE_RADIOBTN
    if (wxDynamicCast(m_validatorWindow, wxRadioButton))
    {
        wxRadioButton* pControl = (wxRadioButton*) m_validatorWindow;
        if (m_pBool)
        {
            pControl->SetValue(*m_pBool) ;
            return true;
        }
    } else
#endif

#if wxUSE_TOGGLEBTN
    if (wxDynamicCast(m_validatorWindow, wxToggleButton))
    {
        wxToggleButton * pControl = (wxToggleButton *) m_validatorWindow;
        if (m_pBool)
        {
            pControl->SetValue(*m_pBool);
            return true;
        }
    } else
#if (defined(__WXMAC__) || defined(__WXMSW__) || defined(__WXGTK20__)) && !defined(__WXUNIVERSAL__)
    if (wxDynamicCast(m_validatorWindow, wxBitmapToggleButton))
    {
        wxBitmapToggleButton * pControl = (wxBitmapToggleButton *) m_validatorWindow;
        if (m_pBool)
        {
            pControl->SetValue(*m_pBool);
            return true;
        }
    } else
#endif
#endif

    // int controls
#if wxUSE_GAUGE
    if (wxDynamicCast(m_validatorWindow, wxGauge))
    {
        wxGauge* pControl = (wxGauge*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetValue(*m_pInt);
            return true;
        }
    } else
#endif
#if wxUSE_RADIOBOX
    if (wxDynamicCast(m_validatorWindow, wxRadioBox))
    {
        wxRadioBox* pControl = (wxRadioBox*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetSelection(*m_pInt) ;
            return true;
        }
    } else
#endif
#if wxUSE_SCROLLBAR
    if (wxDynamicCast(m_validatorWindow, wxScrollBar))
    {
        wxScrollBar* pControl = (wxScrollBar*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetThumbPosition(*m_pInt) ;
            return true;
        }
    } else
#endif
#if wxUSE_SPINCTRL && !defined(__WXMOTIF__)
    if (wxDynamicCast(m_validatorWindow, wxSpinCtrl))
    {
        wxSpinCtrl* pControl = (wxSpinCtrl*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetValue(*m_pInt);
            return true;
        }
    } else
#endif
#if wxUSE_SPINBTN
    if (wxDynamicCast(m_validatorWindow, wxSpinButton))
    {
        wxSpinButton* pControl = (wxSpinButton*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetValue(*m_pInt) ;
            return true;
        }
    } else
#endif
#if wxUSE_SLIDER
    if (wxDynamicCast(m_validatorWindow, wxSlider))
    {
        wxSlider* pControl = (wxSlider*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetValue(*m_pInt) ;
            return true;
        }
    } else
#endif

    // date time controls
#if 0 // wxUSE_DATEPICKCTRL -- temporary fix for shared build linking
    if (wxDynamicCast(m_validatorWindow, wxDatePickerCtrl))
    {
        wxDatePickerCtrl* pControl = (wxDatePickerCtrl*) m_validatorWindow;
        if (m_pDateTime)
        {
            pControl->SetValue(*m_pDateTime) ;
            return true;
        }
    } else
#endif

    // string controls
#if wxUSE_BUTTON
    if (wxDynamicCast(m_validatorWindow, wxButton))
    {
        wxButton* pControl = (wxButton*) m_validatorWindow;
        if (m_pString)
        {
            pControl->SetLabel(*m_pString) ;
            return true;
        }
    } else
#endif
#if wxUSE_COMBOBOX
    if (wxDynamicCast(m_validatorWindow, wxComboBox))
    {
        wxComboBox* pControl = (wxComboBox*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetSelection(*m_pInt) ;
            return true;
        }
        else if (m_pString)
        {
            if (pControl->FindString(* m_pString) != wxNOT_FOUND)
            {
                pControl->SetStringSelection(* m_pString);
            }
            if ((m_validatorWindow->GetWindowStyle() & wxCB_READONLY) == 0)
            {
                pControl->SetValue(* m_pString);
            }
            return true;
        }
    } else
#endif
#if wxUSE_CHOICE
    if (wxDynamicCast(m_validatorWindow, wxChoice))
    {
        wxChoice* pControl = (wxChoice*) m_validatorWindow;
        if (m_pInt)
        {
            pControl->SetSelection(*m_pInt) ;
            return true;
        }
        else if (m_pString)
        {
            if (pControl->FindString(* m_pString) != wxNOT_FOUND)
            {
                pControl->SetStringSelection(* m_pString);
            }
            return true;
        }
    } else
#endif
#if wxUSE_STATTEXT
    if (wxDynamicCast(m_validatorWindow, wxStaticText))
    {
        wxStaticText* pControl = (wxStaticText*) m_validatorWindow;
        if (m_pString)
        {
            pControl->SetLabel(*m_pString) ;
            return true;
        }
    } else
#endif
#if wxUSE_TEXTCTRL
    if (wxDynamicCast(m_validatorWindow, wxTextCtrl))
    {
        wxTextCtrl* pControl = (wxTextCtrl*) m_validatorWindow;
        if (m_pString)
        {
            pControl->SetValue(*m_pString) ;
            return true;
        }
        else if (m_pInt)
        {
            wxString str;
            str.Printf(wxT("%d"), *m_pInt);
            pControl->SetValue(str);
            return true;
        }
        else if (m_pFileName)
        {
            pControl->SetValue(m_pFileName->GetFullPath());
            return true;
        }
        else if (m_pFloat)
        {
            pControl->SetValue(wxString::Format(wxT("%g"), *m_pFloat));
            return true;
        }
        else if (m_pDouble)
        {
            pControl->SetValue(wxString::Format(wxT("%g"), *m_pDouble));
            return true;
        }
    } else
#endif

    // array controls
#if wxUSE_CHECKLISTBOX
    // NOTE: wxCheckListBox is a wxListBox, so wxCheckListBox MUST come first:
    if (wxDynamicCast(m_validatorWindow, wxCheckListBox))
    {
        wxCheckListBox* pControl = (wxCheckListBox*) m_validatorWindow;
        if (m_pArrayInt)
        {
            // clear all selections
            size_t i,
                   count = pControl->GetCount();
            for ( i = 0 ; i < count; i++ )
                pControl->Check(i, false);

            // select each item in our array
            count = m_pArrayInt->GetCount();
            for ( i = 0 ; i < count; i++ )
                pControl->Check(m_pArrayInt->Item(i));

            return true;
        }
        else
            return false;
    } else
#endif
#if wxUSE_LISTBOX
    if (wxDynamicCast(m_validatorWindow, wxListBox))
    {
        wxListBox* pControl = (wxListBox*) m_validatorWindow;
        if (m_pArrayInt)
        {
            // clear all selections
            size_t i,
                   count = pControl->GetCount();
            for ( i = 0 ; i < count; i++ )
                pControl->Deselect(i);

            // select each item in our array
            count = m_pArrayInt->GetCount();
            for ( i = 0 ; i < count; i++ )
                pControl->SetSelection(m_pArrayInt->Item(i));

            return true;
        }
    } else
#endif
    {   // to match the last 'else' above
    }

  // unrecognized control, or bad pointer
  return false;
}

// Called to transfer data from the window
bool wxGenericValidator::TransferFromWindow(void)
{
    if ( !m_validatorWindow )
        return false;

    // BOOL CONTROLS **************************************
#if wxUSE_CHECKBOX
    if (wxDynamicCast(m_validatorWindow, wxCheckBox))
    {
        wxCheckBox* pControl = (wxCheckBox*) m_validatorWindow;
        if (m_pBool)
        {
            *m_pBool = pControl->GetValue() ;
            return true;
        }
    } else
#endif
#if wxUSE_RADIOBTN
    if (wxDynamicCast(m_validatorWindow, wxRadioButton))
    {
        wxRadioButton* pControl = (wxRadioButton*) m_validatorWindow;
        if (m_pBool)
        {
            *m_pBool = pControl->GetValue() ;
            return true;
        }
    } else
#endif
#if wxUSE_TOGGLEBTN
    if (wxDynamicCast(m_validatorWindow, wxToggleButton))
    {
        wxToggleButton *pControl = (wxToggleButton *) m_validatorWindow;
        if (m_pBool)
        {
            *m_pBool = pControl->GetValue() ;
            return true;
        }
    } else
#if (defined(__WXMAC__) || defined(__WXMSW__) || defined(__WXGTK20__)) && !defined(__WXUNIVERSAL__)
    if (wxDynamicCast(m_validatorWindow, wxBitmapToggleButton))
    {
        wxBitmapToggleButton *pControl = (wxBitmapToggleButton *) m_validatorWindow;
        if (m_pBool)
        {
            *m_pBool = pControl->GetValue() ;
            return true;
        }
    } else
#endif
#endif

    // INT CONTROLS ***************************************
#if wxUSE_GAUGE
    if (wxDynamicCast(m_validatorWindow, wxGauge))
    {
        wxGauge* pControl = (wxGauge*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetValue() ;
            return true;
        }
    } else
#endif
#if wxUSE_RADIOBOX
    if (wxDynamicCast(m_validatorWindow, wxRadioBox))
    {
        wxRadioBox* pControl = (wxRadioBox*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetSelection() ;
            return true;
        }
    } else
#endif
#if wxUSE_SCROLLBAR
    if (wxDynamicCast(m_validatorWindow, wxScrollBar))
    {
        wxScrollBar* pControl = (wxScrollBar*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetThumbPosition() ;
            return true;
        }
    } else
#endif
#if wxUSE_SPINCTRL && !defined(__WXMOTIF__)
    if (wxDynamicCast(m_validatorWindow, wxSpinCtrl))
    {
        wxSpinCtrl* pControl = (wxSpinCtrl*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt=pControl->GetValue();
            return true;
        }
    } else
#endif
#if wxUSE_SPINBTN
    if (wxDynamicCast(m_validatorWindow, wxSpinButton))
    {
        wxSpinButton* pControl = (wxSpinButton*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetValue() ;
            return true;
        }
    } else
#endif
#if wxUSE_SLIDER
    if (wxDynamicCast(m_validatorWindow, wxSlider))
    {
        wxSlider* pControl = (wxSlider*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetValue() ;
            return true;
        }
    } else
#endif

    // DATE TIME CONTROLS ************************************
#if 0 // wxUSE_DATEPICKCTRL -- temporary fix for shared build linking
    if (wxDynamicCast(m_validatorWindow, wxDatePickerCtrl))
    {
        wxDatePickerCtrl* pControl = (wxDatePickerCtrl*) m_validatorWindow;
        if (m_pDateTime)
        {
            *m_pDateTime = pControl->GetValue() ;
            return true;
        }
    } else
#endif

    // STRING CONTROLS ************************************
#if wxUSE_BUTTON
    if (wxDynamicCast(m_validatorWindow, wxButton))
    {
        wxButton* pControl = (wxButton*) m_validatorWindow;
        if (m_pString)
        {
            *m_pString = pControl->GetLabel() ;
            return true;
        }
    } else
#endif
#if wxUSE_COMBOBOX
    if (wxDynamicCast(m_validatorWindow, wxComboBox))
    {
        wxComboBox* pControl = (wxComboBox*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetSelection() ;
            return true;
        }
        else if (m_pString)
        {
            if (m_validatorWindow->GetWindowStyle() & wxCB_READONLY)
                *m_pString = pControl->GetStringSelection();
            else
                *m_pString = pControl->GetValue();
            return true;
        }
    } else
#endif
#if wxUSE_CHOICE
    if (wxDynamicCast(m_validatorWindow, wxChoice))
    {
        wxChoice* pControl = (wxChoice*) m_validatorWindow;
        if (m_pInt)
        {
            *m_pInt = pControl->GetSelection() ;
            return true;
        }
        else if (m_pString)
        {
            *m_pString = pControl->GetStringSelection();
            return true;
        }
    } else
#endif
#if wxUSE_STATTEXT
    if (wxDynamicCast(m_validatorWindow, wxStaticText))
    {
        wxStaticText* pControl = (wxStaticText*) m_validatorWindow;
        if (m_pString)
        {
            *m_pString = pControl->GetLabel() ;
            return true;
        }
    } else
#endif
#if wxUSE_TEXTCTRL
    if (wxDynamicCast(m_validatorWindow, wxTextCtrl))
    {
        wxTextCtrl* pControl = (wxTextCtrl*) m_validatorWindow;
        if (m_pString)
        {
            *m_pString = pControl->GetValue() ;
            return true;
        }
        else if (m_pInt)
        {
            *m_pInt = wxAtoi(pControl->GetValue());
            return true;
        }
        else if (m_pFileName)
        {
            m_pFileName->Assign(pControl->GetValue());
            return true;
        }
        else if (m_pFloat)
        {
            *m_pFloat = (float)wxAtof(pControl->GetValue());
            return true;
        }
        else if (m_pDouble)
        {
            *m_pDouble = wxAtof(pControl->GetValue());
            return true;
        }
    } else
#endif

    // ARRAY CONTROLS *************************************
#if wxUSE_CHECKLISTBOX
    // NOTE: wxCheckListBox isa wxListBox, so wxCheckListBox MUST come first:
    if (wxDynamicCast(m_validatorWindow, wxCheckListBox))
    {
        wxCheckListBox* pControl = (wxCheckListBox*) m_validatorWindow;
        if (m_pArrayInt)
        {
            // clear our array
            m_pArrayInt->Clear();

            // add each selected item to our array
            size_t i,
                   count = pControl->GetCount();
            for ( i = 0; i < count; i++ )
            {
                if (pControl->IsChecked(i))
                    m_pArrayInt->Add(i);
            }

            return true;
        }
        else
            return false;
    } else
#endif
#if wxUSE_LISTBOX
    if (wxDynamicCast(m_validatorWindow, wxListBox))
    {
        wxListBox* pControl = (wxListBox*) m_validatorWindow;
        if (m_pArrayInt)
        {
            // clear our array
            m_pArrayInt->Clear();

            // add each selected item to our array
            size_t i,
                   count = pControl->GetCount();
            for ( i = 0; i < count; i++ )
            {
                if (pControl->IsSelected(i))
                    m_pArrayInt->Add(i);
            }

            return true;
        }
    } else
#endif

    // unrecognized control, or bad pointer
        return false;

    return false;
}

/*
  Called by constructors to initialize ALL data members
*/
void wxGenericValidator::Initialize()
{
    m_pBool = NULL;
    m_pInt = NULL;
    m_pString = NULL;
    m_pArrayInt = NULL;
#if wxUSE_DATETIME
    m_pDateTime = NULL;
#endif // wxUSE_DATETIME
    m_pFileName = NULL;
    m_pFloat = NULL;
    m_pDouble = NULL;
}

#endif // wxUSE_VALIDATORS
