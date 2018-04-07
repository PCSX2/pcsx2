/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include <gtk/gtk.h>
#include "GS.h"
#include "GSdx.h"
#include "GSdxResources.h"
#include "GSSetting.h"

// Port of deprecated GTK2 API to recent GTK3. Those defines
// could prove handy for testing
#define GTK3_MONITOR_API (0 && GTK_CHECK_VERSION(3, 22, 0))
#define GTK3_GRID_API (0 && GTK_CHECK_VERSION(3, 10, 0))

static GtkWidget* s_hack_frame;

bool BigEnough()
{
#if GTK3_MONITOR_API
	GdkMonitor *monitor = gdk_display_get_primary_monitor(gdk_display_get_default());
	// int scale = gdk_monitor_get_scale_factor(monitor);
	GdkRectangle my_geometry;
	gdk_monitor_get_geometry(monitor, &my_geometry);
	return my_geometry.height > 1000;
#else
	return (gdk_screen_get_height(gdk_screen_get_default()) > 1000);
#endif
}

void AddTooltip(GtkWidget* w, int idc)
{
	gtk_widget_set_tooltip_text(w, dialog_message(idc));
}

void AddTooltip(GtkWidget* w1, GtkWidget* w2, int idc)
{
	AddTooltip(w1, idc);
	AddTooltip(w2, idc);
}

GtkWidget* CreateVbox()
{
#if GTK_CHECK_VERSION(3, 0, 0)
	return gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
#else
	return gtk_vbox_new(false, 5);
#endif

}

GtkWidget* left_label(const char* lbl)
{
	GtkWidget* w = gtk_label_new(lbl);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_set_halign(w, GTK_ALIGN_START);
#else
	gtk_misc_set_alignment(GTK_MISC(w),0.0,0.5);
#endif
	return w;
}

void CB_ChangedComboBox(GtkComboBox *combo, gpointer user_data)
{
	int p = gtk_combo_box_get_active(combo);
	auto s = reinterpret_cast<std::vector<GSSetting>*>(g_object_get_data(G_OBJECT(combo), "Settings"));

	try {
		theApp.SetConfig((char*)user_data, s->at(p).value);
	} catch (...) {
	}
}

GtkWidget* CreateComboBoxFromVector(const std::vector<GSSetting>& s, const char* opt_name)
{
	GtkWidget* combo_box = gtk_combo_box_text_new();
	int32_t opt_value    = theApp.GetConfigI(opt_name);
	int opt_position     = 0;

	for(size_t i = 0; i < s.size(); i++)
	{
		std::string label = s[i].name;

		if(!s[i].note.empty()) label += format(" (%s)", s[i].note.c_str());

		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), label.c_str());

		if (s[i].value == opt_value)
			opt_position = i;
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), opt_position);

	g_signal_connect(combo_box, "changed", G_CALLBACK(CB_ChangedComboBox), const_cast<char*>(opt_name));
	g_object_set_data(G_OBJECT(combo_box), "Settings", (void*)&s);

	return combo_box;
}

void CB_EntryActived(GtkEntry *entry, gpointer user_data)
{
	int hex_value = 0;
	const gchar *data = gtk_entry_get_text(entry);

	if (sscanf(data,"%X",&hex_value) == 1)
		theApp.SetConfig((char*)user_data, hex_value);
}

GtkWidget* CreateTextBox(const char* opt_name)
{
	GtkWidget* entry = gtk_entry_new();

	int hex_value = theApp.GetConfigI(opt_name);

	gchar* data=(gchar *)g_malloc(sizeof(gchar)*40);
	sprintf(data,"%X", hex_value);
	gtk_entry_set_text(GTK_ENTRY(entry),data);
	g_free(data);

	g_signal_connect(entry, "activate", G_CALLBACK(CB_EntryActived), const_cast<char*>(opt_name));
	g_signal_connect(entry, "changed", G_CALLBACK(CB_EntryActived), const_cast<char*>(opt_name));

	return entry;
}

void CB_ToggleCheckBox(GtkToggleButton *togglebutton, gpointer user_data)
{
	char* opt = (char*)user_data;
	theApp.SetConfig(opt, (int)gtk_toggle_button_get_active(togglebutton));
	if (strcmp(opt, "UserHacks") == 0) {
		gtk_widget_set_sensitive(s_hack_frame, gtk_toggle_button_get_active(togglebutton));
	}
}

GtkWidget* CreateCheckBox(const char* label, const char* opt_name)
{
	GtkWidget* check = gtk_check_button_new_with_label(label);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), theApp.GetConfigB(opt_name));

	g_signal_connect(check, "toggled", G_CALLBACK(CB_ToggleCheckBox), const_cast<char*>(opt_name));

	return check;
}

void CB_SpinButton(GtkSpinButton *spin, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, (int)gtk_spin_button_get_value(spin));
}

GtkWidget* CreateSpinButton(double min, double max, const char* opt_name)
{
	GtkWidget* spin = gtk_spin_button_new_with_range(min, max, 1);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), theApp.GetConfigI(opt_name));

	g_signal_connect(spin, "value-changed", G_CALLBACK(CB_SpinButton), const_cast<char*>(opt_name));

	return spin;
}

void CB_RangeChanged(GtkRange* range, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, (int)gtk_range_get_value(range));
}

GtkWidget* CreateScale(const char* opt_name)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 10);
#else
	GtkWidget* scale = gtk_hscale_new_with_range(0, 100, 10);
#endif

	gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(scale), theApp.GetConfigI(opt_name));

	g_signal_connect(scale, "value-changed", G_CALLBACK(CB_RangeChanged), const_cast<char*>(opt_name));

	return scale;
}

void CB_PickFile(GtkFileChooserButton *chooser, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser)));
}

GtkWidget* CreateFileChooser(GtkFileChooserAction action, const char* label, const char* opt_name)
{
	GtkWidget* chooser = gtk_file_chooser_button_new(label, action);

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), theApp.GetConfigS(opt_name).c_str());

	g_signal_connect(chooser, "file-set", G_CALLBACK(CB_PickFile), const_cast<char*>(opt_name));

	return chooser;
}

static int s_table_line = 0;

void AttachInTable(GtkWidget* table, GtkWidget *w, int pos, int pad = 0, int size = 1)
{
#if GTK3_GRID_API
	g_object_set(w, "margin-left", pad, NULL);
	g_object_set(w, "expand", true, nullptr);
	gtk_grid_attach(GTK_GRID(table), w, pos, s_table_line, size, 1);
#else
	GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default
	gtk_table_attach(GTK_TABLE(table), w, pos, pos + size, s_table_line, s_table_line+1, opt, opt, pad, 0);
#endif
}

void InsertWidgetInTable(GtkWidget* table, GtkWidget *left, GtkWidget *right = NULL, GtkWidget *third = NULL)
{
	guint l_xpad = GTK_IS_CHECK_BUTTON(left) ? 0 : 22;

	if (!left) {
		AttachInTable(table, right, 1);
	} else if (!right) {
		AttachInTable(table, left, 0, l_xpad);
	} else if (right == left) {
		AttachInTable(table, right, 0, 0, 2);
	} else {
		AttachInTable(table, left, 0, l_xpad);
		AttachInTable(table, right, 1);
	}
	if (third) {
		AttachInTable(table, third, 2);
	}

	s_table_line++;
}

GtkWidget* CreateTableInBox(GtkWidget* parent_box, const char* frame_title, int row, int col) {
#if GTK3_GRID_API
	GtkWidget* table = gtk_grid_new();
	g_object_set(table, "expand", true, nullptr);
#else
	GtkWidget* table = gtk_table_new(row, col, false);
#endif
	GtkWidget* container = (frame_title) ? gtk_frame_new (frame_title) : CreateVbox();
	gtk_container_add(GTK_CONTAINER(container), table);
	gtk_container_add(GTK_CONTAINER(parent_box), container);

	return table;
}

void populate_hw_table(GtkWidget* hw_table)
{
	GtkWidget* fsaa_label     = left_label("Internal Resolution:");
	GtkWidget* fsaa_combo_box = CreateComboBoxFromVector(theApp.m_gs_upscale_multiplier, "upscale_multiplier");

	GtkWidget* af_label     = left_label("Anisotropic Filtering:");
	GtkWidget* af_combo_box = CreateComboBoxFromVector(theApp.m_gs_max_anisotropy, "MaxAnisotropy");

	GtkWidget* crc_label     = left_label("Automatic CRC level:");
	GtkWidget* crc_combo_box = CreateComboBoxFromVector(theApp.m_gs_crc_level, "crc_hack_level");

	GtkWidget* paltex_check     = CreateCheckBox("Allow 8 bits textures", "paltex");
	GtkWidget* acc_date_check   = CreateCheckBox("Accurate Date", "accurate_date");
	GtkWidget* large_fb_check   = CreateCheckBox("Large Framebuffer", "large_framebuffer");

	GtkWidget* acc_bld_label     = left_label("Blending Unit Accuracy:");
	GtkWidget* acc_bld_combo_box = CreateComboBoxFromVector(theApp.m_gs_acc_blend_level, "accurate_blending_unit");

	GtkWidget* hack_enable_check   = CreateCheckBox("Enable User Hacks", "UserHacks");

	GtkWidget* mipmap_label     = left_label("Mipmapping (Insert):");
	GtkWidget* mipmap_combo_box = CreateComboBoxFromVector(theApp.m_gs_hw_mipmapping, "mipmap_hw");

	// Some helper string
	AddTooltip(paltex_check, IDC_PALTEX);
	AddTooltip(acc_date_check, IDC_ACCURATE_DATE);
	AddTooltip(large_fb_check, IDC_LARGE_FB);
	AddTooltip(crc_label, crc_combo_box, IDC_CRC_LEVEL);
	AddTooltip(acc_bld_label, acc_bld_combo_box, IDC_ACCURATE_BLEND_UNIT);
	AddTooltip(af_label, af_combo_box, IDC_AFCOMBO);
	gtk_widget_set_tooltip_text(hack_enable_check, "Enable the HW hack option panel");
	AddTooltip(mipmap_label, IDC_MIPMAP_HW);
	AddTooltip(mipmap_combo_box, IDC_MIPMAP_HW);

	s_table_line = 0;
	InsertWidgetInTable(hw_table , paltex_check  , acc_date_check);
	InsertWidgetInTable(hw_table , large_fb_check, hack_enable_check);
	InsertWidgetInTable(hw_table , fsaa_label    , fsaa_combo_box);
	InsertWidgetInTable(hw_table , af_label      , af_combo_box);
	InsertWidgetInTable(hw_table , acc_bld_label , acc_bld_combo_box);
	InsertWidgetInTable(hw_table , crc_label     , crc_combo_box);
	InsertWidgetInTable(hw_table , mipmap_label  , mipmap_combo_box);
}

void populate_gl_table(GtkWidget* gl_table)
{
	GtkWidget* gl_gs_label = left_label("Geometry Shader:");
	GtkWidget* gl_gs_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_geometry_shader");
	GtkWidget* gl_ils_label = left_label("Image Load Store:");
	GtkWidget* gl_ils_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_shader_image_load_store");

	AddTooltip(gl_gs_label, gl_gs_combo, IDC_GEOMETRY_SHADER_OVERRIDE);
	AddTooltip(gl_ils_label, gl_ils_combo, IDC_IMAGE_LOAD_STORE);

	s_table_line = 0;
	InsertWidgetInTable(gl_table , gl_gs_label  , gl_gs_combo);
	InsertWidgetInTable(gl_table , gl_ils_label , gl_ils_combo);
}

void populate_sw_table(GtkWidget* sw_table)
{
	GtkWidget* threads_label = left_label("Extra rendering threads:");
	GtkWidget* threads_spin  = CreateSpinButton(0, 32, "extrathreads");

	GtkWidget* aa_check         = CreateCheckBox("Edge anti-aliasing (AA1)", "aa1");
	GtkWidget* mipmap_check     = CreateCheckBox("Mipmapping", "mipmap");

	AddTooltip(aa_check, IDC_AA1);
	AddTooltip(mipmap_check, IDC_MIPMAP_SW);
	AddTooltip(threads_label, threads_spin, IDC_SWTHREADS);

	s_table_line = 0;
	InsertWidgetInTable(sw_table , threads_label, threads_spin);
	InsertWidgetInTable(sw_table , aa_check     , mipmap_check);
}

void populate_shader_table(GtkWidget* shader_table)
{
	GtkWidget* shader            = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_OPEN, "Select an external shader", "shaderfx_glsl");
	GtkWidget* shader_conf       = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_OPEN, "Then select a config", "shaderfx_conf");
	GtkWidget* shader_label      = left_label("External shader glsl");
	GtkWidget* shader_conf_label = left_label("External shader conf");

	GtkWidget* shadeboost_check = CreateCheckBox("Shade boost", "ShadeBoost");
	GtkWidget* fxaa_check       = CreateCheckBox("Fxaa shader", "fxaa");
	GtkWidget* shaderfx_check   = CreateCheckBox("External shader", "shaderfx");

	GtkWidget* tv_shader_label  = left_label("TV shader:");
	GtkWidget* tv_shader        = CreateComboBoxFromVector(theApp.m_gs_tv_shaders, "TVShader");

	GtkWidget* linear_check     = CreateCheckBox("Texture Filtering of Display", "linear_present");

	// Shadeboost scale
	GtkWidget* sb_brightness       = CreateScale("ShadeBoost_Brightness");
	GtkWidget* sb_brightness_label = left_label("Shade Boost Brightness:");

	GtkWidget* sb_contrast         = CreateScale("ShadeBoost_Contrast");
	GtkWidget* sb_contrast_label   = left_label("Shade Boost Contrast:");

	GtkWidget* sb_saturation       = CreateScale("ShadeBoost_Saturation");
	GtkWidget* sb_saturation_label = left_label("Shade Boost Saturation:");

	AddTooltip(shadeboost_check, IDC_SHADEBOOST);
	AddTooltip(shaderfx_check, IDC_SHADER_FX);
	AddTooltip(fxaa_check, IDC_FXAA);
	AddTooltip(linear_check, IDC_LINEAR_PRESENT);

	s_table_line = 0;
	InsertWidgetInTable(shader_table , linear_check);
	InsertWidgetInTable(shader_table , fxaa_check);
	InsertWidgetInTable(shader_table , shadeboost_check);
	InsertWidgetInTable(shader_table , sb_brightness_label , sb_brightness);
	InsertWidgetInTable(shader_table , sb_contrast_label   , sb_contrast);
	InsertWidgetInTable(shader_table , sb_saturation_label , sb_saturation);
	InsertWidgetInTable(shader_table , shaderfx_check);
	InsertWidgetInTable(shader_table , shader_label        , shader);
	InsertWidgetInTable(shader_table , shader_conf_label   , shader_conf);
	InsertWidgetInTable(shader_table , tv_shader_label     , tv_shader);
}

void populate_hack_table(GtkWidget* hack_table)
{
	GtkWidget* hack_offset_label   = left_label("Half-pixel Offset:");
	GtkWidget* hack_offset_box     = CreateComboBoxFromVector(theApp.m_gs_offset_hack, "UserHacks_HalfPixelOffset");
	GtkWidget* hack_skipdraw_label = left_label("Skipdraw:");
	GtkWidget* hack_skipdraw_spin  = CreateSpinButton(0, 10000, "UserHacks_SkipDraw");
	GtkWidget* hack_wild_check     = CreateCheckBox("Wild Arms Hack", "UserHacks_WildHack");
	GtkWidget* hack_tco_label      = left_label("Texture Offset: 0x");
	GtkWidget* hack_tco_entry      = CreateTextBox("UserHacks_TCOffset");
	GtkWidget* align_sprite_check  = CreateCheckBox("Align Sprite", "UserHacks_align_sprite_X");
	GtkWidget* preload_gs_check    = CreateCheckBox("Preload Frame Data", "preload_frame_with_gs_data");
	GtkWidget* hack_fast_inv       = CreateCheckBox("Fast Texture Invalidation", "UserHacks_DisablePartialInvalidation");
	GtkWidget* hack_depth_check    = CreateCheckBox("Disable Depth Emulation", "UserHacks_DisableDepthSupport");
	GtkWidget* hack_cpu_fbcv       = CreateCheckBox("Frame Buffer Conversion", "UserHacks_CPU_FB_Conversion");
	GtkWidget* hack_auto_flush     = CreateCheckBox("Auto Flush", "UserHacks_AutoFlush");
	GtkWidget* hack_unscale_prim   = CreateCheckBox("Unscale Point and Line", "UserHacks_unscale_point_line");
	GtkWidget* hack_merge_sprite   = CreateCheckBox("Merge Sprite", "UserHacks_merge_pp_sprite");
	GtkWidget* hack_wrap_mem       = CreateCheckBox("Memory Wrapping", "wrap_gs_mem");

	GtkWidget* hack_sprite_box     = CreateComboBoxFromVector(theApp.m_gs_hack, "UserHacks_SpriteHack");
	GtkWidget* hack_sprite_label   = left_label("Sprite:");
	GtkWidget* stretch_hack_box    = CreateComboBoxFromVector(theApp.m_gs_hack, "UserHacks_round_sprite_offset");
	GtkWidget* stretch_hack_label  = left_label("Round Sprite:");
	GtkWidget* trilinear_box       = CreateComboBoxFromVector(theApp.m_gs_trifilter, "UserHacks_TriFilter");
	GtkWidget* trilinear_label     = left_label("Trilinear Filtering:");

	// Reuse windows helper string :)
	AddTooltip(hack_offset_label, IDC_OFFSETHACK);
	AddTooltip(hack_offset_box, IDC_OFFSETHACK);
	AddTooltip(hack_skipdraw_label, IDC_SKIPDRAWHACK);
	AddTooltip(hack_skipdraw_spin, IDC_SKIPDRAWHACK);
	AddTooltip(hack_wild_check, IDC_WILDHACK);
	AddTooltip(hack_sprite_label, hack_sprite_box, IDC_SPRITEHACK);
	AddTooltip(hack_tco_label, IDC_TCOFFSETX);
	AddTooltip(hack_tco_entry, IDC_TCOFFSETX);
	AddTooltip(align_sprite_check, IDC_ALIGN_SPRITE);
	AddTooltip(stretch_hack_label, stretch_hack_box, IDC_ROUND_SPRITE);
	AddTooltip(preload_gs_check, IDC_PRELOAD_GS);
	AddTooltip(hack_fast_inv, IDC_FAST_TC_INV);
	AddTooltip(hack_depth_check, IDC_TC_DEPTH);
	AddTooltip(hack_cpu_fbcv, IDC_CPU_FB_CONVERSION);
	AddTooltip(hack_auto_flush, IDC_AUTO_FLUSH);
	AddTooltip(hack_unscale_prim, IDC_UNSCALE_POINT_LINE);
	AddTooltip(hack_merge_sprite, IDC_MERGE_PP_SPRITE);
	AddTooltip(hack_wrap_mem, IDC_MEMORY_WRAPPING);
	AddTooltip(trilinear_box, IDC_TRI_FILTER);
	AddTooltip(trilinear_label, IDC_TRI_FILTER);


	s_table_line = 0;
	//Hacks
	// Column one and two HW Hacks
	InsertWidgetInTable(hack_table , align_sprite_check  , hack_wrap_mem);
	InsertWidgetInTable(hack_table , hack_auto_flush     , hack_merge_sprite);
	InsertWidgetInTable(hack_table , hack_depth_check    , preload_gs_check);
	InsertWidgetInTable(hack_table , hack_fast_inv       , hack_unscale_prim);
	InsertWidgetInTable(hack_table , hack_cpu_fbcv       , hack_wild_check);
	// Other upscaling hacks
	InsertWidgetInTable(hack_table , trilinear_label     , trilinear_box);
	InsertWidgetInTable(hack_table , hack_offset_label   , hack_offset_box);
	InsertWidgetInTable(hack_table , hack_sprite_label   , hack_sprite_box );
	InsertWidgetInTable(hack_table , stretch_hack_label  , stretch_hack_box );
	InsertWidgetInTable(hack_table , hack_skipdraw_label , hack_skipdraw_spin);
	InsertWidgetInTable(hack_table , hack_tco_label      , hack_tco_entry);
}

void populate_main_table(GtkWidget* main_table)
{
	GtkWidget* render_label        = left_label("Renderer:");
	GtkWidget* render_combo_box    = CreateComboBoxFromVector(theApp.m_gs_renderers, "Renderer");
	GtkWidget* interlace_label     = left_label("Interlacing (F5):");
	GtkWidget* interlace_combo_box = CreateComboBoxFromVector(theApp.m_gs_interlace, "interlace");
	GtkWidget* filter_label        = left_label("Texture Filtering:");
	GtkWidget* filter_combo_box    = CreateComboBoxFromVector(theApp.m_gs_bifilter, "filter");

	AddTooltip(filter_label, filter_combo_box, IDC_FILTER);

	s_table_line = 0;
	InsertWidgetInTable(main_table, render_label, render_combo_box);
	InsertWidgetInTable(main_table, interlace_label, interlace_combo_box);
	InsertWidgetInTable(main_table, filter_label, filter_combo_box);
}

void populate_debug_table(GtkWidget* debug_table)
{
	GtkWidget* glsl_debug_check = CreateCheckBox("GLSL compilation", "debug_glsl_shader");
	GtkWidget* gl_debug_check   = CreateCheckBox("Print GL error", "debug_opengl");
	GtkWidget* gs_dump_check    = CreateCheckBox("Dump GS data", "dump");
	GtkWidget* gs_save_check    = CreateCheckBox("Save RT", "save");
	GtkWidget* gs_savef_check   = CreateCheckBox("Save Frame", "savef");
	GtkWidget* gs_savet_check   = CreateCheckBox("Save Texture", "savet");
	GtkWidget* gs_savez_check   = CreateCheckBox("Save Depth", "savez");

	GtkWidget* gs_saven_label   = left_label("Start of Dump");
	GtkWidget* gs_saven_spin    = CreateSpinButton(0, pow(10, 9), "saven");
	GtkWidget* gs_savel_label   = left_label("Length of Dump");
	GtkWidget* gs_savel_spin    = CreateSpinButton(0, pow(10, 5), "savel");

	s_table_line = 0;
	InsertWidgetInTable(debug_table, gl_debug_check, glsl_debug_check);
	InsertWidgetInTable(debug_table, gs_dump_check);
	InsertWidgetInTable(debug_table, gs_save_check, gs_savef_check);
	InsertWidgetInTable(debug_table, gs_savet_check, gs_savez_check);
	InsertWidgetInTable(debug_table, gs_saven_label, gs_saven_spin);
	InsertWidgetInTable(debug_table, gs_savel_label, gs_savel_spin);
}

void populate_record_table(GtkWidget* record_table)
{
	GtkWidget* capture_check = CreateCheckBox("Enable Recording (with F12)", "capture_enabled");
	GtkWidget* resxy_label   = left_label("Resolution:");
	GtkWidget* resx_spin     = CreateSpinButton(256, 8192, "CaptureWidth");
	GtkWidget* resy_spin     = CreateSpinButton(256, 8192, "CaptureHeight");
	GtkWidget* threads_label = left_label("Saving Threads:");
	GtkWidget* threads_spin  = CreateSpinButton(1, 32, "capture_threads");
	GtkWidget* out_dir_label = left_label("Output Directory:");
	GtkWidget* out_dir       = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Select a directory", "capture_out_dir");
	GtkWidget* png_label     = left_label("PNG Compression Level:");
	GtkWidget* png_level     = CreateSpinButton(1, 9, "png_compression_level");

	InsertWidgetInTable(record_table , capture_check);
	InsertWidgetInTable(record_table , resxy_label   , resx_spin      , resy_spin);
	InsertWidgetInTable(record_table , threads_label , threads_spin);
	InsertWidgetInTable(record_table , png_label     , png_level);
	InsertWidgetInTable(record_table , out_dir_label , out_dir);
}

void populate_osd_table(GtkWidget* osd_table)
{
	GtkWidget* fontname_label  = left_label("Font:");
	GtkWidget* fontname_file = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_OPEN, "Select a font", "osd_fontname");
	GtkWidget* fontsize_label  = left_label("Size:");
	GtkWidget* fontsize_text = CreateSpinButton(1, 100, "osd_fontsize");
	GtkWidget* transparency_label = left_label("Transparency:");
	GtkWidget* transparency_slide = CreateScale("osd_transparency");
	GtkWidget* log_check = CreateCheckBox("Enable Log", "osd_log_enabled");
	GtkWidget* log_speed_label  = left_label("Speed:");
	GtkWidget* log_speed_text = CreateSpinButton(2, 10, "osd_log_speed");
	GtkWidget* max_messages_label = left_label("Maximum Onscreen Log Messages:");
	GtkWidget* max_messages_spin = CreateSpinButton(1, 20, "osd_max_log_messages");
	GtkWidget* monitor_check = CreateCheckBox("Enable Monitor", "osd_monitor_enabled");
	GtkWidget* indicator_check = CreateCheckBox("Enable Indicator", "osd_indicator_enabled");

	AddTooltip(log_check, IDC_OSD_LOG);
	AddTooltip(monitor_check, IDC_OSD_MONITOR);
	AddTooltip(max_messages_label, max_messages_spin, IDC_OSD_MAX_LOG);

	InsertWidgetInTable(osd_table , fontname_label , fontname_file);
	InsertWidgetInTable(osd_table , fontsize_label , fontsize_text);
	InsertWidgetInTable(osd_table , transparency_label , transparency_slide);
	InsertWidgetInTable(osd_table , log_check);
	InsertWidgetInTable(osd_table , log_speed_label, log_speed_text);
	InsertWidgetInTable(osd_table , max_messages_label, max_messages_spin);
	InsertWidgetInTable(osd_table , monitor_check, indicator_check);
}

GtkWidget* ScrollMe(GtkWidget* w)
{
	// the scrolled window add an ugly outline/border even when the scroll bar is off.
	if (BigEnough())
		return w;

	GtkWidget* scrollbar = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollbar), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollbar), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrollbar), true);
#endif

#if GTK_CHECK_VERSION(3, 8, 0)
	gtk_container_add(GTK_CONTAINER(scrollbar), w);
#else
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollbar), w);
#endif

	return scrollbar;
}

bool RunLinuxDialog()
{
	GtkWidget *dialog;
	int return_value;

	/* Create the widgets */
	dialog = gtk_dialog_new_with_buttons (
		"GSdx Config",
		NULL, /* parent window*/
		(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
		"OK", GTK_RESPONSE_ACCEPT,
		// "Cancel", GTK_RESPONSE_REJECT, // Drop because it is too annoying to support call back this way
		NULL);

	// The main area for the whole dialog box.
	GtkWidget* main_box     = CreateVbox();
	GtkWidget* central_box  = CreateVbox();
	GtkWidget* advanced_box = CreateVbox();
	GtkWidget* debug_box    = CreateVbox();
	GtkWidget* osd_box      = CreateVbox();

	// Grab a logo, to make things look nice.
	if (BigEnough()) {
		GResource * resources = GSdx_res_get_resource();
		GInputStream * ogl_stream=g_resource_open_stream(resources,"/GSdx/res/logo-ogl.bmp",G_RESOURCE_LOOKUP_FLAGS_NONE,NULL);
		GdkPixbuf * ogl_logo = gdk_pixbuf_new_from_stream(ogl_stream,NULL,NULL);
		g_object_unref(ogl_stream);
		GtkWidget* logo_image  = gtk_image_new_from_pixbuf(ogl_logo);
		gtk_box_pack_start(GTK_BOX(main_box), logo_image, true, true, 0);
	}

	GtkWidget* main_table   = CreateTableInBox(main_box    , NULL                                   , 2  , 2);

	GtkWidget* hw_table     = CreateTableInBox(central_box , "Hardware Mode Settings"               , 7  , 2);
	GtkWidget* sw_table     = CreateTableInBox(central_box , "Software Mode Settings"               , 2  , 2);

	GtkWidget* hack_table   = CreateTableInBox(advanced_box, "Hacks"                                , 7 , 2);
	GtkWidget* gl_table     = CreateTableInBox(advanced_box, "OpenGL Very Advanced Custom Settings" , 6 , 2);

	GtkWidget* record_table = CreateTableInBox(debug_box   , "Recording Settings"                   , 4  , 3);
	GtkWidget* debug_table  = CreateTableInBox(debug_box   , "OpenGL / GSdx Debug Settings"         , 6  , 3);

	GtkWidget* shader_table = CreateTableInBox(osd_box     , "Custom Shader Settings"               , 9  , 2);
	GtkWidget* osd_table    = CreateTableInBox(osd_box     , "OSD"                                  , 6  , 2);

	// Populate all the tables
	populate_main_table(main_table);

	populate_shader_table(shader_table);
	populate_hw_table(hw_table);
	populate_sw_table(sw_table);

	populate_hack_table(hack_table);
	populate_gl_table(gl_table);

	populate_debug_table(debug_table);
	populate_record_table(record_table);

	populate_osd_table(osd_table);

	// Handle some nice tab
	GtkWidget* notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), central_box , gtk_label_new("Renderer Settings"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advanced_box, gtk_label_new("Advanced Settings"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), debug_box   , gtk_label_new("Debug/Recording"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ScrollMe(osd_box), gtk_label_new("Post-Processing/OSD"));

	// Put everything in the big box.
	gtk_container_add(GTK_CONTAINER(main_box), notebook);

	// Enable/disable hack frame based on enable option
	s_hack_frame = hack_table;
	gtk_widget_set_sensitive(s_hack_frame, theApp.GetConfigB("UserHacks"));

	// Put the box in the dialog and show it to the world.
	gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), main_box);
	gtk_widget_show_all (dialog);
	return_value = gtk_dialog_run (GTK_DIALOG (dialog));

	// Compatibility & not supported option
	int mode_width = theApp.GetConfigI("ModeWidth");
	int mode_height = theApp.GetConfigI("ModeHeight");
	theApp.SetConfig("ModeHeight", mode_height);
	theApp.SetConfig("ModeWidth", mode_width);
	theApp.SetConfig("msaa", 0);
	theApp.SetConfig("windowed", 1);

	gtk_widget_destroy (dialog);

	return (return_value == GTK_RESPONSE_ACCEPT);
}
