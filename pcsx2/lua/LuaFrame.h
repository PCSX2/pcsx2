#pragma once
#ifndef __LUA_FRAME_H__
#define __LUA_FRAME_H__

//#include "AppCommon.h"
#include <wx/wx.h>


class LuaFrame : public wxFrame
{
public:
	LuaFrame(wxWindow * parent);

public:
	void drawState(wxString str);
	void pushRunState();
	void pushStopState();

private:
	wxStatusBar* statusbar;
	wxButton * stopButton;
	wxButton * startButton;
	wxButton * restartButton;
	
private:
	void OnClose(wxCloseEvent& evt);

	void OnOpenFile(wxCommandEvent& event);
	void OnStop(wxCommandEvent& event);
	void OnRun(wxCommandEvent& event);
	void OnRestart(wxCommandEvent& event);

	wxDECLARE_EVENT_TABLE();
};

#endif
