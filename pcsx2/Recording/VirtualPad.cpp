/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "Common.h"

#include "Recording/VirtualPad.h"
#include "Recording/RecordingInputManager.h"

#ifndef DISABLE_RECORDING
wxBEGIN_EVENT_TABLE(VirtualPad, wxFrame)
	EVT_CLOSE(VirtualPad::OnClose)
wxEND_EVENT_TABLE()

// TODO - Problems / Potential improvements:
// - The UI doesn't update to manual controller inputs and actually overrides the controller when opened (easily noticable with analog stick)
//   - This is less than ideal, but it's going to take a rather large / focused refactor, in it's current state the virtual pad does what it needs to do (precise inputs, frame by frame)
VirtualPad::VirtualPad(wxWindow* parent, wxWindowID id, const wxString& title, int controllerPort, const wxPoint& pos, const wxSize& size, long style) :
	wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{
	// Define components
	SetSize(wxSize(1000, 700));
	l2Button = new wxToggleButton(this, wxID_ANY, wxT("L2"));
	l2ButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	l1Button = new wxToggleButton(this, wxID_ANY, wxT("L1"));
	l1ButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	r2Button = new wxToggleButton(this, wxID_ANY, wxT("R2"));
	r2ButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	r1Button = new wxToggleButton(this, wxID_ANY, wxT("R1"));
	r1ButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	upButton = new wxToggleButton(this, wxID_ANY, wxT("Up"));
	upButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	leftButton = new wxToggleButton(this, wxID_ANY, wxT("Left"));
	leftButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	rightButton = new wxToggleButton(this, wxID_ANY, wxT("Right"));
	rightButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	downButton = new wxToggleButton(this, wxID_ANY, wxT("Down"));
	downButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	selectButton = new wxToggleButton(this, wxID_ANY, wxT("Select"));
	startButton = new wxToggleButton(this, wxID_ANY, wxT("Start"));
	triangleButton = new wxToggleButton(this, wxID_ANY, wxT("Triangle"));
	triangleButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	squareButton = new wxToggleButton(this, wxID_ANY, wxT("Square"));
	squareButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	circleButton = new wxToggleButton(this, wxID_ANY, wxT("Circle"));
	circleButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	crossButton = new wxToggleButton(this, wxID_ANY, wxT("Cross"));
	crossButtonPressure = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 255, 255);
	leftAnalogXVal = new wxSlider(this, wxID_ANY, 0, -127, 127);
	leftAnalogXValPrecise = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -127, 128);
	l3Button = new wxToggleButton(this, wxID_ANY, wxT("L3"));
	leftAnalogYVal = new wxSlider(this, wxID_ANY, 0, -127, 127, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL);
	leftAnalogYValPrecise = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -127, 128);
	rightAnalogXVal = new wxSlider(this, wxID_ANY, 0, -127, 127);
	rightAnalogXValPrecise = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -127, 128);
	r3Button = new wxToggleButton(this, wxID_ANY, wxT("R3"));
	rightAnalogYVal = new wxSlider(this, wxID_ANY, 0, -127, 127, wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL);
	rightAnalogYValPrecise = new wxSpinCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -127, 128);

	// Initialize class members
	VirtualPad::controllerPort = controllerPort;

	// NOTE: Order MATTERS, these match enum defined in PadData.h
	wxToggleButton* tempButtons[16] = {
		// Pressure sensitive buttons
		upButton, rightButton, leftButton, downButton,
		crossButton, circleButton, squareButton, triangleButton,
		l1Button, l2Button, r1Button, r2Button,
		// Non-pressure sensitive buttons
		l3Button, r3Button,
		selectButton, startButton};
	std::copy(std::begin(tempButtons), std::end(tempButtons), std::begin(buttons));

	// NOTE: Order MATTERS, these match enum defined in PadData.h
	wxSpinCtrl* tempPressureButtons[16] = {
		// Pressure sensitive buttons
		upButtonPressure, rightButtonPressure, leftButtonPressure, downButtonPressure,
		crossButtonPressure, circleButtonPressure, squareButtonPressure, triangleButtonPressure,
		l1ButtonPressure, l2ButtonPressure, r1ButtonPressure, r2ButtonPressure};
	std::copy(std::begin(tempPressureButtons), std::end(tempPressureButtons), std::begin(buttonsPressure));

	// NOTE: Order MATTERS, these match enum defined in PadData.h
	wxSlider* tempAnalogSliders[4] = { leftAnalogXVal, leftAnalogYVal, rightAnalogXVal, rightAnalogYVal };
	std::copy(std::begin(tempAnalogSliders), std::end(tempAnalogSliders), std::begin(analogSliders));

	// NOTE: Order MATTERS, these match enum defined in PadData.h
	wxSpinCtrl* tempAnalogVals[4] = { leftAnalogXValPrecise, leftAnalogYValPrecise, rightAnalogXValPrecise, rightAnalogYValPrecise };
	std::copy(std::begin(tempAnalogVals), std::end(tempAnalogVals), std::begin(analogVals));

	// Setup event bindings
	for (int i = 0; i < buttonsLength; i++)
	{
		(*buttons[i]).Bind(wxEVT_TOGGLEBUTTON, &VirtualPad::OnButtonPress, this);
	}
	for (int i = 0; i < buttonsPressureLength; i++)
	{
		(*buttonsPressure[i]).Bind(wxEVT_SPINCTRL, &VirtualPad::OnPressureChange, this);
	}
	for (int i = 0; i < analogSlidersLength; i++)
	{
		(*analogSliders[i]).Bind(wxEVT_SLIDER, &VirtualPad::OnAnalogSliderChange, this);
	}
	for (int i = 0; i < analogValsLength; i++)
	{
		(*analogVals[i]).Bind(wxEVT_SPINCTRL, &VirtualPad::OnAnalogValChange, this);
	}

	// Finalize layout
	SetProperties();
	DoLayout();
}


void VirtualPad::SetProperties()
{
	if (controllerPort == 0)
	{
		SetTitle(wxT("Virtual Pad - Port 1"));
	}
	else
	{
		SetTitle(wxT("Virtual Pad - Port 2"));
	}
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
}

bool VirtualPad::Show(bool show)
{
	if (!wxFrame::Show(show))
	{
		return false;
	}
	if (show)
	{
		g_RecordingInput.SetVirtualPadReading(controllerPort, true);
	}
	return true;
}

void VirtualPad::OnClose(wxCloseEvent & event)
{
	g_RecordingInput.SetVirtualPadReading(controllerPort, false);
	Hide();
}

void VirtualPad::OnButtonPress(wxCommandEvent & event)
{
	wxToggleButton* pressedButton = (wxToggleButton*) event.GetEventObject();
	int buttonId = -1;
	for (int i = 0; i < buttonsLength; i++)
	{
		if (pressedButton == buttons[i])
		{
			buttonId = i;
		}
	}
	if (buttonId != -1)
	{
		u8 pressure = 0;
		if (event.IsChecked())
		{
			if (buttonId < 12)
			{
				pressure = buttonsPressure[buttonId]->GetValue();
			}
			else
			{
				pressure = 255;
			}
		}
		g_RecordingInput.SetButtonState(controllerPort, PadData_NormalButton(buttonId), pressure);
	}
}

void VirtualPad::OnPressureChange(wxSpinEvent & event)
{
	wxSpinCtrl* updatedSpinner = (wxSpinCtrl*) event.GetEventObject();
	int spinnerId = -1;
	for (int i = 0; i < buttonsPressureLength; i++)
	{
		if (updatedSpinner == buttonsPressure[i])
		{
			spinnerId = i;
		}
	}

	if (spinnerId != -1)
	{
		u8 pressure = 0;
		if (event.IsChecked())
		{
			pressure = buttonsPressure[spinnerId]->GetValue();
		}
		g_RecordingInput.SetButtonState(controllerPort, PadData_NormalButton(spinnerId), pressure);
	}
}

void VirtualPad::OnAnalogSliderChange(wxCommandEvent & event)
{
	wxSlider* movedSlider = (wxSlider*) event.GetEventObject();
	int sliderId = -1;
	for (int i = 0; i < analogSlidersLength; i++)
	{
		if (movedSlider == analogSliders[i])
		{
			sliderId = i;
		}
	}
	if (sliderId != -1)
	{
		if (sliderId % 2 == 0)
		{
			analogVals[sliderId]->SetValue(event.GetInt());
		}
		else
		{
			analogVals[sliderId]->SetValue(event.GetInt() * -1);
		}

		g_RecordingInput.UpdateAnalog(controllerPort, PadData_AnalogVector(sliderId), event.GetInt() + 127);
	}
}

void VirtualPad::OnAnalogValChange(wxSpinEvent & event)
{
	wxSpinCtrl* updatedSpinner = (wxSpinCtrl*)event.GetEventObject();
	int spinnerId = -1;
	for (int i = 0; i < analogValsLength; i++)
	{
		if (updatedSpinner == analogVals[i])
		{
			spinnerId = i;
		}
	}
	if (spinnerId != -1)
	{
		analogVals[spinnerId]->SetValue(event.GetInt());
		g_RecordingInput.UpdateAnalog(controllerPort, PadData_AnalogVector(spinnerId), event.GetInt() + 127);
	}
}

void VirtualPad::DoLayout()
{
	wxBoxSizer* container = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* analogSticks = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace6 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* rightAnalogContainer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* rightAnalog = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* rightAnalogYContainer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* rightAnalogY = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* rightAnalogButtonAndGUI = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* rightAnalogX = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace5 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* leftAnalogContainer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* leftAnalog = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* leftAnalogYContainer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* leftAnalogY = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* leftAnalogButtonAndGUI = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* leftAnalogX = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace4 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* faceButtonRow = new wxBoxSizer(wxHORIZONTAL);
	wxGridSizer* faceButtons = new wxGridSizer(0, 3, 0, 0);
	wxBoxSizer* cross = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* circle = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* square = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* triangle = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* middleOfController = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* emptySpace8 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* startAndSelect = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace9 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace7 = new wxBoxSizer(wxHORIZONTAL);
	wxGridSizer* dPad = new wxGridSizer(0, 3, 0, 0);
	wxBoxSizer* dPadDown = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dPadRight = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dPadLeft = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dPadUp = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* shoulderButtons = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace3 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* rightShoulderButtons = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* r1ButtonRow = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* r2ButtonRow = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace2 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* leftShoulderButtons = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* l1ButtonRow = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* l2ButtonRow = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* emptySpace1 = new wxBoxSizer(wxVERTICAL);
	emptySpace1->Add(0, 0, 0, 0, 0);
	shoulderButtons->Add(emptySpace1, 2, wxEXPAND, 0);
	l2ButtonRow->Add(l2Button, 0, wxEXPAND, 0);
	l2ButtonRow->Add(l2ButtonPressure, 0, wxEXPAND, 0);
	leftShoulderButtons->Add(l2ButtonRow, 1, wxEXPAND, 0);
	l1ButtonRow->Add(l1Button, 0, wxEXPAND, 0);
	l1ButtonRow->Add(l1ButtonPressure, 0, wxEXPAND, 0);
	leftShoulderButtons->Add(l1ButtonRow, 1, wxEXPAND, 0);
	shoulderButtons->Add(leftShoulderButtons, 5, wxEXPAND, 0);
	emptySpace2->Add(0, 0, 0, 0, 0);
	shoulderButtons->Add(emptySpace2, 13, wxEXPAND, 0);
	r2ButtonRow->Add(r2Button, 0, wxEXPAND, 0);
	r2ButtonRow->Add(r2ButtonPressure, 0, wxEXPAND, 0);
	rightShoulderButtons->Add(r2ButtonRow, 1, wxEXPAND, 0);
	r1ButtonRow->Add(r1Button, 0, wxEXPAND, 0);
	r1ButtonRow->Add(r1ButtonPressure, 0, wxEXPAND, 0);
	rightShoulderButtons->Add(r1ButtonRow, 1, wxEXPAND, 0);
	shoulderButtons->Add(rightShoulderButtons, 5, wxEXPAND, 0);
	emptySpace3->Add(0, 0, 0, 0, 0);
	shoulderButtons->Add(emptySpace3, 2, wxEXPAND, 0);
	container->Add(shoulderButtons, 1, wxBOTTOM | wxEXPAND | wxTOP, 3);
	dPad->Add(0, 0, 0, 0, 0);
	dPadUp->Add(upButton, 0, wxEXPAND, 0);
	dPadUp->Add(upButtonPressure, 0, wxEXPAND, 0);
	dPad->Add(dPadUp, 1, wxEXPAND, 0);
	dPad->Add(0, 0, 0, 0, 0);
	dPadLeft->Add(leftButton, 0, wxEXPAND, 0);
	dPadLeft->Add(leftButtonPressure, 0, wxEXPAND, 0);
	dPad->Add(dPadLeft, 1, wxEXPAND, 0);
	dPad->Add(0, 0, 0, 0, 0);
	dPadRight->Add(rightButton, 0, wxEXPAND, 0);
	dPadRight->Add(rightButtonPressure, 0, wxEXPAND, 0);
	dPad->Add(dPadRight, 1, wxEXPAND, 0);
	dPad->Add(0, 0, 0, 0, 0);
	dPadDown->Add(downButton, 0, wxEXPAND, 0);
	dPadDown->Add(downButtonPressure, 0, wxEXPAND, 0);
	dPad->Add(dPadDown, 1, wxEXPAND, 0);
	dPad->Add(0, 0, 0, 0, 0);
	faceButtonRow->Add(dPad, 9, wxEXPAND | wxLEFT | wxRIGHT, 3);
	emptySpace7->Add(0, 0, 0, 0, 0);
	middleOfController->Add(emptySpace7, 1, wxEXPAND, 0);
	startAndSelect->Add(selectButton, 0, 0, 0);
	emptySpace9->Add(0, 0, 0, 0, 0);
	startAndSelect->Add(emptySpace9, 1, wxEXPAND, 0);
	startAndSelect->Add(startButton, 0, 0, 0);
	middleOfController->Add(startAndSelect, 1, wxEXPAND, 0);
	emptySpace8->Add(0, 0, 0, 0, 0);
	middleOfController->Add(emptySpace8, 1, wxEXPAND, 0);
	faceButtonRow->Add(middleOfController, 8, wxEXPAND | wxLEFT | wxRIGHT, 3);
	faceButtons->Add(0, 0, 0, 0, 0);
	triangle->Add(triangleButton, 0, wxEXPAND, 0);
	triangle->Add(triangleButtonPressure, 0, wxEXPAND, 0);
	faceButtons->Add(triangle, 1, wxEXPAND, 0);
	faceButtons->Add(0, 0, 0, 0, 0);
	square->Add(squareButton, 0, wxEXPAND, 0);
	square->Add(squareButtonPressure, 0, wxEXPAND, 0);
	faceButtons->Add(square, 1, wxEXPAND, 0);
	faceButtons->Add(0, 0, 0, 0, 0);
	circle->Add(circleButton, 0, wxEXPAND, 0);
	circle->Add(circleButtonPressure, 0, wxEXPAND, 0);
	faceButtons->Add(circle, 1, wxEXPAND, 0);
	faceButtons->Add(0, 0, 0, 0, 0);
	cross->Add(crossButton, 0, wxEXPAND, 0);
	cross->Add(crossButtonPressure, 0, wxEXPAND, 0);
	faceButtons->Add(cross, 1, wxEXPAND, 0);
	faceButtons->Add(0, 0, 0, 0, 0);
	faceButtonRow->Add(faceButtons, 9, wxEXPAND | wxLEFT | wxRIGHT, 3);
	container->Add(faceButtonRow, 4, wxBOTTOM | wxEXPAND | wxTOP, 3);
	emptySpace4->Add(0, 0, 0, 0, 0);
	analogSticks->Add(emptySpace4, 6, wxEXPAND, 0);
	leftAnalogX->Add(leftAnalogXVal, 1, wxALL | wxEXPAND, 0);
	leftAnalogX->Add(leftAnalogXValPrecise, 0, wxEXPAND, 0);
	leftAnalog->Add(leftAnalogX, 1, wxEXPAND, 0);
	leftAnalogButtonAndGUI->Add(0, 0, 0, 0, 0);
	leftAnalogButtonAndGUI->Add(l3Button, 0, wxALIGN_CENTER, 0);
	leftAnalogYContainer->Add(leftAnalogButtonAndGUI, 1, wxEXPAND, 0);
	leftAnalogY->Add(leftAnalogYVal, 1, wxALIGN_RIGHT, 0);
	leftAnalogY->Add(leftAnalogYValPrecise, 0, wxALIGN_RIGHT, 0);
	leftAnalogYContainer->Add(leftAnalogY, 1, wxEXPAND, 0);
	leftAnalog->Add(leftAnalogYContainer, 5, wxEXPAND, 0);
	leftAnalogContainer->Add(leftAnalog, 1, wxEXPAND, 0);
	analogSticks->Add(leftAnalogContainer, 6, wxEXPAND, 0);
	emptySpace5->Add(0, 0, 0, 0, 0);
	analogSticks->Add(emptySpace5, 3, wxEXPAND, 0);
	rightAnalogX->Add(rightAnalogXVal, 1, wxEXPAND, 0);
	rightAnalogX->Add(rightAnalogXValPrecise, 0, wxEXPAND, 0);
	rightAnalog->Add(rightAnalogX, 1, wxEXPAND, 0);
	rightAnalogButtonAndGUI->Add(0, 0, 0, 0, 0);
	rightAnalogButtonAndGUI->Add(r3Button, 0, wxALIGN_CENTER, 0);
	rightAnalogYContainer->Add(rightAnalogButtonAndGUI, 1, wxEXPAND, 0);
	rightAnalogY->Add(rightAnalogYVal, 1, wxALIGN_RIGHT, 0);
	rightAnalogY->Add(rightAnalogYValPrecise, 0, wxALIGN_RIGHT | wxEXPAND, 0);
	rightAnalogYContainer->Add(rightAnalogY, 1, wxEXPAND, 0);
	rightAnalog->Add(rightAnalogYContainer, 5, wxEXPAND, 0);
	rightAnalogContainer->Add(rightAnalog, 1, wxEXPAND, 0);
	analogSticks->Add(rightAnalogContainer, 6, wxEXPAND, 0);
	emptySpace6->Add(0, 0, 0, 0, 0);
	analogSticks->Add(emptySpace6, 6, wxEXPAND, 0);
	container->Add(analogSticks, 3, wxBOTTOM | wxEXPAND | wxTOP, 3);
	SetSizer(container);
	Layout();
}
#endif
