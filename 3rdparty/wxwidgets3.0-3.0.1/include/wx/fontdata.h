/////////////////////////////////////////////////////////////////////////////
// Name:        wx/fontdata.h
// Author:      Julian Smart
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONTDATA_H_
#define _WX_FONTDATA_H_

#include "wx/font.h"
#include "wx/colour.h"
#include "wx/encinfo.h"

class WXDLLIMPEXP_CORE wxFontData : public wxObject
{
public:
    wxFontData();
    virtual ~wxFontData();

    wxFontData(const wxFontData& data);
    wxFontData& operator=(const wxFontData& data);

    void SetAllowSymbols(bool flag) { m_allowSymbols = flag; }
    bool GetAllowSymbols() const { return m_allowSymbols; }

    void SetColour(const wxColour& colour) { m_fontColour = colour; }
    const wxColour& GetColour() const { return m_fontColour; }

    void SetShowHelp(bool flag) { m_showHelp = flag; }
    bool GetShowHelp() const { return m_showHelp; }

    void EnableEffects(bool flag) { m_enableEffects = flag; }
    bool GetEnableEffects() const { return m_enableEffects; }

    void SetInitialFont(const wxFont& font) { m_initialFont = font; }
    wxFont GetInitialFont() const { return m_initialFont; }

    void SetChosenFont(const wxFont& font) { m_chosenFont = font; }
    wxFont GetChosenFont() const { return m_chosenFont; }

    void SetRange(int minRange, int maxRange) { m_minSize = minRange; m_maxSize = maxRange; }

    // encoding info is split into 2 parts: the logical wxWin encoding
    // (wxFontEncoding) and a structure containing the native parameters for
    // it (wxNativeEncodingInfo)
    wxFontEncoding GetEncoding() const { return m_encoding; }
    void SetEncoding(wxFontEncoding encoding) { m_encoding = encoding; }

    wxNativeEncodingInfo& EncodingInfo() { return m_encodingInfo; }


    // public for backwards compatibility only: don't use directly
    wxColour        m_fontColour;
    bool            m_showHelp;
    bool            m_allowSymbols;
    bool            m_enableEffects;
    wxFont          m_initialFont;
    wxFont          m_chosenFont;
    int             m_minSize;
    int             m_maxSize;

private:
    wxFontEncoding       m_encoding;
    wxNativeEncodingInfo m_encodingInfo;

    DECLARE_DYNAMIC_CLASS(wxFontData)
};

#endif // _WX_FONTDATA_H_
