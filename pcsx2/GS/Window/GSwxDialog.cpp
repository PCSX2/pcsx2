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
#include "gui/AppConfig.h"
#include "gui/StringHelpers.h"
#include "GS/GSUtil.h"
#include "HostDisplay.h"

#ifdef _WIN32
#include "Frontend/D3D11HostDisplay.h"
#include "Frontend/D3D12HostDisplay.h"
#endif
#include "GS/Renderers/Metal/GSMetalCPPAccessible.h"

#ifdef ENABLE_VULKAN
#include "Frontend/VulkanHostDisplay.h"
#endif

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
				arr.Add(fromUTF8(setting.name + " (" + setting.note + ")"));
			else
				arr.Add(fromUTF8(setting.name));
		}
	}

	size_t get_config_index(const std::vector<GSSetting>& s, int value)
	{
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

	void add_label(wxWindow* parent, wxSizer* sizer, const char* str, int tooltip = -1, wxSizerFlags flags = wxSizerFlags().Centre().Right(), long style = wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL)
	{
		auto* temp_text = new wxStaticText(parent, wxID_ANY, str, wxDefaultPosition, wxDefaultSize, style);
		add_tooltip(temp_text, tooltip);
		sizer->Add(temp_text, flags);
	}

	/// wxBoxSizer with padding
	template <typename OuterSizer>
	struct PaddedBoxSizer
	{
		OuterSizer* outer;
		wxBoxSizer* inner;

		// Make static analyzers happy (memory management is handled by WX)
		// (There's no actual reason we couldn't use the default copy constructor, except cppcheck screams when you do and it's easier to delete than implement one)
		PaddedBoxSizer(const PaddedBoxSizer&) = delete;

		template <typename... Args>
		PaddedBoxSizer(wxOrientation orientation, Args&&... args)
			: outer(new OuterSizer(orientation, std::forward<Args>(args)...))
			, inner(new wxBoxSizer(orientation))
		{
			wxSizerFlags flags = wxSizerFlags().Expand();
#ifdef __APPLE__
			if (!std::is_same<OuterSizer, wxStaticBoxSizer>::value) // wxMac already adds padding to static box sizers
#endif
				flags.Border();
			outer->Add(inner, flags);
		}

		wxBoxSizer* operator->() { return inner; }
	};

	struct CheckboxPrereq
	{
		wxCheckBox* box;
		explicit CheckboxPrereq(wxCheckBox* box)
			: box(box)
		{
		}

		bool operator()()
		{
			return box->GetValue();
		}
	};
} // namespace

GSUIElementHolder::GSUIElementHolder(wxWindow* window)
	: m_window(window)
{
}

wxStaticText* GSUIElementHolder::addWithLabel(wxControl* control, UIElem::Type type, wxSizer* sizer, const char* label, const char* config_name, int tooltip, std::function<bool()> prereq, wxSizerFlags flags)
{
	add_tooltip(control, tooltip);
	wxStaticText* text = new wxStaticText(m_window, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	add_tooltip(text, tooltip);
	sizer->Add(text, wxSizerFlags().Centre().Right());
	sizer->Add(control, flags);
	m_elems.emplace_back(type, control, config_name, prereq);
	return text;
}

wxCheckBox* GSUIElementHolder::addCheckBox(wxSizer* sizer, const char* label, const char* config_name, int tooltip, std::function<bool()> prereq)
{
	wxCheckBox* box = new wxCheckBox(m_window, wxID_ANY, label);
	add_tooltip(box, tooltip);
	if (sizer)
		sizer->Add(box);
	m_elems.emplace_back(UIElem::Type::CheckBox, box, config_name, prereq);
	return box;
}

std::pair<wxChoice*, wxStaticText*> GSUIElementHolder::addComboBoxAndLabel(wxSizer* sizer, const char* label, const char* config_name, const std::vector<GSSetting>* settings, int tooltip, std::function<bool()> prereq)
{
	wxArrayString temp;
	add_settings_to_array_string(*settings, temp);
	wxChoice* choice = new GSwxChoice(m_window, wxID_ANY, wxDefaultPosition, wxDefaultSize, temp, settings);
	return std::make_pair(choice, addWithLabel(choice, UIElem::Type::Choice, sizer, label, config_name, tooltip, prereq));
}

wxSpinCtrl* GSUIElementHolder::addSpin(wxSizer* sizer, const char* config_name, int min, int max, int initial, int tooltip, std::function<bool()> prereq)
{
	wxSpinCtrl* spin = new wxSpinCtrl(m_window, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, min, max, initial);
	add_tooltip(spin, tooltip);
	if (sizer)
		sizer->Add(spin, wxSizerFlags(1));
	m_elems.emplace_back(UIElem::Type::Spin, spin, config_name, prereq);
	return spin;
}

std::pair<wxSpinCtrl*, wxStaticText*> GSUIElementHolder::addSpinAndLabel(wxSizer* sizer, const char* label, const char* config_name, int min, int max, int initial, int tooltip, std::function<bool()> prereq)
{
	wxSpinCtrl* spin = new wxSpinCtrl(m_window, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, min, max, initial);
	return std::make_pair(spin, addWithLabel(spin, UIElem::Type::Spin, sizer, label, config_name, tooltip, prereq, wxSizerFlags().Centre().Left().Expand()));
}

std::pair<wxSlider*, wxStaticText*> GSUIElementHolder::addSliderAndLabel(wxSizer* sizer, const char* label, const char* config_name, int min, int max, int initial, int tooltip, std::function<bool()> prereq)
{
	wxSlider* slider = new wxSlider(m_window, wxID_ANY, initial, min, max, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
	return std::make_pair(slider, addWithLabel(slider, UIElem::Type::Slider, sizer, label, config_name, tooltip, prereq));
}

std::pair<wxFilePickerCtrl*, wxStaticText*> GSUIElementHolder::addFilePickerAndLabel(wxSizer* sizer, const char* label, const char* config_name, int tooltip, std::function<bool()> prereq)
{
	wxFilePickerCtrl* picker = new wxFilePickerCtrl(m_window, wxID_ANY);
	return std::make_pair(picker, addWithLabel(picker, UIElem::Type::File, sizer, label, config_name, tooltip, prereq));
}

std::pair<wxDirPickerCtrl*, wxStaticText*> GSUIElementHolder::addDirPickerAndLabel(wxSizer* sizer, const char* label, const char* config_name, int tooltip, std::function<bool()> prereq)
{
	wxDirPickerCtrl* picker = new wxDirPickerCtrl(m_window, wxID_ANY);
	return std::make_pair(picker, addWithLabel(picker, UIElem::Type::Directory, sizer, label, config_name, tooltip, prereq));
}

void GSUIElementHolder::Load()
{
	for (const UIElem& elem : m_elems)
	{
		switch (elem.type)
		{
			case UIElem::Type::CheckBox:
				static_cast<wxCheckBox*>(elem.control)->SetValue(theApp.GetConfigB(elem.config));
				break;
			case UIElem::Type::Choice:
			{
				GSwxChoice* choice = static_cast<GSwxChoice*>(elem.control);
				choice->SetSelection(get_config_index(choice->settings, theApp.GetConfigI(elem.config)));
				break;
			}
			case UIElem::Type::Spin:
				static_cast<wxSpinCtrl*>(elem.control)->SetValue(theApp.GetConfigI(elem.config));
				break;
			case UIElem::Type::Slider:
				static_cast<wxSlider*>(elem.control)->SetValue(theApp.GetConfigI(elem.config));
				break;
			case UIElem::Type::File:
			case UIElem::Type::Directory:
			{
				auto* picker = static_cast<wxFileDirPickerCtrlBase*>(elem.control);
				picker->SetInitialDirectory(fromUTF8(theApp.GetConfigS(elem.config)));
				picker->SetPath(fromUTF8(theApp.GetConfigS(elem.config)));
				break;
			}
		}
	}
}

void GSUIElementHolder::Save()
{
	for (const UIElem& elem : m_elems)
	{
		switch (elem.type)
		{
			case UIElem::Type::CheckBox:
				theApp.SetConfig(elem.config, static_cast<wxCheckBox*>(elem.control)->GetValue());
				break;
			case UIElem::Type::Choice:
			{
				GSwxChoice* choice = static_cast<GSwxChoice*>(elem.control);
				set_config_from_choice(choice, choice->settings, elem.config);
				break;
			}
			case UIElem::Type::Spin:
				theApp.SetConfig(elem.config, static_cast<wxSpinCtrl*>(elem.control)->GetValue());
				break;
			case UIElem::Type::Slider:
				theApp.SetConfig(elem.config, static_cast<wxSlider*>(elem.control)->GetValue());
				break;
			case UIElem::Type::File:
			case UIElem::Type::Directory:
				theApp.SetConfig(elem.config, static_cast<wxFileDirPickerCtrlBase*>(elem.control)->GetPath().ToUTF8());
				break;
		}
	}
}

void GSUIElementHolder::Update()
{
	for (const UIElem& elem : m_elems)
	{
		if (elem.prereq)
			elem.control->Enable(elem.prereq());
	}
}

void GSUIElementHolder::DisableAll()
{
	for (const UIElem& elem : m_elems)
	{
		if (elem.prereq)
			elem.control->Enable(false);
	}
}

RendererTab::RendererTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	auto hw_prereq = [this]{ return m_is_hardware; };
	auto sw_prereq = [this]{ return !m_is_hardware; };
	auto upscale_prereq = [this]{ return !m_is_native_res; };

	PaddedBoxSizer<wxBoxSizer> tab_box(wxVERTICAL);
	PaddedBoxSizer<wxStaticBoxSizer> general_box(wxVERTICAL, this, "General GS Settings");
	PaddedBoxSizer<wxStaticBoxSizer> hardware_box(wxVERTICAL, this, "Hardware Mode");
	PaddedBoxSizer<wxStaticBoxSizer> software_box(wxVERTICAL, this, "Software Mode");

	auto* hw_checks_box = new wxWrapSizer(wxHORIZONTAL);

	m_ui.addCheckBox(hw_checks_box, "Accurate Destination Alpha Test", "accurate_date",            IDC_ACCURATE_DATE,   hw_prereq);
	m_ui.addCheckBox(hw_checks_box, "Conservative Buffer Allocation",  "conservative_framebuffer", IDC_CONSERVATIVE_FB, upscale_prereq);

	auto* paltex_prereq = m_ui.addCheckBox(hw_checks_box, "GPU Palette Conversion", "paltex", IDC_PALTEX, hw_prereq);
	auto aniso_prereq = [this, paltex_prereq]{ return m_is_hardware && paltex_prereq->GetValue() == false; };

	auto* hw_choice_grid = new wxFlexGridSizer(2, space, space);

	m_internal_resolution = m_ui.addComboBoxAndLabel(hw_choice_grid, "Internal Resolution:", "upscale_multiplier", &theApp.m_gs_upscale_multiplier, -1, hw_prereq).first;

	m_ui.addComboBoxAndLabel(hw_choice_grid, "Anisotropic Filtering:", "MaxAnisotropy",          &theApp.m_gs_max_anisotropy,     IDC_AFCOMBO,             aniso_prereq);
	m_ui.addComboBoxAndLabel(hw_choice_grid, "Dithering (PgDn):",      "dithering_ps2",          &theApp.m_gs_dithering,          IDC_DITHERING,           hw_prereq);
	m_ui.addComboBoxAndLabel(hw_choice_grid, "Mipmapping:",            "mipmap_hw",              &theApp.m_gs_hw_mipmapping,      IDC_MIPMAP_HW,           hw_prereq);
	m_ui.addComboBoxAndLabel(hw_choice_grid, "CRC Hack Level:",        "crc_hack_level",         &theApp.m_gs_crc_level,          IDC_CRC_LEVEL,           hw_prereq);
	m_ui.addComboBoxAndLabel(hw_choice_grid, "Blending Accuracy:",     "accurate_blending_unit", &theApp.m_gs_acc_blend_level,    IDC_ACCURATE_BLEND_UNIT, hw_prereq);
	m_ui.addComboBoxAndLabel(hw_choice_grid, "Texture Preloading:",    "texture_preloading",     &theApp.m_gs_texture_preloading, IDC_PRELOAD_TEXTURES,    hw_prereq);

	hardware_box->Add(hw_checks_box, wxSizerFlags().Centre());
	hardware_box->AddSpacer(space);
	hardware_box->Add(hw_choice_grid, wxSizerFlags().Centre());

	auto* sw_checks_box = new wxWrapSizer(wxHORIZONTAL);
	m_ui.addCheckBox(sw_checks_box, "Auto Flush",              "autoflush_sw", IDC_AUTO_FLUSH_SW, sw_prereq);
	m_ui.addCheckBox(sw_checks_box, "Edge Antialiasing (Del)", "aa1",          IDC_AA1,           sw_prereq);
	m_ui.addCheckBox(sw_checks_box, "Mipmapping",              "mipmap",       IDC_MIPMAP_SW,     sw_prereq);

	software_box->Add(sw_checks_box, wxSizerFlags().Centre());
	software_box->AddSpacer(space);

	// Rendering threads
	auto* thread_box = new wxFlexGridSizer(2, space, space);
	m_ui.addSpinAndLabel(thread_box, "Extra Rendering threads:", "extrathreads", 0, 32, 2, IDC_SWTHREADS, sw_prereq);
	software_box->Add(thread_box, wxSizerFlags().Centre());

	// General GS Settings box
	auto* pcrtc_checks_box = new wxWrapSizer(wxHORIZONTAL);

	m_ui.addCheckBox(pcrtc_checks_box, "Screen Offsets", "pcrtc_offsets", IDC_PCRTC_OFFSETS);
	m_ui.addCheckBox(pcrtc_checks_box, "Disable Interlace Offset", "disable_interlace_offset", IDC_DISABLE_INTERLACE_OFFSETS);

	general_box->Add(pcrtc_checks_box, wxSizerFlags().Center());

	tab_box->Add(hardware_box.outer, wxSizerFlags().Expand());
	tab_box->Add(software_box.outer, wxSizerFlags().Expand());
	tab_box->Add(general_box.outer, wxSizerFlags().Expand());

	SetSizerAndFit(tab_box.outer);
}

HacksTab::HacksTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	PaddedBoxSizer<wxBoxSizer> tab_box(wxVERTICAL);

	auto hw_prereq = [this]{ return m_is_hardware; };
	auto* hacks_check_box = m_ui.addCheckBox(tab_box.inner, "Manual HW Hacks (Disables automatic settings if checked)", "UserHacks", -1, hw_prereq);
	m_ui.addCheckBox(tab_box.inner, "Skip Presenting Duplicate Frames", "skip_duplicate_frames", -1);

	auto hacks_prereq = [this, hacks_check_box]{ return m_is_hardware && hacks_check_box->GetValue(); };
	auto upscale_hacks_prereq = [this, hacks_check_box]{ return !m_is_native_res && hacks_check_box->GetValue(); };

	PaddedBoxSizer<wxStaticBoxSizer> rend_hacks_box   (wxVERTICAL, this, "Renderer Hacks");
	PaddedBoxSizer<wxStaticBoxSizer> upscale_hacks_box(wxVERTICAL, this, "Upscale Hacks");

	auto* rend_hacks_grid    = new wxFlexGridSizer(2, space, space);
	auto* upscale_hacks_grid = new wxFlexGridSizer(3, space, space);

	// Renderer Hacks
	m_ui.addCheckBox(rend_hacks_grid, "Auto Flush",                   "UserHacks_AutoFlush",                     IDC_AUTO_FLUSH_HW,            hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Frame Buffer Conversion",      "UserHacks_CPU_FB_Conversion",             IDC_CPU_FB_CONVERSION,        hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Disable Depth Emulation",      "UserHacks_DisableDepthSupport",           IDC_TC_DEPTH,                 hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Memory Wrapping",              "wrap_gs_mem",                             IDC_MEMORY_WRAPPING,          hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Disable Safe Features",        "UserHacks_Disable_Safe_Features",         IDC_SAFE_FEATURES,            hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Preload Frame Data",           "preload_frame_with_gs_data",              IDC_PRELOAD_GS,               hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Disable Partial Invalidation", "UserHacks_DisablePartialInvalidation",    IDC_DISABLE_PARTIAL_TC_INV,   hacks_prereq);
	m_ui.addCheckBox(rend_hacks_grid, "Texture Inside RT",            "UserHacks_TextureInsideRt",               IDC_TEX_IN_RT,                hacks_prereq);

	// Upscale
	m_ui.addCheckBox(upscale_hacks_grid, "Align Sprite",   "UserHacks_align_sprite_X",  IDC_ALIGN_SPRITE,    upscale_hacks_prereq);
	m_ui.addCheckBox(upscale_hacks_grid, "Merge Sprite",   "UserHacks_merge_pp_sprite", IDC_MERGE_PP_SPRITE, upscale_hacks_prereq);
	m_ui.addCheckBox(upscale_hacks_grid, "Wild Arms Hack", "UserHacks_WildHack",        IDC_WILDHACK,        upscale_hacks_prereq);

	auto* rend_hack_choice_grid    = new wxFlexGridSizer(2, space, space);
	auto* upscale_hack_choice_grid = new wxFlexGridSizer(2, space, space);
	rend_hack_choice_grid   ->AddGrowableCol(1);
	upscale_hack_choice_grid->AddGrowableCol(1);

	// Renderer Hacks:
	m_ui.addComboBoxAndLabel(rend_hack_choice_grid, "Half Screen Fix:",     "UserHacks_Half_Bottom_Override", &theApp.m_gs_generic_list, IDC_HALF_SCREEN_TS, hacks_prereq);
	m_ui.addComboBoxAndLabel(rend_hack_choice_grid, "Trilinear Filtering:", "UserHacks_TriFilter",            &theApp.m_gs_trifilter,    IDC_TRI_FILTER,     hacks_prereq);

	// Skipdraw Range
	add_label(this, rend_hack_choice_grid, "Skipdraw Range:", IDC_SKIPDRAWEND);
	auto* skip_box = new wxBoxSizer(wxHORIZONTAL);
	skip_x_spin = m_ui.addSpin(skip_box, "UserHacks_SkipDraw_Start",      0, 10000, 0, IDC_SKIPDRAWSTART, hacks_prereq);
	skip_y_spin = m_ui.addSpin(skip_box, "UserHacks_SkipDraw_End",        0, 10000, 0, IDC_SKIPDRAWEND,   hacks_prereq);

	rend_hack_choice_grid->Add(skip_box, wxSizerFlags().Expand());

	// Upscale Hacks:
	m_ui.addComboBoxAndLabel(upscale_hack_choice_grid, "Half-Pixel Offset:", "UserHacks_HalfPixelOffset",     &theApp.m_gs_offset_hack, IDC_OFFSETHACK,   upscale_hacks_prereq);
	m_ui.addComboBoxAndLabel(upscale_hack_choice_grid, "Round Sprite:",      "UserHacks_round_sprite_offset", &theApp.m_gs_hack,        IDC_ROUND_SPRITE, upscale_hacks_prereq);

	// Texture Offsets
	add_label(this, upscale_hack_choice_grid, "Texture Offsets:", IDC_TCOFFSETX);
	auto* tex_off_box = new wxBoxSizer(wxHORIZONTAL);
	add_label(this, tex_off_box, "X:", IDC_TCOFFSETX, wxSizerFlags().Centre());
	tex_off_box->AddSpacer(space);
	m_ui.addSpin(tex_off_box, "UserHacks_TCOffsetX", 0, 10000, 0, IDC_TCOFFSETX, hacks_prereq);
	tex_off_box->AddSpacer(space);
	add_label(this, tex_off_box, "Y:", IDC_TCOFFSETY, wxSizerFlags().Centre());
	tex_off_box->AddSpacer(space);
	m_ui.addSpin(tex_off_box, "UserHacks_TCOffsetY", 0, 10000, 0, IDC_TCOFFSETY, hacks_prereq);

	upscale_hack_choice_grid->Add(tex_off_box, wxSizerFlags().Expand());

	rend_hacks_box->Add(rend_hacks_grid, wxSizerFlags().Centre());
	rend_hacks_box->AddSpacer(space);
	rend_hacks_box->Add(rend_hack_choice_grid, wxSizerFlags().Expand());

	upscale_hacks_box->Add(upscale_hacks_grid, wxSizerFlags().Centre());
	upscale_hacks_box->AddSpacer(space);
	upscale_hacks_box->Add(upscale_hack_choice_grid, wxSizerFlags().Expand());

	tab_box->Add(rend_hacks_box.outer, wxSizerFlags().Expand());
	tab_box->Add(upscale_hacks_box.outer, wxSizerFlags().Expand());

	SetSizerAndFit(tab_box.outer);
}

void HacksTab::DoUpdate()
{
	m_ui.Update();

	if (skip_x_spin->GetValue() == 0)
		skip_y_spin->SetValue(0);
	if (skip_y_spin->GetValue() < skip_x_spin->GetValue())
		skip_y_spin->SetValue(skip_x_spin->GetValue());
}

RecTab::RecTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	PaddedBoxSizer<wxBoxSizer> tab_box(wxVERTICAL);

	auto* record_check = m_ui.addCheckBox(tab_box.inner, "Enable Recording (F12)", "capture_enabled");
	CheckboxPrereq record_prereq(record_check);
	PaddedBoxSizer<wxStaticBoxSizer> record_box(wxVERTICAL, this, "Recording");
	auto* record_grid_box = new wxFlexGridSizer(2, space, space);
	record_grid_box->AddGrowableCol(1);

	// Resolution
	add_label(this, record_grid_box, "Resolution:");
	auto* res_box = new wxBoxSizer(wxHORIZONTAL);
	m_ui.addSpin(res_box, "CaptureWidth",  256, 8192, 640, -1, record_prereq);
	m_ui.addSpin(res_box, "CaptureHeight", 256, 8192, 480, -1, record_prereq);

	record_grid_box->Add(res_box, wxSizerFlags().Expand());

	m_ui.addSpinAndLabel(record_grid_box, "Saving Threads:",        "capture_threads",       1, 32, 4, -1, record_prereq);
	m_ui.addSpinAndLabel(record_grid_box, "PNG Compression Level:", "png_compression_level", 1,  9, 1, -1, record_prereq);

	m_ui.addDirPickerAndLabel(record_grid_box, "Output Directory:", "capture_out_dir", -1, record_prereq);

	record_box->Add(record_grid_box, wxSizerFlags().Expand());

	tab_box->Add(record_box.outer, wxSizerFlags().Expand());
	SetSizerAndFit(tab_box.outer);
}

PostTab::PostTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	PaddedBoxSizer<wxBoxSizer> tab_box(wxVERTICAL);
	PaddedBoxSizer<wxStaticBoxSizer> shader_box(wxVERTICAL, this, "Custom Shader");

	auto not_vk_prereq = [this] { return !m_is_vk_hw; };

	m_ui.addCheckBox(shader_box.inner, "Texture Filtering of Display", "linear_present", IDC_LINEAR_PRESENT);
	m_ui.addCheckBox(shader_box.inner, "FXAA Shader (PgUp)",           "fxaa",           IDC_FXAA);

	CheckboxPrereq shade_boost_check(m_ui.addCheckBox(shader_box.inner, "Enable Shade Boost", "ShadeBoost", IDC_SHADEBOOST));

	PaddedBoxSizer<wxStaticBoxSizer> shade_boost_box(wxVERTICAL, this, "Shade Boost");
	auto* shader_boost_grid = new wxFlexGridSizer(2, space, space);
	shader_boost_grid->AddGrowableCol(1);

	auto shader_boost_prereq = [shade_boost_check, this] { return shade_boost_check.box->GetValue(); };
	m_ui.addSliderAndLabel(shader_boost_grid, "Brightness:", "ShadeBoost_Brightness", 0, 100, 50, -1, shader_boost_prereq);
	m_ui.addSliderAndLabel(shader_boost_grid, "Contrast:",   "ShadeBoost_Contrast",   0, 100, 50, -1, shader_boost_prereq);
	m_ui.addSliderAndLabel(shader_boost_grid, "Saturation:", "ShadeBoost_Saturation", 0, 100, 50, -1, shader_boost_prereq);

	shade_boost_box->Add(shader_boost_grid, wxSizerFlags().Expand());
	shader_box->Add(shade_boost_box.outer, wxSizerFlags().Expand());

	CheckboxPrereq ext_shader_check(m_ui.addCheckBox(shader_box.inner, "Enable External Shader", "shaderfx", IDC_SHADER_FX, not_vk_prereq));

	PaddedBoxSizer<wxStaticBoxSizer> ext_shader_box(wxVERTICAL, this, "External Shader (Home)");
	auto* ext_shader_grid = new wxFlexGridSizer(2, space, space);
	ext_shader_grid->AddGrowableCol(1);

	auto shaderext_prereq = [ext_shader_check, this] { return !m_is_vk_hw && ext_shader_check.box->GetValue(); };
	m_ui.addFilePickerAndLabel(ext_shader_grid, "GLSL fx File:", "shaderfx_glsl", -1, shaderext_prereq);
	m_ui.addFilePickerAndLabel(ext_shader_grid, "Config File:",  "shaderfx_conf", -1, shaderext_prereq);

	ext_shader_box->Add(ext_shader_grid, wxSizerFlags().Expand());
	shader_box->Add(ext_shader_box.outer, wxSizerFlags().Expand());

	// TV Shader
	auto* tv_box = new wxFlexGridSizer(2, space, space);
	tv_box->AddGrowableCol(1);
	m_ui.addComboBoxAndLabel(tv_box, "TV Shader:", "TVShader", &theApp.m_gs_tv_shaders);
	shader_box->Add(tv_box, wxSizerFlags().Expand());

	tab_box->Add(shader_box.outer, wxSizerFlags().Expand());
	SetSizerAndFit(tab_box.outer);
}

OSDTab::OSDTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	PaddedBoxSizer<wxBoxSizer> tab_box(wxVERTICAL);

	PaddedBoxSizer<wxStaticBoxSizer> font_box(wxVERTICAL, this, "Visuals");
	auto* font_grid = new wxFlexGridSizer(2, space, space);
	font_grid->AddGrowableCol(1);

	m_ui.addSliderAndLabel(font_grid, "Scale:",   "OsdScale", 50, 300, 100, -1);

	font_box->Add(font_grid, wxSizerFlags().Expand());
	tab_box->Add(font_box.outer, wxSizerFlags().Expand());

	PaddedBoxSizer<wxStaticBoxSizer> log_box(wxVERTICAL, this, "Log Messages");
	auto* log_grid = new wxFlexGridSizer(2, space, space);
	log_grid->AddGrowableCol(1);

	m_ui.addCheckBox(log_grid, "Show Messages",   "OsdShowMessages",   -1);
	m_ui.addCheckBox(log_grid, "Show Speed",      "OsdShowSpeed",      -1);
	m_ui.addCheckBox(log_grid, "Show FPS",        "OsdShowFPS",        -1);
	m_ui.addCheckBox(log_grid, "Show CPU Usage",  "OsdShowCPU",        -1);
	m_ui.addCheckBox(log_grid, "Show GPU Usage",  "OsdShowGPU",        -1);
	m_ui.addCheckBox(log_grid, "Show Resolution", "OsdShowResolution", -1);
	m_ui.addCheckBox(log_grid, "Show Statistics", "OsdShowGSStats",    -1);
	m_ui.addCheckBox(log_grid, "Show Indicators", "OsdShowIndicators", -1);

	log_box->Add(log_grid, wxSizerFlags().Expand());
	tab_box->Add(log_box.outer, wxSizerFlags().Expand());

	SetSizerAndFit(tab_box.outer);
}

DebugTab::DebugTab(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	PaddedBoxSizer<wxBoxSizer> tab_box(wxVERTICAL);

	auto ogl_hw_prereq = [this]{ return m_is_ogl_hw; };
	auto vk_ogl_hw_prereq = [this] { return m_is_ogl_hw || m_is_vk_hw; };

	if (g_Conf->DevMode || IsDevBuild)
	{
		PaddedBoxSizer<wxStaticBoxSizer> debug_box(wxVERTICAL, this, "Debug");
		auto* debug_check_box = new wxWrapSizer(wxHORIZONTAL);
		m_ui.addCheckBox(debug_check_box, "Use Blit Swap Chain",          "UseBlitSwapChain");
		m_ui.addCheckBox(debug_check_box, "Disable Shader Cache",         "disable_shader_cache");
		m_ui.addCheckBox(debug_check_box, "Disable Framebuffer Fetch",    "DisableFramebufferFetch");
		m_ui.addCheckBox(debug_check_box, "Disable Dual-Source Blending", "DisableDualSourceBlend");
		m_ui.addCheckBox(debug_check_box, "Use Debug Device",             "UseDebugDevice");
		m_ui.addCheckBox(debug_check_box, "Dump GS data",                 "dump");

		auto* debug_save_check_box = new wxWrapSizer(wxHORIZONTAL);
		m_ui.addCheckBox(debug_save_check_box, "Save RT",      "save");
		m_ui.addCheckBox(debug_save_check_box, "Save Frame",   "savef");
		m_ui.addCheckBox(debug_save_check_box, "Save Texture", "savet");
		m_ui.addCheckBox(debug_save_check_box, "Save Depth",   "savez");

		debug_box->Add(debug_check_box);
		debug_box->Add(debug_save_check_box);

		auto* dump_grid = new wxFlexGridSizer(2, space, space);

		m_ui.addSpinAndLabel(dump_grid, "Start of Dump:",  "saven", 0, pow(10, 9),    0);
		m_ui.addSpinAndLabel(dump_grid, "Length of Dump:", "savel", 1, pow(10, 5), 5000);

		debug_box->AddSpacer(space);
		debug_box->Add(dump_grid);

		tab_box->Add(debug_box.outer, wxSizerFlags().Expand());
	}

	PaddedBoxSizer<wxStaticBoxSizer> ogl_box(wxVERTICAL, this, "Overrides");
	auto* ogl_grid = new wxFlexGridSizer(2, space, space);
	m_ui.addComboBoxAndLabel(ogl_grid, "Texture Barriers:", "OverrideTextureBarriers",                 &theApp.m_gs_generic_list, -1,                           vk_ogl_hw_prereq);
	m_ui.addComboBoxAndLabel(ogl_grid, "Geometry Shader:",  "OverrideGeometryShaders",                 &theApp.m_gs_generic_list, IDC_GEOMETRY_SHADER_OVERRIDE, vk_ogl_hw_prereq);
	m_ui.addComboBoxAndLabel(ogl_grid, "Image Load Store:", "override_GL_ARB_shader_image_load_store", &theApp.m_gs_generic_list, IDC_IMAGE_LOAD_STORE,         ogl_hw_prereq);
	m_ui.addComboBoxAndLabel(ogl_grid, "Sparse Texture:",   "override_GL_ARB_sparse_texture",          &theApp.m_gs_generic_list, IDC_SPARSE_TEXTURE,           ogl_hw_prereq);
	m_ui.addComboBoxAndLabel(ogl_grid, "Dump Compression:", "GSDumpCompression",                       &theApp.m_gs_dump_compression, -1);
	ogl_box->Add(ogl_grid);

	tab_box->Add(ogl_box.outer, wxSizerFlags().Expand());

	PaddedBoxSizer<wxStaticBoxSizer> tex_box(wxVERTICAL, this, "Texture Replacements");
	auto* tex_grid = new wxFlexGridSizer(2, space, space);
	m_ui.addCheckBox(tex_grid, "Dump Textures",         "DumpReplaceableTextures",      -1);
	m_ui.addCheckBox(tex_grid, "Dump Mipmaps",          "DumpReplaceableMipmaps",       -1);
	m_ui.addCheckBox(tex_grid, "Dump FMV Textures",     "DumpTexturesWithFMVActive",    -1);
	m_ui.addCheckBox(tex_grid, "Async Texture Loading", "LoadTextureReplacementsAsync", -1);
	m_ui.addCheckBox(tex_grid, "Load Textures",         "LoadTextureReplacements",      -1);
	m_ui.addCheckBox(tex_grid, "Precache Textures",     "PrecacheTextureReplacements",  -1);
	tex_box->Add(tex_grid);

	tab_box->Add(tex_box.outer, wxSizerFlags().Expand());

	SetSizerAndFit(tab_box.outer);
}

void DebugTab::DoUpdate()
{
	m_ui.Update();
}

Dialog::Dialog()
	: wxDialog(nullptr, wxID_ANY, "Graphics Settings", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_ui(this)
{
	const int space = wxSizerFlags().Border().GetBorderInPixels();
	auto* padding = new wxBoxSizer(wxVERTICAL);
	m_top_box = new wxBoxSizer(wxVERTICAL);

	auto* top_grid = new wxFlexGridSizer(2, space, space);
	top_grid->SetFlexibleDirection(wxHORIZONTAL);

	m_renderer_select = m_ui.addComboBoxAndLabel(top_grid, "Renderer:", "Renderer", &theApp.m_gs_renderers).first;
	m_renderer_select->Bind(wxEVT_CHOICE, &Dialog::OnRendererChange, this);

	add_label(this, top_grid, "Adapter:");
	m_adapter_select = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, {});
	top_grid->Add(m_adapter_select, wxSizerFlags().Expand());

	m_ui.addComboBoxAndLabel(top_grid, "Deinterlacing (F5):", "deinterlace", &theApp.m_gs_deinterlace);

	m_bifilter_select = m_ui.addComboBoxAndLabel(top_grid, "Texture Filtering:", "filter", &theApp.m_gs_bifilter, IDC_FILTER).first;

	auto* book = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);

	m_renderer_panel = new RendererTab(book);
	m_hacks_panel = new HacksTab(book);
	m_rec_panel = new RecTab(book);
	m_post_panel = new PostTab(book);
	m_osd_panel = new OSDTab(book);
	m_debug_panel = new DebugTab(book);

	book->AddPage(m_renderer_panel, "Renderer", true);
	book->AddPage(m_hacks_panel,    "Hacks");
	book->AddPage(m_post_panel,     "Shader");
	book->AddPage(m_osd_panel,      "OSD");
	book->AddPage(m_rec_panel,      "Recording");
	book->AddPage(m_debug_panel,    "Advanced");

	m_top_box->Add(top_grid, wxSizerFlags().Centre());
	m_top_box->AddSpacer(space);
	m_top_box->Add(book, wxSizerFlags(1).Expand());

	padding->Add(m_top_box, wxSizerFlags(1).Expand().Border());

	m_top_box->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right());

	SetSizerAndFit(padding);
	Bind(wxEVT_CHECKBOX, &Dialog::CallUpdate, this);
	Bind(wxEVT_SPINCTRL, &Dialog::CallUpdate, this);
	Bind(wxEVT_CHOICE,   &Dialog::CallUpdate, this);
}

Dialog::~Dialog()
{
}

void Dialog::CallUpdate(wxCommandEvent&)
{
	Update();
}

void Dialog::OnRendererChange(wxCommandEvent&)
{
	RendererChange();
	Update();
}

GSRendererType Dialog::GetSelectedRendererType()
{
	const int index = m_renderer_select->GetSelection();

	// there is no currently selected renderer or the combo box has more entries than the renderer list or the current selection is negative
	// make sure you haven't made a mistake initializing everything
	ASSERT(index < static_cast<int>(theApp.m_gs_renderers.size()) || index >= 0);

	const GSRendererType type = static_cast<GSRendererType>(theApp.m_gs_renderers[index].value);
	return (type == GSRendererType::Auto) ? GSUtil::GetPreferredRenderer() : type;
}

void Dialog::RendererChange()
{
	const GSRendererType renderer = GetSelectedRendererType();
	const std::string current_adapter(theApp.GetConfigS("Adapter"));

	HostDisplay::AdapterAndModeList list;
	switch (renderer)
	{
#ifdef _WIN32
	case GSRendererType::DX11:
		list = D3D11HostDisplay::StaticGetAdapterAndModeList();
		break;
	case GSRendererType::DX12:
		list = D3D12HostDisplay::StaticGetAdapterAndModeList();
		break;
#endif
#ifdef ENABLE_VULKAN
	case GSRendererType::VK:
		list = VulkanHostDisplay::StaticGetAdapterAndModeList(nullptr);
		break;
#endif
#ifdef __APPLE__
	case GSRendererType::Metal:
		list = GetMetalAdapterAndModeList();
		break;
#endif
	default:
		break;
	}

	m_adapter_select->Clear();
	m_adapter_select->Insert(_("Default Adapter"), 0);
	if (current_adapter.empty())
		m_adapter_select->SetSelection(0);

	for (const std::string& name : list.adapter_names)
	{
		m_adapter_select->Insert(fromUTF8(name), m_adapter_select->GetCount());
		if (current_adapter == name)
			m_adapter_select->SetSelection(m_adapter_select->GetCount() - 1);
	}

	m_adapter_select->Enable(!list.adapter_names.empty());

#ifdef _WIN32
	m_renderer_panel->Layout(); // The version of wx we use on Windows is dumb and something prevents relayout from happening to notebook pages
#endif
}

void Dialog::Load()
{
	m_ui.Load();

	const GSRendererType renderer = GSRendererType(theApp.GetConfigI("Renderer"));
	m_renderer_select->SetSelection(get_config_index(theApp.m_gs_renderers, static_cast<int>(renderer)));

	RendererChange();

	m_hacks_panel->Load();
	m_renderer_panel->Load();
	m_rec_panel->Load();
	m_post_panel->Load();
	m_osd_panel->Load();
	m_debug_panel->Load();
}

void Dialog::Save()
{
	m_ui.Save();
	// only save the adapter when it makes sense to
	// prevents changing the adapter, switching to another renderer and saving
	if (m_adapter_select->GetCount() > 1)
	{
		// First option is system default
		if (m_adapter_select->GetSelection() == 0)
			theApp.SetConfig("Adapter", "");
		else
			theApp.SetConfig("Adapter", m_adapter_select->GetStringSelection().c_str());
	}

	m_hacks_panel->Save();
	m_renderer_panel->Save();
	m_rec_panel->Save();
	m_post_panel->Save();
	m_osd_panel->Save();
	m_debug_panel->Save();
}

void Dialog::Update()
{
	GSRendererType renderer = GetSelectedRendererType();
	if (renderer == GSRendererType::Null)
	{
		m_ui.DisableAll();
		m_renderer_select->Enable();
		m_hacks_panel->m_ui.DisableAll();
		m_renderer_panel->m_ui.DisableAll();
		m_rec_panel->m_ui.DisableAll();
		m_post_panel->m_ui.DisableAll();
		m_osd_panel->m_ui.DisableAll();
		m_debug_panel->m_ui.DisableAll();
	}
	else
	{
		// cross-tab dependencies yay
		const bool is_hw = renderer == GSRendererType::OGL || renderer == GSRendererType::DX11 || renderer == GSRendererType::VK || renderer == GSRendererType::Metal || renderer == GSRendererType::DX12;
		const bool is_upscale = m_renderer_panel->m_internal_resolution->GetSelection() != 0;
		m_hacks_panel->m_is_native_res = !is_hw || !is_upscale;
		m_hacks_panel->m_is_hardware = is_hw;
		m_renderer_panel->m_is_hardware = is_hw;
		m_renderer_panel->m_is_native_res = !is_hw || !is_upscale;
		m_post_panel->m_is_vk_hw = renderer == GSRendererType::VK;
		m_debug_panel->m_is_ogl_hw = renderer == GSRendererType::OGL;
		m_debug_panel->m_is_vk_hw = renderer == GSRendererType::VK;

		m_ui.Update();
		m_hacks_panel->DoUpdate();
		m_renderer_panel->DoUpdate();
		m_rec_panel->DoUpdate();
		m_post_panel->DoUpdate();
		m_osd_panel->DoUpdate();
		m_debug_panel->DoUpdate();
	}
}

bool RunwxDialog()
{
	Dialog GSSettingsDialog;

	GSSettingsDialog.Load();
	GSSettingsDialog.Update();
	if (GSSettingsDialog.ShowModal() == wxID_OK)
		GSSettingsDialog.Save();

	return true;
}
