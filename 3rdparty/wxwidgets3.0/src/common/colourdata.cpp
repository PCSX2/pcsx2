/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/colourdata.cpp
// Author:      Julian Smart
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_COLOURDLG || wxUSE_COLOURPICKERCTRL

#include "wx/colourdata.h"
#include "wx/tokenzr.h"

// ----------------------------------------------------------------------------
// wxColourData
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxColourData, wxObject)

wxColourData::wxColourData()
{
    m_chooseFull = false;
    m_dataColour.Set(0,0,0);
    // m_custColours are wxNullColours initially
}

wxColourData::wxColourData(const wxColourData& data)
            : wxObject()
{
    (*this) = data;
}

wxColourData::~wxColourData()
{
}

void wxColourData::SetCustomColour(int i, const wxColour& colour)
{
    wxCHECK_RET( i >= 0 && i < NUM_CUSTOM, wxT("custom colour index out of range") );

    m_custColours[i] = colour;
}

wxColour wxColourData::GetCustomColour(int i) const
{
    wxCHECK_MSG( i >= 0 && i < NUM_CUSTOM, wxColour(0,0,0),
                 wxT("custom colour index out of range") );

    return m_custColours[i];
}

wxColourData& wxColourData::operator=(const wxColourData& data)
{
    for ( int i = 0; i < NUM_CUSTOM; i++)
        m_custColours[i] = data.m_custColours[i];

    m_dataColour = data.m_dataColour;
    m_chooseFull = data.m_chooseFull;

    return *this;
}

// ----------------------------------------------------------------------------
// [de]serialization
// ----------------------------------------------------------------------------

// separator used between different fields
static const char wxCOL_DATA_SEP = ',';

wxString wxColourData::ToString() const
{
    wxString str(m_chooseFull ? '1' : '0');

    for ( int i = 0; i < NUM_CUSTOM; i++ )
    {
        str += wxCOL_DATA_SEP;

        const wxColour& clr = m_custColours[i];
        if ( clr.IsOk() )
            str += clr.GetAsString(wxC2S_HTML_SYNTAX);
    }

    return str;
}

bool wxColourData::FromString(const wxString& str)
{
    wxStringTokenizer tokenizer(str, wxCOL_DATA_SEP);
    wxString token = tokenizer.GetNextToken();
    m_chooseFull = token == '1';
    bool success = m_chooseFull || token == '0';
    for (int i = 0; success && i < NUM_CUSTOM; i++)
    {
        token = tokenizer.GetNextToken();
        if (token.empty())
            m_custColours[i] = wxColour();
        else
            success = m_custColours[i].Set(token);
    }
    return success;
}

#if wxUSE_COLOURDLG

#include "wx/colordlg.h"

wxColour wxGetColourFromUser(wxWindow *parent,
                             const wxColour& colInit,
                             const wxString& caption,
                             wxColourData *ptrData)
{
    // contains serialized representation of wxColourData used the last time
    // the dialog was shown: we want to reuse it the next time in order to show
    // the same custom colours to the user (and we can't just have static
    // wxColourData itself because it's a GUI object and so should be destroyed
    // before GUI shutdown and doing it during static cleanup is too late)
    static wxString s_strColourData;

    wxColourData data;
    if ( !ptrData )
    {
        ptrData = &data;
        if ( !s_strColourData.empty() )
        {
            if ( !data.FromString(s_strColourData) )
            {
                wxFAIL_MSG( "bug in wxColourData::FromString()?" );
            }

#ifdef __WXMSW__
            // we don't get back the "choose full" flag value from the native
            // dialog and so we can't preserve it between runs, so we decide to
            // always use it as it seems better than not using it (user can
            // just ignore the extra controls in the dialog but having to click
            // a button each time to show them would be very annoying
            data.SetChooseFull(true);
#endif // __WXMSW__
        }
    }

    if ( colInit.IsOk() )
    {
        ptrData->SetColour(colInit);
    }

    wxColour colRet;
    wxColourDialog dialog(parent, ptrData);
    if (!caption.empty())
        dialog.SetTitle(caption);
    if ( dialog.ShowModal() == wxID_OK )
    {
        *ptrData = dialog.GetColourData();
        colRet = ptrData->GetColour();
        s_strColourData = ptrData->ToString();
    }
    //else: leave colRet invalid

    return colRet;
}

#endif // wxUSE_COLOURDLG
#endif // wxUSE_COLOURDLG || wxUSE_COLOURPICKERCTRL
