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

#include <queue>

struct ImageFile
{
	wxBitmap image;
	wxPoint coords;
	u32 width = 0;
	u32 height = 0;
};

struct AnalogVector
{
    // GUI
    wxSlider *slider = 0;
    wxSpinCtrl *spinner = 0;

    u8 val = 127;

    bool widgetUpdateRequired = false;
    bool isControllerBypassed = false;
    u8 prevVal = 127;

    bool UpdateData(u8 &padDataVal, bool ignoreRealController, bool readOnly);
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

    virtual void UpdateGuiElement(std::queue<VirtualPadElement *> *renderQueue, bool &clearScreenRequired) = 0;
    virtual void Render(wxDC &dc) = 0;
};

class ControllerButton
{
public:
    bool pressed = false;
    bool widgetUpdateRequired = false;
    bool isControllerPressBypassed = false;
    bool prevPressedVal = false;

	bool UpdateButtonData(bool &padDataVal, bool ignoreRealController, bool readOnly);
};

class ControllerNormalButton : public ControllerButton, VirtualPadElement
{
public:
	/// GUI
	ImageFile icon;
	wxCheckBox *pressedBox = 0;

	/// State

	void UpdateGuiElement(std::queue<VirtualPadElement *> *renderQueue, bool &clearScreenRequired) override;
    void Render(wxDC &dc) override;
    bool UpdateData(bool &padDataVal, bool ignoreRealController, bool readOnly);
};

class ControllerPressureButton : public ControllerButton, VirtualPadElement
{
public:
	/// GUI
	ImageFile icon;
	wxSpinCtrl *pressureSpinner = 0;

	u8 pressure = 0;

	/// State Management
	bool isControllerPressureBypassed = false;
	u8 prevPressureVal = 0;

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
