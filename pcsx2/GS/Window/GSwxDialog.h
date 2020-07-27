/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS.h"
#include "GSSetting.h"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/wrapsizer.h>
#include <wx/statline.h>
#include <wx/filepicker.h>

namespace GSSettingsDialog
{

	class RendererTab : public wxPanel
	{
	public:
		wxCheckBox *acc_date_check, *eight_bit_check, *framebuffer_check, *flush_check, *edge_check, *mipmap_check;
		wxChoice *m_res_select, *m_anisotropic_select, *m_dither_select, *m_mipmap_select, *m_crc_select, *m_blend_select;
		wxSpinCtrl* thread_spin;

		RendererTab(wxWindow* parent);
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

	class HacksTab : public wxPanel
	{
	public:
		wxCheckBox* hacks_check;
		wxCheckBox *align_sprite_check, *fb_convert_check, *auto_flush_check, *mem_wrap_check, *dis_depth_check;
		wxCheckBox *merge_sprite_check, *dis_safe_features_check, *preload_gs_check, *fast_inv_check, *wild_arms_check;

		wxChoice *m_half_select, *m_tri_select, *m_gs_offset_hack_select, *m_round_hack_select;
		wxSpinCtrl *skip_x_spin, *skip_y_spin, *tex_off_x_spin, *tex_off_y_spin;

		HacksTab(wxWindow* parent);
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

	class DebugTab : public wxPanel
	{
	public:
		wxCheckBox *glsl_debug_check, *gl_debug_check, *gs_dump_check, *gs_save_check, *gs_savef_check, *gs_savet_check, *gs_savez_check;
		wxSpinCtrl *start_dump_spin, *end_dump_spin;
		wxChoice *m_geo_shader_select, *m_image_load_store_select, *m_sparse_select;

		DebugTab(wxWindow* parent);
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

	class RecTab : public wxPanel
	{
	public:
		wxCheckBox* record_check;
		wxSpinCtrl *res_x_spin, *res_y_spin, *thread_spin, *png_spin;
		wxDirPickerCtrl* dir_select;

		RecTab(wxWindow* parent);
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

	class PostTab : public wxPanel
	{
	public:
		wxCheckBox *tex_filter_check, *fxaa_check, *shade_boost_check, *ext_shader_check;
		wxSlider *sb_brightness_slider, *sb_contrast_slider, *sb_saturation_slider;
		wxDirPickerCtrl *glsl_select, *config_select;
		wxChoice* m_tv_select;
		wxStaticBoxSizer *shade_boost_box, *ext_shader_box;

		PostTab(wxWindow* parent);
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

	class OSDTab : public wxPanel
	{
	public:
		wxCheckBox *monitor_check, *log_check;
		wxSpinCtrl *size_spin, *timeout_spin, *max_spin;
		wxSlider *red_slider, *green_slider, *blue_slider, *opacity_slider;
		wxStaticBoxSizer *font_box, *log_box;

		OSDTab(wxWindow* parent);
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

	class Dialog : public wxDialog
	{
		wxBoxSizer* m_top_box;
		wxChoice *m_renderer_select, *m_interlace_select, *m_texture_select, *m_adapter_select;
		RendererTab* m_renderer_panel;
		HacksTab* m_hacks_panel;
		DebugTab* m_debug_panel;
		RecTab* m_rec_panel;
		PostTab* m_post_panel;
		OSDTab* m_osd_panel;

	public:
		Dialog();
		~Dialog();
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
	};

} // namespace GSSettingsDialog

extern bool RunwxDialog();
