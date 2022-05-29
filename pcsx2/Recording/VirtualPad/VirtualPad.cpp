/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#ifndef PCSX2_CORE

#include <math.h>

#include "gui/App.h"
#include "gui/MSWstuff.h"
#include "gui/EmbeddedImage.h"
#include "wx/dcbuffer.h"
#include "wx/display.h"
#include "wx/spinctrl.h"

#include "Recording/VirtualPad/VirtualPad.h"
#include "Recording/VirtualPad/VirtualPadResources.h"

#include "Recording/VirtualPad/img/circlePressed.h"
#include "Recording/VirtualPad/img/controllerFull.h"
#include "Recording/VirtualPad/img/controllerThreeQuarters.h"
#include "Recording/VirtualPad/img/controllerHalf.h"
#include "Recording/VirtualPad/img/crossPressed.h"
#include "Recording/VirtualPad/img/downPressed.h"
#include "Recording/VirtualPad/img/l1Pressed.h"
#include "Recording/VirtualPad/img/l2Pressed.h"
#include "Recording/VirtualPad/img/l3Pressed.h"
#include "Recording/VirtualPad/img/leftPressed.h"
#include "Recording/VirtualPad/img/r1Pressed.h"
#include "Recording/VirtualPad/img/r2Pressed.h"
#include "Recording/VirtualPad/img/r3Pressed.h"
#include "Recording/VirtualPad/img/rightPressed.h"
#include "Recording/VirtualPad/img/selectPressed.h"
#include "Recording/VirtualPad/img/squarePressed.h"
#include "Recording/VirtualPad/img/startPressed.h"
#include "Recording/VirtualPad/img/trianglePressed.h"
#include "Recording/VirtualPad/img/upPressed.h"


VirtualPad::VirtualPad(wxWindow* parent, int controllerPort, AppConfig::InputRecordingOptions& options)
	: wxFrame(parent, wxID_ANY, wxEmptyString)
	, options(options)
{
	// Images at 1.00 scale are designed to work well on HiDPI (4k) at 150% scaling (default recommended setting on windows)
	// Therefore, on a 1080p monitor we halve the scaling, on 1440p we reduce it by 25%, which from some quick tests looks comparable
	//		Side-note - Getting the DPI scaling amount is platform specific (with some platforms only supporting
	//		integer scaling as well) this is likely not reliable.
	// Slight multi-monitor support, will use whatever window pcsx2 is opened with, but won't currently re-init if
	// windows are dragged between differing monitors!
	wxDisplay display(wxDisplay::GetFromWindow(this));
	const wxRect screen = display.GetClientArea();
	float dpiScale = MSW_GetDPIScale();                // linux returns 1.0
	if (screen.height > 1080 && screen.height <= 1440) // 1440p display
		scalingFactor = 0.75 * dpiScale;
	else if (screen.height <= 1080) // 1080p display
	{
		scalingFactor = 0.5 * dpiScale;
	}
	// otherwise use default 1.0 scaling

	virtualPadData = VirtualPadData();
	// Based on the scaling factor, select the appropriate background image
	// Don't scale these images as they've already been pre-scaled
	if (floatCompare(scalingFactor, 0.5))
		virtualPadData.background = NewBitmap(EmbeddedImage<res_controllerHalf>().Get(), wxPoint(0, 0), true);
	else if (floatCompare(scalingFactor, 0.75))
		virtualPadData.background = NewBitmap(EmbeddedImage<res_controllerThreeQuarters>().Get(), wxPoint(0, 0), true);
	else
		// Otherwise, scale down/up (or don't in the case of 1.0) the largst image
		virtualPadData.background = NewBitmap(EmbeddedImage<res_controllerFull>().Get(), wxPoint(0, 0));

	// Use the background image's size to define the window size
	SetClientSize(virtualPadData.background.width, virtualPadData.background.height);

	// These hard-coded pixels correspond to where the background image's components are (ie. the buttons)
	// Everything is automatically scaled and adjusted based on the `scalingFactor` variable
	InitPressureButtonGuiElements(virtualPadData.square, NewBitmap(EmbeddedImage<res_squarePressed>().Get(), wxPoint(852, 287)), this, wxPoint(1055, 525));
	InitPressureButtonGuiElements(virtualPadData.triangle, NewBitmap(EmbeddedImage<res_trianglePressed>().Get(), wxPoint(938, 201)), this, wxPoint(1055, 565));
	InitPressureButtonGuiElements(virtualPadData.circle, NewBitmap(EmbeddedImage<res_circlePressed>().Get(), wxPoint(1024, 286)), this, wxPoint(1055, 605));
	InitPressureButtonGuiElements(virtualPadData.cross, NewBitmap(EmbeddedImage<res_crossPressed>().Get(), wxPoint(938, 369)), this, wxPoint(1055, 645));

	InitPressureButtonGuiElements(virtualPadData.left, NewBitmap(EmbeddedImage<res_leftPressed>().Get(), wxPoint(110, 303)), this, wxPoint(175, 525), true);
	InitPressureButtonGuiElements(virtualPadData.up, NewBitmap(EmbeddedImage<res_upPressed>().Get(), wxPoint(186, 227)), this, wxPoint(175, 565), true);	
	InitPressureButtonGuiElements(virtualPadData.right, NewBitmap(EmbeddedImage<res_rightPressed>().Get(), wxPoint(248, 302)), this, wxPoint(175, 605), true);
	InitPressureButtonGuiElements(virtualPadData.down, NewBitmap(EmbeddedImage<res_downPressed>().Get(), wxPoint(186, 359)), this, wxPoint(175, 645), true);

	InitPressureButtonGuiElements(virtualPadData.l1, NewBitmap(EmbeddedImage<res_l1Pressed>().Get(), wxPoint(156, 98)), this, wxPoint(170, 135));
	InitPressureButtonGuiElements(virtualPadData.l2, NewBitmap(EmbeddedImage<res_l2Pressed>().Get(), wxPoint(156, 57)), this, wxPoint(170, 52), false, true);
	InitPressureButtonGuiElements(virtualPadData.r1, NewBitmap(EmbeddedImage<res_r1Pressed>().Get(), wxPoint(921, 98)), this, wxPoint(1035, 135), true);
	InitPressureButtonGuiElements(virtualPadData.r2, NewBitmap(EmbeddedImage<res_r2Pressed>().Get(), wxPoint(921, 57)), this, wxPoint(1035, 52), true, true);

	InitNormalButtonGuiElements(virtualPadData.select, NewBitmap(EmbeddedImage<res_selectPressed>().Get(), wxPoint(458, 313)), this, wxPoint(530, 315));
	InitNormalButtonGuiElements(virtualPadData.start, NewBitmap(EmbeddedImage<res_startPressed>().Get(), wxPoint(688, 311)), this, wxPoint(646, 315));
	InitNormalButtonGuiElements(virtualPadData.l3, NewBitmap(EmbeddedImage<res_l3Pressed>().Get(), wxPoint(336, 453)), this, wxPoint(560, 638));
	InitNormalButtonGuiElements(virtualPadData.r3, NewBitmap(EmbeddedImage<res_r3Pressed>().Get(), wxPoint(726, 453)), this, wxPoint(615, 638));

	InitAnalogStickGuiElements(virtualPadData.leftAnalog, this, wxPoint(404, 522), 100, wxPoint(314, 642), wxPoint(526, 432), false, wxPoint(504, 685), wxPoint(570, 425), true);
	InitAnalogStickGuiElements(virtualPadData.rightAnalog, this, wxPoint(794, 522), 100, wxPoint(706, 642), wxPoint(648, 432), true, wxPoint(700, 685), wxPoint(635, 425));

	ignoreRealControllerBox = new wxCheckBox(this, wxID_ANY, wxEmptyString, ScaledPoint(wxPoint(586, 135)), wxDefaultSize);
	resetButton = new wxButton(this, wxID_ANY, _("Reset"), ScaledPoint(wxPoint(1195, 5), wxSize(100, 50), true), ScaledSize(wxSize(100, 50)));

	Bind(wxEVT_CHECKBOX, &VirtualPad::OnIgnoreRealController, this, ignoreRealControllerBox->GetId());
	Bind(wxEVT_BUTTON, &VirtualPad::OnResetButton, this, resetButton->GetId());

	// Bind Window Events
	Bind(wxEVT_MOVE, &VirtualPad::OnMoveAround, this);
	Bind(wxEVT_CLOSE_WINDOW, &VirtualPad::OnClose, this);
	Bind(wxEVT_ICONIZE, &VirtualPad::OnIconize, this);
	Bind(wxEVT_ERASE_BACKGROUND, &VirtualPad::OnEraseBackground, this);
	// Temporary Paint event handler so the window displays properly before the controller-interrupt routine takes over with manual drawing.
	// The reason for this is in order to minimize the performance impact, we need total control over when render is called
	// Windows redraws the window _alot_ otherwise which leads to major performance problems (when GS is using the software renderer)
	Bind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
	// DevCon.WriteLn("Paint Event Bound");

	// Finalize layout
	SetIcons(wxGetApp().GetIconBundle());
	SetTitle(wxString::Format("Virtual Pad - Port %d", controllerPort + 1));
	SetPosition(options.VirtualPadPosition);
	SetBackgroundColour(*wxWHITE);
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	// This window does not allow for resizing for sake of simplicity: all images are scaled initially and stored, ready to be rendered
	SetWindowStyle(wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER & ~wxMAXIMIZE_BOX);

	// Causes flickering, despite it supposed to be preventing it!
	// SetDoubleBuffered(true);
}

void VirtualPad::OnMoveAround(wxMoveEvent& event)
{
	if (IsBeingDeleted() || !IsVisible() || IsIconized())
		return;

	if (!IsMaximized())
		options.VirtualPadPosition = GetPosition();
	event.Skip();
}

void VirtualPad::OnClose(wxCloseEvent& event)
{
	// Re-bind the Paint event in case this is due to a game being opened/closed
	manualRedrawMode = false;
	Bind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
	// DevCon.WriteLn("Paint Event Bound");
	Hide();
}

void VirtualPad::OnIconize(wxIconizeEvent& event)
{
	if (event.IsIconized())
	{
		manualRedrawMode = false;
		Bind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
		// DevCon.WriteLn("Paint Event Bound");
	}
}

void VirtualPad::OnEraseBackground(wxEraseEvent& event)
{
	// Intentionally Empty
	// See - https://wiki.wxwidgets.org/Flicker-Free_Drawing
}

void VirtualPad::OnPaint(wxPaintEvent& event)
{
	// DevCon.WriteLn("Paint Event Called");
	wxBufferedPaintDC dc(this, wxBUFFER_VIRTUAL_AREA);
	Render(dc);
}

void VirtualPad::Redraw()
{
	wxClientDC cdc(this);
	wxBufferedDC dc(&cdc);
	Render(dc);
}

void VirtualPad::Render(wxDC& bdc)
{
	// Update GUI Elements and figure out what needs to be rendered
	for (VirtualPadElement* virtualPadElement : virtualPadElements)
		virtualPadElement->UpdateGuiElement(renderQueue, clearScreenRequired);

	// Update Graphic Elements off render stack
	// Before we start rendering (if we have to) clear and re-draw the background
	if (!manualRedrawMode || clearScreenRequired || !renderQueue.empty())
	{
		bdc.SetBrush(*wxWHITE);
		bdc.DrawRectangle(wxPoint(0, 0), bdc.GetSize());
		bdc.SetBrush(wxNullBrush);
		bdc.DrawBitmap(virtualPadData.background.image, virtualPadData.background.coords, true);
		clearScreenRequired = false;

		// Switch to Manual Rendering once the first user action on the VirtualPad is taken
		if (!manualRedrawMode && !renderQueue.empty())
		{
			// DevCon.WriteLn("Paint Event Unbound");
			Unbind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
			manualRedrawMode = true;
		}

		// NOTE - there is yet another (and I think final) micro-optimization that can be done:
		// It can be assumed that if the element has already been drawn to the screen (and not cleared) that we can skip rendering it
		//
		// For example - you hold a single button for several frames, it will currently draw that every frame
		// despite the screen never being cleared - so this is not strictly necessary.
		//
		// Though after some tests, the performance impact is well within reason, and on the hardware renderer modes, is almost non-existant.
		while (!renderQueue.empty())
		{
			VirtualPadElement* element = renderQueue.front();
			if (element)
				element->Render(bdc);
			renderQueue.pop();
		}
	}
}

bool VirtualPad::UpdateControllerData(u16 const bufIndex, PadData* padData)
{
	return virtualPadData.UpdateVirtualPadData(bufIndex, padData, ignoreRealController && !readOnlyMode, readOnlyMode);
}

void VirtualPad::enableUiElements(bool enable)
{
	ignoreRealControllerBox->Enable(enable);
	resetButton->Enable(enable);
	for (VirtualPadElement* virtualPadElement : virtualPadElements)
		virtualPadElement->EnableWidgets(enable);
}

void VirtualPad::SetReadOnlyMode(bool readOnly)
{
	enableUiElements(!readOnly);
	readOnlyMode = readOnly;
}

void VirtualPad::OnIgnoreRealController(wxCommandEvent& event)
{
	const wxCheckBox* ignoreButton = (wxCheckBox*)event.GetEventObject();
	if (ignoreButton)
		ignoreRealController = ignoreButton->GetValue();
}

void VirtualPad::OnResetButton(wxCommandEvent& event)
{
	if (readOnlyMode)
		return;

	for (VirtualPadElement* virtualPadElement : virtualPadElements)
		virtualPadElement->Reset(this);
}

void VirtualPad::OnNormalButtonPress(wxCommandEvent& event)
{
	const wxCheckBox* pressedButton = (wxCheckBox*)event.GetEventObject();
	ControllerNormalButton* eventBtn = buttonElements[pressedButton->GetId()];

	if (pressedButton)
		eventBtn->pressed = pressedButton->GetValue();

	if (!eventBtn->isControllerPressBypassed)
		eventBtn->isControllerPressBypassed = true;
}

void VirtualPad::OnPressureButtonPressureChange(wxCommandEvent& event)
{
	const wxSpinCtrl* pressureSpinner = (wxSpinCtrl*)event.GetEventObject();
	ControllerPressureButton* eventBtn = pressureElements[pressureSpinner->GetId()];

	if (pressureSpinner)
		eventBtn->pressure = pressureSpinner->GetValue();

	eventBtn->pressed = eventBtn->pressure > 0;

	if (!eventBtn->isControllerPressureBypassed || !eventBtn->isControllerPressBypassed)
	{
		eventBtn->isControllerPressureBypassed = true;
		eventBtn->isControllerPressBypassed = true;
	}
}

void VirtualPad::OnAnalogSpinnerChange(wxCommandEvent& event)
{
	const wxSpinCtrl* analogSpinner = (wxSpinCtrl*)event.GetEventObject();
	AnalogVector* eventVector = analogElements[analogSpinner->GetId()];

	if (analogSpinner)
		eventVector->val = analogSpinner->GetValue();

	eventVector->slider->SetValue(eventVector->val);

	if (!eventVector->isControllerBypassed)
		eventVector->isControllerBypassed = true;
}

void VirtualPad::OnAnalogSliderChange(wxCommandEvent& event)
{
	const wxSlider* analogSlider = (wxSlider*)event.GetEventObject();
	AnalogVector* eventVector = analogElements[analogSlider->GetId()];

	if (analogSlider)
		eventVector->val = analogSlider->GetValue();

	eventVector->spinner->SetValue(eventVector->val);

	if (!eventVector->isControllerBypassed)
		eventVector->isControllerBypassed = true;
}

/// GUI Element Utility Functions

bool VirtualPad::floatCompare(float a, float b, float epsilon)
{
	return (fabs(a - b) < epsilon);
}

wxPoint VirtualPad::ScaledPoint(wxPoint point, wxSize widgetWidth, bool rightAlignedCoord, bool bottomAlignedCoord)
{
	return ScaledPoint(point.x, point.y, widgetWidth.x, widgetWidth.y, rightAlignedCoord, bottomAlignedCoord);
}

wxPoint VirtualPad::ScaledPoint(int x, int y, int widgetWidth, int widgetHeight, bool rightAlignedCoord, bool bottomAlignedCoord)
{
	wxPoint scaledPoint = wxPoint(x * scalingFactor, y * scalingFactor);
	if (rightAlignedCoord)
	{
		scaledPoint.x -= widgetWidth * scalingFactor;
		if (scaledPoint.x < 0)
			scaledPoint.x = 0;
	}
	if (bottomAlignedCoord)
	{
		scaledPoint.y -= widgetHeight * scalingFactor;
		if (scaledPoint.y < 0)
			scaledPoint.y = 0;
	}
	return scaledPoint;
}

wxSize VirtualPad::ScaledSize(wxSize size)
{
	return ScaledSize(size.x, size.y);
}

wxSize VirtualPad::ScaledSize(int x, int y)
{
	return wxSize(x * scalingFactor, y * scalingFactor);
}

ImageFile VirtualPad::NewBitmap(wxImage resource, wxPoint imgCoord, bool dontScale)
{
	return NewBitmap(dontScale ? 1 : scalingFactor, resource, imgCoord);
}

ImageFile VirtualPad::NewBitmap(float scalingFactor, wxImage resource, wxPoint imgCoord)
{
	wxBitmap bitmap = wxBitmap(resource.Rescale(resource.GetWidth() * scalingFactor, resource.GetHeight() * scalingFactor, wxIMAGE_QUALITY_HIGH));

	ImageFile image = ImageFile();
	image.image = bitmap;
	image.width = bitmap.GetWidth();
	image.height = bitmap.GetHeight();
	image.coords = ScaledPoint(imgCoord);
	return image;
}

void VirtualPad::InitNormalButtonGuiElements(ControllerNormalButton& button, ImageFile image, wxWindow* parentWindow, wxPoint checkboxCoord)
{
	button.icon = image;
	button.pressedBox = new wxCheckBox(parentWindow, wxID_ANY, wxEmptyString, ScaledPoint(checkboxCoord), wxDefaultSize);
	Bind(wxEVT_CHECKBOX, &VirtualPad::OnNormalButtonPress, this, button.pressedBox->GetId());
	buttonElements[button.pressedBox->GetId()] = &button;
	virtualPadElements.push_back(&button);
}

void VirtualPad::InitPressureButtonGuiElements(ControllerPressureButton& button, ImageFile image, wxWindow* parentWindow, wxPoint pressureSpinnerCoord, bool rightAlignedCoord, bool bottomAlignedCoord)
{
	const wxPoint scaledPoint = ScaledPoint(pressureSpinnerCoord, SPINNER_SIZE, rightAlignedCoord, bottomAlignedCoord);
	wxSpinCtrl* spinner = new wxSpinCtrl(parentWindow, wxID_ANY, wxEmptyString, scaledPoint, ScaledSize(SPINNER_SIZE), wxSP_ARROW_KEYS, 0, 255, 0);

	button.icon = image;
	button.pressureSpinner = spinner;
	Bind(wxEVT_SPINCTRL, &VirtualPad::OnPressureButtonPressureChange, this, button.pressureSpinner->GetId());
	pressureElements[button.pressureSpinner->GetId()] = &button;
	virtualPadElements.push_back(&button);
}

void VirtualPad::InitAnalogStickGuiElements(AnalogStick& analog, wxWindow* parentWindow, wxPoint centerPoint, int radius,
											wxPoint xSliderPoint, wxPoint ySliderPoint, bool flipYSlider, wxPoint xSpinnerPoint, wxPoint ySpinnerPoint, bool rightAlignedSpinners)
{
	AnalogPosition analogPos = AnalogPosition();
	analogPos.centerCoords = ScaledPoint(centerPoint);
	analogPos.endCoords = ScaledPoint(centerPoint);
	analogPos.radius = radius * scalingFactor;
	analogPos.lineThickness = 6 * scalingFactor;

	const wxPoint xSpinnerScaledPoint = ScaledPoint(xSpinnerPoint, SPINNER_SIZE, rightAlignedSpinners);
	const wxPoint ySpinnerScaledPoint = ScaledPoint(ySpinnerPoint, SPINNER_SIZE, rightAlignedSpinners, true);

	wxSlider* xSlider = new wxSlider(parentWindow, wxID_ANY, ANALOG_NEUTRAL, 0, ANALOG_MAX,
									 ScaledPoint(xSliderPoint), ScaledSize(ANALOG_SLIDER_WIDTH, ANALOG_SLIDER_HEIGHT));
	wxSlider* ySlider = new wxSlider(parentWindow, wxID_ANY, ANALOG_NEUTRAL, 0, ANALOG_MAX,
									 ScaledPoint(ySliderPoint), ScaledSize(ANALOG_SLIDER_HEIGHT, ANALOG_SLIDER_WIDTH), flipYSlider ? wxSL_LEFT : wxSL_RIGHT);
	wxSpinCtrl* xSpinner = new wxSpinCtrl(parentWindow, wxID_ANY, wxEmptyString, xSpinnerScaledPoint, ScaledSize(SPINNER_SIZE), wxSP_ARROW_KEYS, 0, 255, 127);
	wxSpinCtrl* ySpinner = new wxSpinCtrl(parentWindow, wxID_ANY, wxEmptyString, ySpinnerScaledPoint, ScaledSize(SPINNER_SIZE), wxSP_ARROW_KEYS, 0, 255, 127);

	analog.xVector.slider = xSlider;
	analog.yVector.slider = ySlider;
	analog.xVector.spinner = xSpinner;
	analog.yVector.spinner = ySpinner;
	analog.positionGraphic = analogPos;
	Bind(wxEVT_SLIDER, &VirtualPad::OnAnalogSliderChange, this, xSlider->GetId());
	Bind(wxEVT_SLIDER, &VirtualPad::OnAnalogSliderChange, this, ySlider->GetId());
	Bind(wxEVT_SPINCTRL, &VirtualPad::OnAnalogSpinnerChange, this, xSpinner->GetId());
	Bind(wxEVT_SPINCTRL, &VirtualPad::OnAnalogSpinnerChange, this, ySpinner->GetId());
	analogElements[xSlider->GetId()] = &analog.xVector;
	analogElements[ySlider->GetId()] = &analog.yVector;
	analogElements[xSpinner->GetId()] = &analog.xVector;
	analogElements[ySpinner->GetId()] = &analog.yVector;
	virtualPadElements.push_back(&analog);
}

#endif
