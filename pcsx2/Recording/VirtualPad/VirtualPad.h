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

#pragma once

#ifndef PCSX2_CORE

#include <map>
#include <queue>

#include "gui/AppConfig.h"
#include "common/Pcsx2Types.h"

#include "wx/button.h"
#include "wx/checkbox.h"
#include "wx/dc.h"
#include "wx/event.h"
#include "wx/frame.h"
#include "wx/gdicmn.h"
#include "wx/string.h"
#include "wx/window.h"
#include "wx/windowid.h"

#include "Recording/PadData.h"
#include "Recording/VirtualPad/VirtualPadData.h"

class VirtualPad : public wxFrame
{
public:
	VirtualPad(wxWindow* parent, int controllerPort, AppConfig::InputRecordingOptions& options);
	// Updates the VirtualPad's data if necessary, as well as updates the provided PadData if the VirtualPad overrides it
	// - PadData will not be updated if ReadOnly mode is set
	// - returns a bool to indicate if the PadData has been updated
	bool UpdateControllerData(u16 const bufIndex, PadData* padData);
	// Enables/Disables read only mode and enables/disables GUI widgets
	void SetReadOnlyMode(bool readOnly);
	// To be called at maximum, once per frame to update widget's value and re-render the VirtualPad's graphics
	void Redraw();

private:
	/// Constants
	const wxSize SPINNER_SIZE = wxSize(100, 40);
	static const int ANALOG_SLIDER_WIDTH = 185;
	static const int ANALOG_SLIDER_HEIGHT = 30;

	static const int PRESSURE_MAX = 255;
	static const int ANALOG_NEUTRAL = 127;
	static const int ANALOG_MAX = 255;

	AppConfig::InputRecordingOptions& options;

	bool clearScreenRequired = false;
	bool ignoreRealController = false;
	// When enabled, forces the VirtualPad to be re-rendered even if no updates are made.
	// This helps to make sure the UI is rendered prior to receiving data from the controller
	bool manualRedrawMode = false;
	bool readOnlyMode = false;

	VirtualPadData virtualPadData;

	std::vector<VirtualPadElement*> virtualPadElements;
	std::queue<VirtualPadElement*> renderQueue;

	void enableUiElements(bool enable);

	/// GUI Elements
	wxCheckBox* ignoreRealControllerBox;
	wxButton* resetButton;

	std::map<wxWindowID, ControllerNormalButton*> buttonElements;
	std::map<wxWindowID, ControllerPressureButton*> pressureElements;
	std::map<wxWindowID, AnalogVector*> analogElements;

	/// Event Listeners
	void OnMoveAround(wxMoveEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnIconize(wxIconizeEvent& event);
	void OnEraseBackground(wxEraseEvent& event);
	void OnPaint(wxPaintEvent& event);
	void Render(wxDC& dc);

	void OnAnalogSliderChange(wxCommandEvent& event);
	void OnAnalogSpinnerChange(wxCommandEvent& event);
	void OnIgnoreRealController(wxCommandEvent& event);
	void OnNormalButtonPress(wxCommandEvent& event);
	void OnPressureButtonPressureChange(wxCommandEvent& event);
	void OnResetButton(wxCommandEvent& event);

	/// GUI Creation Utility Functions
	float scalingFactor = 1.0;
	bool floatCompare(float A, float B, float epsilon = 0.005f);

	wxSize ScaledSize(wxSize size);
	wxSize ScaledSize(int x, int y);
	wxPoint ScaledPoint(wxPoint point, wxSize widgetSize = wxDefaultSize, bool rightAlignedCoord = false, bool bottomAlignedCoord = false);
	wxPoint ScaledPoint(int x, int y, int widgetWidth, int widgetHeight, bool rightAlignedCoord = false, bool bottomAlignedCoord = false);

	ImageFile NewBitmap(wxImage resource, wxPoint imgCoord, bool dontScale = false);
	ImageFile NewBitmap(float scalingFactor, wxImage resource, wxPoint imgCoord);

	void InitPressureButtonGuiElements(ControllerPressureButton& button, ImageFile image, wxWindow* parentWindow, wxPoint pressureSpinnerCoord, bool rightAlignedCoord = false, bool bottomAlignedCoord = false);
	void InitNormalButtonGuiElements(ControllerNormalButton& btn, ImageFile image, wxWindow* parentWindow, wxPoint checkboxCoord);
	void InitAnalogStickGuiElements(AnalogStick& analog, wxWindow* parentWindow, wxPoint centerPoint, int radius, wxPoint xSliderPoint,
									wxPoint ySliderPoint, bool flipYSlider, wxPoint xSpinnerPoint, wxPoint ySpinnerPoint, bool rightAlignedSpinners = false);
};

#endif
