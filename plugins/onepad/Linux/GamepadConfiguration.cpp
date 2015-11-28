/*  GamepadConfiguration.cpp
 *  PCSX2 Dev Team
 *  Copyright (C) 2015
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "GamepadConfiguration.h"

// Construtor of GamepadConfiguration
GamepadConfiguration::GamepadConfiguration(int pad, wxWindow *parent) : wxFrame(
    parent, // Parent
    wxID_ANY, // ID
    _T("Gamepad configuration"), // Title
    wxDefaultPosition, // Position
    wxSize(400, 200), // Width + Lenght
    // Style
    wxSYSTEM_MENU |
    wxCAPTION |
    wxCLOSE_BOX |
    wxCLIP_CHILDREN
                        )
{

    this->pad_id = pad;
    this->pan_gamepad_config = new wxPanel(
        this, // Parent
        wxID_ANY, // ID
        wxDefaultPosition, // Prosition
        wxSize(300, 200) // Size
    );
    this->cb_rumble = new wxCheckBox(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        _T("&Enable rumble"), // Label
        wxPoint(20, 20) // Position
    );

    this->cb_hack_sixaxis_usb = new wxCheckBox(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        _T("&Hack: Sixaxis/DS3 plugged in USB"), // Label
        wxPoint(20, 40) // Position
    );

    this->cb_hack_sixaxis_pressure = new wxCheckBox(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        _T("&Hack: Sixaxis/DS3 pressure"), // Label
        wxPoint(20, 60) // Position
    );

    wxString txt_rumble = wxT("Rumble intensity");
    this->lbl_rumble_intensity = new wxStaticText(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        txt_rumble, // Text which must be displayed
        wxPoint(20, 90), // Position
        wxDefaultSize // Size
    );

    this->sl_rumble_intensity = new wxSlider(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        0, // value
        0, // min value 0x0000
        0x7FFF,  // max value 0x7FFF
        wxPoint(150, 83), // Position
        wxSize(200, 30) // Size
    );

    wxString txt_joystick = wxT("Joystick sensibility");
    this->lbl_rumble_intensity = new wxStaticText(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        txt_joystick, // Text which must be displayed
        wxPoint(20, 120), // Position
        wxDefaultSize // Size
    );

    this->sl_joystick_sensibility = new wxSlider(
        this->pan_gamepad_config, // Parent
        wxID_ANY, // ID
        0, // value
        0, // min value
        100,  // max value
        wxPoint(150, 113), // Position
        wxSize(200, 30) // Size
    );

    this->bt_ok = new wxButton(
      this->pan_gamepad_config, // Parent
      wxID_ANY, // ID
      _T("&OK"), // Label
      wxPoint(250, 160), // Position
      wxSize(60,25) // Size
    );

    this->bt_cancel = new wxButton(
      this->pan_gamepad_config, // Parent
      wxID_ANY, // ID
      _T("&Cancel"), // Label
      wxPoint(320, 160), // Position
      wxSize(60,25) // Size
    );

    // Connect the buttons to the OnButtonClicked Event
    this->Connect(
        wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(GamepadConfiguration::OnButtonClicked)
    );
    // Connect the sliders to the OnSliderReleased Event
    this->Connect(
        wxEVT_SCROLL_THUMBRELEASE,
        wxCommandEventHandler(GamepadConfiguration::OnSliderReleased)
    );
    // Connect the checkboxes to the OnCheckboxClicked Event
    this->Connect(
        wxEVT_CHECKBOX,
        wxCommandEventHandler(GamepadConfiguration::OnCheckboxChange)
    );
}

/**
    Initialize the frame
    Check if a gamepad is detected
    Check if the gamepad support rumbles
*/
void GamepadConfiguration::InitGamepadConfiguration()
{
    this->repopulate(); // Set label and fit simulated key array
    /*
     * Check if there exist at least one pad available
     * if the pad id is 0, you need at least 1 gamepad connected,
     * if the pad id is 1, you need at least 2 gamepad connected,
     * Prevent to use a none initialized value on s_vgamePad (core dump)
    */
    if(s_vgamePad.size() >= this->pad_id+1)
    {
        /*
         * Determine if the device can use rumble
         * Use TestForce with a very low strength (can't be felt)
         * May be better to create a new function in order to check only that
        */

        if(!s_vgamePad[this->pad_id]->TestForce(0.001f))
        {
            wxMessageBox("Rumble is not available for your device.");
            this->cb_rumble->Disable(); // disable the rumble checkbox
            this->sl_rumble_intensity->Disable(); // disable the rumble intensity slider
        }
    }
    else
    {
        wxMessageBox("No gamepad detected.");
        this->sl_joystick_sensibility->Disable(); // disable the joystick sensibility slider
        this->cb_rumble->Disable(); // disable the rumble checkbox
        this->sl_rumble_intensity->Disable(); // disable the rumble intensity slider
    }
}

/****************************************/
/*********** Events functions ***********/
/****************************************/

/**
 * Button event, called when a button is clicked
*/
void GamepadConfiguration::OnButtonClicked(wxCommandEvent &event)
{
    // Affichage d'un message à chaque clic sur le bouton
    wxButton* bt_tmp = (wxButton*)event.GetEventObject(); // get the button object
    int bt_id = bt_tmp->GetId(); // get the real ID
    if(bt_id == this->bt_ok->GetId()) // If the button ID is equals to the Ok button ID
    {
        this->Close(); // Close the window
    }
    else if(bt_id == this->bt_cancel->GetId()) // If the button ID is equals to the cancel button ID
    {
        this->reset(); // reinitialize the value of each parameters
        this->Close(); // Close the window
    }
}

/**
 * Slider event, called when the use release the slider button
 * @FIXME The current solution can't change the joystick sensibility and the rumble intensity
 *        for a specific gamepad. The same value is used for both
*/
void GamepadConfiguration::OnSliderReleased(wxCommandEvent &event)
{
    wxSlider* sl_tmp = (wxSlider*)event.GetEventObject(); // get the slider object
    int sl_id = sl_tmp->GetId(); // slider id
    if(sl_id == this->sl_rumble_intensity->GetId()) // if this is the rumble intensity slider
    {
        u32 intensity = this->sl_rumble_intensity->GetValue();  // get the new value
        conf->set_ff_intensity(intensity); // and set the force feedback intensity value with it
         // get the rumble intensity
        float strength = this->sl_rumble_intensity->GetValue();
        /*
        * convert in a float value between 0 and 1, and run rumble feedback
        * 1 -> 0x7FFF
        * 0 -> 0x0000
        * x -> ?
        *
        * formula : strength = x*1/0x7FFF
        * x : intensity variable
        * 0x7FFF : maximum intensity
        * 1 : maximum value of the intensity for the sdl rumble test
        */
        s_vgamePad[this->pad_id]->TestForce(strength/0x7FFF);
    }
    else if(sl_id == this->sl_joystick_sensibility->GetId())
    {
        u32 sensibility = this->sl_joystick_sensibility->GetValue(); // get the new value
        conf->set_sensibility(sensibility); // and set the joystick sensibility
    }
}

/**
 * Checkbox event, called when the value of the checkbox change
*/
void GamepadConfiguration::OnCheckboxChange(wxCommandEvent& event)
{
    wxCheckBox* cb_tmp = (wxCheckBox*) event.GetEventObject(); // get the slider object
    int cb_id = cb_tmp->GetId();
    if(cb_id == this->cb_rumble->GetId())
    {
        conf->pad_options[this->pad_id].forcefeedback = (this->cb_rumble->GetValue())?(u32)1:(u32)0;
        if(this->cb_rumble->GetValue())
        {
            s_vgamePad[this->pad_id]->TestForce();
            this->sl_rumble_intensity->Enable();
        }
        else
        {
            this->sl_rumble_intensity->Disable();
        }
    }
    else if(cb_id == this->cb_hack_sixaxis_usb->GetId())
    {
        conf->pad_options[this->pad_id].sixaxis_usb = (this->cb_hack_sixaxis_usb->GetValue())?(u32)1:(u32)0;
    }
    else if(cb_id == this->cb_hack_sixaxis_pressure->GetId())
    {
        conf->pad_options[this->pad_id].sixaxis_pressure = (this->cb_hack_sixaxis_pressure->GetValue())?(u32)1:(u32)0;
    }
}

/****************************************/
/*********** Methods functions **********/
/****************************************/

// Reset checkbox and slider values
void GamepadConfiguration::reset()
{
    this->cb_rumble->SetValue(this->init_rumble);
    this->cb_hack_sixaxis_usb->SetValue(this->init_hack_sixaxis);
    this->sl_rumble_intensity->SetValue(this->init_rumble_intensity);
    this->sl_joystick_sensibility->SetValue(this->init_joystick_sensibility);
}

// Set button values
void GamepadConfiguration::repopulate()
{
    bool val = conf->pad_options[this->pad_id].forcefeedback;
    this->init_rumble = val;
    this->cb_rumble->SetValue(val);
    val = conf->pad_options[this->pad_id].sixaxis_usb;
    this->init_hack_sixaxis = val;
    this->cb_hack_sixaxis_usb->SetValue(val);
    int tmp = conf->get_ff_intensity();
    this->sl_rumble_intensity->SetValue(tmp);
    this->init_rumble_intensity = tmp;
    tmp = conf->get_sensibility();
    this->sl_joystick_sensibility->SetValue(tmp);
    this->init_joystick_sensibility = tmp;

    // enable rumble intensity slider if the checkbox is checked
    if(this->cb_rumble->GetValue())
        this->sl_rumble_intensity->Enable();
    else // disable otherwise
        this->sl_rumble_intensity->Disable();
}
