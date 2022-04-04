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

#include <queue>

#include "common/Pcsx2Types.h"
#include "wx/bitmap.h"
#include "wx/checkbox.h"
#include "wx/gdicmn.h"
#include "wx/slider.h"
#include "wx/spinctrl.h"
#include "wx/dcbuffer.h"

struct ImageFile
{
	wxBitmap image;
	wxPoint coords;
	u32 width = 0;
	u32 height = 0;
};

struct AnalogVector
{
	wxSlider* slider = 0;
	wxSpinCtrl* spinner = 0;

	u8 val = 127;

	bool isControllerBypassed = false;
	bool widgetUpdateRequired = false;
	u8 prevVal = 127;

	bool UpdateData(u8& padDataVal, bool ignoreRealController, bool readOnly);
};


struct AnalogPosition
{
	wxPoint centerCoords;
	wxPoint endCoords;

	int lineThickness = 0;
	int radius = 0;
};

class VirtualPadElement
{
public:
	bool currentlyRendered = false;

	wxCommandEvent ConstructEvent(wxEventTypeTag<wxCommandEvent> eventType, wxWindow *obj);
	wxCommandEvent ConstructEvent(wxEventTypeTag<wxSpinEvent> eventType, wxWindow *obj);

	virtual void EnableWidgets(bool enable) = 0;
	virtual void Render(wxDC& dc) = 0;
	virtual void Reset(wxEvtHandler* destWindow) = 0;
	virtual void UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired) = 0;
};

class ControllerButton
{
public:
	bool isControllerPressBypassed = false;
	bool pressed = false;
	bool prevPressedVal = false;
	bool widgetUpdateRequired = false;

	bool UpdateButtonData(bool& padDataVal, bool ignoreRealController, bool readOnly);
};

class ControllerNormalButton : public ControllerButton, public VirtualPadElement
{
public:
	ImageFile icon;
	wxCheckBox* pressedBox = 0;

	bool UpdateData(bool& padDataVal, bool ignoreRealController, bool readOnly);
	void EnableWidgets(bool enable) override;
	void Render(wxDC& dc) override;
	void Reset(wxEvtHandler* destWindow) override;
	void UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired) override;
};

class ControllerPressureButton : public ControllerButton, public VirtualPadElement
{
public:
	ImageFile icon;
	wxSpinCtrl* pressureSpinner = 0;

	u8 pressure = 0;

	bool isControllerPressureBypassed = false;
	u8 prevPressureVal = 0;

	bool UpdateData(bool& padDataVal, bool ignoreRealController, bool readOnly);
	bool UpdateData(u8& padDataVal, bool ignoreRealController, bool readOnly);
	void EnableWidgets(bool enable) override;
	void Render(wxDC& dc) override;
	void Reset(wxEvtHandler* destWindow) override;
	void UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired) override;
};

class AnalogStick : public VirtualPadElement
{
public:
	AnalogVector xVector;
	AnalogVector yVector;

	AnalogPosition positionGraphic;

	void EnableWidgets(bool enable) override;
	void Render(wxDC& dc) override;
	void Reset(wxEvtHandler* destWindow) override;
	void UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired) override;
};

#endif
