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

#include "GamepadConfiguration.h"

GamepadConfiguration::GamepadConfiguration(int pad, wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _T("Gamepad"), wxDefaultPosition, wxDefaultSize,
			   wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN)
{
	m_pad_id = pad;

	wxBoxSizer* gamepad_box = new wxBoxSizer(wxVERTICAL);

	wxArrayString choices;
	for (const auto& j : device_manager.devices)
	{
		choices.Add(j->GetName());
	}

	m_joy_map = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
	m_cb_rumble = new wxCheckBox(this, enable_rumble_id, _T("&Enable rumble"));

	wxStaticBoxSizer* rumble_box = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Rumble intensity"));
	m_sl_rumble_intensity = new wxSlider(this, rumble_slider_id, 0, 0, 0x7FFF, wxDefaultPosition, wxDefaultSize,
										 wxSL_HORIZONTAL | wxSL_LABELS | wxSL_BOTTOM);

	wxStaticBoxSizer* joy_box = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Joystick sensibility"));
	m_sl_joystick_sensibility = new wxSlider(this, joy_slider_id, 0, 0, 200, wxDefaultPosition, wxDefaultSize,
											 wxSL_HORIZONTAL | wxSL_LABELS | wxSL_BOTTOM);

	gamepad_box->Add(m_joy_map, wxSizerFlags().Expand().Border(wxALL, 5));
	gamepad_box->Add(m_cb_rumble, wxSizerFlags().Expand());

	rumble_box->Add(m_sl_rumble_intensity, wxSizerFlags().Expand().Border(wxALL, 5));
	joy_box->Add(m_sl_joystick_sensibility, wxSizerFlags().Expand().Border(wxALL, 5));

	gamepad_box->Add(rumble_box, wxSizerFlags().Expand().Border(wxALL, 5));
	gamepad_box->Add(joy_box, wxSizerFlags().Expand().Border(wxALL, 5));

	Bind(wxEVT_UPDATE_UI, &GamepadConfiguration::OnUpdateEvent, this);
	Bind(wxEVT_SCROLL_THUMBRELEASE, &GamepadConfiguration::OnSliderReleased, this);
	Bind(wxEVT_CHECKBOX, &GamepadConfiguration::OnCheckboxChange, this);
	Bind(wxEVT_CHOICE, &GamepadConfiguration::OnChoiceChange, this);

	repopulate();

	SetSizerAndFit(gamepad_box);
}

/**
 * Initialize the frame
 * Check if a gamepad is detected
 * Check if the gamepad support rumbles
 */
void GamepadConfiguration::InitGamepadConfiguration()
{
	repopulate(); // Set label and fit simulated key array
	/*
	 * Check if there exist at least one pad available
	 * if the pad id is 0, you need at least 1 gamepad connected,
	 * if the pad id is 1, you need at least 2 gamepads connected,
	 * Prevent to use a non-initialized value (core dump)
	 */
	if (device_manager.devices.size() >= m_pad_id + 1)
	{
		/*
		 * Determine if the device can use rumble
		 * Use TestForce with a very low strength (can't be felt)
		 * May be better to create a new function in order to check only that
		 */

		// Bad idea. Some connected devices might support rumble but not all connected devices.
		//        if (!device_manager.devices[m_pad_id]->TestForce(0.001f)) {
		//            wxMessageBox(L"Rumble is not available for your device.");
		//            m_cb_rumble->Disable();           // disable the rumble checkbox
		//            m_sl_rumble_intensity->Disable(); // disable the rumble intensity slider
		//        }
	}
	else
	{
		wxMessageBox(L"No gamepad detected.");
		m_sl_joystick_sensibility->Disable(); // disable the joystick sensibility slider
		m_cb_rumble->Disable();               // disable the rumble checkbox
		m_sl_rumble_intensity->Disable();     // disable the rumble intensity slider
	}
}

void GamepadConfiguration::OnUpdateEvent(wxCommandEvent& event)
{
	// Makes sure joystick rumble testing works properly
	SDL_GameControllerUpdate();
}

/**
 * Slider event, called when the use release the slider button
 * @FIXME The current solution can't change the joystick sensibility and the rumble intensity
 *        for a specific gamepad. The same value is used for both
 */
void GamepadConfiguration::OnSliderReleased(wxCommandEvent& event)
{
	wxSlider* sl_tmp = (wxSlider*)event.GetEventObject();
	int sl_id = sl_tmp->GetId();
	if (!sl_tmp->IsEnabled()) // wxCocoa sends events even when the button is disabled
		return;

	if (sl_id == rumble_slider_id)
	{
		float value = static_cast<float>(m_sl_rumble_intensity->GetValue()) / 0x7FFF;
		g_conf.set_ff_intensity(value);

		// convert in a float value between 0 and 1, and run rumble feedback.
		// 0 to 1 scales to 0x0 to 0x7FFF
		device_manager.devices[m_pad_id]->TestForce(value);
	}
	else if (sl_id == joy_slider_id)
	{
		g_conf.set_sensibility(m_sl_joystick_sensibility->GetValue());
	}
}

/**
 * Checkbox event, called when the value of the checkbox change
 */
void GamepadConfiguration::OnCheckboxChange(wxCommandEvent& event)
{
	wxCheckBox* cb_tmp = (wxCheckBox*)event.GetEventObject(); // get the slider object
	int cb_id = cb_tmp->GetId();

	if (cb_id == enable_rumble_id)
	{
		g_conf.pad_options[m_pad_id].forcefeedback = (m_cb_rumble->GetValue()) ? (u32)1 : (u32)0;
		if (m_cb_rumble->GetValue())
		{
			device_manager.devices[m_pad_id]->TestForce();
			m_sl_rumble_intensity->Enable();
		}
		else
		{
			m_sl_rumble_intensity->Disable();
		}
	}
}

/**
 * Checkbox event, called when the value of the choice box change
 */
void GamepadConfiguration::OnChoiceChange(wxCommandEvent& event)
{
	wxChoice* choice_tmp = (wxChoice*)event.GetEventObject();
	int id = choice_tmp->GetSelection();
	if (id != wxNOT_FOUND)
	{
		g_conf.set_joy_uid(m_pad_id, Device::index_to_uid(id));
	}
}

/****************************************/
/*********** Methods functions **********/
/****************************************/

// Set button values
void GamepadConfiguration::repopulate()
{
	m_cb_rumble->SetValue(g_conf.pad_options[m_pad_id].forcefeedback);

	m_sl_rumble_intensity->SetValue(g_conf.get_ff_intensity() * 0x7FFF);
	m_sl_joystick_sensibility->SetValue(g_conf.get_sensibility());

	u32 joyid = Device::uid_to_index(m_pad_id);
	if (joyid < m_joy_map->GetCount() && !m_joy_map->IsEmpty())
		m_joy_map->SetSelection(joyid);

	// enable rumble intensity slider if the checkbox is checked
	m_sl_rumble_intensity->Enable(m_cb_rumble->GetValue());
}
