#pragma once
#ifndef __KEY_EDITOR_H__
#define __KEY_EDITOR_H__

#include <wx/wx.h>
#include <wx/listctrl.h>

#include "PadData.h"

class InputRecordingEditor : public wxFrame
{
public:
	InputRecordingEditor(wxWindow * parent);

public:
	void FrameUpdate();
	
private:
	
	void DrawKeyFrameList(long frame);
	void DrawKeyButtonCheck();

private:

	long frameListStartFrame = 0;

private:
	wxListBox * frameList;

	wxStatusBar* statusbar;
	
	wxTextCtrl* keyTextView;
	wxTextCtrl* keyTextEdit;

	wxTextCtrl* frameTextFoeMove;
	wxTextCtrl*	analogKeyText[PadDataAnalogKeysSize];

	wxCheckListBox* keyCheckList1;

private:
	void OnClose(wxCloseEvent& evt);

	void OnMenuAuthor(wxCommandEvent& event);
	void OnMenuInputRecordingInfo(wxCommandEvent& event);

	void OnBtnUpdate(wxCommandEvent& event);
	void OnBtnDelete(wxCommandEvent& event);
	void OnBtnInsert(wxCommandEvent& event);
	void OnBtnCopy(wxCommandEvent& event);

	void OnBtnDrawFrame(wxCommandEvent& event);
	void OnBtnDrawNowFrame(wxCommandEvent& event);

	void OnListBox(wxCommandEvent& event);
	void OnCheckList_NormalKey1(wxCommandEvent& event);

	void OnText_Edit(wxCommandEvent& event);

	void _OnText_Analog(int num);
	void OnText_Analog1(wxCommandEvent& event);
	void OnText_Analog2(wxCommandEvent& event);
	void OnText_Analog3(wxCommandEvent& event);
	void OnText_Analog4(wxCommandEvent& event);


	wxDECLARE_EVENT_TABLE();
};

#endif
