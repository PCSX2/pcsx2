#pragma once

#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <wx/spinctrl.h>

#include "Recording/PadData.h"

class VirtualPad : public wxFrame
{
public:
	VirtualPad(wxWindow* parent, wxWindowID id, const wxString& title, int controllerPort, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE);

	bool Show(bool show = true) override;

private:
	void SetProperties();
	void DoLayout();

	int controllerPort;

	wxToggleButton* l2Button;
	wxSpinCtrl* l2ButtonPressure;
	wxToggleButton* l1Button;
	wxSpinCtrl* l1ButtonPressure;
	wxToggleButton* r2Button;
	wxSpinCtrl* r2ButtonPressure;
	wxToggleButton* r1Button;
	wxSpinCtrl* r1ButtonPressure;
	wxToggleButton* upButton;
	wxSpinCtrl* upButtonPressure;
	wxToggleButton* leftButton;
	wxSpinCtrl* leftButtonPressure;
	wxToggleButton* rightButton;
	wxSpinCtrl* rightButtonPressure;
	wxToggleButton* downButton;
	wxSpinCtrl* downButtonPressure;
	wxToggleButton* startButton;
	wxToggleButton* selectButton;
	wxToggleButton* triangleButton;
	wxSpinCtrl* triangleButtonPressure;
	wxToggleButton* squareButton;
	wxSpinCtrl* squareButtonPressure;
	wxToggleButton* circleButton;
	wxSpinCtrl* circleButtonPressure;
	wxToggleButton* crossButton;
	wxSpinCtrl* crossButtonPressure;
	wxSlider* leftAnalogXVal;
	wxSpinCtrl* leftAnalogXValPrecise;
	wxToggleButton* l3Button;
	wxSlider* leftAnalogYVal;
	wxSpinCtrl* leftAnalogYValPrecise;
	wxSlider* rightAnalogXVal;
	wxSpinCtrl* rightAnalogXValPrecise;
	wxToggleButton* r3Button;
	wxSlider* rightAnalogYVal;
	wxSpinCtrl* rightAnalogYValPrecise;

	wxToggleButton* buttons[16];
	int buttonsLength = 16;
	wxSpinCtrl* buttonsPressure[12];
	int buttonsPressureLength = 12;
	wxSlider* analogSliders[4];
	int analogSlidersLength = 4;
	wxSpinCtrl* analogVals[4];
	int analogValsLength = 4;

	void OnClose(wxCloseEvent &event);
	void OnButtonPress(wxCommandEvent &event);
	void OnPressureChange(wxSpinEvent &event);
	void OnAnalogValChange(wxSpinEvent &event);
	void OnAnalogSliderChange(wxCommandEvent &event);
	// TODO - reset button

protected:
	wxDECLARE_EVENT_TABLE();
};
