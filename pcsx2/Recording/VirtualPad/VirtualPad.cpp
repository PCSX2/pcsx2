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

#include <math.h>

#include "App.h"
#include "Utilities/EmbeddedImage.h"
#include "wx/dcbuffer.h"
#include "wx/display.h"
#include "wx/spinctrl.h"

#include "Recording/VirtualPad/VirtualPad.h"
#include "Recording/VirtualPad/VirtualPadResources.h"

#include "Recording/VirtualPad/img/circlePressed.h"
#include "Recording/VirtualPad/img/controller.h"
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

// TODO - Store position of frame in an (possibly the main) .ini file

VirtualPad::VirtualPad(wxWindow* parent, wxWindowID id, const wxString& title, int controllerPort, const wxPoint& pos, const wxSize& size, long style) :
	wxFrame(parent, id, title, pos, size, wxDEFAULT_FRAME_STYLE)
{
	// Images at 1.00 scale are designed to work well on HiDPI (4k) at 150% scaling (default recommended setting on windows)
	// Therefore, on a 1080p monitor we halve the scaling, on 1440p we reduce it by 25%, which from some quick tests looks comparable
	//		Side-note - It would be possible to factor in monitor scaling, but considering that is platform specific (with some platforms only supporting
	//		integer scaling) this is likely not reliable.
	// Slight multi-monitor support, will use whatever window pcsx2 is opened with, but won't currently re-init if 
	// windows are dragged between differing monitors!
    wxDisplay display(wxDisplay::GetFromWindow(this));
    const wxRect screen = display.GetClientArea();
    if (screen.height > 1080 && screen.height <= 1440) // 1440p display
    {
        scalingFactor = 0.75;
    } 
	else if (screen.height <= 1080) // 1080p display
	{
        scalingFactor = 0.5;
	} // otherwise use default 1.0 scaling

	virtualPadData = VirtualPadData();
	virtualPadData.background = NewBitmap(EmbeddedImage<res_controller>().Get(), wxPoint(0, 0));
	// Use the background image's size to define the window size
	SetClientSize(virtualPadData.background.width, virtualPadData.background.height);
    
	InitPressureButtonGuiElements(virtualPadData.cross, NewBitmap(EmbeddedImage<res_crossPressed>().Get(), wxPoint(968, 498)), this, wxPoint(1062, 660));
	InitPressureButtonGuiElements(virtualPadData.circle, NewBitmap(EmbeddedImage<res_circlePressed>().Get(), wxPoint(1057, 413)), this, wxPoint(1062, 700));
	InitPressureButtonGuiElements(virtualPadData.triangle, NewBitmap(EmbeddedImage<res_trianglePressed>().Get(), wxPoint(968, 325)), this, wxPoint(1062, 740));
	InitPressureButtonGuiElements(virtualPadData.square, NewBitmap(EmbeddedImage<res_squarePressed>().Get(), wxPoint(879, 413)), this, wxPoint(1062, 780));
	InitPressureButtonGuiElements(virtualPadData.down, NewBitmap(EmbeddedImage<res_downPressed>().Get(), wxPoint(191, 488)), this, wxPoint(199, 660), true);
	InitPressureButtonGuiElements(virtualPadData.right, NewBitmap(EmbeddedImage<res_rightPressed>().Get(), wxPoint(255, 429)), this, wxPoint(199, 700), true);
	InitPressureButtonGuiElements(virtualPadData.up, NewBitmap(EmbeddedImage<res_upPressed>().Get(), wxPoint(191, 354)), this, wxPoint(199, 740), true);
	InitPressureButtonGuiElements(virtualPadData.left, NewBitmap(EmbeddedImage<res_leftPressed>().Get(), wxPoint(115, 429)), this, wxPoint(199, 780), true);
	InitPressureButtonGuiElements(virtualPadData.l1, NewBitmap(EmbeddedImage<res_l1Pressed>().Get(), wxPoint(166, 8)), this, wxPoint(294, 20));
	InitPressureButtonGuiElements(virtualPadData.l2, NewBitmap(EmbeddedImage<res_l2Pressed>().Get(), wxPoint(166, 81)), this, wxPoint(294, 100));
	InitPressureButtonGuiElements(virtualPadData.r1, NewBitmap(EmbeddedImage<res_r1Pressed>().Get(), wxPoint(958, 7)), this, wxPoint(940, 20), true);
	InitPressureButtonGuiElements(virtualPadData.r2, NewBitmap(EmbeddedImage<res_r2Pressed>().Get(), wxPoint(958, 81)), this, wxPoint(940, 100), true);

	InitNormalButtonGuiElements(virtualPadData.select, NewBitmap(EmbeddedImage<res_selectPressed>().Get(), wxPoint(473, 441)), this, wxPoint(545, 448));
	InitNormalButtonGuiElements(virtualPadData.start, NewBitmap(EmbeddedImage<res_startPressed>().Get(), wxPoint(710, 440)), this, wxPoint(675, 448));
	InitNormalButtonGuiElements(virtualPadData.l3, NewBitmap(EmbeddedImage<res_r3Pressed>().Get(), wxPoint(347, 585)), this, wxPoint(440, 835));
	InitNormalButtonGuiElements(virtualPadData.r3, NewBitmap(EmbeddedImage<res_l3Pressed>().Get(), wxPoint(750, 585)), this, wxPoint(844, 835));

	InitAnalogStickGuiElements(virtualPadData.leftAnalog, this, wxPoint(418, 656), 105, wxPoint(326, 782), wxPoint(545, 568), false, wxPoint(522, 800), wxPoint(522, 760));
	InitAnalogStickGuiElements(virtualPadData.rightAnalog, this, wxPoint(821, 656), 105, wxPoint(730, 782), wxPoint(672, 568), true, wxPoint(720, 800), wxPoint(720, 760), true);

	ignoreRealControllerBox = new wxCheckBox(this, wxID_ANY, wxEmptyString, ScaledPoint(605, 256), wxDefaultSize);
    Bind(wxEVT_CHECKBOX, &VirtualPad::OnIgnoreRealController, this, ignoreRealControllerBox->GetId());

	// Bind Window Events
    Bind(wxEVT_ERASE_BACKGROUND, &VirtualPad::OnEraseBackground, this);
    Bind(wxEVT_CLOSE_WINDOW, &VirtualPad::OnClose, this);
    Bind(wxEVT_ICONIZE, &VirtualPad::OnIconize, this);
	// Temporary Paint event handler so the window displays properly before the controller-interrupt routine takes over with manual drawing.
	// The reason for this is in order to minimize the performance impact, we need total control over when render is called
	// Windows redraws the window _alot_ otherwise which leads to major performance problems (when GS is using the software renderer)
    Bind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
    // DevCon.WriteLn("Paint Event Bound");

	// Finalize layout
	SetIcons(wxGetApp().GetIconBundle());
	SetTitle(wxString::Format("Virtual Pad - Port %d", controllerPort + 1));
	SetBackgroundColour(*wxWHITE);
	SetBackgroundStyle(wxBG_STYLE_PAINT);
    // This window does not allow for resizing for sake of simplicity: all images are scaled initially and stored, ready to be rendered
	SetWindowStyle(style & ~wxRESIZE_BORDER);
	SetDoubleBuffered(true);
}

void VirtualPad::OnClose(wxCloseEvent &event)
{
	// Re-bind the Paint event in case this is due to a game being opened/closed
    manualRedrawMode = false;
    Bind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
    // DevCon.WriteLn("Paint Event Bound");
	Hide();
}

void VirtualPad::OnIconize(wxIconizeEvent &event)
{
    if (event.IsIconized()) 
	{
        manualRedrawMode = false;
        Bind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
        // DevCon.WriteLn("Paint Event Bound");
    }
}

void VirtualPad::OnEraseBackground(wxEraseEvent &event)
{
	// Intentionally Empty
	// See - https://wiki.wxwidgets.org/Flicker-Free_Drawing
}

void VirtualPad::OnPaint(wxPaintEvent &event)
{
    // DevCon.WriteLn("Paint Event Called");
	wxPaintDC dc(this);
	Render(dc);
}

void VirtualPad::Redraw()
{
	wxClientDC dc(this);
	Render(dc);
}

void VirtualPad::Render(wxDC &dc)
{
	// Update GUI Elements and figure out what needs to be rendered
	virtualPadData.circle.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.cross.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.square.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.triangle.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.down.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.left.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.right.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.up.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.l1.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.l2.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.r1.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.r2.UpdateGuiElement(renderQueue, clearScreenRequired);

	virtualPadData.select.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.start.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.l3.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.r3.UpdateGuiElement(renderQueue, clearScreenRequired);

	virtualPadData.leftAnalog.UpdateGuiElement(renderQueue, clearScreenRequired);
    virtualPadData.rightAnalog.UpdateGuiElement(renderQueue, clearScreenRequired);

	// Update Graphic Elements off render stack
	// Before we start rendering (if we have to) clear and re-draw the background
    if (!manualRedrawMode || clearScreenRequired || !renderQueue.empty()) 
	{
        wxBufferedDC bdc(&dc, dc.GetSize());
        bdc.SetBrush(*wxRED);
        bdc.DrawRectangle(wxPoint(0, 0), bdc.GetSize());
        bdc.SetBrush(wxNullBrush);
        bdc.DrawBitmap(virtualPadData.background.image, virtualPadData.background.coords, true);
        clearScreenRequired = false;

		// Switch to Manual Rendering once the first user action on the VirtualPad is taken
		if (!manualRedrawMode && !renderQueue.empty()) {
            // DevCon.WriteLn("Paint Event Unbound");
            Unbind(wxEVT_PAINT, &VirtualPad::OnPaint, this);
            manualRedrawMode = true;
        }

		// NOTE - there is yet another (and I think final) micro-optimization that can be done:
		// It can be assumed that if the element has already been drawn to the screen (and not cleared) that we can skip rendering it
		//
		// For example - you hold a single button for several frames, it will currently draw that every frame
		// despite the screen never being cleared, this is not strictly necessary.
		//
		// After some tests, the performance impact is well within reason, and on the hardware renderer modes, is almost non-existant.
		while (!renderQueue.empty()) {
            VirtualPadElement *element = renderQueue.front();
            if (element) 
			{
                element->Render(bdc);
            }
            renderQueue.pop();
        }
	}
}

bool VirtualPad::UpdateControllerData(u16 const bufIndex, PadData *padData)
{
    return virtualPadData.UpdateVirtualPadData(bufIndex, padData, ignoreRealController && !readOnlyMode, readOnlyMode);
}

void VirtualPad::enablePadElements(bool enable)
{
    virtualPadData.circle.EnableWidgets(enable);
    virtualPadData.cross.EnableWidgets(enable);
    virtualPadData.square.EnableWidgets(enable);
    virtualPadData.triangle.EnableWidgets(enable);
    virtualPadData.down.EnableWidgets(enable);
    virtualPadData.left.EnableWidgets(enable);
    virtualPadData.right.EnableWidgets(enable);
    virtualPadData.up.EnableWidgets(enable);
    virtualPadData.l1.EnableWidgets(enable);
    virtualPadData.l2.EnableWidgets(enable);
    virtualPadData.r1.EnableWidgets(enable);
    virtualPadData.r2.EnableWidgets(enable);

    virtualPadData.select.EnableWidgets(enable);
    virtualPadData.start.EnableWidgets(enable);
    virtualPadData.l3.EnableWidgets(enable);
    virtualPadData.r3.EnableWidgets(enable);

    virtualPadData.leftAnalog.EnableWidgets(enable);
    virtualPadData.rightAnalog.EnableWidgets(enable);
}

void VirtualPad::SetReadOnlyMode()
{
    enablePadElements(true);
    readOnlyMode = true;
}

void VirtualPad::ClearReadOnlyMode()
{
    enablePadElements(false);
    readOnlyMode = false;
}

void VirtualPad::OnIgnoreRealController(wxCommandEvent const &event)
{
	const wxCheckBox* ignoreButton = (wxCheckBox*) event.GetEventObject();
    if (ignoreButton) 
	{
        ignoreRealController = ignoreButton->GetValue();
    }
}

void VirtualPad::OnNormalButtonPress(wxCommandEvent &event)
{
	const wxCheckBox* pressedButton = (wxCheckBox*) event.GetEventObject();
	ControllerNormalButton *eventBtn = buttonElements[pressedButton->GetId()];

	if (pressedButton) 
	{
        eventBtn->pressed = pressedButton->GetValue();
    }

	if (!eventBtn->isControllerPressBypassed) {
        eventBtn->isControllerPressBypassed = true;
	}
}

void VirtualPad::OnPressureButtonPressureChange(wxCommandEvent &event)
{
	const wxSpinCtrl* pressureSpinner = (wxSpinCtrl*) event.GetEventObject();
	ControllerPressureButton *eventBtn = pressureElements[pressureSpinner->GetId()];

	if (pressureSpinner)
	{
        eventBtn->pressure = pressureSpinner->GetValue();
    }
	eventBtn->pressed = eventBtn->pressure > 0;

	if (!eventBtn->isControllerPressureBypassed || !eventBtn->isControllerPressBypassed) {
		eventBtn->isControllerPressureBypassed = true;
		eventBtn->isControllerPressBypassed = true;
	}
}

void VirtualPad::OnAnalogSpinnerChange(wxCommandEvent &event)
{
	const wxSpinCtrl* analogSpinner = (wxSpinCtrl*) event.GetEventObject();
	AnalogVector *eventVector = analogElements[analogSpinner->GetId()];

	if (analogSpinner) 
	{
        eventVector->val = analogSpinner->GetValue();
    }
	eventVector->slider->SetValue(eventVector->val);

	if (!eventVector->isControllerBypassed) {
		eventVector->isControllerBypassed = true;
	}
}

void VirtualPad::OnAnalogSliderChange(wxCommandEvent &event)
{
	const wxSlider* analogSlider = (wxSlider*) event.GetEventObject();
	AnalogVector *eventVector = analogElements[analogSlider->GetId()];

	if (analogSlider)
	{
        eventVector->val = analogSlider->GetValue();
    }
	eventVector->spinner->SetValue(eventVector->val);

	if (!eventVector->isControllerBypassed) {
		eventVector->isControllerBypassed = true;
	}
}

/// GUI Element Utility Functions

wxPoint VirtualPad::ScaledPoint(wxPoint point, int widgetWidth, bool rightAligned)
{
	return ScaledPoint(point.x, point.y, widgetWidth, rightAligned);
}

wxPoint VirtualPad::ScaledPoint(int x, int y, int widgetWidth, bool rightAligned)
{
    wxPoint scaledPoint = wxPoint(x * scalingFactor, y * scalingFactor);
    if (rightAligned) {
        scaledPoint.x -= widgetWidth * scalingFactor;
        if (scaledPoint.x < 0) 
		{
            scaledPoint.x = 0;
        }
	}
	return scaledPoint;
}

wxSize VirtualPad::ScaledSize(int x, int y)
{
    return wxSize(x * scalingFactor, y * scalingFactor);
}

ImageFile VirtualPad::NewBitmap(wxImage resource, wxPoint point)
{
	return NewBitmap(scalingFactor, resource, point);
}

ImageFile VirtualPad::NewBitmap(float scalingFactor, wxImage resource, wxPoint point)
{
	wxBitmap bitmap = wxBitmap(resource.Rescale(resource.GetWidth() * scalingFactor, resource.GetHeight() * scalingFactor, wxIMAGE_QUALITY_HIGH));

	ImageFile image = ImageFile();
	image.image = bitmap;
	image.width = bitmap.GetWidth();
	image.height = bitmap.GetHeight();
	image.coords = ScaledPoint(point);
	return image;
}

void VirtualPad::InitNormalButtonGuiElements(ControllerNormalButton &button, ImageFile image, wxWindow *parentWindow, wxPoint point)
{
	button.icon = image;
	button.pressedBox = new wxCheckBox(parentWindow, wxID_ANY, wxEmptyString, ScaledPoint(point), wxDefaultSize);
	Bind(wxEVT_CHECKBOX, &VirtualPad::OnNormalButtonPress, this, button.pressedBox->GetId());
	buttonElements[button.pressedBox->GetId()] = &button;
}

void VirtualPad::InitPressureButtonGuiElements(ControllerPressureButton &button, ImageFile image, wxWindow *parentWindow, wxPoint point, bool rightAlignedPoint)
{
    const int spinnerWidth = 100;
	const wxPoint scaledPoint = ScaledPoint(point.x, point.y, spinnerWidth, rightAlignedPoint);
    wxSpinCtrl *spinner = new wxSpinCtrl(parentWindow, wxID_ANY, wxEmptyString, scaledPoint, ScaledSize(spinnerWidth, wxDefaultSize.GetHeight()), wxSP_ARROW_KEYS, 0, 255, 0);

	button.icon = image;
	button.pressureSpinner = spinner;
	Bind(wxEVT_SPINCTRL, &VirtualPad::OnPressureButtonPressureChange, this, button.pressureSpinner->GetId());
	pressureElements[button.pressureSpinner->GetId()] = &button;
}

void VirtualPad::InitAnalogStickGuiElements(AnalogStick &analog, wxWindow *parentWindow, wxPoint centerPoint, int radius, wxPoint xSliderPoint, wxPoint ySliderPoint, bool flipYSlider, wxPoint xSpinnerPoint, wxPoint ySpinnerPoint, bool rightAlignedSpinners)
{
	AnalogPosition analogPos = AnalogPosition();
	analogPos.centerCoords = ScaledPoint(centerPoint);
	analogPos.endCoords = ScaledPoint(centerPoint);
	analogPos.radius = radius * scalingFactor;
	analogPos.lineThickness = 6 * scalingFactor;

	const int spinnerWidth = 90;
	const wxPoint xSpinnerScaledPoint = ScaledPoint(xSpinnerPoint, spinnerWidth, rightAlignedSpinners);
    const wxPoint ySpinnerScaledPoint = ScaledPoint(ySpinnerPoint, spinnerWidth, rightAlignedSpinners);

	wxSlider *xSlider = new wxSlider(parentWindow, wxID_ANY, 127, 0, 255, ScaledPoint(xSliderPoint), ScaledSize(185, 30));
    wxSlider *ySlider = new wxSlider(parentWindow, wxID_ANY, 127, 0, 255, ScaledPoint(ySliderPoint), ScaledSize(30, 185), flipYSlider ? wxSL_LEFT : wxSL_RIGHT);
	wxSpinCtrl *xSpinner = new wxSpinCtrl(parentWindow, wxID_ANY, wxEmptyString, xSpinnerScaledPoint, ScaledSize(90, wxDefaultSize.GetHeight()), wxSP_ARROW_KEYS, 0, 255, 127);
	wxSpinCtrl *ySpinner = new wxSpinCtrl(parentWindow, wxID_ANY, wxEmptyString, ySpinnerScaledPoint, ScaledSize(90, wxDefaultSize.GetHeight()), wxSP_ARROW_KEYS, 0, 255, 127);

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
}
