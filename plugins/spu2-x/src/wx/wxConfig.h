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

#ifdef __unix__
#include <SDL.h>
#include <SDL_audio.h>
#include "Linux/Config.h"
#endif

class Dialog : public wxDialog
{
    wxPanel *m_panel;
    wxBoxSizer *m_top_box, *m_left_box, *m_right_box;
    wxBoxSizer *m_portaudio_box, *m_sdl_box;
    wxStaticBoxSizer *m_mix_box, *m_debug_box, *m_output_box, *m_volume_box, *m_latency_box;

    wxArrayString m_interpolation, m_module, m_portaudio, m_sdl, m_sync;
    wxChoice *m_inter_select, *m_module_select, *m_portaudio_select, *m_sdl_select, *m_sync_select;
    wxStaticText *m_portaudio_text, *m_sdl_text;

    wxCheckBox *effect_check, *dealias_check, *debug_check;
    wxSlider *m_latency_slider, *m_volume_slider;
    wxButton *launch_debug_dialog, *launch_adv_dialog;
public:
    Dialog();
    void Display();
    void ResetToValues();
    void SaveValues();
    void Reconfigure();
    void CallReconfigure(wxCommandEvent &event);
};