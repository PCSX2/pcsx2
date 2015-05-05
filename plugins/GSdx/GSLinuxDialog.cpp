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

GtkWidget* CreateRenderComboBox()
{
	GtkWidget  *render_combo_box;
	int renderer_box_position = 0;

	render_combo_box = gtk_combo_box_text_new ();

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

	switch (theApp.GetConfig("renderer", 0)) {
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
	return render_combo_box;
}

GtkWidget* CreateInterlaceComboBox()
{
	GtkWidget  *combo_box;
	combo_box = gtk_combo_box_text_new ();

	for(size_t i = 0; i < theApp.m_gs_interlace.size(); i++)
	{
		const GSSetting& s = theApp.m_gs_interlace[i];

		string label = s.name;

		if(!s.note.empty()) label += format(" (%s)", s.note.c_str());

		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), label.c_str());
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), theApp.GetConfig("interlace", 0));
	return combo_box;
}

GtkWidget* CreateFsaaComboBox()
{
	GtkWidget  *combo_box;
	combo_box = gtk_combo_box_text_new ();

	// For now, let's just put in the same vaues that show up in the windows combo box.
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Custom");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "2x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "3x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "4x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "5x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "6x");

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), theApp.GetConfig("upscale_multiplier", 2)-1);
	return combo_box;
}

GtkWidget* CreateFilterComboBox()
{
	GtkWidget  *combo_box;
	combo_box = gtk_combo_box_text_new ();

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Always flat");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Always bilinear");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Normal");

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), theApp.GetConfig("filter", 0));

	gtk_widget_set_tooltip_text(combo_box, "Allow to control the texture interpolation.\nAlways flat is smoother\nAlways flat is more pixelated");

	return combo_box;
}

GtkWidget* CreateAfComboBox()
{
	GtkWidget  *combo_box;
	combo_box = gtk_combo_box_text_new ();

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "OFF");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "2x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "4x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "8x");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "16x");

	if (theApp.GetConfig("AnisotropicFiltering", 0)) {
		int p = round(log2(theApp.GetConfig("MaxAnisotropy", 0)));
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), p);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
	}

	return combo_box;
}

void set_hex_entry(GtkWidget *text_box, int hex_value) {
	gchar* data=(gchar *)g_malloc(sizeof(gchar)*40);
	sprintf(data,"%X", hex_value);
	gtk_entry_set_text(GTK_ENTRY(text_box),data);
}

int get_hex_entry(GtkWidget *text_box) {
	int hex_value = 0;
	const gchar *data = gtk_entry_get_text(GTK_ENTRY(text_box));

	sscanf(data,"%X",&hex_value);

	return hex_value;
}

GtkWidget* CreateGlComboBox(const char* option)
{
	GtkWidget* combo;
	combo = gtk_combo_box_text_new();

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Auto");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Force-Disabled");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Force-Enabled");

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), theApp.GetConfig(option, -1) + 1);
	return combo;
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
		"Cancel", GTK_RESPONSE_REJECT,
		NULL);

	// The main area for the whole dialog box.
	GtkWidget* main_box    = gtk_vbox_new(false, 5);
	GtkWidget* central_box = gtk_vbox_new(false, 5);
	GtkWidget* advance_box = gtk_vbox_new(false, 5);

	// Grab a logo, to make things look nice.
	GdkPixbuf* logo_pixmap = gdk_pixbuf_from_pixdata(&gsdx_ogl_logo, false, NULL);
	GtkWidget* logo_image  = gtk_image_new_from_pixbuf(logo_pixmap);
	gtk_box_pack_start(GTK_BOX(main_box), logo_image, true, true, 0);

	GtkWidget* main_table   = CreateTableInBox(main_box    , NULL                                   , 2  , 2);

	GtkWidget* res_table    = CreateTableInBox(central_box , "OpenGL Internal Resolution"           , 3  , 3);
	GtkWidget* shader_table = CreateTableInBox(central_box , "Custom Shader Settings"               , 8  , 2);
	GtkWidget* hw_table     = CreateTableInBox(central_box , "Hardware Mode Settings"               , 5  , 2);
	GtkWidget* sw_table     = CreateTableInBox(central_box , "Software Mode Settings"               , 5  , 2);

	GtkWidget* hack_table   = CreateTableInBox(advance_box , "Hacks"                                , 7  , 2);
	GtkWidget* gl_table     = CreateTableInBox(advance_box , "OpenGL Very Advanced Custom Settings" , 10 , 2);

	// Main
	GtkWidget* render_label     = gtk_label_new ("Renderer:");
	GtkWidget* render_combo_box = CreateRenderComboBox();
	GtkWidget* interlace_label     = gtk_label_new ("Interlacing (F5):");
	GtkWidget* interlace_combo_box = CreateInterlaceComboBox();

	s_table_line = 0;
	InsertWidgetInTable(main_table, render_label, render_combo_box);
	InsertWidgetInTable(main_table, interlace_label, interlace_combo_box);

	// HW
	GtkWidget* filter_label     = gtk_label_new ("Texture Filtering:");
	GtkWidget* filter_combo_box = CreateFilterComboBox();

	GtkWidget* af_label     = gtk_label_new("Anisotropic Filtering:");
	GtkWidget* af_combo_box = CreateAfComboBox();

	GtkWidget* paltex_check     = gtk_check_button_new_with_label("Allow 8 bits textures");
	GtkWidget* fba_check        = gtk_check_button_new_with_label("Alpha correction (FBA)");

	s_table_line = 0;
	InsertWidgetInTable(hw_table, filter_label, filter_combo_box);
	InsertWidgetInTable(hw_table, af_label, af_combo_box);
	InsertWidgetInTable(hw_table, paltex_check, fba_check);

	// SW
	GtkWidget* threads_label = gtk_label_new("Extra rendering threads:");
	GtkWidget* threads_spin  = gtk_spin_button_new_with_range(0,100,1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(threads_spin), theApp.GetConfig("extrathreads", 0));

	GtkWidget* aa_check         = gtk_check_button_new_with_label("Edge anti-aliasing (AA1)");
	GtkWidget* spin_thread_check= gtk_check_button_new_with_label("Disable thread sleeping (6+ cores CPU)");

	s_table_line = 0;
	InsertWidgetInTable(sw_table , threads_label     , threads_spin);
	InsertWidgetInTable(sw_table , aa_check);
	InsertWidgetInTable(sw_table , spin_thread_check , spin_thread_check);

	// Resolution
	GtkWidget* native_label     = gtk_label_new("Original PS2 Resolution: ");
	GtkWidget* native_res_check = gtk_check_button_new_with_label("Native");

	GtkWidget* fsaa_label     = gtk_label_new("Or Use Scaling:");
	GtkWidget* fsaa_combo_box = CreateFsaaComboBox();

	GtkWidget* resxy_label = gtk_label_new("Custom Resolution:");
	GtkWidget* resx_spin   = gtk_spin_button_new_with_range(256,8192,1);
	GtkWidget* resy_spin   = gtk_spin_button_new_with_range(256,8192,1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(resx_spin), theApp.GetConfig("resx", 1024));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(resy_spin), theApp.GetConfig("resy", 1024));

	s_table_line = 0;
	InsertWidgetInTable(res_table, native_label, native_res_check);
	InsertWidgetInTable(res_table, fsaa_label, fsaa_combo_box);
	InsertWidgetInTable(res_table, resxy_label, resx_spin, resy_spin);

	// Create our hack settings.
	GtkWidget* hack_alpha_check    = gtk_check_button_new_with_label("Alpha Hack");
	GtkWidget* hack_date_check     = gtk_check_button_new_with_label("Date Hack");
	GtkWidget* hack_offset_check   = gtk_check_button_new_with_label("Offset Hack");
	GtkWidget* hack_skipdraw_label = gtk_label_new("Skipdraw:");
	GtkWidget* hack_skipdraw_spin  = gtk_spin_button_new_with_range(0,1000,1);
	GtkWidget* hack_enble_check    = gtk_check_button_new_with_label("Enable User Hacks");
	GtkWidget* hack_wild_check     = gtk_check_button_new_with_label("Wild arm Hack");
	GtkWidget* hack_sprite_check   = gtk_check_button_new_with_label("Sprite Hack");
	GtkWidget* hack_tco_label      = gtk_label_new("Texture Offset: 0x");
	GtkWidget* hack_tco_entry      = gtk_entry_new();
	GtkWidget* hack_logz_check     = gtk_check_button_new_with_label("Log Depth Hack");
	GtkWidget* align_sprite_check  = gtk_check_button_new_with_label("Anti vertical line hack");
	GtkWidget* stretch_hack_check  = gtk_check_button_new_with_label("Improve 2D sprite scaling accuracy");

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(hack_skipdraw_spin), theApp.GetConfig("UserHacks_SkipDraw", 0));
	set_hex_entry(hack_tco_entry, theApp.GetConfig("UserHacks_TCOffset", 0));

	// Reuse windows helper string :)
	gtk_widget_set_tooltip_text(hack_alpha_check, dialog_message(IDC_ALPHAHACK));
	gtk_widget_set_tooltip_text(hack_date_check, "Disable opengl barrier for performance with DATE operation");
	gtk_widget_set_tooltip_text(hack_offset_check, dialog_message(IDC_TCOFFSETX));
	gtk_widget_set_tooltip_text(hack_skipdraw_label, dialog_message(IDC_SKIPDRAWHACK));
	gtk_widget_set_tooltip_text(hack_skipdraw_spin, dialog_message(IDC_SKIPDRAWHACK));
	gtk_widget_set_tooltip_text(hack_enble_check, "Allow to use hack below");
	gtk_widget_set_tooltip_text(hack_wild_check, dialog_message(IDC_WILDHACK));
	gtk_widget_set_tooltip_text(hack_sprite_check, dialog_message(IDC_SPRITEHACK));
	gtk_widget_set_tooltip_text(hack_tco_label, dialog_message(IDC_TCOFFSETX));
	gtk_widget_set_tooltip_text(hack_tco_entry, dialog_message(IDC_TCOFFSETX));
	gtk_widget_set_tooltip_text(hack_logz_check, "Use a logarithm depth instead of a linear depth (superseeded by ARB_clip_control)");
	gtk_widget_set_tooltip_text(align_sprite_check, dialog_message(IDC_ALIGN_SPRITE));
	gtk_widget_set_tooltip_text(stretch_hack_check, dialog_message(IDC_ROUND_SPRITE));


	// Tables are strange. The numbers are for their position: left, right, top, bottom.
	s_table_line = 0;
	InsertWidgetInTable(hack_table , hack_enble_check);
	InsertWidgetInTable(hack_table , hack_alpha_check    , hack_offset_check);
	InsertWidgetInTable(hack_table , hack_sprite_check   , hack_wild_check);
	InsertWidgetInTable(hack_table , hack_logz_check     , hack_date_check);
	InsertWidgetInTable(hack_table , stretch_hack_check  , align_sprite_check);
	InsertWidgetInTable(hack_table , hack_skipdraw_label , hack_skipdraw_spin);
	InsertWidgetInTable(hack_table , hack_tco_label      , hack_tco_entry);

	// shader
	GtkWidget* shader            = gtk_file_chooser_button_new("Select an external shader", GTK_FILE_CHOOSER_ACTION_OPEN);
	GtkWidget* shader_conf       = gtk_file_chooser_button_new("Then select a config", GTK_FILE_CHOOSER_ACTION_OPEN);
	GtkWidget* shader_label      = gtk_label_new("External shader glsl");
	GtkWidget* shader_conf_label = gtk_label_new("External shader conf");

	GtkWidget* shadeboost_check = gtk_check_button_new_with_label("Shade boost");
	GtkWidget* fxaa_check       = gtk_check_button_new_with_label("Fxaa shader");
	GtkWidget* shaderfx_check   = gtk_check_button_new_with_label("External shader");

	// Shadeboost scale
#if GTK_MAJOR_VERSION < 3
	GtkWidget* sb_brightness = gtk_hscale_new_with_range(0, 200, 10);
#else
	GtkWidget* sb_brightness = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 200, 10);
#endif
	GtkWidget* sb_brightness_label = gtk_label_new("Shade Boost Brightness");
	gtk_scale_set_value_pos(GTK_SCALE(sb_brightness), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(sb_brightness), theApp.GetConfig("ShadeBoost_Brightness", 50));

#if GTK_MAJOR_VERSION < 3
	GtkWidget* sb_contrast = gtk_hscale_new_with_range(0, 200, 10);
#else
	GtkWidget* sb_contrast = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 200, 10);
#endif
	GtkWidget* sb_contrast_label = gtk_label_new("Shade Boost Contrast");
	gtk_scale_set_value_pos(GTK_SCALE(sb_contrast), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(sb_contrast), theApp.GetConfig("ShadeBoost_Contrast", 50));

#if GTK_MAJOR_VERSION < 3
	GtkWidget* sb_saturation = gtk_hscale_new_with_range(0, 200, 10);
#else
	GtkWidget* sb_saturation = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 200, 10);
#endif
	GtkWidget* sb_saturation_label = gtk_label_new("Shade Boost Saturation");
	gtk_scale_set_value_pos(GTK_SCALE(sb_saturation), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(sb_saturation), theApp.GetConfig("ShadeBoost_Saturation", 50));

	// external shader entry
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(shader), theApp.GetConfig("shaderfx_glsl", "dummy.glsl").c_str());
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(shader_conf), theApp.GetConfig("shaderfx_conf", "dummy.ini").c_str());

	s_table_line = 0;
	InsertWidgetInTable(shader_table , fxaa_check);
	InsertWidgetInTable(shader_table , shadeboost_check);
	InsertWidgetInTable(shader_table , sb_brightness_label , sb_brightness);
	InsertWidgetInTable(shader_table , sb_contrast_label   , sb_contrast);
	InsertWidgetInTable(shader_table , sb_saturation_label , sb_saturation);
	InsertWidgetInTable(shader_table , shaderfx_check);
	InsertWidgetInTable(shader_table , shader_label        , shader);
	InsertWidgetInTable(shader_table , shader_conf_label   , shader_conf);

	// The GL advance options
	GtkWidget* gl_bs_label = gtk_label_new("Buffer Storage:");
	GtkWidget* gl_bs_combo = CreateGlComboBox("override_GL_ARB_buffer_storage");
	GtkWidget* gl_bt_label = gtk_label_new("Bindless Texture:");
	GtkWidget* gl_bt_combo = CreateGlComboBox("override_GL_ARB_bindless_texture");
	GtkWidget* gl_sso_label = gtk_label_new("Separate Shader:");
	GtkWidget* gl_sso_combo = CreateGlComboBox("override_GL_ARB_separate_shader_objects");
	GtkWidget* gl_ss_label = gtk_label_new("Shader Subroutine:");
	GtkWidget* gl_ss_combo = CreateGlComboBox("override_GL_ARB_shader_subroutine");
	GtkWidget* gl_gs_label = gtk_label_new("Geometry Shader:");
	GtkWidget* gl_gs_combo = CreateGlComboBox("override_geometry_shader");
	GtkWidget* gl_ils_label = gtk_label_new("Image Load Store:");
	GtkWidget* gl_ils_combo = CreateGlComboBox("override_GL_ARB_shader_image_load_store");
	GtkWidget* gl_cc_label = gtk_label_new("Clip Control (depth accuracy):");
	GtkWidget* gl_cc_combo = CreateGlComboBox("override_GL_ARB_clip_control");
	GtkWidget* gl_tb_label = gtk_label_new("Texture Barrier:");
	GtkWidget* gl_tb_combo = CreateGlComboBox("override_GL_ARB_texture_barrier");

	s_table_line = 0;
	InsertWidgetInTable(gl_table , gl_gs_label  , gl_gs_combo);
	InsertWidgetInTable(gl_table , gl_bs_label  , gl_bs_combo);
	InsertWidgetInTable(gl_table , gl_bt_label  , gl_bt_combo);
	InsertWidgetInTable(gl_table , gl_sso_label , gl_sso_combo);
	InsertWidgetInTable(gl_table , gl_ss_label  , gl_ss_combo);
	InsertWidgetInTable(gl_table , gl_ils_label , gl_ils_combo);
	InsertWidgetInTable(gl_table , gl_cc_label  , gl_cc_combo);
	InsertWidgetInTable(gl_table , gl_tb_label  , gl_tb_combo);

	// Handle some nice tab

	GtkWidget* notebook = gtk_notebook_new();
	GtkWidget* page_label[2];
	page_label[0] = gtk_label_new("Global Setting");
	page_label[1] = gtk_label_new("Advance Setting");

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), central_box, page_label[0]);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advance_box, page_label[1]);

	// Put everything in the big box.
	gtk_container_add(GTK_CONTAINER(main_box), notebook);

	{ // Set current value of checkboxes.
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shadeboost_check)   , theApp.GetConfig("shadeboost"                    , 1));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(paltex_check)       , theApp.GetConfig("paltex"                        , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fba_check)          , theApp.GetConfig("fba"                           , 1));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(aa_check)           , theApp.GetConfig("aa1"                           , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spin_thread_check)  , theApp.GetConfig("spin_thread"                   , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fxaa_check)         , theApp.GetConfig("fxaa"                          , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shaderfx_check)     , theApp.GetConfig("shaderfx"                      , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(native_res_check)   , theApp.GetConfig("nativeres"                     , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stretch_hack_check) , theApp.GetConfig("UserHacks_round_sprite_offset" , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(align_sprite_check) , theApp.GetConfig("UserHacks_align_sprite_X"      , 0));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_alpha_check)   , theApp.GetConfig("UserHacks_AlphaHack"           , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_offset_check)  , theApp.GetConfig("UserHacks_HalfPixelOffset"     , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_date_check)    , theApp.GetConfig("UserHacks_DateGL4"             , 0));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_enble_check)   , theApp.GetConfig("UserHacks"                     , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_wild_check)    , theApp.GetConfig("UserHacks_WildHack"            , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_sprite_check)  , theApp.GetConfig("UserHacks_SpriteHack"          , 0));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hack_logz_check)    , theApp.GetConfig("logz"                          , 1));
	}

	// Put the box in the dialog and show it to the world.
	gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), main_box);
	gtk_widget_show_all (dialog);
	return_value = gtk_dialog_run (GTK_DIALOG (dialog));

	if (return_value == GTK_RESPONSE_ACCEPT) {
		int mode_height = 0, mode_width = 0;

		mode_width = theApp.GetConfig("ModeWidth", 640);
		mode_height = theApp.GetConfig("ModeHeight", 480);
		theApp.SetConfig("ModeHeight", mode_height);
		theApp.SetConfig("ModeWidth", mode_width);

		// Get all the settings from the dialog box.
		if (gtk_combo_box_get_active(GTK_COMBO_BOX(render_combo_box)) != -1) {
			// Note the value are based on m_gs_renderers vector on GSdx.cpp
			switch (gtk_combo_box_get_active(GTK_COMBO_BOX(render_combo_box))) {
				case 0: theApp.SetConfig("renderer", 10); break;
				case 1: theApp.SetConfig("renderer", 16); break;
				case 2: theApp.SetConfig("renderer", 11); break;
				case 3: theApp.SetConfig("renderer", 12); break;
				case 4: theApp.SetConfig("renderer", 13); break;
				case 5: theApp.SetConfig("renderer", 17); break;

						// Fallback to SW opengl
				default: theApp.SetConfig("renderer", 13); break;
			}
		}

		if (gtk_combo_box_get_active(GTK_COMBO_BOX(interlace_combo_box)) != -1)
			theApp.SetConfig( "interlace", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(interlace_combo_box)));

		if (gtk_combo_box_get_active(GTK_COMBO_BOX(af_combo_box)) != -1) {
			int af = gtk_combo_box_get_active(GTK_COMBO_BOX(af_combo_box));
			theApp.SetConfig("AnisotropicFiltering", (af) ? 1 : 0);
			theApp.SetConfig("MaxAnisotropy", round(exp2(af)));
		}

		theApp.SetConfig("extrathreads", (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(threads_spin)));

		theApp.SetConfig("filter", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(filter_combo_box)));
		theApp.SetConfig("shadeboost", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shadeboost_check)));
		theApp.SetConfig("paltex", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(paltex_check)));
		theApp.SetConfig("fba", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fba_check)));
		theApp.SetConfig("aa1", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(aa_check)));
		theApp.SetConfig("spin_thread", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spin_thread_check)));
		theApp.SetConfig("fxaa", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fxaa_check)));
		theApp.SetConfig("shaderfx", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shaderfx_check)));
		theApp.SetConfig("nativeres", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(native_res_check)));
		theApp.SetConfig("UserHacks_round_sprite_offset", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stretch_hack_check)));
		theApp.SetConfig("UserHacks_align_sprite_X", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(align_sprite_check)));

		theApp.SetConfig("shaderfx_glsl", gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(shader)));
		theApp.SetConfig("shaderfx_conf", gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(shader_conf)));

		theApp.SetConfig("ShadeBoost_Saturation", (int)gtk_range_get_value(GTK_RANGE(sb_saturation)));
		theApp.SetConfig("ShadeBoost_Brightness", (int)gtk_range_get_value(GTK_RANGE(sb_brightness)));
		theApp.SetConfig("ShadeBoost_Contrast", (int)gtk_range_get_value(GTK_RANGE(sb_contrast)));

		theApp.SetConfig("upscale_multiplier", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(fsaa_combo_box))+1);
		theApp.SetConfig("resx", (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(resx_spin)));
		theApp.SetConfig("resy", (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(resy_spin)));

		theApp.SetConfig("UserHacks_SkipDraw", (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(hack_skipdraw_spin)));
		theApp.SetConfig("UserHacks_HalfPixelOffset", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_offset_check)));
		theApp.SetConfig("UserHacks_AlphaHack", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_alpha_check)));
		theApp.SetConfig("logz", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_logz_check)));
		theApp.SetConfig("UserHacks_DateGL4", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_date_check)));

		theApp.SetConfig("UserHacks_WildHack", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_wild_check)));
		theApp.SetConfig("UserHacks_SpriteHack", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_sprite_check)));
		theApp.SetConfig("UserHacks", (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hack_enble_check)));
		theApp.SetConfig("UserHacks_TCOffset", get_hex_entry(hack_tco_entry));

		theApp.SetConfig("override_GL_ARB_texture_barrier", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_tb_combo)) - 1);
		theApp.SetConfig("override_GL_ARB_bindless_texture", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_bt_combo)) - 1);
		theApp.SetConfig("override_GL_ARB_buffer_storage", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_bs_combo)) - 1);
		theApp.SetConfig("override_GL_ARB_separate_shader_objects", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_sso_combo)) - 1);
		theApp.SetConfig("override_GL_ARB_shader_subroutine", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_ss_combo)) - 1);
		theApp.SetConfig("override_geometry_shader", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_gs_combo)) - 1);
		theApp.SetConfig("override_GL_ARB_shader_image_load_store", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_ils_combo)) - 1);
		theApp.SetConfig("override_GL_ARB_clip_control", (int)gtk_combo_box_get_active(GTK_COMBO_BOX(gl_cc_combo)) - 1);

		// NOT supported yet
		theApp.SetConfig("msaa", 0);

		// Let's just be windowed for the moment.
		theApp.SetConfig("windowed", 1);

		gtk_widget_destroy (dialog);

		return true;
	}

	gtk_widget_destroy (dialog);

	return false;
}
