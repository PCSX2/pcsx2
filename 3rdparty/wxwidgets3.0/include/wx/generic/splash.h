/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/splash.h
// Purpose:     Splash screen class
// Author:      Julian Smart
// Modified by:
// Created:     28/6/2000
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SPLASH_H_
#define _WX_SPLASH_H_

#include "wx/bitmap.h"
#include "wx/eventfilter.h"
#include "wx/frame.h"
#include "wx/timer.h"


/*
 * A window for displaying a splash screen
 */

#define wxSPLASH_CENTRE_ON_PARENT   0x01
#define wxSPLASH_CENTRE_ON_SCREEN   0x02
#define wxSPLASH_NO_CENTRE          0x00
#define wxSPLASH_TIMEOUT            0x04
#define wxSPLASH_NO_TIMEOUT         0x00

class WXDLLIMPEXP_FWD_ADV wxSplashScreenWindow;

/*
 * wxSplashScreen
 */

class WXDLLIMPEXP_ADV wxSplashScreen: public wxFrame,
                                      public wxEventFilter
{
public:
    // for RTTI macros only
    wxSplashScreen() { Init(); }
    wxSplashScreen(const wxBitmap& bitmap, long splashStyle, int milliseconds,
                   wxWindow* parent, wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxSIMPLE_BORDER|wxFRAME_NO_TASKBAR|wxSTAY_ON_TOP);
    virtual ~wxSplashScreen();

    void OnCloseWindow(wxCloseEvent& event);
    void OnNotify(wxTimerEvent& event);

    long GetSplashStyle() const { return m_splashStyle; }
    wxSplashScreenWindow* GetSplashWindow() const { return m_window; }
    int GetTimeout() const { return m_milliseconds; }

    // Override wxEventFilter method to hide splash screen on any user input.
    virtual int FilterEvent(wxEvent& event);

protected:
    // Common part of all ctors.
    void Init();

    wxSplashScreenWindow*   m_window;
    long                    m_splashStyle;
    int                     m_milliseconds;
    wxTimer                 m_timer;

    DECLARE_DYNAMIC_CLASS(wxSplashScreen)
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxSplashScreen);
};

/*
 * wxSplashScreenWindow
 */

class WXDLLIMPEXP_ADV wxSplashScreenWindow: public wxWindow
{
public:
    wxSplashScreenWindow(const wxBitmap& bitmap, wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxNO_BORDER);

    void OnPaint(wxPaintEvent& event);
    void OnEraseBackground(wxEraseEvent& event);

    void SetBitmap(const wxBitmap& bitmap) { m_bitmap = bitmap; }
    wxBitmap& GetBitmap() { return m_bitmap; }

protected:
    wxBitmap    m_bitmap;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxSplashScreenWindow);
};


#endif
    // _WX_SPLASH_H_
