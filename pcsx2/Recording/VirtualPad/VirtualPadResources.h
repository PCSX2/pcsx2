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

#pragma once

#include <queue>

#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <wx/spinctrl.h>
#include <wx/dcbuffer.h>
#include <wx/event.h>

#include "Common.h"

struct ImageFile
{
	wxBitmap image;
	wxPoint coords;
	u32 width;
	u32 height;
};

struct AnalogVector
{
    // GUI
    wxSlider *slider;
    wxSpinCtrl *spinner;

    u8 val = 127;

    bool renderRequired = false;
    bool isControllerBypassed = false;
    u8 prevVal = 127;

    bool UpdateData(u8 &padDataVal, bool ignoreRealController, bool readOnly);
};


struct AnalogPosition
{
	wxPoint centerCoords;
	wxPoint endCoords;

	int lineThickness;
	int radius;
};

class VirtualPadElement
{
public:
    virtual void UpdateGuiElement(std::queue<VirtualPadElement *> *renderQueue, bool &clearScreenRequired) = 0;
    virtual void Render(wxDC &dc) = 0;
};

class ControllerNormalButton : VirtualPadElement
{
public:
	/// GUI
	ImageFile icon;
	wxCheckBox *pressedBox;

	bool pressed = false;

	/// State 
	bool renderRequired = false;
	bool isControllerBypassed;
	bool prevPressedVal;

	void UpdateGuiElement(std::queue<VirtualPadElement *> *renderQueue, bool &clearScreenRequired) override;
    void Render(wxDC &dc) override;
    bool UpdateData(bool &padDataVal, bool ignoreRealController, bool readOnly);
};

class ControllerPressureButton : VirtualPadElement
{
public:
	/// GUI
	ImageFile icon;
	wxSpinCtrl *pressureSpinner;

	bool pressed = false;
	u8 pressure;

	/// State Management
	bool renderRequired = false;
	bool isControllerPressBypassed;
	bool isControllerPressureBypassed;
	bool prevPressedVal;
	u8 prevPressureVal;

	void UpdateGuiElement(std::queue<VirtualPadElement *> *renderQueue, bool &clearScreenRequired) override;
    void Render(wxDC &dc) override;
    bool UpdateData(bool &padDataVal, bool ignoreRealController, bool readOnly);
    bool UpdateData(u8 &padDataVal, bool ignoreRealController, bool readOnly);
};

class AnalogStick : VirtualPadElement
{
public:
	AnalogVector xVector;
	AnalogVector yVector;

	AnalogPosition positionGraphic;

	void UpdateGuiElement(std::queue<VirtualPadElement *> *renderQueue, bool &clearScreenRequired) override;
    void Render(wxDC &dc) override;
};
