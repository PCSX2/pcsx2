/*  JoystickConfiguration.cpp
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

#include "JoystickConfiguration.h"

// Construtor of JoystickConfiguration
JoystickConfiguration::JoystickConfiguration(int pad, bool left, wxWindow *parent) : wxFrame(
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

    m_pad_id = pad;
    m_isForLeftJoystick = left;
    m_pan_joystick_config = new wxPanel(
        this, // Parent
        wxID_ANY, // ID
        wxDefaultPosition, // Prosition
        wxSize(300, 200) // Size
    );

    if(m_isForLeftJoystick)
    {
        m_cb_reverse_Lx = new wxCheckBox(
            m_pan_joystick_config, // Parent
            wxID_ANY, // ID
            _T("Reverse Lx"), // Label
            wxPoint(20, 20) // Position
        );

        m_cb_reverse_Ly = new wxCheckBox(
            m_pan_joystick_config, // Parent
            wxID_ANY, // ID
            _T("Reverse Ly"), // Label
            wxPoint(20, 40) // Position
        );

        m_cb_mouse_Ljoy = new wxCheckBox(
            m_pan_joystick_config, // Parent
            wxID_ANY, // ID
            _T("Use mouse for left analog joystick"), // Label
            wxPoint(20, 60) // Position
        );
    }
    else
    {
        m_cb_reverse_Rx = new wxCheckBox(
            m_pan_joystick_config, // Parent
            wxID_ANY, // ID
            _T("Reverse Rx"), // Label
            wxPoint(20, 20) // Position
        );

        m_cb_reverse_Ry = new wxCheckBox(
            m_pan_joystick_config, // Parent
            wxID_ANY, // ID
            _T("Reverse Ry"), // Label
            wxPoint(20, 40) // Position
        );

        m_cb_mouse_Rjoy = new wxCheckBox(
            m_pan_joystick_config, // Parent
            wxID_ANY, // ID
            _T("Use mouse for right analog joystick"), // Label
            wxPoint(20, 60) // Position
        );
    }

    m_bt_ok = new wxButton(
      m_pan_joystick_config, // Parent
      wxID_ANY, // ID
      _T("&OK"), // Label
      wxPoint(250, 130), // Position
      wxSize(60,25) // Size
    );

    m_bt_cancel = new wxButton(
      m_pan_joystick_config, // Parent
      wxID_ANY, // ID
      _T("&Cancel"), // Label
      wxPoint(320, 130), // Position
      wxSize(60,25) // Size
    );

    // Connect the buttons to the OnButtonClicked Event
    Connect(
        wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(JoystickConfiguration::OnButtonClicked)
    );

    // Connect the checkboxes to the OnCheckboxClicked Event
    #if wxMAJOR_VERSION >= 3
        Connect(
            wxEVT_CHECKBOX,
            wxCommandEventHandler(JoystickConfiguration::OnCheckboxChange)
        );
    #else
        Connect(
            wxEVT_COMMAND_CHECKBOX_CLICKED,
            wxCommandEventHandler(JoystickConfiguration::OnCheckboxChange)
        );
    #endif
}

/**
    Initialize the frame
    Check if a gamepad is detected
*/
void JoystickConfiguration::InitJoystickConfiguration()
{
    repopulate(); // Set label and fit simulated key array
    /*
     * Check if there exist at least one pad available
     * if the pad id is 0, you need at least 1 gamepad connected,
     * if the pad id is 1, you need at least 2 gamepad connected,
     * Prevent to use a none initialized value on s_vgamePad (core dump)
    */
    if(s_vgamePad.size() < m_pad_id+1)
    {
        wxMessageBox(L"No gamepad detected.");
        // disable all checkbox
        if(m_isForLeftJoystick)
        {
            m_cb_reverse_Lx->Disable();
            m_cb_reverse_Ly->Disable();
        }
        else
        {
            m_cb_reverse_Rx->Disable();
            m_cb_reverse_Ry->Disable();
        }
    }
}

/****************************************/
/*********** Events functions ***********/
/****************************************/

/**
 * Button event, called when a button is clicked
*/
void JoystickConfiguration::OnButtonClicked(wxCommandEvent &event)
{
    // Affichage d'un message à chaque clic sur le bouton
    wxButton* bt_tmp = (wxButton*)event.GetEventObject(); // get the button object
    int bt_id = bt_tmp->GetId(); // get the real ID
    if(bt_id == m_bt_ok->GetId()) // If the button ID is equals to the Ok button ID
    {
        Close(); // Close the window
    }
    else if(bt_id == m_bt_cancel->GetId()) // If the button ID is equals to the cancel button ID
    {
        reset(); // reinitialize the value of each parameters
        Close(); // Close the window
    }
}

/**
 * Checkbox event, called when the value of the checkbox change
*/
void JoystickConfiguration::OnCheckboxChange(wxCommandEvent& event)
{
    wxCheckBox* cb_tmp = (wxCheckBox*) event.GetEventObject(); // get the slider object
    int cb_id = cb_tmp->GetId();
    bool val;
    if(m_isForLeftJoystick)
    {
        if(cb_id == m_cb_reverse_Ly->GetId())
        {
            val = m_cb_reverse_Ly->GetValue();
            conf->pad_options[m_pad_id].reverse_ly = val;
        }
        else if(cb_id == m_cb_reverse_Lx->GetId())
        {
            val = m_cb_reverse_Lx->GetValue();
            conf->pad_options[m_pad_id].reverse_lx = val;
        }
        else if(cb_id == m_cb_mouse_Ljoy->GetId())
        {
            val = m_cb_mouse_Ljoy->GetValue();
            conf->pad_options[m_pad_id].mouse_l = val;
        }
    }
    else
    {
        if(cb_id == m_cb_reverse_Ry->GetId())
        {
            val = m_cb_reverse_Ry->GetValue();
            conf->pad_options[m_pad_id].reverse_ry = val;
        }
        else if(cb_id == m_cb_reverse_Rx->GetId())
        {
            val = m_cb_reverse_Rx->GetValue();
            conf->pad_options[m_pad_id].reverse_rx = val;
        }
        else if(cb_id == m_cb_mouse_Rjoy->GetId())
        {
            val = m_cb_mouse_Rjoy->GetValue();
            conf->pad_options[m_pad_id].mouse_r = val;
        }
    }
}

/****************************************/
/*********** Methods functions **********/
/****************************************/

// Reset checkbox and slider values
void JoystickConfiguration::reset()
{
    if(m_isForLeftJoystick)
    {
        m_cb_reverse_Lx->SetValue(m_init_reverse_Lx);
        m_cb_reverse_Ly->SetValue(m_init_reverse_Ly);
        m_cb_mouse_Ljoy->SetValue(m_init_mouse_Ljoy);
    }
    else
    {
        m_cb_reverse_Rx->SetValue(m_init_reverse_Rx);
        m_cb_reverse_Ry->SetValue(m_init_reverse_Ry);
        m_cb_mouse_Rjoy->SetValue(m_init_mouse_Rjoy);
    }
}

// Set button values
void JoystickConfiguration::repopulate()
{
    bool val;
    if(m_isForLeftJoystick)
    {
        val = conf->pad_options[m_pad_id].reverse_lx;
        m_init_reverse_Lx = val;
        m_cb_reverse_Lx->SetValue(val);

        val = conf->pad_options[m_pad_id].reverse_ly;
        m_init_reverse_Ly = val;
        m_cb_reverse_Ly->SetValue(val);

        val = conf->pad_options[m_pad_id].mouse_l;
        m_init_mouse_Ljoy = val;
        m_cb_mouse_Ljoy->SetValue(val);
    }
    else
    {
        val = conf->pad_options[m_pad_id].reverse_rx;
        m_init_reverse_Rx = val;
        m_cb_reverse_Rx->SetValue(val);

        val = conf->pad_options[m_pad_id].reverse_ry;
        m_init_reverse_Ry = val;
        m_cb_reverse_Ry->SetValue(val);

        val = conf->pad_options[m_pad_id].mouse_r;
        m_init_mouse_Rjoy = val;
        m_cb_mouse_Rjoy->SetValue(val);
    }
}
