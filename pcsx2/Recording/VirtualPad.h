#pragma once
#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <wx/spinctrl.h>

#include "Recording/PadData.h"

class VirtualPad : public wxFrame
{
public:
	VirtualPad(wxWindow *parent, int controllerPort);

	bool Show(bool show = true) override;

protected:
	int port;

	wxToggleButton *buttons[16];
	wxSpinCtrl *buttonsPressure[16];
	wxButton *reset;

	wxSlider *sticks[4];
	wxSpinCtrl *sticksText[4];

protected:
	void OnClose(wxCloseEvent &event);

	void OnClick(wxCommandEvent &event);
	void OnResetButton(wxCommandEvent &event);
	void OnPressureCtrlChange(wxSpinEvent &event);
	void OnTextCtrlChange(wxSpinEvent &event);
	void OnSliderMove(wxCommandEvent &event);

	int getButtonIdFromPressure(int pressureCtrlId);

	bool isPressureSensitive(int buttonId);

	wxDECLARE_EVENT_TABLE();
};
