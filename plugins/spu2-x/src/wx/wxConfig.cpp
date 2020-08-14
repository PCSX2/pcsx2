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

Dialog::Dialog()
    : wxDialog(nullptr, wxID_ANY, "SPU2-X Config", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    m_top_box = new wxBoxSizer(wxHORIZONTAL);
    m_left_box = new wxBoxSizer(wxVERTICAL);
    m_right_box = new wxBoxSizer(wxVERTICAL);
#ifdef SPU2X_PORTAUDIO
    m_portaudio_box = new wxBoxSizer(wxVERTICAL);
#endif
    m_sdl_box = new wxBoxSizer(wxVERTICAL);

    m_mix_box = new wxStaticBoxSizer(wxVERTICAL, this, "Mixing Settings");
    m_debug_box = new wxStaticBoxSizer(wxVERTICAL, this, "Debug Settings");
    m_output_box = new wxStaticBoxSizer(wxVERTICAL, this, "Output Settings");

    // Mixing Settings
    m_mix_box->Add(new wxStaticText(this, wxID_ANY, "Interpolation"), wxSizerFlags().Centre());

    m_interpolation.Add("Nearest (Fastest/bad quality)");
    m_interpolation.Add("Linear (Simple/okay sound)");
    m_interpolation.Add("Cubic (Artificial highs)");
    m_interpolation.Add("Hermite (Better highs)");
    m_interpolation.Add("Catmull-Rom (PS2-like/slow)");

    m_inter_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_interpolation);

    effect_check = new wxCheckBox(this, wxID_ANY, "Disable Effects Processing (Speedup)");
    dealias_check = new wxCheckBox(this, wxID_ANY, "Use the de-alias filter (Overemphasizes the highs)");

    m_mix_box->Add(m_inter_select, wxSizerFlags().Centre());
    m_mix_box->Add(effect_check, wxSizerFlags().Centre());
    m_mix_box->Add(dealias_check, wxSizerFlags().Centre());

    // Debug Settings
    debug_check = new wxCheckBox(this, wxID_ANY, "Enable Debug Options");
    launch_debug_dialog = new wxButton(this, wxID_ANY, "Configure...");

    m_debug_box->Add(debug_check, wxSizerFlags().Expand());
    m_debug_box->Add(launch_debug_dialog, wxSizerFlags().Expand());

    // Output Settings

    // Module
    m_output_box->Add(new wxStaticText(this, wxID_ANY, "Module"), wxSizerFlags().Centre());
    m_module.Add("No Sound (Emulate SPU2 only)");
#ifdef SPU2X_PORTAUDIO
    m_module.Add("PortAudio (Cross-platform)");
#endif
    m_module.Add("SDL Audio (Recommended for PulseAudio)");
    m_module_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_module);
    m_output_box->Add(m_module_select, wxSizerFlags().Centre());
#ifdef SPU2X_PORTAUDIO
    // Portaudio
    m_portaudio_text = new wxStaticText(this, wxID_ANY, "Portaudio API");
    m_portaudio_box->Add(m_portaudio_text, wxSizerFlags().Centre());
#ifdef __linux__
    m_portaudio.Add("ALSA (recommended)");
    m_portaudio.Add("OSS (legacy)");
    m_portaudio.Add("JACK");
#elif defined(__APPLE__)
    m_portaudio.Add("CoreAudio");
#else
    m_portaudio.Add("OSS");
#endif
    m_portaudio_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_portaudio);
    m_portaudio_box->Add(m_portaudio_select, wxSizerFlags().Centre());
#endif

    // SDL
    m_sdl_text = new wxStaticText(this, wxID_ANY, "SDL API");
    m_sdl_box->Add(m_sdl_text, wxSizerFlags().Centre());

    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
        m_sdl.Add(SDL_GetAudioDriver(i));

    m_sdl_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_sdl);
    m_sdl_box->Add(m_sdl_select, wxSizerFlags().Centre());
#ifdef SPU2X_PORTAUDIO
    m_output_box->Add(m_portaudio_box, wxSizerFlags().Expand());
#endif
    m_output_box->Add(m_sdl_box, wxSizerFlags().Expand());

    // Synchronization Mode
    m_sync_box = new wxStaticBoxSizer(wxHORIZONTAL, this, "Synchronization ");
    m_sync.Add("TimeStretch (Recommended)");
    m_sync.Add("Async Mix (Breaks some games!)");
    m_sync.Add("None (Audio can skip.)");
    m_sync_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_sync);
    m_sync_box->Add(m_sync_select, wxSizerFlags().Centre());

    launch_adv_dialog = new wxButton(this, wxID_ANY, "Advanced...");
    m_sync_box->Add(launch_adv_dialog);

    m_left_box->Add(m_output_box, wxSizerFlags().Expand());
    m_left_box->Add(m_mix_box, wxSizerFlags().Expand());
    m_left_box->Add(m_sync_box, wxSizerFlags().Expand().Border(wxALL, 5));

    // Latency Slider
    const int min_latency = SynchMode == 0 ? LATENCY_MIN_TIMESTRETCH : LATENCY_MIN;

    m_latency_box = new wxStaticBoxSizer(wxVERTICAL, this, "Latency");
    m_latency_slider = new wxSlider(this, wxID_ANY, SndOutLatencyMS, min_latency, LATENCY_MAX, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_latency_box->Add(m_latency_slider, wxSizerFlags().Expand());

    // Volume Slider
    m_volume_box = new wxStaticBoxSizer(wxVERTICAL, this, "Volume");
    m_volume_slider = new wxSlider(this, wxID_ANY, FinalVolume * 100, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_volume_box->Add(m_volume_slider, wxSizerFlags().Expand());

    m_right_box->Add(m_latency_box, wxSizerFlags().Expand());
    m_right_box->Add(m_volume_box, wxSizerFlags().Expand());

    m_audio_box = new wxBoxSizer(wxVERTICAL);
    m_audio_box->Add(new wxStaticText(this, wxID_ANY, "Audio Expansion Mode"));
    m_audio.Add("Stereo (None, Default)");
    m_audio.Add("Quadrafonic");
    m_audio.Add("Surround 5.1");
    m_audio.Add("Surround 7.1");
    m_audio_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_audio);
    m_audio_box->Add(m_audio_select, wxSizerFlags().Expand());

    m_right_box->Add(m_audio_box);
    m_right_box->Add(m_debug_box);

    m_top_box->Add(m_left_box, wxSizerFlags().Left());
    m_top_box->Add(m_right_box, wxSizerFlags().Right());


    SetSizerAndFit(m_top_box);

    Bind(wxEVT_BUTTON, &Dialog::OnButtonClicked, this);
    Bind(wxEVT_CHOICE, &Dialog::CallReconfigure, this);
    Bind(wxEVT_CHECKBOX, &Dialog::CallReconfigure, this);
}

Dialog::~Dialog()
{
}

void Dialog::Reconfigure()
{
    const int mod = m_module_select->GetCurrentSelection();
    bool show_portaudio = false, show_sdl = false;

    switch (mod) {
        case 0:
            show_portaudio = false;
            show_sdl = false;
            break;

        case 1:
            show_portaudio = true;
            show_sdl = false;
            break;

        case 2:
            show_portaudio = false;
            show_sdl = true;
            break;

        default:
            show_portaudio = false;
            show_sdl = false;
            break;
    }
#ifdef SPU2X_PORTAUDIO
    m_output_box->Show(m_portaudio_box, show_portaudio, true);
#endif
    m_output_box->Show(m_sdl_box, show_sdl, true);

    // Recalculating both of these accounts for if neither was showing initially.
    m_top_box->Layout();
    SetSizerAndFit(m_top_box);

    launch_debug_dialog->Enable(debug_check->GetValue());
    launch_adv_dialog->Enable(m_sync_select->GetCurrentSelection() == 0);
}

void Dialog::CallReconfigure(wxCommandEvent &event)
{
    Reconfigure();
}

void Dialog::OnButtonClicked(wxCommandEvent &event)
{
    wxButton *bt = (wxButton *)event.GetEventObject();

    if (bt == launch_debug_dialog)
    {
        auto debug_dialog = new DebugDialog;
        debug_dialog->Display();
        wxDELETE(debug_dialog);
    }

    if (bt == launch_adv_dialog)
    {
        auto adv_dialog = new SoundtouchCfg::AdvDialog;
        adv_dialog->Display();
        wxDELETE(adv_dialog);
    }
}

void Dialog::ResetToValues()
{
    m_inter_select->SetSelection(Interpolation);
    m_module_select->SetSelection(OutputModule);
#ifdef SPU2X_PORTAUDIO
    m_portaudio_select->SetSelection(OutputAPI);
#endif
    m_sdl_select->SetSelection(SdlOutputAPI);
    m_sync_select->SetSelection(SynchMode);
    m_audio_select->SetSelection(numSpeakers);

    effect_check->SetValue(EffectsDisabled);
    dealias_check->SetValue(postprocess_filter_dealias);
    debug_check->SetValue(DebugEnabled);

    m_volume_slider->SetValue(FinalVolume * 100);
    m_latency_slider->SetValue(SndOutLatencyMS);

    Reconfigure();
}

void Dialog::SaveValues()
{
    Interpolation = m_inter_select->GetSelection();
    OutputModule = m_module_select->GetSelection();
#ifdef SPU2X_PORTAUDIO
    OutputAPI = m_portaudio_select->GetSelection();
#endif
    SdlOutputAPI = m_sdl_select->GetSelection();
    SynchMode = m_sync_select->GetSelection();
    numSpeakers = m_audio_select->GetSelection();

    EffectsDisabled = effect_check->GetValue();
    postprocess_filter_dealias = dealias_check->GetValue();
    DebugEnabled = debug_check->GetValue();

    FinalVolume = m_volume_slider->GetValue() / 100.0;
    SndOutLatencyMS = m_latency_slider->GetValue();
}

// Main
void Dialog::Display()
{
    ResetToValues();
    ShowModal();
    SaveValues();
}

// Debug dialog box
DebugDialog::DebugDialog()
    : wxDialog(nullptr, wxID_ANY, "SPU2-X Debug", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    m_debug_top_box = new wxBoxSizer(wxHORIZONTAL);
    m_together_box = new wxBoxSizer(wxVERTICAL);

    show_check = new wxCheckBox(this, wxID_ANY, "Show in console");
    m_together_box->Add(show_check);

    m_console_box = new wxStaticBoxSizer(wxVERTICAL, this, "Events");

    key_check = new wxCheckBox(this, wxID_ANY, "Key On/Off");
    voice_check = new wxCheckBox(this, wxID_ANY, "Voice Stop");
    dma_check = new wxCheckBox(this, wxID_ANY, "DMA Operations");
    autodma_check = new wxCheckBox(this, wxID_ANY, "AutoDMA Operations");
    buffer_check = new wxCheckBox(this, wxID_ANY, "Buffer Over/Underruns");
    adpcm_check = new wxCheckBox(this, wxID_ANY, "ADPCM Cache");
    m_console_box->Add(key_check);
    m_console_box->Add(voice_check);
    m_console_box->Add(dma_check);
    m_console_box->Add(autodma_check);
    m_console_box->Add(buffer_check);
    m_console_box->Add(adpcm_check);

    m_log_only_box = new wxStaticBoxSizer(wxVERTICAL, this, "Log Only");
    dma_actions_check = new wxCheckBox(this, wxID_ANY, "Register/DMA Actions");
    dma_writes_check = new wxCheckBox(this, wxID_ANY, "DMA Writes");
    auto_output_check = new wxCheckBox(this, wxID_ANY, "Audio Output");
    m_log_only_box->Add(dma_actions_check);
    m_log_only_box->Add(dma_writes_check);
    m_log_only_box->Add(auto_output_check);

    dump_box = new wxStaticBoxSizer(wxVERTICAL, this, "Dump on Close");
    core_voice_check = new wxCheckBox(this, wxID_ANY, "Core && Voice Stats");
    memory_check = new wxCheckBox(this, wxID_ANY, "Memory Contents");
    register_check = new wxCheckBox(this, wxID_ANY, "Register Data");
    dump_box->Add(core_voice_check);
    dump_box->Add(memory_check);
    dump_box->Add(register_check);

    m_together_box->Add(m_console_box);
    m_debug_top_box->Add(m_together_box, wxSizerFlags().Expand());
    m_debug_top_box->Add(m_log_only_box, wxSizerFlags().Expand());
    m_debug_top_box->Add(dump_box, wxSizerFlags().Expand());

    SetSizerAndFit(m_debug_top_box);
    Bind(wxEVT_CHECKBOX, &DebugDialog::CallReconfigure, this);
}

DebugDialog::~DebugDialog()
{
}

void DebugDialog::Reconfigure()
{
    if (show_check->GetValue()) {
        _MsgKeyOnOff = key_check->Enable();
        _MsgVoiceOff = voice_check->Enable();
        _MsgDMA = dma_check->Enable();
        _MsgAutoDMA = autodma_check->Enable();
        _MsgOverruns = buffer_check->Enable();
        _MsgCache = adpcm_check->Enable();
    } else {
        _MsgKeyOnOff = key_check->Disable();
        _MsgVoiceOff = voice_check->Disable();
        _MsgDMA = dma_check->Disable();
        _MsgAutoDMA = autodma_check->Disable();
        _MsgOverruns = buffer_check->Disable();
        _MsgCache = adpcm_check->Disable();
    }
}

void DebugDialog::CallReconfigure(wxCommandEvent &event)
{
    Reconfigure();
}

void DebugDialog::ResetToValues()
{
    show_check->SetValue(_MsgToConsole);
    key_check->SetValue(_MsgKeyOnOff);
    voice_check->SetValue(_MsgVoiceOff);
    dma_check->SetValue(_MsgDMA);
    autodma_check->SetValue(_MsgAutoDMA);
    buffer_check->SetValue(_MsgOverruns);
    adpcm_check->SetValue(_MsgCache);

    dma_actions_check->SetValue(_AccessLog);
    dma_writes_check->SetValue(_DMALog);
    auto_output_check->SetValue(_WaveLog);

    core_voice_check->SetValue(_CoresDump);
    memory_check->SetValue(_MemDump);
    register_check->SetValue(_RegDump);
    Reconfigure();
}

void DebugDialog::SaveValues()
{
    _MsgToConsole = show_check->GetValue();
    _MsgKeyOnOff = key_check->GetValue();
    _MsgVoiceOff = voice_check->GetValue();
    _MsgDMA = dma_check->GetValue();
    _MsgAutoDMA = autodma_check->GetValue();
    _MsgOverruns = buffer_check->GetValue();
    _MsgCache = adpcm_check->GetValue();

    _AccessLog = dma_actions_check->GetValue();
    _DMALog = dma_writes_check->GetValue();
    _WaveLog = auto_output_check->GetValue();

    _CoresDump = core_voice_check->GetValue();
    _MemDump = memory_check->GetValue();
    _RegDump = register_check->GetValue();
}

void DebugDialog::Display()
{
    ResetToValues();
    ShowModal();
    SaveValues();
    WriteSettings();
}
namespace SoundtouchCfg
{
AdvDialog::AdvDialog()
    : wxDialog(nullptr, wxID_ANY, "Soundtouch Config", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    m_adv_box = new wxBoxSizer(wxVERTICAL);
    m_babble_box = new wxBoxSizer(wxVERTICAL);

    m_adv_text = new wxStaticText(this, wxID_ANY, "These are advanced configuration options for fine tuning time stretching behavior.", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    m_adv_text2 = new wxStaticText(this, wxID_ANY, "Larger values are better for slowdown, while smaller values are better for speedup (more then 60 fps.).", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    m_adv_text3 = new wxStaticText(this, wxID_ANY, "All options are in microseconds.", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);

    m_adv_text->Wrap(200);
    m_adv_text2->Wrap(200);
    m_adv_text2->Wrap(200);

    m_babble_box->Add(m_adv_text, wxSizerFlags().Expand().Border(wxALL, 5).Centre());
    m_babble_box->Add(m_adv_text2, wxSizerFlags().Expand().Border(wxALL, 5).Centre());
    m_babble_box->Add(m_adv_text3, wxSizerFlags().Expand().Border(wxALL, 5).Centre());

    m_adv_box->Add(m_babble_box, wxSizerFlags().Expand().Centre());

    reset_button = new wxButton(this, wxID_ANY, "Reset To Defaults");
    m_adv_box->Add(reset_button, wxSizerFlags().Expand().Centre().Border(wxALL, 5));

    // Volume Slider
    seq_box = new wxStaticBoxSizer(wxVERTICAL, this, "Sequence Length");
    seq_slider = new wxSlider(this, wxID_ANY, SoundtouchCfg::SequenceLenMS, SoundtouchCfg::SequenceLen_Min, SoundtouchCfg::SequenceLen_Max, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    seq_box->Add(seq_slider, wxSizerFlags().Expand());
    m_adv_box->Add(seq_box, wxSizerFlags().Expand().Centre().Border(wxALL, 5));

    // Volume Slider
    seek_box = new wxStaticBoxSizer(wxVERTICAL, this, "Seek Window Size");
    seek_slider = new wxSlider(this, wxID_ANY, SoundtouchCfg::SeekWindowMS, SoundtouchCfg::SeekWindow_Min, SoundtouchCfg::SeekWindow_Max, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    seek_box->Add(seek_slider, wxSizerFlags().Expand());
    m_adv_box->Add(seek_box, wxSizerFlags().Expand().Centre().Border(wxALL, 5));

    // Volume Slider
    overlap_box = new wxStaticBoxSizer(wxVERTICAL, this, "Overlap");
    overlap_slider = new wxSlider(this, wxID_ANY, SoundtouchCfg::OverlapMS, SoundtouchCfg::Overlap_Min, SoundtouchCfg::Overlap_Max, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    overlap_box->Add(overlap_slider, wxSizerFlags().Expand().Centre());
    m_adv_box->Add(overlap_box, wxSizerFlags().Expand().Centre().Border(wxALL, 5));

    SetSizerAndFit(m_adv_box);
    Bind(wxEVT_BUTTON, &AdvDialog::CallReset, this);
}

AdvDialog::~AdvDialog()
{
}

void AdvDialog::Reset()
{
    seq_slider->SetValue(30);
    seek_slider->SetValue(20);
    overlap_slider->SetValue(10);
}

void AdvDialog::CallReset(wxCommandEvent &event)
{
    Reset();
}

void AdvDialog::LoadValues()
{
    SoundtouchCfg::ReadSettings();
    seq_slider->SetValue(SoundtouchCfg::SequenceLenMS);
    seek_slider->SetValue(SoundtouchCfg::SeekWindowMS);
    overlap_slider->SetValue(SoundtouchCfg::OverlapMS);
}

void AdvDialog::SaveValues()
{
    SoundtouchCfg::SequenceLenMS = seq_slider->GetValue();
    SoundtouchCfg::SeekWindowMS = seek_slider->GetValue();
    SoundtouchCfg::OverlapMS = overlap_slider->GetValue();
    SoundtouchCfg::WriteSettings();
}

void AdvDialog::Display()
{
    LoadValues();
    ShowModal();
    SaveValues();
}
}; // namespace SoundtouchCfg