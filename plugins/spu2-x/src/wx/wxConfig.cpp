/* SPU2-X, A plugin for Emulating the Sound Processing Unit of the Playstation 2
 * Developed and maintained by the Pcsx2 Development Team.
 *
 * Original portions from SPU2ghz are (c) 2008 by David Quintana [gigaherz]
 *
 * SPU2-X is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * SPU2-X is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with SPU2-X.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"
#include "wxConfig.h"

const uint32_t DEFAULT_WIDTH = 600;
const uint32_t DEFAULT_HEIGHT = 400;

Dialog::Dialog(): wxDialog(nullptr, wxID_ANY, "SPU2-X Config", wxDefaultPosition, wxSize(DEFAULT_WIDTH, DEFAULT_HEIGHT))
{
    m_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

    m_top_box = new wxBoxSizer(wxHORIZONTAL);
    m_left_box = new wxBoxSizer(wxVERTICAL);
    m_right_box = new wxBoxSizer(wxVERTICAL);

    m_portaudio_box = new wxBoxSizer(wxVERTICAL);
    m_sdl_box = new wxBoxSizer(wxVERTICAL);

    m_mix_box = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Mixing Settings");
    m_debug_box = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Debug Settings");
    m_output_box = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Output Settings");

    m_left_box->Add(m_mix_box);
    m_left_box->Add(m_debug_box);
    m_right_box->Add(m_output_box);

    // Mixing Settings
    m_mix_box->Add(new wxStaticText(m_panel, wxID_ANY, "Interpolation"));

    m_interpolation.Add("Nearest (Fastest/bad quality)");
    m_interpolation.Add("Linear (Simple/okay sound)");
    m_interpolation.Add("Cubic (Artificial highs)");
    m_interpolation.Add("Hermite (Better highs)");
    m_interpolation.Add("Catmull-Rom (PS2-like/slow)");

    m_inter_select = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_interpolation);
    m_inter_select->SetSelection(Interpolation);

    wxCheckBox *effect_check = new wxCheckBox(m_panel, wxID_ANY, "Disable Effects Processing");
    wxCheckBox *dealias_check = new wxCheckBox(m_panel, wxID_ANY, "Use the de-alias filter(Overemphasizes the highs)");

    m_mix_box->Add(m_inter_select);
    m_mix_box->Add(effect_check);
    m_mix_box->Add(dealias_check);

    // Debug Settings
    wxCheckBox *debug_check = new wxCheckBox(m_panel, wxID_ANY, "Enable Debug Options");
    wxButton *launch_debug_dialog = new wxButton(m_panel, wxID_ANY, "Configure...");

    m_debug_box->Add(debug_check);
    m_debug_box->Add(launch_debug_dialog);

    // Output Settings

    // Module
    m_output_box->Add(new wxStaticText(m_panel, wxID_ANY, "Module"));
    m_module.Add("No Sound (Emulate SPU2 only)");
    m_module.Add("PortAudio (Cross-platform)");
    m_module.Add("SDL Audio (Recommended for PulseAudio)");
    //m_module.Add("Alsa (probably doesn't work)");
    m_module_select = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_module);
    m_module_select->SetSelection(OutputModule);
    m_output_box->Add(m_module_select);

    // Portaudio
    m_portaudio_box->Add(new wxStaticText(m_panel, wxID_ANY, "Portaudio API"));
    #ifdef __linux__
    m_portaudio.Add("ALSA (recommended)");
    m_portaudio.Add("OSS (legacy)");
    m_portaudio.Add("JACK");
    #elif defined(__APPLE__)
    m_portaudio.Add("CoreAudio");
    #else
    m_portaudio.Add("OSS");
    #endif
    m_portaudio_select = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_portaudio);
    m_portaudio_select->SetSelection(OutputAPI);
    m_portaudio_box->Add(m_portaudio_select);

    // SDL
    m_sdl_box->Add(new wxStaticText(m_panel, wxID_ANY, "SDL API"));

    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
        m_sdl.Add(SDL_GetAudioDriver(i));

    m_sdl_select = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_sdl);
    m_sdl_select->SetSelection(SdlOutputAPI);
    m_sdl_box->Add(m_sdl_select);

    m_output_box->Add(m_portaudio_box, wxSizerFlags().Expand());
    m_output_box->Add(m_sdl_box, wxSizerFlags().Expand());

    // Synchronization Mode
    m_right_box->Add(new wxStaticText(m_panel, wxID_ANY, "Synchronization"));
    m_sync.Add("TimeStretch (Recommended)");
    m_sync.Add("Async Mix (Breaks some games!)");
    m_sync.Add("None (Audio can skip.)");
    m_sync_select = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_sync);
    m_sync_select->SetSelection(SynchMode);
    m_right_box->Add(m_sync_select, wxSizerFlags().Expand());

    // Latency Slider
    const int min_latency = SynchMode == 0 ? LATENCY_MIN_TIMESTRETCH : LATENCY_MIN;

    m_latency_box = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Latency");
    m_latency_slider = new wxSlider(m_panel, wxID_ANY, SndOutLatencyMS, min_latency, LATENCY_MAX, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_latency_box->Add(m_latency_slider, wxSizerFlags().Expand());
    m_right_box->Add(m_latency_box, wxSizerFlags().Expand());

    // Volume Slider
    m_volume_box = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Volume");
    m_volume_slider = new wxSlider(m_panel, wxID_ANY, FinalVolume * 100, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_volume_box->Add(m_volume_slider, wxSizerFlags().Expand());
    m_right_box->Add(m_volume_box, wxSizerFlags().Expand());

    wxButton *launch_adv_dialog = new wxButton(m_panel, wxID_ANY, "Advanced...");
    m_right_box->Add(launch_adv_dialog);

    m_top_box->Add(m_left_box, wxSizerFlags().Expand());
    m_top_box->Add(m_right_box, wxSizerFlags().Expand());

    m_panel->SetSizerAndFit(m_top_box);
}

void Dialog::InitDialog()
{
}

// Main
void DisplayDialog()
{
    Dialog dialog;

    dialog.InitDialog();
    dialog.ShowModal();
}
