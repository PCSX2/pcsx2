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

#include <map>
#include <queue>

#include "Pcsx2Types.h"
#include "wx/checkbox.h"
#include "wx/frame.h"
#include "wx/window.h"

#include "Recording/PadData.h"
#include "Recording/VirtualPad/VirtualPadData.h"

class VirtualPad : public wxFrame
{
public:
	VirtualPad(wxWindow *parent, wxWindowID id, const wxString& title, int controllerPort, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE);
	// Updates the VirtualPad if necessary, as well as updates the PadData fields if the VirtualPad is actively overriding them
	bool UpdateControllerData(u16 const bufIndex, PadData *padData, bool readOnly = false);
	void Redraw();

private:
    bool manualRedrawMode = false;
    bool clearScreenRequired = false;

    std::queue<VirtualPadElement*> renderQueue;

	/// GUI Creation Utility Functions
	float scalingFactor = 1.0;

    wxSize ScaledSize(int x, int y);
    wxPoint ScaledPoint(wxPoint point, int widgetWidth = 0, bool rightAligned = false);
    wxPoint ScaledPoint(int x, int y, int widgetWidth = 0, bool rightAligned = false);

	ImageFile NewBitmap(wxImage resource, wxPoint point);
	ImageFile NewBitmap(float scalingFactor, wxImage resource, wxPoint point);

	void InitPressureButtonGuiElements(ControllerPressureButton &button, ImageFile image, wxWindow *parentWindow, wxPoint point, bool rightAlignedPoint = false);
	void InitNormalButtonGuiElements(ControllerNormalButton &btn, ImageFile image, wxWindow *parentWindow, wxPoint point);
	void InitAnalogStickGuiElements(AnalogStick &analog, wxWindow *parentWindow, wxPoint centerPoint, int radius, wxPoint xSliderPoint, wxPoint ySliderPoint, bool flipYSlider, wxPoint xSpinnerPoint, wxPoint ySpinnerPoint, bool rightAlignedSpinners = false);

	/// GUI Elements
	wxCheckBox *ignoreRealControllerBox;

	// Data
	std::map<wxWindowID, ControllerNormalButton*> buttonElements;
	std::map<wxWindowID, ControllerPressureButton*> pressureElements;
	std::map<wxWindowID, AnalogVector*> analogElements;
	
	bool ignoreRealController = false;
	VirtualPadData virtualPadData;

	bool renderGraphics = false;
	int imgWrites = 0;
	int analogWrites = 0;

	// Events
	void OnEraseBackground(wxEraseEvent& event);
	void OnPaint(wxPaintEvent & evt);
	void Render(wxDC& dc);
	void OnClose(wxCloseEvent &event);
	void OnMouseEvent(wxMouseEvent &event);
	void OnFocusEvent(wxFocusEvent &event);

	void OnNormalButtonPress(wxCommandEvent &event);
	void OnPressureButtonPressureChange(wxCommandEvent &event);
	void OnAnalogSliderChange(wxCommandEvent &event);
	void OnAnalogSpinnerChange(wxCommandEvent &event);
	void OnIgnoreRealController(wxCommandEvent const &event);
};
