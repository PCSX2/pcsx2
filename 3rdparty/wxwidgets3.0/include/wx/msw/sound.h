/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/sound.h
// Purpose:     wxSound class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SOUND_H_
#define _WX_SOUND_H_

#if wxUSE_SOUND

class WXDLLIMPEXP_ADV wxSound : public wxSoundBase
{
public:
    wxSound();
    wxSound(const wxString& fileName, bool isResource = false);
    wxSound(size_t size, const void* data);
    virtual ~wxSound();

    // Create from resource or file
    bool Create(const wxString& fileName, bool isResource = false);

    // Create from data
    bool Create(size_t size, const void* data);

    bool IsOk() const { return m_data != NULL; }

    static void Stop();

protected:
    void Init() { m_data = NULL; }
    bool CheckCreatedOk();
    void Free();

    virtual bool DoPlay(unsigned flags) const;

private:
    // data of this object
    class wxSoundData *m_data;

    wxDECLARE_NO_COPY_CLASS(wxSound);
};

#endif // wxUSE_SOUND

#endif // _WX_SOUND_H_

