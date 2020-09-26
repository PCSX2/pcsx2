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

#ifndef DISABLE_RECORDING

#include <wx/spinctrl.h>

#include "Recording/VirtualPad/VirtualPadResources.h"
#include "Recording/PadData.h"

wxCommandEvent VirtualPadElement::ConstructEvent(wxEventTypeTag<wxCommandEvent> eventType, wxWindow* obj)
{
	wxCommandEvent event(eventType, obj->GetId());
	event.SetEventObject(obj);
	return event;
}

wxCommandEvent VirtualPadElement::ConstructEvent(wxEventTypeTag<wxSpinEvent> eventType, wxWindow* obj)
{
	wxCommandEvent event(eventType, obj->GetId());
	event.SetEventObject(obj);
	return event;
}

void ControllerNormalButton::UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired)
{
	// This boolean is set when we parse the PadData in VirtualPadData::UpdateVirtualPadData
	// Updating wxWidget elements can be expensive, we only want to do this if required
	if (m_widgetUpdateRequired)
		m_pressedBox->SetValue(m_pressed);

	// We only render the button if it is pressed
	if (m_pressed)
		renderQueue.push(this);
	// However, if the button has been drawn to the screen in the past
	// we need to ensure the screen is cleared.
	// This is needed in the scenario where only a single button is being pressed/released
	// As no other elements will trigger a clear
	else if (m_currentlyRendered)
	{
		m_currentlyRendered = false;
		clearScreenRequired = true;
	}
}

void ControllerPressureButton::UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired)
{
	if (m_widgetUpdateRequired)
		m_pressureSpinner->SetValue(m_pressure);

	if (m_pressed)
		renderQueue.push(this);
	else if (m_currentlyRendered)
	{
		m_currentlyRendered = false;
		clearScreenRequired = true;
	}
}

void AnalogStick::UpdateGuiElement(std::queue<VirtualPadElement*>& renderQueue, bool& clearScreenRequired)
{
	if (m_xVector.m_widgetUpdateRequired)
	{
		m_xVector.m_slider->SetValue(m_xVector.m_val);
		m_xVector.m_spinner->SetValue(m_xVector.m_val);
	}
	if (m_yVector.m_widgetUpdateRequired)
	{
		m_yVector.m_slider->SetValue(m_yVector.m_val);
		m_yVector.m_spinner->SetValue(m_yVector.m_val);
	}

	// We render the analog sticks as long as they are not in the neutral position
	if (!(m_xVector.m_val == PadData::s_ANALOG_VECTOR_NEUTRAL && m_yVector.m_val == PadData::s_ANALOG_VECTOR_NEUTRAL))
		renderQueue.push(this);
	else if (m_currentlyRendered)
	{
		m_currentlyRendered = false;
		clearScreenRequired = true;
	}
}

void ControllerNormalButton::EnableWidgets(bool enable)
{
	m_pressedBox->Enable(enable);
}

void ControllerPressureButton::EnableWidgets(bool enable)
{
	m_pressureSpinner->Enable(enable);
}

void AnalogStick::EnableWidgets(bool enable)
{
	m_xVector.m_slider->Enable(enable);
	m_yVector.m_slider->Enable(enable);
	m_xVector.m_spinner->Enable(enable);
	m_yVector.m_spinner->Enable(enable);
}

void ControllerNormalButton::Render(wxDC& dc)
{
	dc.DrawBitmap(m_icon.m_image, m_icon.m_coords, true);
	m_currentlyRendered = true;
}

void ControllerPressureButton::Render(wxDC& dc)
{
	dc.DrawBitmap(m_icon.m_image, m_icon.m_coords, true);
	m_currentlyRendered = true;
}

void AnalogStick::Render(wxDC& dc)
{
	// Render graphic
	AnalogPosition analogPos = m_positionGraphic;
	// Determine new end coordinates
	int newXCoord = analogPos.m_centerCoords.x + ((m_xVector.m_val - 127) / 127.0) * analogPos.m_radius;
	int newYCoord = analogPos.m_centerCoords.y + ((m_yVector.m_val - 127) / 127.0) * analogPos.m_radius;
	// We want to ensure the line segment length is capped at the defined radius
	// NOTE - The conventional way to do this is using arctan2, but the analog values that come out
	// of the Pad plugins in pcsx2 do not permit this, the coordinates returned do not define a circle.
	const float lengthOfLine = sqrt(pow(newXCoord - analogPos.m_centerCoords.x, 2) + pow(newYCoord - analogPos.m_centerCoords.y, 2));
	if (lengthOfLine > analogPos.m_radius)
	{
		newXCoord = ((1 - analogPos.m_radius / lengthOfLine) * analogPos.m_centerCoords.x) + analogPos.m_radius / lengthOfLine * newXCoord;
		newYCoord = ((1 - analogPos.m_radius / lengthOfLine) * analogPos.m_centerCoords.y) + analogPos.m_radius / lengthOfLine * newYCoord;
	}
	// Set the new end coordinate
	analogPos.m_endCoords = wxPoint(newXCoord, newYCoord);
	// Draw line and tip
	dc.SetPen(wxPen(*wxBLUE, analogPos.m_lineThickness));
	dc.DrawLine(analogPos.m_centerCoords, analogPos.m_endCoords);
	dc.DrawCircle(analogPos.m_endCoords, wxCoord(analogPos.m_lineThickness));
	dc.SetPen(wxNullPen);
	m_currentlyRendered = true;
}

void ControllerNormalButton::Reset(wxEvtHandler* destWindow)
{
	m_pressedBox->SetValue(false);
	wxPostEvent(destWindow, ConstructEvent(wxEVT_CHECKBOX, m_pressedBox));
}

void ControllerPressureButton::Reset(wxEvtHandler* destWindow)
{
	m_pressureSpinner->SetValue(0);
	wxPostEvent(destWindow, ConstructEvent(wxEVT_SPINCTRL, m_pressureSpinner));
}

void AnalogStick::Reset(wxEvtHandler* destWindow)
{
	m_xVector.m_slider->SetValue(127);
	m_yVector.m_slider->SetValue(127);
	wxPostEvent(destWindow, ConstructEvent(wxEVT_SLIDER, m_xVector.m_slider));
	wxPostEvent(destWindow, ConstructEvent(wxEVT_SLIDER, m_yVector.m_slider));
	m_xVector.m_spinner->SetValue(127);
	m_xVector.m_spinner->SetValue(127);
	wxPostEvent(destWindow, ConstructEvent(wxEVT_SPINCTRL, m_xVector.m_spinner));
	wxPostEvent(destWindow, ConstructEvent(wxEVT_SPINCTRL, m_yVector.m_spinner));
}

bool ControllerNormalButton::UpdateData(bool& padDataVal, bool ignoreRealController, bool readOnly)
{
	return this->UpdateButtonData(padDataVal, ignoreRealController, readOnly);
}

bool ControllerPressureButton::UpdateData(bool& padDataVal, bool ignoreRealController, bool readOnly)
{
	return this->UpdateButtonData(padDataVal, ignoreRealController, readOnly);
}

bool ControllerButton::UpdateButtonData(bool& padDataVal, bool ignoreRealController, bool readOnly)
{
	if (!ignoreRealController || readOnly)
	{
		// If controller is being bypassed and controller's state has changed
		const bool bypassedWithChangedState = m_isControllerPressBypassed && padDataVal != m_prevPressedVal;
		if (bypassedWithChangedState)
		{
			m_prevPressedVal = padDataVal;
			m_isControllerPressBypassed = false;
		}
		// If we aren't bypassing the controller OR the previous condition was met
		if (bypassedWithChangedState || !m_isControllerPressBypassed || readOnly)
		{
			m_widgetUpdateRequired = m_pressed != padDataVal;
			m_pressed = padDataVal;
			return false;
		}
	}
	// Otherwise, we update the real PadData value, which will in turn be used to update the interrupt's buffer
	m_prevPressedVal = padDataVal;
	padDataVal = m_pressed;
	return m_prevPressedVal != m_pressed;
}

bool ControllerPressureButton::UpdateData(u8& padDataVal, bool ignoreRealController, bool readOnly)
{
	if (!ignoreRealController || readOnly)
	{
		const bool bypassedWithChangedState = m_isControllerPressureBypassed && padDataVal != m_prevPressureVal;
		if (bypassedWithChangedState)
		{
			m_prevPressureVal = padDataVal;
			m_isControllerPressureBypassed = false;
		}
		if (bypassedWithChangedState || !m_isControllerPressureBypassed || readOnly)
		{
			m_widgetUpdateRequired = m_pressure != padDataVal;
			m_pressure = padDataVal;
			return false;
		}
	}
	m_prevPressureVal = padDataVal;
	padDataVal = m_pressure;
	return m_prevPressureVal != m_pressure;
}

bool AnalogVector::UpdateData(u8& padDataVal, bool ignoreRealController, bool readOnly)
{
	if (!ignoreRealController || readOnly)
	{
		const bool bypassedWithChangedState = m_isControllerBypassed && padDataVal != m_prevVal;
		if (bypassedWithChangedState)
		{
			m_prevVal = padDataVal;
			m_isControllerBypassed = false;
		}
		if (bypassedWithChangedState || !m_isControllerBypassed || readOnly)
		{
			m_widgetUpdateRequired = m_val != padDataVal;
			m_val = padDataVal;
			return false;
		}
	}
	m_prevVal = padDataVal;
	padDataVal = m_val;
	return m_prevVal != m_val;
}

#endif
