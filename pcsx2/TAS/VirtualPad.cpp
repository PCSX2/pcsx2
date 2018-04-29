#include "PrecompiledHeader.h"

#include "Common.h"

#include "TAS/VirtualPad.h"
#include "TAS/TASInputManager.h"

enum
{
	// Normal
	ID_UP = 1,
	ID_RIGHT,
	ID_LEFT,
	ID_DOWN,
	ID_SELECT,
	ID_START,
	ID_X,
	ID_CIRCLE,
	ID_SQUARE,
	ID_TRIANGLE,
	ID_L1,
	ID_L2,
	ID_L3,
	ID_R1,
	ID_R2,
	ID_R3,
	// Button Pressure
	ID_UP_PRESSURE,
	ID_RIGHT_PRESSURE,
	ID_LEFT_PRESSURE,
	ID_DOWN_PRESSURE,
	ID_X_PRESSURE,
	ID_CIRCLE_PRESSURE,
	ID_SQUARE_PRESSURE,
	ID_TRIANGLE_PRESSURE,
	ID_L1_PRESSURE,
	ID_L2_PRESSURE,
	ID_R1_PRESSURE,
	ID_R2_PRESSURE,
	// Analog (sliders)
	ID_L_UPDOWN,
	ID_L_RIGHTLEFT,
	ID_R_UPDOWN,
	ID_R_RIGHTLEFT,
	// Analog (TextCtrl)
	ID_L_UPDOWN_TEXT,
	ID_L_RIGHTLEFT_TEXT,
	ID_R_UPDOWN_TEXT,
	ID_R_RIGHTLEFT_TEXT,
	// Reset
	ID_RESET
};

wxBEGIN_EVENT_TABLE(VirtualPad, wxFrame)
EVT_CLOSE(VirtualPad::OnClose)
wxEND_EVENT_TABLE()

VirtualPad::VirtualPad(wxWindow * parent, int controllerPort)
	: wxFrame(parent, wxID_ANY, wxString::Format("Virtual Pad %d", controllerPort), wxDefaultPosition, wxSize(600, 520), wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX)),
	port(controllerPort)
{
	// Global
	wxPanel *panel = new wxPanel(this, wxID_ANY);
	int x = 10, y = 2;
	int w = 50, h = 35;
	int space = 5;

	// Left triggers
	buttons[ID_L2 - 1] = new wxToggleButton(panel, ID_L2, L"L2", wxPoint(x, y), wxSize(w, h));
	buttonsPressure[ID_L2 - 1] = new wxSpinCtrl(panel, ID_L2_PRESSURE, "255", wxPoint(x + w + space, y), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_L1 - 1] = new wxToggleButton(panel, ID_L1, L"L1", wxPoint(x, y + h + space), wxSize(w, h));
	buttonsPressure[ID_L1 - 1] = new wxSpinCtrl(panel, ID_L1_PRESSURE, "255", wxPoint(x + w + space, y + h + space), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);

	// Cross Key
	x = 15;
	y = 100;
	buttons[ID_UP - 1] = new wxToggleButton(panel, ID_UP, _("Up"), wxPoint(x + w + space, y), wxSize(w, h));
	buttonsPressure[ID_UP - 1] = new wxSpinCtrl(panel, ID_UP_PRESSURE, "255", wxPoint(x + 2 * (w + space), y), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_DOWN - 1] = new wxToggleButton(panel, ID_DOWN, _("Down"), wxPoint(x + w + space, y + 2 * h + 2 * space), wxSize(w, h));
	buttonsPressure[ID_DOWN - 1] = new wxSpinCtrl(panel, ID_DOWN_PRESSURE, "255", wxPoint(x + 2 * (w + space), y + 2 * h + 2 * space), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_LEFT - 1] = new wxToggleButton(panel, ID_LEFT, _("Left"), wxPoint(x, y + h + 5), wxSize(w, h));
	buttonsPressure[ID_LEFT - 1] = new wxSpinCtrl(panel, ID_LEFT_PRESSURE, "255", wxPoint(x + w + space, y + h + 5), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_RIGHT - 1] = new wxToggleButton(panel, ID_RIGHT, _("Right"), wxPoint(x + 2 * w + 2 * space, y + h + space), wxSize(w, h));
	buttonsPressure[ID_RIGHT - 1] = new wxSpinCtrl(panel, ID_RIGHT_PRESSURE, "255", wxPoint(x + 3 * (w + space), y + h + space), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);

	// Right triggers
	x = 475;
	y = 2;
	buttons[ID_R2 - 1] = new wxToggleButton(panel, ID_R2, L"R2", wxPoint(x, y), wxSize(w, h));
	buttonsPressure[ID_R2 - 1] = new wxSpinCtrl(panel, ID_R2_PRESSURE, "255", wxPoint(x + w + space, y), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_R1 - 1] = new wxToggleButton(panel, ID_R1, L"R1", wxPoint(x, y + h + space), wxSize(w, h));
	buttonsPressure[ID_R1 - 1] = new wxSpinCtrl(panel, ID_R1_PRESSURE, "255", wxPoint(x + w + space, y + h + space), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);

	// Action buttons
	x = 365;
	y = 100;
	buttons[ID_TRIANGLE - 1] = new wxToggleButton(panel, ID_TRIANGLE, _("Triangle"), wxPoint(x + w + space, y), wxSize(w, h));
	buttonsPressure[ID_TRIANGLE - 1] = new wxSpinCtrl(panel, ID_TRIANGLE_PRESSURE, "255", wxPoint(x + 2 * (w + space), y), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_X - 1] = new wxToggleButton(panel, ID_X, _("X"), wxPoint(x + w + space, y + 2 * h + 2 * space), wxSize(w, h));
	buttonsPressure[ID_X - 1] = new wxSpinCtrl(panel, ID_X_PRESSURE, "255", wxPoint(x + 2 * (w + space), y + 2 * h + 2 * space), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_SQUARE - 1] = new wxToggleButton(panel, ID_SQUARE, _("Square"), wxPoint(x, y + h + 5), wxSize(w, h));
	buttonsPressure[ID_SQUARE - 1] = new wxSpinCtrl(panel, ID_SQUARE_PRESSURE, "255", wxPoint(x + w + space, y + h + 5), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);
	buttons[ID_CIRCLE - 1] = new wxToggleButton(panel, ID_CIRCLE, _("Circle"), wxPoint(x + 2 * w + 2 * space, y + h + space), wxSize(w, h));
	buttonsPressure[ID_CIRCLE - 1] = new wxSpinCtrl(panel, ID_CIRCLE_PRESSURE, "255", wxPoint(x + 3 * (w + space), y + h + space), wxSize(w, h),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 255);

	// L3, R3
	y = 20;
	buttons[ID_L3 - 1] = new wxToggleButton(panel, ID_L3, L"L3", wxPoint(150, y), wxSize(w, h));
	buttons[ID_R3 - 1] = new wxToggleButton(panel, ID_R3, L"R3", wxPoint(350, y), wxSize(w, h));

	// Start, select
	buttons[ID_SELECT - 1] = new wxToggleButton(panel, ID_SELECT, _("Select"), wxPoint(150, y + h + space), wxSize(w, h));
	buttons[ID_START - 1] = new wxToggleButton(panel, ID_START, _("Start"), wxPoint(350, y + h + space), wxSize(w, h));

	// Left analog
	x = 5;
	y = 220;
	w = 200;
	h = 30;
	space = 3;
	sticks[0] = new wxSlider(panel, ID_L_UPDOWN, 127, 0, 255, wxPoint(x + w + space, y), wxSize(h, w),
		wxSL_VERTICAL | wxSL_INVERSE | wxSL_LEFT);
	sticks[1] = new wxSlider(panel, ID_L_RIGHTLEFT, 127, 0, 255, wxPoint(x, y + w + space), wxSize(w, h), wxSL_HORIZONTAL);

	sticksText[0] = new wxSpinCtrl(panel, ID_L_UPDOWN_TEXT, L"127", wxPoint(x + w + space + 30, y + w / 2 - 10), wxSize(55, 20),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 127);
	sticksText[1] = new wxSpinCtrl(panel, ID_L_RIGHTLEFT_TEXT, L"127", wxPoint(x + w / 2 - 10, y + w + space + 30), wxSize(55, 20),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 127);

	// Right analog
	x = 275;
	sticks[2] = new wxSlider(panel, ID_R_UPDOWN, 127, 0, 255, wxPoint(x + w + space, y), wxSize(h, w),
		wxSL_VERTICAL | wxSL_INVERSE | wxSL_LEFT);
	sticks[3] = new wxSlider(panel, ID_R_RIGHTLEFT, 127, 0, 255, wxPoint(x, y + w + space), wxSize(w, h), wxSL_HORIZONTAL);

	sticksText[2] = new wxSpinCtrl(panel, ID_R_UPDOWN_TEXT, L"127", wxPoint(x + w + space + 30, y + w / 2 - 10), wxSize(55, 20),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 127);
	sticksText[3] = new wxSpinCtrl(panel, ID_R_RIGHTLEFT_TEXT, L"127", wxPoint(x + w / 2 - 10, y + w + space + 30), wxSize(55, 20),
		wxSP_ARROW_KEYS | wxALIGN_LEFT, 0, 255, 127);

	// Reset
	reset = new wxButton(panel, ID_RESET, _("Reset"), wxPoint(515, 430), wxSize(50, 35));
	Bind(wxEVT_BUTTON, &VirtualPad::OnResetButton, this, ID_RESET);

	// Handling buttons (normal keys)
	for (int i = ID_UP; i <= 16; i++)
		Bind(wxEVT_TOGGLEBUTTON, &VirtualPad::OnClick, this, i);

	// Handling changes in pressure sensitivity
	for (int i = ID_UP_PRESSURE; i <= ID_R2_PRESSURE; i++)
		Bind(wxEVT_SPINCTRL, &VirtualPad::OnPressureCtrlChange, this, i);

	// Handling TextCtrl changes (analog keys)
	for (int i = ID_L_UPDOWN_TEXT; i <= ID_R_RIGHTLEFT_TEXT; i++)
		Bind(wxEVT_SPINCTRL, &VirtualPad::OnTextCtrlChange, this, i);

	// Handling Slider changes (analog keys)
	for (int i = ID_L_UPDOWN; i <= ID_R_RIGHTLEFT; i++)
		Bind(wxEVT_SLIDER, &VirtualPad::OnSliderMove, this, i);
}

bool VirtualPad::Show(bool show)
{
	if (!wxFrame::Show(show))
		return false;
	if (show)
		g_TASInput.SetVirtualPadReading(port, true);
	return true;
}

void VirtualPad::OnClose(wxCloseEvent & event)
{
	g_TASInput.SetVirtualPadReading(port, false);
	Hide();
}

void VirtualPad::OnClick(wxCommandEvent & event)
{
	if (0 < event.GetId() && event.GetId() <= 16) {
		int id = event.GetId() - ID_UP;
		u8 pressure = 0;
		if (event.IsChecked()) {
			// for a button to be pressed, it just has to be > 0
			pressure = 255;
			// skip non-pressure sensitive buttons
			if (isPressureSensitive(id + ID_UP)) {
				pressure = buttonsPressure[id]->GetValue(); // null ptrs on buttons with pressure sensitivity (should just skip them or add them back because i skip them later)
				if (pressure > 255)
					pressure = 255;
				else if (pressure < 0)
					pressure = 0;
			}
		}
		g_TASInput.SetButtonState(port, PadDataNormalKeys[id], pressure);
	}
	else
		tasConLog("[VirtualPad]: Unknown toggle button pressed.\n");
}

// TODO TAS - should probably implement an OnRelease (if wxwidgets has that?)

void VirtualPad::OnResetButton(wxCommandEvent & event)
{
	// Normal buttons
	for (int i = 0; i < 16; i++)
	{
		buttons[i]->SetValue(false);
		g_TASInput.SetButtonState(port, PadDataNormalKeys[i], 0);
	}
	// Pressure sensitivity
	for (int i = 0; i < 12; i++) {
		buttonsPressure[i]->SetValue(255);
	}

	// Analog
	for (int i = 0; i < 4; i++)
	{
		sticks[i]->SetValue(127);
		sticksText[i]->SetValue(127);
		g_TASInput.UpdateAnalog(port, PadDataAnalogKeys[i], 127);
	}
}

void VirtualPad::OnPressureCtrlChange(wxSpinEvent & event)
{
	if (ID_UP_PRESSURE <= event.GetId() && event.GetId() <= ID_R2_PRESSURE)
	{
		int id = getButtonIdFromPressure(event.GetId()) - ID_UP;
		if (id == -1)
			return;
		u8 pressure = 0;
		if (event.IsChecked()) {
			// this event is only fired on buttons with a 
			// pressure spinctrl, so no validation needed!
			pressure = buttonsPressure[id]->GetValue();
			if (pressure > 255)
				pressure = 255;
			else if (pressure < 0)
				pressure = 0;
		}
		g_TASInput.SetButtonState(port, PadDataNormalKeys[id], pressure);
	}
	else
		tasConLog("[VirtualPad]: Unknown pressure sensitivty change.\n");
}

void VirtualPad::OnTextCtrlChange(wxSpinEvent & event)
{
	if (ID_L_UPDOWN_TEXT <= event.GetId() && event.GetId() <= ID_R_RIGHTLEFT_TEXT)
	{
		int id = event.GetId() - ID_L_UPDOWN_TEXT;
		sticks[id]->SetValue(event.GetInt());
		// We inverse up and down for more confort
		if (id % 2 == 0)
			g_TASInput.UpdateAnalog(port, PadDataAnalogKeys[id], 255 - event.GetInt());
		else
			g_TASInput.UpdateAnalog(port, PadDataAnalogKeys[id], event.GetInt());
	}
	else
		tasConLog("[VirtualPad]: Unknown TextCtrl change.\n");
}

void VirtualPad::OnSliderMove(wxCommandEvent & event)
{
	if (ID_L_UPDOWN <= event.GetId() && event.GetId() <= ID_R_RIGHTLEFT)
	{
		int id = event.GetId() - ID_L_UPDOWN;
		sticksText[id]->SetValue(event.GetInt());
		// We inverse up and down for more confort
		if (id % 2 == 0)
			g_TASInput.UpdateAnalog(port, PadDataAnalogKeys[id], 255 - event.GetInt());
		else
			g_TASInput.UpdateAnalog(port, PadDataAnalogKeys[id], event.GetInt());
	}
	else
		tasConLog("[VirtualPad]: Unknown TextCtrl change.\n");
}

int VirtualPad::getButtonIdFromPressure(int pressureCtrlId) {

	if (pressureCtrlId <= ID_DOWN_PRESSURE) {
		return pressureCtrlId - 16;
	}
	else if (pressureCtrlId >= ID_X_PRESSURE && pressureCtrlId <= ID_L2_PRESSURE) {
		// Skip start and select buttons
		return pressureCtrlId - 16 + 2;
	}
	else if (pressureCtrlId >= ID_R1_PRESSURE && pressureCtrlId <= ID_R2_PRESSURE) {
		// skip L3 button
		return pressureCtrlId - 16 + 2 + 1;
	}
	else {
		// else, wasnt a pressure sensitive event
		return -1;
	}
}

bool VirtualPad::isPressureSensitive(int buttonId) {


	if (buttonId != ID_SELECT ||
		buttonId != ID_START ||
		buttonId != ID_L3 ||
		buttonId != ID_R3) {
		return false;
	}
	return true;
}