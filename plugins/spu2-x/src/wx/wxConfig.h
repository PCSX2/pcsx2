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

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/wrapsizer.h>

#if defined(__unix__) || defined(__APPLE__)
#include <SDL.h>
#include <SDL_audio.h>
#include "Linux/Config.h"
#endif
namespace SoundtouchCfg
{
class AdvDialog : public wxDialog
{
    wxBoxSizer *m_adv_box, *m_babble_box;

    wxButton *reset_button;
    wxStaticText *m_adv_text, *m_adv_text2, *m_adv_text3;
    wxSlider *seq_slider, *seek_slider, *overlap_slider;
    wxStaticBoxSizer *seq_box, *seek_box, *overlap_box;

public:
    AdvDialog();
    ~AdvDialog();
    void Display();
    void LoadValues();
    void SaveValues();
    void Reset();
    void CallReset(wxCommandEvent &event);
};
}; // namespace SoundtouchCfg

class DebugDialog : public wxDialog
{
    wxBoxSizer *m_debug_top_box;
    wxBoxSizer *m_together_box;
    wxStaticBoxSizer *m_console_box, *m_log_only_box, *dump_box;
    wxCheckBox *show_check;
    wxCheckBox *key_check, *voice_check, *dma_check, *autodma_check, *buffer_check, *adpcm_check;
    wxCheckBox *dma_actions_check, *dma_writes_check, *auto_output_check;
    wxCheckBox *core_voice_check, *memory_check, *register_check;

public:
    DebugDialog();
    ~DebugDialog();
    void Display();
    void ResetToValues();
    void SaveValues();
    void Reconfigure();
    void CallReconfigure(wxCommandEvent &event);
};

class Dialog : public wxDialog
{
    wxBoxSizer *m_top_box, *m_left_box, *m_right_box;
    wxBoxSizer *m_portaudio_box, *m_sdl_box, *m_audio_box;
    wxStaticBoxSizer *m_mix_box, *m_debug_box, *m_output_box, *m_volume_box, *m_latency_box, *m_sync_box;

    wxArrayString m_interpolation, m_module, m_portaudio, m_sdl, m_sync, m_audio;
    wxChoice *m_inter_select, *m_module_select, *m_portaudio_select, *m_sdl_select, *m_sync_select, *m_audio_select;
    wxStaticText *m_portaudio_text, *m_sdl_text;

    wxCheckBox *effect_check, *dealias_check, *debug_check;
    wxSlider *m_latency_slider, *m_volume_slider;
    wxButton *launch_debug_dialog, *launch_adv_dialog;

public:
    Dialog();
    ~Dialog();
    void Display();
    void ResetToValues();
    void SaveValues();
    void Reconfigure();
    void CallReconfigure(wxCommandEvent &event);
    void OnButtonClicked(wxCommandEvent &event);
};