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
#include "GSdx.h"
#include "GSLinuxLogo.h"
#include "GSSetting.h"

void CB_ChangedRenderComboBox(GtkComboBox *combo, gpointer user_data)
{
	if (gtk_combo_box_get_active(combo) == -1) return;

	// Note the value are based on m_gs_renderers vector on GSdx.cpp
	switch (gtk_combo_box_get_active(combo)) {
		case 0: theApp.SetConfig("Renderer", 10); break;
		case 1: theApp.SetConfig("Renderer", 16); break;
		case 2: theApp.SetConfig("Renderer", 11); break;
		case 3: theApp.SetConfig("Renderer", 12); break;
		case 4: theApp.SetConfig("Renderer", 13); break;
		case 5: theApp.SetConfig("Renderer", 17); break;

				// Fallback to SW opengl
		default: theApp.SetConfig("Renderer", 13); break;
	}
}

GtkWidget* CreateRenderComboBox()
{
	GtkWidget* render_combo_box = gtk_combo_box_text_new ();
	int renderer_box_position = 0;

	for(auto s = theApp.m_gs_renderers.begin(); s != theApp.m_gs_renderers.end(); s++)
	{
		string label = s->name;

		if(!s->note.empty()) label += format(" (%s)", s->note.c_str());

		// Add some tags to ease users selection
		switch (s->id) {
			// Supported opengl
			case 12:
			case 13:
			case 17:
				break;

			// (dev only) for any NULL stuff
			case 10:
			case 11:
			case 16:
				label += " (debug only)";
				break;

			default:
				continue;
		}

		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(render_combo_box), label.c_str());
	}

	switch (theApp.GetConfig("Renderer", 0)) {
		// Note the value are based on m_gs_renderers vector on GSdx.cpp
		case 10: renderer_box_position = 0; break;
		case 16: renderer_box_position = 1; break;
		case 11: renderer_box_position = 2; break;
		case 12: renderer_box_position = 3; break;
		case 13: renderer_box_position = 4; break;
		case 17: renderer_box_position = 5; break;

		// Fallback to openGL SW
		default: renderer_box_position = 4; break;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(render_combo_box), renderer_box_position);

	g_signal_connect(render_combo_box, "changed", G_CALLBACK(CB_ChangedRenderComboBox), NULL);

	return render_combo_box;
}

void CB_ChangedComboBox(GtkComboBox *combo, gpointer user_data)
{
	int p = gtk_combo_box_get_active(combo);
	vector<GSSetting>* s = (vector<GSSetting>*)g_object_get_data(G_OBJECT(combo), "Settings");

	try {
		theApp.SetConfig((char*)user_data, s->at(p).id);
	} catch (...) {
	}
}

GtkWidget* CreateComboBoxFromVector(const vector<GSSetting>& s, const char* opt_name, int opt_default = 0)
{
	GtkWidget* combo_box = gtk_combo_box_text_new();
	int opt_value        = theApp.GetConfig(opt_name, opt_default);
	int opt_position     = 0;

	for(size_t i = 0; i < s.size(); i++)
	{
		string label = s[i].name;

		if(!s[i].note.empty()) label += format(" (%s)", s[i].note.c_str());

		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), label.c_str());

		if ((int)s[i].id == opt_value)
			opt_position = i;
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), opt_position);

	g_signal_connect(combo_box, "changed", G_CALLBACK(CB_ChangedComboBox), const_cast<char*>(opt_name));
	g_object_set_data(G_OBJECT(combo_box), "Settings", (void*)&s);

	return combo_box;
}

void CB_PreEntryActived(GtkEntry *entry, gchar* preedit, gpointer user_data)
{
	int hex_value = 0;
	sscanf(preedit,"%X",&hex_value);

	theApp.SetConfig((char*)user_data, hex_value);
}

void CB_EntryActived(GtkEntry *entry, gpointer user_data)
{
	int hex_value = 0;
	const gchar *data = gtk_entry_get_text(entry);
	sscanf(data,"%X",&hex_value);

	theApp.SetConfig((char*)user_data, hex_value);
}

GtkWidget* CreateTextBox(const char* opt_name, int opt_default = 0) {
	GtkWidget* entry = gtk_entry_new();

	int hex_value = theApp.GetConfig(opt_name, opt_default);

	gchar* data=(gchar *)g_malloc(sizeof(gchar)*40);
	sprintf(data,"%X", hex_value);
	gtk_entry_set_text(GTK_ENTRY(entry),data);
	g_free(data);

	g_signal_connect(entry, "activate", G_CALLBACK(CB_EntryActived), const_cast<char*>(opt_name));
	// Note it doesn't seem to work as expected
	g_signal_connect(entry, "preedit-changed", G_CALLBACK(CB_PreEntryActived), const_cast<char*>(opt_name));

	return entry;
}

void CB_ToggleCheckBox(GtkToggleButton *togglebutton, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, (int)gtk_toggle_button_get_active(togglebutton));
}

GtkWidget* CreateCheckBox(const char* label, const char* opt_name, bool opt_default = false)
{
	GtkWidget* check = gtk_check_button_new_with_label(label);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), theApp.GetConfig(opt_name, opt_default));

	g_signal_connect(check, "toggled", G_CALLBACK(CB_ToggleCheckBox), const_cast<char*>(opt_name));

	return check;
}

void CB_SpinButton(GtkSpinButton *spin, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, (int)gtk_spin_button_get_value(spin));
}

GtkWidget* CreateSpinButton(double min, double max, const char* opt_name, int opt_default = 0)
{
	GtkWidget* spin = gtk_spin_button_new_with_range(min, max, 1);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), theApp.GetConfig(opt_name, opt_default));

	g_signal_connect(spin, "value-changed", G_CALLBACK(CB_SpinButton), const_cast<char*>(opt_name));

	return spin;
}

void CB_RangeChanged(GtkRange* range, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, (int)gtk_range_get_value(range));
}

GtkWidget* CreateScale(const char* opt_name, int opt_default = 0)
{
#if GTK_MAJOR_VERSION < 3
	GtkWidget* scale = gtk_hscale_new_with_range(0, 200, 10);
#else
	GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 200, 10);
#endif

	gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(scale), theApp.GetConfig(opt_name, opt_default));

	g_signal_connect(scale, "value-changed", G_CALLBACK(CB_RangeChanged), const_cast<char*>(opt_name));

	return scale;
}

void CB_PickFile(GtkFileChooserButton *chooser, gpointer user_data)
{
	theApp.SetConfig((char*)user_data, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser)));
}

GtkWidget* CreateFileChooser(GtkFileChooserAction action, const char* label, const char* opt_name, const char* opt_default)
{
	GtkWidget* chooser = gtk_file_chooser_button_new(label, action);

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), theApp.GetConfig(opt_name, opt_default).c_str());

	g_signal_connect(chooser, "file-set", G_CALLBACK(CB_PickFile), const_cast<char*>(opt_name));

	return chooser;
}

static int s_table_line = 0;
static void InsertWidgetInTable(GtkWidget* table, GtkWidget *left, GtkWidget *right = NULL, GtkWidget *third = NULL) {
	if (!left) {
		gtk_table_attach_defaults(GTK_TABLE(table), right, 1, 2, s_table_line, s_table_line+1);
	} else if (!right) {
		gtk_table_attach_defaults(GTK_TABLE(table), left, 0, 1, s_table_line, s_table_line+1);
	} else if (right == left) {
		gtk_table_attach_defaults(GTK_TABLE(table), left, 0, 2, s_table_line, s_table_line+1);
	} else {
		gtk_table_attach_defaults(GTK_TABLE(table), left, 0, 1, s_table_line, s_table_line+1);
		gtk_table_attach_defaults(GTK_TABLE(table), right, 1, 2, s_table_line, s_table_line+1);
	}
	if (third) {
		gtk_table_attach_defaults(GTK_TABLE(table), third, 2, 3, s_table_line, s_table_line+1);
	}
	s_table_line++;
}

GtkWidget* CreateTableInBox(GtkWidget* parent_box, const char* frame_title, int row, int col) {
	GtkWidget* table = gtk_table_new(row, col, false);
	GtkWidget* container = (frame_title) ? gtk_frame_new (frame_title) : gtk_vbox_new(false, 5);
	gtk_container_add(GTK_CONTAINER(container), table);
	gtk_container_add(GTK_CONTAINER(parent_box), container);

	return table;
}

void populate_res_table(GtkWidget* res_table)
{
	GtkWidget* native_label     = gtk_label_new("Original PS2 Resolution: ");
	GtkWidget* native_res_check = CreateCheckBox("Native", "nativeres");

	GtkWidget* fsaa_label     = gtk_label_new("Or Use Scaling:");
	GtkWidget* fsaa_combo_box = CreateComboBoxFromVector(theApp.m_gs_upscale_multiplier, "upscale_multiplier", 2);

	GtkWidget* resxy_label = gtk_label_new("Custom Resolution:");
	GtkWidget* resx_spin   = CreateSpinButton(256, 8192, "resx", 1024);
	GtkWidget* resy_spin   = CreateSpinButton(256, 8192, "resy", 1024);

	s_table_line = 0;
	InsertWidgetInTable(res_table, native_label, native_res_check);
	InsertWidgetInTable(res_table, fsaa_label, fsaa_combo_box);
	InsertWidgetInTable(res_table, resxy_label, resx_spin, resy_spin);
}

void populate_hw_table(GtkWidget* hw_table)
{
	GtkWidget* filter_label     = gtk_label_new ("Texture Filtering:");
	GtkWidget* filter_combo_box = CreateComboBoxFromVector(theApp.m_gs_filter, "filter", 2);

	GtkWidget* af_label     = gtk_label_new("Anisotropic Filtering:");
	GtkWidget* af_combo_box = CreateComboBoxFromVector(theApp.m_gs_max_anisotropy, "MaxAnisotropy", 1);

	GtkWidget* crc_label     = gtk_label_new("Automatic CRC level:");
	GtkWidget* crc_combo_box = CreateComboBoxFromVector(theApp.m_gs_crc_level, "crc_hack_level", 3);

	GtkWidget* paltex_check     = CreateCheckBox("Allow 8 bits textures", "paltex");
	GtkWidget* acc_blend_check  = CreateCheckBox("Accurate Blend", "accurate_blend", true);
	GtkWidget* acc_date_check   = CreateCheckBox("Accurate Date", "accurate_date", false);
	GtkWidget* acc_cclip_check  = CreateCheckBox("Accurate Color Clipping", "accurate_colclip", false);
	GtkWidget* acc_fbmsk_check  = CreateCheckBox("Accurate FrameBuffer Mask", "accurate_fbmask", false);

	GtkWidget* MT_nvidia_check   = CreateCheckBox("Nvidia Multi-Thread support", "enable_nvidia_multi_thread", true);

	// Some helper string
	gtk_widget_set_tooltip_text(paltex_check, dialog_message(IDC_PALTEX));
	gtk_widget_set_tooltip_text(acc_blend_check, dialog_message(IDC_ACCURATE_BLEND));
	gtk_widget_set_tooltip_text(acc_date_check, dialog_message(IDC_ACCURATE_DATE));
	gtk_widget_set_tooltip_text(acc_cclip_check, dialog_message(IDC_ACCURATE_COLCLIP));
	gtk_widget_set_tooltip_text(acc_fbmsk_check, dialog_message(IDC_ACCURATE_FBMASK));
	gtk_widget_set_tooltip_text(MT_nvidia_check, "Huge speedup on Nvidia binary driver! No effect otherwise.");
	gtk_widget_set_tooltip_text(crc_label, dialog_message(IDC_CRC_LEVEL));
	gtk_widget_set_tooltip_text(crc_combo_box, dialog_message(IDC_CRC_LEVEL));

	s_table_line = 0;
	InsertWidgetInTable(hw_table, paltex_check, MT_nvidia_check);
	InsertWidgetInTable(hw_table, acc_blend_check, acc_date_check);
	InsertWidgetInTable(hw_table, acc_cclip_check, acc_fbmsk_check);
	InsertWidgetInTable(hw_table, filter_label, filter_combo_box);
	InsertWidgetInTable(hw_table, af_label, af_combo_box);
	InsertWidgetInTable(hw_table, crc_label, crc_combo_box);
}

void populate_gl_table(GtkWidget* gl_table)
{
	GtkWidget* gl_bs_label = gtk_label_new("Buffer Storage:");
	GtkWidget* gl_bs_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_buffer_storage", -1);
	GtkWidget* gl_bt_label = gtk_label_new("Bindless Texture:");
	GtkWidget* gl_bt_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_bindless_texture", -1);
	GtkWidget* gl_sso_label = gtk_label_new("Separate Shader:");
	GtkWidget* gl_sso_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_separate_shader_objects", -1);
	GtkWidget* gl_ss_label = gtk_label_new("Shader Subroutine:");
	GtkWidget* gl_ss_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_shader_subroutine", -1);
	GtkWidget* gl_gs_label = gtk_label_new("Geometry Shader:");
	GtkWidget* gl_gs_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_geometry_shader", -1);
	GtkWidget* gl_ils_label = gtk_label_new("Image Load Store:");
	GtkWidget* gl_ils_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_shader_image_load_store", -1);
	GtkWidget* gl_cc_label = gtk_label_new("Clip Control (depth accuracy):");
	GtkWidget* gl_cc_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_clip_control", -1);
	GtkWidget* gl_tb_label = gtk_label_new("Texture Barrier:");
	GtkWidget* gl_tb_combo = CreateComboBoxFromVector(theApp.m_gs_gl_ext, "override_GL_ARB_texture_barrier", -1);

	s_table_line = 0;
	InsertWidgetInTable(gl_table , gl_gs_label  , gl_gs_combo);
	InsertWidgetInTable(gl_table , gl_bs_label  , gl_bs_combo);
	InsertWidgetInTable(gl_table , gl_bt_label  , gl_bt_combo);
	InsertWidgetInTable(gl_table , gl_sso_label , gl_sso_combo);
	InsertWidgetInTable(gl_table , gl_ss_label  , gl_ss_combo);
	InsertWidgetInTable(gl_table , gl_ils_label , gl_ils_combo);
	InsertWidgetInTable(gl_table , gl_cc_label  , gl_cc_combo);
	InsertWidgetInTable(gl_table , gl_tb_label  , gl_tb_combo);
}

void populate_sw_table(GtkWidget* sw_table)
{
	GtkWidget* threads_label = gtk_label_new("Extra rendering threads:");
	GtkWidget* threads_spin  = CreateSpinButton(0, 32, "extrathreads", 0);

	GtkWidget* aa_check         = CreateCheckBox("Edge anti-aliasing (AA1)", "aa1");
	GtkWidget* spin_thread_check= CreateCheckBox("Disable thread sleeping (6+ cores CPU)", "spin_thread");

	s_table_line = 0;
	InsertWidgetInTable(sw_table , threads_label     , threads_spin);
	InsertWidgetInTable(sw_table , aa_check);
	InsertWidgetInTable(sw_table , spin_thread_check , spin_thread_check);
}

void populate_shader_table(GtkWidget* shader_table)
{
	GtkWidget* shader            = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_OPEN, "Select an external shader", "shaderfx_glsl", "dummy.glsl");
	GtkWidget* shader_conf       = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_OPEN, "Then select a config", "shaderfx_conf", "dummy.ini");
	GtkWidget* shader_label      = gtk_label_new("External shader glsl");
	GtkWidget* shader_conf_label = gtk_label_new("External shader conf");

	GtkWidget* shadeboost_check = CreateCheckBox("Shade boost", "shadeboost");
	GtkWidget* fxaa_check       = CreateCheckBox("Fxaa shader", "fxaa");
	GtkWidget* shaderfx_check   = CreateCheckBox("External shader", "shaderfx");

	// Shadeboost scale
	GtkWidget* sb_brightness       = CreateScale("ShadeBoost_Brightness", 50);
	GtkWidget* sb_brightness_label = gtk_label_new("Shade Boost Brightness");

	GtkWidget* sb_contrast         = CreateScale("ShadeBoost_Contrast", 50);
	GtkWidget* sb_contrast_label   = gtk_label_new("Shade Boost Contrast");

	GtkWidget* sb_saturation       = CreateScale("ShadeBoost_Saturation", 50);
	GtkWidget* sb_saturation_label = gtk_label_new("Shade Boost Saturation");

	s_table_line = 0;
	InsertWidgetInTable(shader_table , fxaa_check);
	InsertWidgetInTable(shader_table , shadeboost_check);
	InsertWidgetInTable(shader_table , sb_brightness_label , sb_brightness);
	InsertWidgetInTable(shader_table , sb_contrast_label   , sb_contrast);
	InsertWidgetInTable(shader_table , sb_saturation_label , sb_saturation);
	InsertWidgetInTable(shader_table , shaderfx_check);
	InsertWidgetInTable(shader_table , shader_label        , shader);
	InsertWidgetInTable(shader_table , shader_conf_label   , shader_conf);
}

void populate_hack_table(GtkWidget* hack_table)
{
	GtkWidget* hack_alpha_check    = CreateCheckBox("Alpha Hack", "UserHacks_AlphaHack");
	GtkWidget* hack_offset_check   = CreateCheckBox("Offset Hack", "UserHacks_HalfPixelOffset");
	GtkWidget* hack_skipdraw_label = gtk_label_new("Skipdraw:");
	GtkWidget* hack_skipdraw_spin  = CreateSpinButton(0, 1000, "UserHacks_SkipDraw");
	GtkWidget* hack_enble_check    = CreateCheckBox("Enable User Hacks", "UserHacks");
	GtkWidget* hack_wild_check     = CreateCheckBox("Wild arm Hack", "UserHacks_WildHack");
	GtkWidget* hack_tco_label      = gtk_label_new("Texture Offset: 0x");
	GtkWidget* hack_tco_entry      = CreateTextBox("UserHacks_TCOffset");
	GtkWidget* hack_logz_check     = CreateCheckBox("Log Depth Hack", "logz", true);
	GtkWidget* align_sprite_check  = CreateCheckBox("Align sprite hack", "UserHacks_align_sprite_X");

	GtkWidget* hack_sprite_box     = CreateComboBoxFromVector(theApp.m_gs_hack, "UserHacks_SpriteHack");
	GtkWidget* hack_sprite_label   = gtk_label_new("Alpha-Sprite Hack:");
	GtkWidget* stretch_hack_box    = CreateComboBoxFromVector(theApp.m_gs_hack, "UserHacks_round_sprite_offset");
	GtkWidget* stretch_hack_label  = gtk_label_new("Align Sprite Texture:");

	// Reuse windows helper string :)
	gtk_widget_set_tooltip_text(hack_alpha_check, dialog_message(IDC_ALPHAHACK));
	gtk_widget_set_tooltip_text(hack_offset_check, dialog_message(IDC_TCOFFSETX));
	gtk_widget_set_tooltip_text(hack_skipdraw_label, dialog_message(IDC_SKIPDRAWHACK));
	gtk_widget_set_tooltip_text(hack_skipdraw_spin, dialog_message(IDC_SKIPDRAWHACK));
	gtk_widget_set_tooltip_text(hack_enble_check, "Allow to use hack below");
	gtk_widget_set_tooltip_text(hack_wild_check, dialog_message(IDC_WILDHACK));
	gtk_widget_set_tooltip_text(hack_sprite_box, dialog_message(IDC_SPRITEHACK));
	gtk_widget_set_tooltip_text(hack_sprite_label, dialog_message(IDC_SPRITEHACK));
	gtk_widget_set_tooltip_text(hack_tco_label, dialog_message(IDC_TCOFFSETX));
	gtk_widget_set_tooltip_text(hack_tco_entry, dialog_message(IDC_TCOFFSETX));
	gtk_widget_set_tooltip_text(hack_logz_check, "Use a logarithmic depth instead of a linear depth (superseded by ARB_clip_control)");
	gtk_widget_set_tooltip_text(align_sprite_check, dialog_message(IDC_ALIGN_SPRITE));
	gtk_widget_set_tooltip_text(stretch_hack_box, dialog_message(IDC_ROUND_SPRITE));
	gtk_widget_set_tooltip_text(stretch_hack_label, dialog_message(IDC_ROUND_SPRITE));


	s_table_line = 0;
	InsertWidgetInTable(hack_table , hack_enble_check);
	InsertWidgetInTable(hack_table , hack_alpha_check    , hack_offset_check);
	InsertWidgetInTable(hack_table , hack_logz_check);
	InsertWidgetInTable(hack_table , hack_wild_check     , align_sprite_check);
	InsertWidgetInTable(hack_table , hack_sprite_label   , hack_sprite_box );
	InsertWidgetInTable(hack_table , stretch_hack_label  , stretch_hack_box );
	InsertWidgetInTable(hack_table , hack_skipdraw_label , hack_skipdraw_spin);
	InsertWidgetInTable(hack_table , hack_tco_label      , hack_tco_entry);
}

void populate_main_table(GtkWidget* main_table)
{
	GtkWidget* render_label     = gtk_label_new ("Renderer:");
	GtkWidget* render_combo_box = CreateRenderComboBox();
	GtkWidget* interlace_label     = gtk_label_new ("Interlacing (F5):");
	GtkWidget* interlace_combo_box = CreateComboBoxFromVector(theApp.m_gs_interlace, "interlace", 7);

	s_table_line = 0;
	InsertWidgetInTable(main_table, render_label, render_combo_box);
	InsertWidgetInTable(main_table, interlace_label, interlace_combo_box);
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

	GtkWidget* gs_saven_label   = gtk_label_new("Start of Dump");
	GtkWidget* gs_saven_spin    = CreateSpinButton(0, pow(10, 9), "saven");
	GtkWidget* gs_savel_label   = gtk_label_new("Length of Dump");
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
	GtkWidget* resxy_label   = gtk_label_new("Resolution:");
	GtkWidget* resx_spin     = CreateSpinButton(256, 8192, "capture_resx", 1280);
	GtkWidget* resy_spin     = CreateSpinButton(256, 8192, "capture_resy", 1024);
	GtkWidget* threads_label = gtk_label_new("Saving Threads:");
	GtkWidget* threads_spin  = CreateSpinButton(1, 32, "capture_threads", 4);
	GtkWidget* out_dir_label = gtk_label_new("Output Directory:");
	GtkWidget* out_dir       = CreateFileChooser(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Select a directory", "capture_out_dir", "/tmp");

	InsertWidgetInTable(record_table , capture_check);
	InsertWidgetInTable(record_table , resxy_label   , resx_spin      , resy_spin);
	InsertWidgetInTable(record_table , threads_label , threads_spin);
	InsertWidgetInTable(record_table , out_dir_label , out_dir);
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
	GtkWidget* main_box    = gtk_vbox_new(false, 5);
	GtkWidget* central_box = gtk_vbox_new(false, 5);
	GtkWidget* advance_box = gtk_vbox_new(false, 5);
	GtkWidget* debug_box   = gtk_vbox_new(false, 5);

	// Grab a logo, to make things look nice.
	GdkPixbuf* logo_pixmap = gdk_pixbuf_from_pixdata(&gsdx_ogl_logo, false, NULL);
	GtkWidget* logo_image  = gtk_image_new_from_pixbuf(logo_pixmap);
	gtk_box_pack_start(GTK_BOX(main_box), logo_image, true, true, 0);

	GtkWidget* main_table   = CreateTableInBox(main_box    , NULL                                   , 2  , 2);

	GtkWidget* res_table    = CreateTableInBox(central_box , "OpenGL Internal Resolution"           , 3  , 3);
	GtkWidget* shader_table = CreateTableInBox(central_box , "Custom Shader Settings"               , 8  , 2);
	GtkWidget* hw_table     = CreateTableInBox(central_box , "Hardware Mode Settings"               , 5  , 2);
	GtkWidget* sw_table     = CreateTableInBox(central_box , "Software Mode Settings"               , 3  , 2);

	GtkWidget* hack_table   = CreateTableInBox(advance_box , "Hacks"                                , 9 , 2);
	GtkWidget* gl_table     = CreateTableInBox(advance_box , "OpenGL Very Advanced Custom Settings" , 8 , 2);

	GtkWidget* record_table = CreateTableInBox(debug_box   , "Recording Settings"                   , 3  , 3);
	GtkWidget* debug_table  = CreateTableInBox(debug_box   , "OpenGL / GSdx Debug Settings"         , 5  , 3);

	// Populate all the tables
	populate_main_table(main_table);

	populate_res_table(res_table);
	populate_shader_table(shader_table);
	populate_hw_table(hw_table);
	populate_sw_table(sw_table);

	populate_hack_table(hack_table);
	populate_gl_table(gl_table);

	populate_debug_table(debug_table);
	populate_record_table(record_table);

	// Handle some nice tab
	GtkWidget* notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), central_box, gtk_label_new("Global Setting"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advance_box, gtk_label_new("Advance Setting"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), debug_box  , gtk_label_new("Debug/Recording Setting"));

	// Put everything in the big box.
	gtk_container_add(GTK_CONTAINER(main_box), notebook);

	// Put the box in the dialog and show it to the world.
	gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), main_box);
	gtk_widget_show_all (dialog);
	return_value = gtk_dialog_run (GTK_DIALOG (dialog));

	// Compatibility & not supported option
	int mode_width = theApp.GetConfig("ModeWidth", 640);
	int mode_height = theApp.GetConfig("ModeHeight", 480);
	theApp.SetConfig("ModeHeight", mode_height);
	theApp.SetConfig("ModeWidth", mode_width);
	theApp.SetConfig("msaa", 0);
	theApp.SetConfig("windowed", 1);
	// Anisotropic is disabled when it is 1x, no need of an extra check box
	theApp.SetConfig("AnisotropicFiltering", 1);

	gtk_widget_destroy (dialog);

	return (return_value == GTK_RESPONSE_ACCEPT);
}
