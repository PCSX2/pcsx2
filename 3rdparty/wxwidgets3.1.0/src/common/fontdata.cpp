/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fontdata.cpp
// Author:      Julian Smart
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FONTDLG || wxUSE_FONTPICKERCTRL

#include "wx/fontdata.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxFontData, wxObject);

wxFontData::wxFontData()
{
    m_showHelp = false;
    m_allowSymbols = true;
    m_enableEffects = true;
    m_minSize = 0;
    m_maxSize = 0;

    m_encoding = wxFONTENCODING_SYSTEM;
}

wxFontData::~wxFontData()
{
}

wxFontData::wxFontData(const wxFontData& data)
    : wxObject(),
      m_fontColour(data.m_fontColour),
      m_showHelp(data.m_showHelp),
      m_allowSymbols(data.m_allowSymbols),
      m_enableEffects(data.m_enableEffects),
      m_initialFont(data.m_initialFont),
      m_chosenFont(data.m_chosenFont),
      m_minSize(data.m_minSize),
      m_maxSize(data.m_maxSize),
      m_encoding(data.m_encoding),
      m_encodingInfo(data.m_encodingInfo)
{
}

wxFontData& wxFontData::operator=(const wxFontData& data)
{
    if (&data != this)
    {
        wxObject::operator=(data);
        m_fontColour     = data.m_fontColour;
        m_showHelp       = data.m_showHelp;
        m_allowSymbols   = data.m_allowSymbols;
        m_enableEffects  = data.m_enableEffects;
        m_initialFont    = data.m_initialFont;
        m_chosenFont     = data.m_chosenFont;
        m_minSize        = data.m_minSize;
        m_maxSize        = data.m_maxSize;
        m_encoding       = data.m_encoding;
        m_encodingInfo   = data.m_encodingInfo;
    }
    return *this;
}
#endif // wxUSE_FONTDLG || wxUSE_FONTPICKERCTRL

#if wxUSE_FONTDLG

#include "wx/fontdlg.h"

wxFont wxGetFontFromUser(wxWindow *parent, const wxFont& fontInit, const wxString& caption)
{
    wxFontData data;
    if ( fontInit.IsOk() )
    {
        data.SetInitialFont(fontInit);
    }

    wxFont fontRet;
    wxFontDialog dialog(parent, data);
    if (!caption.empty())
        dialog.SetTitle(caption);
    if ( dialog.ShowModal() == wxID_OK )
    {
        fontRet = dialog.GetFontData().GetChosenFont();
    }
    //else: leave it invalid

    return fontRet;
}
#endif // wxUSE_FONTDLG
