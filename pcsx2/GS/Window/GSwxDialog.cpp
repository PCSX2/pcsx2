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

#include "PrecompiledHeader.h"
#include "GSwxDialog.h"

using namespace GSSettingsDialog;

namespace
{
	void add_tooltip(wxWindow* widget, int tooltip)
	{
		if (tooltip != -1)
			widget->SetToolTip(dialog_message(tooltip));
	}

	void add_settings_to_array_string(const std::vector<GSSetting>& s, wxArrayString& arr)
	{
		for (const GSSetting& setting : s)
		{
			if (!setting.note.empty())
				arr.Add(setting.name + " (" + setting.note + ")");
			else
				arr.Add(setting.name);
		}
	}

	size_t get_config_index(const std::vector<GSSetting>& s, const char* str)
	{
		int value = theApp.GetConfigI(str);

		for (size_t i = 0; i < s.size(); i++)
		{
			if (s[i].value == value)
				return i;
		}
		return 0;
	}

	void set_config_from_choice(const wxChoice* choice, const std::vector<GSSetting>& s, const char* str)
	{
		int idx = choice->GetSelection();

		if (idx == wxNOT_FOUND)
			return;

		theApp.SetConfig(str, s[idx].value);
	}

	void add_label(wxWindow* parent, wxSizer* sizer, const char* str, int tooltip = -1, long style = wxALIGN_RIGHT | wxALIGN_CENTRE_HORIZONTAL, wxSizerFlags flags = wxSizerFlags().Right().DoubleBorder())
	{
		auto* temp_text = new wxStaticText(parent, wxID_ANY, str, wxDefaultPosition, wxDefaultSize, style);
		add_tooltip(temp_text, tooltip);
		sizer->Add(temp_text, flags);
	}

	void add_combo_box(wxWindow* parent, wxSizer* sizer, wxChoice*& choice, const std::vector<GSSetting>& s, int tooltip = -1, wxSizerFlags flags = wxSizerFlags().Expand().Left())
	{
		wxArrayString temp;
		add_settings_to_array_string(s, temp);
		choice = new wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, temp);

		add_tooltip(choice, tooltip);
		sizer->Add(choice, flags);
	}

	void add_label_and_combo_box(wxWindow* parent, wxSizer* sizer, wxChoice*& choice, const char* str, const std::vector<GSSetting>& s, int tooltip = -1)
	{
		add_label(parent, sizer, str, tooltip);
		add_combo_box(parent, sizer, choice, s, tooltip);
	}
} // namespace

RendererTab::RendererTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	auto* tab_box = new wxBoxSizer(wxVERTICAL);
	auto* hardware_box = new wxStaticBoxSizer(wxVERTICAL, this, "Hardware Mode");
	auto* software_box = new wxStaticBoxSizer(wxVERTICAL, this, "Software Mode");

	eight_bit_check = new wxCheckBox(this, wxID_ANY, "GPU Palette Conversion");
	add_tooltip(eight_bit_check, IDC_PALTEX);
	framebuffer_check = new wxCheckBox(this, wxID_ANY, "Conservative Buffer Allocation");
	add_tooltip(framebuffer_check, IDC_CONSERVATIVE_FB);
	acc_date_check = new wxCheckBox(this, wxID_ANY, "Accurate DATE");
	add_tooltip(acc_date_check, IDC_ACCURATE_DATE);

	auto* hw_choice_grid = new wxFlexGridSizer(2, 0, 0);

	add_label_and_combo_box(this, hw_choice_grid, m_res_select, "Internal Resolution:", theApp.m_gs_upscale_multiplier);
	add_label_and_combo_box(this, hw_choice_grid, m_anisotropic_select, "Anisotropic Filtering:", theApp.m_gs_max_anisotropy, IDC_AFCOMBO);
	add_label_and_combo_box(this, hw_choice_grid, m_dither_select, "Dithering (PgDn):", theApp.m_gs_dithering, IDC_DITHERING);
	add_label_and_combo_box(this, hw_choice_grid, m_mipmap_select, "Mipmapping (Insert):", theApp.m_gs_hw_mipmapping, IDC_MIPMAP_HW);
	add_label_and_combo_box(this, hw_choice_grid, m_crc_select, "CRC Hack Level:", theApp.m_gs_crc_level, IDC_CRC_LEVEL);
	add_label_and_combo_box(this, hw_choice_grid, m_blend_select, "Blending Accuracy:", theApp.m_gs_acc_blend_level, IDC_ACCURATE_BLEND_UNIT);

	auto* top_checks_box = new wxWrapSizer(wxHORIZONTAL);
	top_checks_box->Add(eight_bit_check, wxSizerFlags().Centre());
	top_checks_box->Add(framebuffer_check, wxSizerFlags().Centre());
	top_checks_box->Add(acc_date_check, wxSizerFlags().Centre());

	hardware_box->Add(top_checks_box, wxSizerFlags().Centre());
	hardware_box->Add(hw_choice_grid, wxSizerFlags().Centre());

	auto* bottom_checks_box = new wxWrapSizer(wxHORIZONTAL);
	flush_check = new wxCheckBox(this, wxID_ANY, "Auto Flush");
	add_tooltip(flush_check, IDC_AUTO_FLUSH_SW);

	edge_check = new wxCheckBox(this, wxID_ANY, "Edge Antialiasing (Del)");
	add_tooltip(edge_check, IDC_AA1);

	mipmap_check = new wxCheckBox(this, wxID_ANY, "Mipmapping");
	add_tooltip(mipmap_check, IDC_MIPMAP_SW);

	bottom_checks_box->Add(flush_check, wxSizerFlags().Centre());
	bottom_checks_box->Add(edge_check, wxSizerFlags().Centre());
	bottom_checks_box->Add(mipmap_check, wxSizerFlags().Centre());
	software_box->Add(bottom_checks_box, wxSizerFlags().Centre());

	// Rendering threads
	auto* thread_box = new wxBoxSizer(wxHORIZONTAL);

	add_label(this, thread_box, "Extra Rendering threads:", IDC_SWTHREADS);
	thread_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 32, 2);
	thread_box->Add(thread_spin, wxSizerFlags().Centre());
	software_box->Add(thread_box, wxSizerFlags().Centre());

	tab_box->Add(hardware_box, wxSizerFlags().Centre().Expand());
	tab_box->Add(software_box, wxSizerFlags().Centre().Expand());

	SetSizerAndFit(tab_box);
	Bind(wxEVT_CHECKBOX, &RendererTab::CallUpdate, this);
}

void RendererTab::CallUpdate(wxCommandEvent&)
{
	Update();
}

void RendererTab::Load()
{
	acc_date_check->SetValue(theApp.GetConfigB("accurate_date"));
	eight_bit_check->SetValue(theApp.GetConfigB("paltex"));
	framebuffer_check->SetValue(theApp.GetConfigB("conservative_framebuffer"));
	flush_check->SetValue(theApp.GetConfigB("autoflush_sw"));
	edge_check->SetValue(theApp.GetConfigB("aa1"));
	mipmap_check->SetValue(theApp.GetConfigB("mipmap"));

	thread_spin->SetValue(theApp.GetConfigI("extrathreads"));

	m_res_select->SetSelection(get_config_index(theApp.m_gs_upscale_multiplier, "upscale_multiplier"));
	m_anisotropic_select->SetSelection(get_config_index(theApp.m_gs_max_anisotropy, "MaxAnisotropy"));
	m_dither_select->SetSelection(get_config_index(theApp.m_gs_dithering, "dithering_ps2"));
	m_mipmap_select->SetSelection(get_config_index(theApp.m_gs_hw_mipmapping, "mipmap_hw"));
	m_crc_select->SetSelection(get_config_index(theApp.m_gs_crc_level, "crc_hack_level"));
	m_blend_select->SetSelection(get_config_index(theApp.m_gs_acc_blend_level, "accurate_blending_unit"));
	Update();
}

void RendererTab::Save()
{
	theApp.SetConfig("accurate_date", acc_date_check->GetValue());
	theApp.SetConfig("paltex", eight_bit_check->GetValue());
	theApp.SetConfig("conservative_framebuffer", framebuffer_check->GetValue());
	theApp.SetConfig("autoflush_sw", flush_check->GetValue());
	theApp.SetConfig("aa1", edge_check->GetValue());
	theApp.SetConfig("mipmap", mipmap_check->GetValue());

	theApp.SetConfig("extrathreads", thread_spin->GetValue());

	set_config_from_choice(m_res_select, theApp.m_gs_upscale_multiplier, "upscale_multiplier");
	set_config_from_choice(m_anisotropic_select, theApp.m_gs_max_anisotropy, "MaxAnisotropy");
	set_config_from_choice(m_dither_select, theApp.m_gs_dithering, "dithering_ps2");
	set_config_from_choice(m_mipmap_select, theApp.m_gs_hw_mipmapping, "mipmap_hw");
	set_config_from_choice(m_crc_select, theApp.m_gs_crc_level, "crc_hack_level");
	set_config_from_choice(m_blend_select, theApp.m_gs_acc_blend_level, "accurate_blending_unit");
}

void RendererTab::Update()
{
}

HacksTab::HacksTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	auto* tab_box = new wxBoxSizer(wxVERTICAL);

	hacks_check = new wxCheckBox(this, wxID_ANY, "Enable User Hacks");

	auto* rend_hacks_box = new wxStaticBoxSizer(wxVERTICAL, this, "Renderer Hacks");
	auto* upscale_hacks_box = new wxStaticBoxSizer(wxVERTICAL, this, "Upscale Hacks");

	auto* rend_hacks_grid = new wxFlexGridSizer(2, 0, 0);
	auto* upscale_hacks_grid = new wxFlexGridSizer(3, 0, 0);

	// Renderer Hacks
	auto_flush_check = new wxCheckBox(this, wxID_ANY, "Auto Flush");
	fast_inv_check = new wxCheckBox(this, wxID_ANY, "Fast Texture Invalidation");
	dis_depth_check = new wxCheckBox(this, wxID_ANY, "Disable Depth Emulation");
	fb_convert_check = new wxCheckBox(this, wxID_ANY, "Frame Buffer Conversion");
	dis_safe_features_check = new wxCheckBox(this, wxID_ANY, "Disable Safe Features");
	mem_wrap_check = new wxCheckBox(this, wxID_ANY, "Memory Wrapping");
	preload_gs_check = new wxCheckBox(this, wxID_ANY, "Preload Frame Data");

	add_tooltip(auto_flush_check, IDC_AUTO_FLUSH_HW);
	add_tooltip(fast_inv_check, IDC_FAST_TC_INV);
	add_tooltip(dis_depth_check, IDC_TC_DEPTH);
	add_tooltip(fb_convert_check, IDC_CPU_FB_CONVERSION);
	add_tooltip(dis_safe_features_check, IDC_SAFE_FEATURES);
	add_tooltip(mem_wrap_check, IDC_MEMORY_WRAPPING);
	add_tooltip(preload_gs_check, IDC_PRELOAD_GS);

	// Upscale
	align_sprite_check = new wxCheckBox(this, wxID_ANY, "Align Sprite");
	merge_sprite_check = new wxCheckBox(this, wxID_ANY, "Merge Sprite");
	wild_arms_check = new wxCheckBox(this, wxID_ANY, "Wild Arms Hack");

	add_tooltip(align_sprite_check, IDC_ALIGN_SPRITE);
	add_tooltip(merge_sprite_check, IDC_MERGE_PP_SPRITE);
	add_tooltip(wild_arms_check, IDC_WILDHACK);

	rend_hacks_grid->Add(auto_flush_check);
	rend_hacks_grid->Add(fast_inv_check);
	rend_hacks_grid->Add(dis_depth_check);
	rend_hacks_grid->Add(fb_convert_check);
	rend_hacks_grid->Add(dis_safe_features_check);
	rend_hacks_grid->Add(mem_wrap_check);
	rend_hacks_grid->Add(preload_gs_check);

	upscale_hacks_grid->Add(align_sprite_check);
	upscale_hacks_grid->Add(merge_sprite_check);
	upscale_hacks_grid->Add(wild_arms_check);

	auto* rend_hack_choice_grid = new wxFlexGridSizer(2, 0, 0);
	auto* upscale_hack_choice_grid = new wxFlexGridSizer(2, 0, 0);

	// Renderer Hacks:
	add_label_and_combo_box(this, rend_hack_choice_grid, m_half_select, "Half Screen Fix:", theApp.m_gs_generic_list, IDC_HALF_SCREEN_TS);
	add_label_and_combo_box(this, rend_hack_choice_grid, m_tri_select, "Trilinear Filtering:", theApp.m_gs_trifilter, IDC_TRI_FILTER);

	// Skipdraw Range
	add_label(this, rend_hack_choice_grid, "Skipdraw Range:");
	auto* skip_box = new wxBoxSizer(wxHORIZONTAL);
	skip_x_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 0);
	skip_y_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 0);
	add_tooltip(skip_x_spin, IDC_TCOFFSETX);
	add_tooltip(skip_y_spin, IDC_TCOFFSETY);
	skip_box->Add(skip_x_spin);
	skip_box->Add(skip_y_spin);

	rend_hack_choice_grid->Add(skip_box);

	// Upscale Hacks:
	add_label_and_combo_box(this, upscale_hack_choice_grid, m_gs_offset_hack_select, "Half-Pixel Offset:", theApp.m_gs_offset_hack, IDC_OFFSETHACK);
	add_label_and_combo_box(this, upscale_hack_choice_grid, m_round_hack_select, "Round Sprite:", theApp.m_gs_hack, IDC_ROUND_SPRITE);

	// Texture Offsets
	add_label(this, upscale_hack_choice_grid, "Texture Offsets:");
	auto* tex_off_box = new wxBoxSizer(wxHORIZONTAL);
	tex_off_x_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 0);
	tex_off_y_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, 0);
	tex_off_box->Add(tex_off_x_spin);
	tex_off_box->Add(tex_off_y_spin);

	upscale_hack_choice_grid->Add(tex_off_box);

	rend_hacks_box->Add(rend_hacks_grid);
	rend_hacks_box->Add(new wxStaticLine(this, wxID_ANY), wxSizerFlags().Centre().Expand().Border(5));
	rend_hacks_box->Add(rend_hack_choice_grid, wxSizerFlags().Expand());

	upscale_hacks_box->Add(upscale_hacks_grid);
	upscale_hacks_box->Add(new wxStaticLine(this, wxID_ANY), wxSizerFlags().Centre().Expand().Border(5));
	upscale_hacks_box->Add(upscale_hack_choice_grid, wxSizerFlags().Expand());

	tab_box->Add(hacks_check, wxSizerFlags().Left());
	tab_box->Add(rend_hacks_box, wxSizerFlags().Centre().Expand());
	tab_box->Add(upscale_hacks_box, wxSizerFlags().Centre().Expand());

	SetSizerAndFit(tab_box);
	Bind(wxEVT_SPINCTRL, &HacksTab::CallUpdate, this);
	Bind(wxEVT_CHECKBOX, &HacksTab::CallUpdate, this);
}

void HacksTab::CallUpdate(wxCommandEvent&)
{
	Update();
}

void HacksTab::Load()
{
	hacks_check->SetValue(theApp.GetConfigB("UserHacks"));

	align_sprite_check->SetValue(theApp.GetConfigB("UserHacks_align_sprite_X"));
	fb_convert_check->SetValue(theApp.GetConfigB("UserHacks_CPU_FB_Conversion"));
	auto_flush_check->SetValue(theApp.GetConfigB("UserHacks_AutoFlush"));
	mem_wrap_check->SetValue(theApp.GetConfigB("wrap_gs_mem"));
	dis_depth_check->SetValue(theApp.GetConfigB("UserHacks_DisableDepthSupport"));
	merge_sprite_check->SetValue(theApp.GetConfigB("UserHacks_merge_pp_sprite"));
	dis_safe_features_check->SetValue(theApp.GetConfigB("UserHacks_Disable_Safe_Features"));
	preload_gs_check->SetValue(theApp.GetConfigB("preload_frame_with_gs_data"));
	fast_inv_check->SetValue(theApp.GetConfigB("UserHacks_DisablePartialInvalidation"));
	wild_arms_check->SetValue(theApp.GetConfigB("UserHacks_WildHack"));

	m_half_select->SetSelection(get_config_index(theApp.m_gs_offset_hack, "UserHacks_HalfPixelOffset"));
	m_tri_select->SetSelection(get_config_index(theApp.m_gs_trifilter, "UserHacks_TriFilter"));
	m_gs_offset_hack_select->SetSelection(get_config_index(theApp.m_gs_generic_list, "UserHacks_Half_Bottom_Override"));
	m_round_hack_select->SetSelection(get_config_index(theApp.m_gs_hack, "UserHacks_round_sprite_offset"));

	skip_x_spin->SetValue(theApp.GetConfigI("UserHacks_SkipDraw_Offset"));
	skip_y_spin->SetValue(theApp.GetConfigI("UserHacks_SkipDraw"));
	tex_off_x_spin->SetValue(theApp.GetConfigI("UserHacks_TCOffsetX"));
	tex_off_y_spin->SetValue(theApp.GetConfigI("UserHacks_TCOffsetY"));
	Update();
}

void HacksTab::Save()
{
	theApp.SetConfig("UserHacks", hacks_check->GetValue());

	theApp.SetConfig("UserHacks_align_sprite_X", align_sprite_check->GetValue());
	theApp.SetConfig("UserHacks_CPU_FB_Conversion", fb_convert_check->GetValue());
	theApp.SetConfig("UserHacks_AutoFlush", auto_flush_check->GetValue());
	theApp.SetConfig("wrap_gs_mem", mem_wrap_check->GetValue());
	theApp.SetConfig("UserHacks_DisableDepthSupport", dis_depth_check->GetValue());
	theApp.SetConfig("UserHacks_merge_pp_sprite", merge_sprite_check->GetValue());
	theApp.SetConfig("UserHacks_Disable_Safe_Features", dis_safe_features_check->GetValue());
	theApp.SetConfig("preload_frame_with_gs_data", preload_gs_check->GetValue());
	theApp.SetConfig("UserHacks_DisablePartialInvalidation", fast_inv_check->GetValue());
	theApp.SetConfig("UserHacks_WildHack", wild_arms_check->GetValue());

	set_config_from_choice(m_half_select, theApp.m_gs_offset_hack, "UserHacks_HalfPixelOffset");
	set_config_from_choice(m_tri_select, theApp.m_gs_trifilter, "UserHacks_TriFilter");
	set_config_from_choice(m_gs_offset_hack_select, theApp.m_gs_generic_list, "UserHacks_Half_Bottom_Override");
	set_config_from_choice(m_round_hack_select, theApp.m_gs_hack, "UserHacks_round_sprite_offset");

	theApp.SetConfig("UserHacks_SkipDraw_Offset", skip_x_spin->GetValue());
	theApp.SetConfig("UserHacks_SkipDraw", skip_y_spin->GetValue());
	theApp.SetConfig("UserHacks_TCOffsetX", tex_off_x_spin->GetValue());
	theApp.SetConfig("UserHacks_TCOffsetY", tex_off_y_spin->GetValue());
}

void HacksTab::Update()
{
	bool hacks_enabled = hacks_check->GetValue();

	align_sprite_check->Enable(hacks_enabled);
	fb_convert_check->Enable(hacks_enabled);
	auto_flush_check->Enable(hacks_enabled);
	mem_wrap_check->Enable(hacks_enabled);
	dis_depth_check->Enable(hacks_enabled);
	merge_sprite_check->Enable(hacks_enabled);
	dis_safe_features_check->Enable(hacks_enabled);
	preload_gs_check->Enable(hacks_enabled);
	fast_inv_check->Enable(hacks_enabled);
	wild_arms_check->Enable(hacks_enabled);

	m_half_select->Enable(hacks_enabled);
	m_tri_select->Enable(hacks_enabled);
	m_gs_offset_hack_select->Enable(hacks_enabled);
	m_round_hack_select->Enable(hacks_enabled);

	skip_x_spin->Enable(hacks_enabled);
	skip_y_spin->Enable(hacks_enabled);
	tex_off_x_spin->Enable(hacks_enabled);
	tex_off_y_spin->Enable(hacks_enabled);

	if (skip_x_spin->GetValue() == 0)
		skip_y_spin->SetValue(0);
	if (skip_y_spin->GetValue() < skip_x_spin->GetValue())
		skip_y_spin->SetValue(skip_x_spin->GetValue());
	//if (tex_off_y_spin->GetValue() < tex_off_x_spin->GetValue()) tex_off_y_spin->SetValue(tex_off_x_spin->GetValue());
}

RecTab::RecTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	auto* tab_box = new wxBoxSizer(wxVERTICAL);

	record_check = new wxCheckBox(this, wxID_ANY, "Enable Recording (F12)");
	auto* record_box = new wxStaticBoxSizer(wxVERTICAL, this, "Recording");
	auto* record_grid_box = new wxFlexGridSizer(2, 0, 0);

	// Resolution
	add_label(this, record_grid_box, "Resolution:");
	auto* res_box = new wxBoxSizer(wxHORIZONTAL);
	res_x_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 256, 8192, 640);
	res_y_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 256, 8192, 480);
	res_box->Add(res_x_spin);
	res_box->Add(res_y_spin);

	record_grid_box->Add(res_box);

	add_label(this, record_grid_box, "Saving Threads:");
	thread_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 32, 4);
	record_grid_box->Add(thread_spin);

	add_label(this, record_grid_box, "PNG Compression Level:");
	png_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 9, 1);
	record_grid_box->Add(png_spin);

	add_label(this, record_grid_box, "Output Directory:");
	dir_select = new wxDirPickerCtrl(this, wxID_ANY);
	record_grid_box->Add(dir_select, wxSizerFlags().Expand());

	record_box->Add(record_grid_box, wxSizerFlags().Centre().Expand());

	tab_box->Add(record_check, wxSizerFlags().Left());
	tab_box->Add(record_box, wxSizerFlags().Centre().Expand());
	SetSizerAndFit(tab_box);
	Bind(wxEVT_CHECKBOX, &RecTab::CallUpdate, this);
}

void RecTab::CallUpdate(wxCommandEvent&)
{
	Update();
}

void RecTab::Load()
{
	record_check->SetValue(theApp.GetConfigB("capture_enabled"));

	res_x_spin->SetValue(theApp.GetConfigI("CaptureWidth"));
	res_y_spin->SetValue(theApp.GetConfigI("CaptureHeight"));
	thread_spin->SetValue(theApp.GetConfigI("capture_threads"));
	png_spin->SetValue(theApp.GetConfigI("png_compression_level"));

	dir_select->SetInitialDirectory(theApp.GetConfigS("capture_out_dir"));
	Update();
}

void RecTab::Save()
{
	theApp.SetConfig("capture_enabled", record_check->GetValue());

	theApp.SetConfig("CaptureWidth", res_x_spin->GetValue());
	theApp.SetConfig("CaptureHeight", res_y_spin->GetValue());
	theApp.SetConfig("capture_threads", thread_spin->GetValue());
	theApp.SetConfig("png_compression_level", png_spin->GetValue());

	theApp.SetConfig("capture_out_dir", dir_select->GetPath());
}

void RecTab::Update()
{
	bool capture_enabled = record_check->GetValue();

	res_x_spin->Enable(capture_enabled);
	res_y_spin->Enable(capture_enabled);
	thread_spin->Enable(capture_enabled);
	png_spin->Enable(capture_enabled);

	dir_select->Enable(capture_enabled);
}

PostTab::PostTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	auto* tab_box = new wxBoxSizer(wxVERTICAL);
	auto* shader_box = new wxStaticBoxSizer(wxVERTICAL, this, "Custom Shader");

	tex_filter_check = new wxCheckBox(this, wxID_ANY, "Texture Filtering of Display");
	fxaa_check = new wxCheckBox(this, wxID_ANY, "FXAA Shader (PgUp)");
	add_tooltip(tex_filter_check, IDC_LINEAR_PRESENT);
	add_tooltip(fxaa_check, IDC_FXAA);

	shader_box->Add(tex_filter_check);
	shader_box->Add(fxaa_check);

	shade_boost_check = new wxCheckBox(this, wxID_ANY, "Enable Shade Boost");
	add_tooltip(shade_boost_check, IDC_SHADEBOOST);
	shader_box->Add(shade_boost_check);

	shade_boost_box = new wxStaticBoxSizer(wxVERTICAL, this, "Shade Boost");
	auto* shader_boost_grid = new wxFlexGridSizer(2, 0, 0);

	add_label(this, shader_boost_grid, "Brightness:");
	sb_brightness_slider = new wxSlider(this, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	shader_boost_grid->Add(sb_brightness_slider, wxSizerFlags().Expand().Shaped());


	add_label(this, shader_boost_grid, "Contrast:");
	sb_contrast_slider = new wxSlider(this, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	shader_boost_grid->Add(sb_contrast_slider, wxSizerFlags().Centre().Expand());

	add_label(this, shader_boost_grid, "Saturation:");
	sb_saturation_slider = new wxSlider(this, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	shader_boost_grid->Add(sb_saturation_slider, wxSizerFlags().Centre().Expand());

	shade_boost_box->Add(shader_boost_grid, wxSizerFlags().Centre());
	shader_box->Add(shade_boost_box, wxSizerFlags().Expand().Centre());

	ext_shader_check = new wxCheckBox(this, wxID_ANY, "Enable External Shader");
	add_tooltip(ext_shader_check, IDC_SHADER_FX);
	shader_box->Add(ext_shader_check);

	ext_shader_box = new wxStaticBoxSizer(wxVERTICAL, this, "External Shader (Home)");
	auto* ext_shader_grid = new wxFlexGridSizer(4, 0, 0);

	ext_shader_grid->Add(new wxStaticText(this, wxID_ANY, "GLSL fx File:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), wxSizerFlags().Centre());
	glsl_select = new wxDirPickerCtrl(this, wxID_ANY);
	ext_shader_grid->Add(glsl_select, wxSizerFlags().Expand());

	ext_shader_grid->Add(new wxStaticText(this, wxID_ANY, "Config File:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), wxSizerFlags().Centre());
	config_select = new wxDirPickerCtrl(this, wxID_ANY);
	ext_shader_grid->Add(config_select, wxSizerFlags().Expand());

	ext_shader_box->Add(ext_shader_grid, wxSizerFlags().Centre());
	shader_box->Add(ext_shader_box, wxSizerFlags().Expand().Centre());

	// TV Shader
	auto* tv_box = new wxBoxSizer(wxHORIZONTAL);
	add_label_and_combo_box(this, tv_box, m_tv_select, "TV Shader:", theApp.m_gs_tv_shaders);
	shader_box->Add(tv_box);

	tab_box->Add(shader_box, wxSizerFlags().Centre().Expand());
	SetSizerAndFit(tab_box);
	Bind(wxEVT_CHECKBOX, &PostTab::CallUpdate, this);
}

void PostTab::CallUpdate(wxCommandEvent&)
{
	Update();
}

void PostTab::Load()
{
	tex_filter_check->SetValue(theApp.GetConfigB("linear_present"));
	fxaa_check->SetValue(theApp.GetConfigB("fxaa"));
	shade_boost_check->SetValue(theApp.GetConfigB("ShadeBoost"));
	ext_shader_check->SetValue(theApp.GetConfigB("shaderfx"));

	m_tv_select->SetSelection(get_config_index(theApp.m_gs_tv_shaders, "TVShader"));

	glsl_select->SetInitialDirectory(theApp.GetConfigS("shaderfx_glsl"));
	config_select->SetInitialDirectory(theApp.GetConfigS("shaderfx_conf"));

	sb_brightness_slider->SetValue(theApp.GetConfigI("ShadeBoost_Brightness"));
	sb_contrast_slider->SetValue(theApp.GetConfigI("ShadeBoost_Contrast"));
	sb_saturation_slider->SetValue(theApp.GetConfigI("ShadeBoost_Saturation"));
	Update();
}

void PostTab::Save()
{
	theApp.SetConfig("linear_present", tex_filter_check->GetValue());
	theApp.SetConfig("fxaa", fxaa_check->GetValue());
	theApp.SetConfig("ShadeBoost", shade_boost_check->GetValue());
	theApp.SetConfig("shaderfx", ext_shader_check->GetValue());

	set_config_from_choice(m_tv_select, theApp.m_gs_tv_shaders, "TVShader");

	theApp.SetConfig("shaderfx_glsl", glsl_select->GetPath());
	theApp.SetConfig("shaderfx_conf", config_select->GetPath());

	theApp.SetConfig("ShadeBoost_Brightness", sb_brightness_slider->GetValue());
	theApp.SetConfig("ShadeBoost_Contrast", sb_contrast_slider->GetValue());
	theApp.SetConfig("ShadeBoost_Saturation", sb_saturation_slider->GetValue());
}

void PostTab::Update()
{
	bool shade_boost_enabled = shade_boost_check->GetValue();
	bool ext_shader_enabled = ext_shader_check->GetValue();

	shade_boost_box->GetStaticBox()->Enable(shade_boost_enabled);
	sb_brightness_slider->Enable(shade_boost_enabled);
	sb_contrast_slider->Enable(shade_boost_enabled);
	sb_saturation_slider->Enable(shade_boost_enabled);

	ext_shader_box->GetStaticBox()->Enable(ext_shader_enabled);
	glsl_select->Enable(ext_shader_enabled);
	config_select->Enable(ext_shader_enabled);
}

OSDTab::OSDTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	auto* tab_box = new wxBoxSizer(wxVERTICAL);

	monitor_check = new wxCheckBox(this, wxID_ANY, "Enable Monitor");
	tab_box->Add(monitor_check);

	font_box = new wxStaticBoxSizer(wxVERTICAL, this, "Font");
	auto* font_grid = new wxFlexGridSizer(2, 0, 0);

	add_label(this, font_grid, "Size:");
	size_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 100, 25);
	font_grid->Add(size_spin, wxSizerFlags().Expand());

	add_label(this, font_grid, "Red:");
	red_slider = new wxSlider(this, wxID_ANY, 0, 0, 255, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	font_grid->Add(red_slider, wxSizerFlags().Expand().Shaped());

	add_label(this, font_grid, "Green:");
	green_slider = new wxSlider(this, wxID_ANY, 0, 0, 255, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	font_grid->Add(green_slider, wxSizerFlags().Expand().Shaped());

	add_label(this, font_grid, "Blue:");
	blue_slider = new wxSlider(this, wxID_ANY, 0, 0, 255, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	font_grid->Add(blue_slider, wxSizerFlags().Expand().Shaped());

	add_label(this, font_grid, "Opacity:");
	opacity_slider = new wxSlider(this, wxID_ANY, 100, 0, 100, wxDefaultPosition, wxSize(250, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	font_grid->Add(opacity_slider, wxSizerFlags().Expand().Shaped());

	font_box->Add(font_grid, wxSizerFlags().Centre().Expand());
	tab_box->Add(font_box, wxSizerFlags().Centre());

	log_check = new wxCheckBox(this, wxID_ANY, "Enable Log");
	tab_box->Add(log_check);

	log_box = new wxStaticBoxSizer(wxVERTICAL, this, "Log Messages");
	auto* log_grid = new wxFlexGridSizer(2, 0, 0);

	add_label(this, log_grid, "Timeout (seconds):");
	timeout_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 2, 10, 4);
	log_grid->Add(timeout_spin);

	add_label(this, log_grid, "Max On-Screen Messages:");
	max_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 10, 2);
	log_grid->Add(max_spin);

	log_box->Add(log_grid, wxSizerFlags().Centre().Expand());
	tab_box->Add(log_box, wxSizerFlags().Centre());

	SetSizerAndFit(tab_box);
	Bind(wxEVT_CHECKBOX, &OSDTab::CallUpdate, this);
}

void OSDTab::CallUpdate(wxCommandEvent&)
{
	Update();
}

void OSDTab::Load()
{
	monitor_check->SetValue(theApp.GetConfigB("osd_monitor_enabled"));
	log_check->SetValue(theApp.GetConfigB("osd_log_enabled"));

	size_spin->SetValue(theApp.GetConfigI("osd_fontsize"));
	timeout_spin->SetValue(theApp.GetConfigI("osd_log_timeout"));
	max_spin->SetValue(theApp.GetConfigI("osd_max_log_messages"));

	red_slider->SetValue(theApp.GetConfigI("osd_color_r"));
	green_slider->SetValue(theApp.GetConfigI("osd_color_g"));
	blue_slider->SetValue(theApp.GetConfigI("osd_color_b"));
	opacity_slider->SetValue(theApp.GetConfigI("osd_color_opacity"));
	Update();
}

void OSDTab::Save()
{
	theApp.SetConfig("osd_monitor_enabled", monitor_check->GetValue());
	theApp.SetConfig("osd_log_enabled", log_check->GetValue());

	theApp.SetConfig("osd_fontsize", size_spin->GetValue());
	theApp.SetConfig("osd_log_timeout", timeout_spin->GetValue());
	theApp.SetConfig("osd_max_log_messages", max_spin->GetValue());

	theApp.SetConfig("osd_color_r", red_slider->GetValue());
	theApp.SetConfig("osd_color_g", green_slider->GetValue());
	theApp.SetConfig("osd_color_b", blue_slider->GetValue());
	theApp.SetConfig("osd_color_opacity", opacity_slider->GetValue());
}

void OSDTab::Update()
{
	bool font_enabled = monitor_check->GetValue();
	bool log_enabled = log_check->GetValue();

	font_box->GetStaticBox()->Enable(font_enabled);
	size_spin->Enable(font_enabled);
	red_slider->Enable(font_enabled);
	green_slider->Enable(font_enabled);
	blue_slider->Enable(font_enabled);
	opacity_slider->Enable(font_enabled);

	log_box->GetStaticBox()->Enable(log_enabled);
	timeout_spin->Enable(log_enabled);
	max_spin->Enable(log_enabled);
}

DebugTab::DebugTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	auto* tab_box = new wxBoxSizer(wxVERTICAL);

	auto* debug_box = new wxStaticBoxSizer(wxVERTICAL, this, "Debug");
	auto* debug_check_box = new wxWrapSizer(wxHORIZONTAL);

	glsl_debug_check = new wxCheckBox(this, wxID_ANY, "GLSL compilation");
	debug_check_box->Add(glsl_debug_check);

	gl_debug_check = new wxCheckBox(this, wxID_ANY, "Print GL error");
	debug_check_box->Add(gl_debug_check);

	gs_dump_check = new wxCheckBox(this, wxID_ANY, "Dump GS data");
	debug_check_box->Add(gs_dump_check);

	auto* debug_save_check_box = new wxWrapSizer(wxHORIZONTAL);

	gs_save_check = new wxCheckBox(this, wxID_ANY, "Save RT");
	debug_save_check_box->Add(gs_save_check);

	gs_savef_check = new wxCheckBox(this, wxID_ANY, "Save Frame");
	debug_save_check_box->Add(gs_savef_check);

	gs_savet_check = new wxCheckBox(this, wxID_ANY, "Save Texture");
	debug_save_check_box->Add(gs_savet_check);

	gs_savez_check = new wxCheckBox(this, wxID_ANY, "Save Depth");
	debug_save_check_box->Add(gs_savez_check);

	debug_box->Add(debug_check_box);
	debug_box->Add(debug_save_check_box);

	auto* dump_grid = new wxFlexGridSizer(2, 0, 0);

	add_label(this, dump_grid, "Start of Dump:");
	start_dump_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, pow(10, 9), 0);
	dump_grid->Add(start_dump_spin);

	add_label(this, dump_grid, "End of Dump:");
	end_dump_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, pow(10, 5), 5000);
	dump_grid->Add(end_dump_spin);

	debug_box->Add(dump_grid);

	auto* ogl_box = new wxStaticBoxSizer(wxVERTICAL, this, "OpenGL");
	auto* ogl_grid = new wxFlexGridSizer(2, 0, 0);
	add_label_and_combo_box(this, ogl_grid, m_geo_shader_select, "Geometry Shader:", theApp.m_gs_generic_list);
	add_label_and_combo_box(this, ogl_grid, m_image_load_store_select, "Image Load Store:", theApp.m_gs_generic_list);
	add_label_and_combo_box(this, ogl_grid, m_sparse_select, "Sparse Texture:", theApp.m_gs_generic_list);
	ogl_box->Add(ogl_grid);

	tab_box->Add(debug_box, wxSizerFlags().Centre().Expand());
	tab_box->Add(ogl_box, wxSizerFlags().Centre().Expand());

	SetSizerAndFit(tab_box);
	Bind(wxEVT_SPINCTRL, &DebugTab::CallUpdate, this);
	Bind(wxEVT_CHECKBOX, &DebugTab::CallUpdate, this);
}

void DebugTab::CallUpdate(wxCommandEvent&)
{
	Update();
}

void DebugTab::Load()
{
	glsl_debug_check->SetValue(theApp.GetConfigB("debug_glsl_shader"));
	gl_debug_check->SetValue(theApp.GetConfigB("debug_opengl"));
	gs_dump_check->SetValue(theApp.GetConfigB("dump"));
	gs_save_check->SetValue(theApp.GetConfigB("save"));
	gs_savef_check->SetValue(theApp.GetConfigB("savef"));
	gs_savet_check->SetValue(theApp.GetConfigB("savet"));
	gs_savez_check->SetValue(theApp.GetConfigB("savez"));

	start_dump_spin->SetValue(theApp.GetConfigI("saven"));
	end_dump_spin->SetValue(theApp.GetConfigI("savel"));

	m_geo_shader_select->SetSelection(get_config_index(theApp.m_gs_generic_list, "override_geometry_shader"));
	m_image_load_store_select->SetSelection(get_config_index(theApp.m_gs_generic_list, "override_GL_ARB_shader_image_load_store"));
	m_sparse_select->SetSelection(get_config_index(theApp.m_gs_generic_list, "override_GL_ARB_sparse_texture"));
	Update();
}

void DebugTab::Save()
{
	theApp.SetConfig("debug_glsl_shader", glsl_debug_check->GetValue());
	theApp.SetConfig("debug_opengl", gl_debug_check->GetValue());
	theApp.SetConfig("dump", gs_dump_check->GetValue());
	theApp.SetConfig("save", gs_save_check->GetValue());
	theApp.SetConfig("savef", gs_savef_check->GetValue());
	theApp.SetConfig("savet", gs_savet_check->GetValue());
	theApp.SetConfig("savez", gs_savez_check->GetValue());

	theApp.SetConfig("saven", start_dump_spin->GetValue());
	theApp.SetConfig("savel", end_dump_spin->GetValue());

	set_config_from_choice(m_geo_shader_select, theApp.m_gs_generic_list, "override_geometry_shader");
	set_config_from_choice(m_image_load_store_select, theApp.m_gs_generic_list, "override_GL_ARB_shader_image_load_store");
	set_config_from_choice(m_sparse_select, theApp.m_gs_generic_list, "override_GL_ARB_sparse_texture");
}

void DebugTab::Update()
{
	if (end_dump_spin->GetValue() < start_dump_spin->GetValue())
		end_dump_spin->SetValue(start_dump_spin->GetValue());
}

Dialog::Dialog()
	: wxDialog(nullptr, wxID_ANY, "Graphics Settings", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
	auto* padding = new wxBoxSizer(wxVERTICAL);
	m_top_box = new wxBoxSizer(wxVERTICAL);

	// Right now, I'm hardcoding the location of the picture.
	wxString fileLocation = wxStandardPaths::Get().GetExecutablePath();
	fileLocation = wxFileName(fileLocation).GetPath() + L"/plugins/logo-ogl.bmp";
	if (wxFileName(fileLocation).Exists())
	{
		wxBitmap logo(fileLocation, wxBITMAP_TYPE_BMP);
		m_top_box->Add(new wxStaticBitmap(this, wxID_ANY, logo), wxSizerFlags().Centre());
	}

	auto* top_grid = new wxFlexGridSizer(2, 0, 0);
	top_grid->SetFlexibleDirection(wxHORIZONTAL);

	add_label_and_combo_box(this, top_grid, m_renderer_select, "Renderer:", theApp.m_gs_renderers);

#ifdef _WIN32
	add_label(this, top_grid, "Adapter:");
	wxArrayString m_adapter_str;
	//add_settings_to_array_string(theApp.m_gs_renderers, m_adapter_str);
	m_adapter_str.Add("Default");
	m_adapter_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_adapter_str);
	top_grid->Add(m_adapter_select, wxSizerFlags().Expand());
#endif

	add_label_and_combo_box(this, top_grid, m_interlace_select, "Interlacing(F5):", theApp.m_gs_interlace);
	add_label_and_combo_box(this, top_grid, m_texture_select, "Texture Filtering:", theApp.m_gs_bifilter);

	auto* book = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

	m_renderer_panel = new RendererTab(book);
	m_hacks_panel = new HacksTab(book);
	m_rec_panel = new RecTab(book);
	m_post_panel = new PostTab(book);
	m_osd_panel = new OSDTab(book);
	m_debug_panel = new DebugTab(book);

	book->AddPage(m_renderer_panel, "Renderer", true);
	book->AddPage(m_hacks_panel, "Hacks");
	book->AddPage(m_post_panel, "Shader");
	book->AddPage(m_osd_panel, "OSD");
	book->AddPage(m_rec_panel, "Recording");
	book->AddPage(m_debug_panel, "Debug/OGL");

	m_top_box->Add(top_grid, wxSizerFlags().Centre());
	m_top_box->Add(book, wxSizerFlags().Centre().Expand());

	padding->Add(m_top_box, wxSizerFlags().Centre().Expand().Border(wxALL, 5));

	m_top_box->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right());

	SetSizerAndFit(padding);
	Bind(wxEVT_CHECKBOX, &Dialog::CallUpdate, this);
}

Dialog::~Dialog()
{
}

void Dialog::CallUpdate(wxCommandEvent&)
{
	Update();
}

void Dialog::Load()
{
	m_renderer_select->SetSelection(get_config_index(theApp.m_gs_renderers, "Renderer"));
#ifdef _WIN32
	m_adapter_select->SetSelection(0);
#endif
	m_interlace_select->SetSelection(get_config_index(theApp.m_gs_interlace, "interlace"));
	m_texture_select->SetSelection(get_config_index(theApp.m_gs_bifilter, "filter"));

	m_hacks_panel->Load();
	m_renderer_panel->Load();
	m_rec_panel->Load();
	m_post_panel->Load();
	m_osd_panel->Load();
	m_debug_panel->Load();
}

void Dialog::Save()
{
	set_config_from_choice(m_renderer_select, theApp.m_gs_renderers, "Renderer");
	set_config_from_choice(m_interlace_select, theApp.m_gs_interlace, "interlace");
	set_config_from_choice(m_texture_select, theApp.m_gs_bifilter, "filter");

	m_hacks_panel->Save();
	m_renderer_panel->Save();
	m_rec_panel->Save();
	m_post_panel->Save();
	m_osd_panel->Save();
	m_debug_panel->Save();
}

void Dialog::Update()
{
	m_adapter_select->Disable();

	m_hacks_panel->Update();
	m_renderer_panel->Update();
	m_rec_panel->Update();
	m_post_panel->Update();
	m_osd_panel->Update();
	m_debug_panel->Update();
}

bool RunwxDialog()
{
	Dialog GSSettingsDialog;

	GSSettingsDialog.Load();
	if (GSSettingsDialog.ShowModal() == wxID_OK)
		GSSettingsDialog.Save();

	return true;
}
