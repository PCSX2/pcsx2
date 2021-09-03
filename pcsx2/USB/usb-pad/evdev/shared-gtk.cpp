/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "shared.h"
#include "USB/icon_buzz_24.h"
#include "common/Console.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>

namespace usb_pad
{
	namespace evdev
	{

		using sys_clock = std::chrono::system_clock;
		using ms = std::chrono::milliseconds;

		constexpr auto CONTROL = "control";
		constexpr auto CFG = "cfg";

		// Buttons from 0, axes after buttons
		bool LoadMappings(const char* dev_type, int port, const std::string& joyname, u32 max_buttons, u32 max_axes, ConfigMapping& cfg)
		{
			std::stringstream str;
			const bool use_control_names = !strcmp(dev_type, PadDevice::TypeName());

			if (joyname.empty())
				return false;

			cfg.controls.resize(max_buttons + max_axes);
			for (u32 i = 0; i < max_buttons + max_axes; i++)
			{
				str.clear();
				str.str("");
				if (i < max_buttons)
				{
					str << "button_";
					if (use_control_names && i < (u32)countof(JoystickMapNames))
						str << JoystickMapNames[i];
					else
						str << i;
				}
				else
				{
					str << "axis_";
					u32 axis = i - max_buttons;
					if (use_control_names && (JOY_STEERING + axis) < (u32)countof(JoystickMapNames))
						str << JoystickMapNames[JOY_STEERING + axis];
					else
						str << axis;
				}

				const std::string& name = str.str();
				int32_t var;
				if (LoadSetting(dev_type, port, joyname, name.c_str(), var))
					cfg.controls[i] = var;
				else
					cfg.controls[i] = -1;
			}

			for (int i = 0; i < 3; i++)
			{
				str.clear();
				str.str("");
				str << "inverted_";
				if (use_control_names)
					str << JoystickMapNames[JOY_STEERING + i];
				else
					str << i;

				{
					const std::string& name = str.str();
					if (!LoadSetting(dev_type, port, joyname, name.c_str(), cfg.inverted[i]))
						cfg.inverted[i] = 0;
				}

				str.clear();
				str.str("");
				str << "initial_";
				if (use_control_names)
					str << JoystickMapNames[JOY_STEERING + i];
				else
					str << i;

				{
					const std::string& name = str.str();
					if (!LoadSetting(dev_type, port, joyname, name.c_str(), cfg.initial[i]))
						cfg.initial[i] = 0;
				}
			}
			return true;
		}

		bool SaveMappings(const char* dev_type, int port, const std::string& joyname, u32 max_buttons, u32 max_axes, const ConfigMapping& cfg)
		{
			if (joyname.empty() || cfg.controls.size() != max_buttons + max_axes)
				return false;

			RemoveSection(dev_type, port, joyname);
			std::stringstream str;
			const bool use_control_names = !strcmp(dev_type, PadDevice::TypeName());
			bool has_axes = false;

			for (u32 i = 0; i < max_buttons + max_axes; i++)
			{
				str.clear();
				str.str("");
				if (i < max_buttons)
				{
					str << "button_";
					if (use_control_names && i < (u32)countof(JoystickMapNames))
						str << JoystickMapNames[i];
					else
						str << i;
				}
				else
				{
					str << "axis_";
					u32 axis = i - max_buttons;
					if (use_control_names && (JOY_STEERING + axis) < (u32)countof(JoystickMapNames))
						str << JoystickMapNames[JOY_STEERING + axis];
					else
						str << axis;
				}

				const std::string& name = str.str();
				if (cfg.controls[i] >= 0)
				{
					if (!SaveSetting(dev_type, port, joyname, name.c_str(), static_cast<int32_t>(cfg.controls[i])))
						return false;
					if (i >= max_buttons)
						has_axes = true;
				}
			}

			for (u32 i = 0; i < 3 && has_axes; i++)
			{
				str.clear();
				str.str("");
				str << "inverted_";
				if (use_control_names)
					str << JoystickMapNames[JOY_STEERING + i];
				else
					str << i;

				{
					const std::string& name = str.str();
					if (!SaveSetting(dev_type, port, joyname, name.c_str(), cfg.inverted[i]))
						return false;
				}

				str.clear();
				str.str("");
				str << "initial_";
				if (use_control_names)
					str << JoystickMapNames[JOY_STEERING + i];
				else
					str << i;

				{
					const std::string& name = str.str();
					if (!SaveSetting(dev_type, port, joyname, name.c_str(), cfg.initial[i]))
						return false;
				}
			}
			return true;
		}

		bool LoadBuzzMappings(const char* dev_type, int port, const std::string& joyname, ConfigMapping& cfg)
		{
			std::stringstream str;

			if (joyname.empty())
				return false;

			int j = 0;

			cfg.controls.resize(countof(buzz_map_names) * 4);
			for (auto& i : cfg.controls)
			{
				str.str("");
				str.clear();
				str << "map_" << buzz_map_names[j % 5] << "_" << (j / 5);
				const std::string& name = str.str();
				int32_t var;
				if (LoadSetting(dev_type, port, joyname, name.c_str(), var))
					i = var;
				else
					i = -1;
				j++;
			}
			return true;
		}

		bool SaveBuzzMappings(const char* dev_type, int port, const std::string& joyname, const ConfigMapping& cfg)
		{
			if (joyname.empty())
				return false;

			RemoveSection(dev_type, port, joyname);
			std::stringstream str;

			const size_t c = countof(buzz_map_names);
			for (size_t i = 0; i < cfg.controls.size(); i++)
			{
				str.str("");
				str.clear();
				str << "map_" << buzz_map_names[i % c] << "_" << (i / c);
				const std::string& name = str.str();
				if (cfg.controls[i] >= 0 && !SaveSetting(dev_type, port, joyname, name.c_str(), static_cast<int32_t>(cfg.controls[i])))
					return false;
			}
			return true;
		}

		static void refresh_store(ConfigData* cfg)
		{
			GtkTreeIter iter;
			std::string name;

			gtk_list_store_clear(cfg->store);
			for (auto& it : cfg->jsconf)
			{
				for (size_t i = 0; i < it.second.controls.size(); i++)
				{
					if (it.second.controls[i] < 0)
						continue;

					const char* pc_name = "Unknown";
					bool is_button = (i < cfg->max_buttons);
					cfg->cb->get_event_name(cfg->dev_type, i, it.second.controls[i], is_button, &pc_name);

					gtk_list_store_append(cfg->store, &iter);

					if (!strcmp(cfg->dev_type, BuzzDevice::TypeName()))
					{
						std::stringstream ss;
						ss << (1 + i / countof(buzz_map_names));
						ss << " ";
						ss << buzz_map_names[i % countof(buzz_map_names)];
						name = ss.str();
					}
					else if (!strcmp(cfg->dev_type, PadDevice::TypeName()))
						name = JoystickMapNames[i];
					else if (!strcmp(cfg->dev_type, KeyboardmaniaDevice::TypeName()))
						name = kbdmania_key_labels[i];
					else
					{
						std::stringstream ss;
						if (is_button)
							ss << "Button " << i;
						else
							ss << "Axis " << (i - cfg->max_buttons);
						name = ss.str();
					}

					gtk_list_store_set(cfg->store, &iter,
									   COL_NAME, it.first.c_str(),
									   COL_PS2, name.c_str(),
									   COL_PC, pc_name,
									   COL_BINDING, i,
									   -1);
				}
			}
		}

		static void joystick_changed(GtkComboBox* widget, gpointer data)
		{
			gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
			//int port = reinterpret_cast<uintptr_t>(data);
			ConfigData* cfg = reinterpret_cast<ConfigData*>(g_object_get_data(G_OBJECT(widget), CFG));

			if (!cfg)
				return;

			if (idx > -1)
				cfg->js_iter = (cfg->joysticks.begin() + idx);
		}

		static void button_clicked(GtkWidget* widget, gpointer data)
		{
			u32 control = reinterpret_cast<uintptr_t>(g_object_get_data(G_OBJECT(widget), CONTROL));
			ConfigData* cfg = reinterpret_cast<ConfigData*>(g_object_get_data(G_OBJECT(widget), CFG));
			if (!cfg)
				return;

			int value, initial = 0;
			std::string dev_name;
			bool inverted = false;
			bool is_axis = (control >= cfg->max_buttons);

			gtk_label_set_text(GTK_LABEL(cfg->label), "Polling for input for 5 seconds...");

			// let label change its text
			while (gtk_events_pending())
				gtk_main_iteration_do(FALSE);

			if (cfg->cb->poll(cfg->jsconf, dev_name, is_axis, value, inverted, initial))
			{
				auto it = std::find_if(cfg->jsconf.begin(), cfg->jsconf.end(),
									   [&dev_name](MappingPair& i) -> bool {
										   return i.first == dev_name;
									   });

				if (it != cfg->jsconf.end() && control < (u32)it->second.controls.size())
				{
					it->second.controls[control] = value;
					if (is_axis && control - cfg->max_buttons < countof(it->second.inverted))
					{
						it->second.inverted[control - cfg->max_buttons] = inverted;
						it->second.initial[control - cfg->max_buttons] = initial;
					}
					refresh_store(cfg);
				}
			}
			gtk_label_set_text(GTK_LABEL(cfg->label), "");
		}

		// save references to row paths, automatically updated when store changes
		static void view_selected_foreach_func(GtkTreeModel* model,
											   GtkTreePath* path, GtkTreeIter* iter, gpointer userdata)
		{
			GList** rr_list = (GList**)userdata;
			GtkTreeRowReference* rowref;
			rowref = gtk_tree_row_reference_new(model, path);
			*rr_list = g_list_append(*rr_list, rowref);
		}

		static void view_remove_binding(GtkTreeModel* model,
										GtkTreeIter* iter, ConfigData* cfg)
		{
			gchar* dev_name;
			int binding;

			gtk_tree_model_get(model, iter, COL_NAME, &dev_name, COL_BINDING, &binding, -1);

			auto& js = cfg->jsconf;
			auto it = std::find_if(js.begin(), js.end(),
								   [&dev_name](MappingPair i) {
									   return i.first == dev_name;
								   });
			if (it != js.end())
			{
				it->second.controls[binding] = (uint16_t)-1;
			}
			gtk_list_store_remove(GTK_LIST_STORE(model), iter);
			//refresh_store(cfg);

			g_free(dev_name);
		}

		static void clear_binding_clicked(GtkWidget* widget, gpointer data)
		{
			GtkTreeModel* model = nullptr;
			GList* rr_list = nullptr;
			GList* node = nullptr;

			ConfigData* cfg = (ConfigData*)g_object_get_data(G_OBJECT(widget), CFG);
			GtkTreeSelection* sel = gtk_tree_view_get_selection(cfg->treeview);

			gtk_tree_selection_selected_foreach(sel, view_selected_foreach_func, &rr_list);

			GList* list = gtk_tree_selection_get_selected_rows(sel, &model);
			// remove rows from store pointed to by row references
			for (node = g_list_first(rr_list); node != nullptr; node = node->next)
			{
				GtkTreePath* path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);
				if (path)
				{
					GtkTreeIter iter;
					if (gtk_tree_model_get_iter(model, &iter, path))
					{
						view_remove_binding(model, &iter, cfg);
					}
				}
			}

			g_list_free_full(rr_list, (GDestroyNotify)gtk_tree_row_reference_free);
			g_list_free_full(list, (GDestroyNotify)gtk_tree_path_free);
		}

		static void clear_all_clicked(GtkWidget* widget, gpointer data)
		{
			ConfigData* cfg = (ConfigData*)g_object_get_data(G_OBJECT(widget), CFG);
			for (auto& it : cfg->jsconf)
				it.second.controls.assign(it.second.controls.size(), -1);
			refresh_store(cfg);
		}

		static void checkbox_toggled(GtkToggleButton* widget, gpointer data)
		{
			gboolean* val = reinterpret_cast<gboolean*>(data);
			if (val)
			{
				*val = (bool)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
			}
		}

		static GtkWidget* make_dialog(GtkWindow* parent, const std::string& title, int w = 1200, int h = 700)
		{
			auto dlg = gtk_dialog_new_with_buttons(
				title.c_str(), parent, GTK_DIALOG_MODAL,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
			gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
			gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
			gtk_window_set_default_size(GTK_WINDOW(dlg), w, h);
			return dlg;
		}

		static void create_panes(GtkWidget* container, GtkWidget*& left_vbox, GtkWidget*& right_vbox)
		{
			left_vbox = gtk_vbox_new(FALSE, 5);
			right_vbox = gtk_vbox_new(FALSE, 15);

#if 0
			GtkWidget* paned = gtk_hpaned_new();
			gtk_container_add(GTK_CONTAINER(container), paned);
			gtk_paned_add1(GTK_PANED(paned), left_vbox);

			GtkWidget* sc_win = gtk_scrolled_window_new(NULL, NULL);
			gtk_container_add(GTK_CONTAINER(sc_win), right_vbox);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc_win), GTK_POLICY_AUTOMATIC,
										   GTK_POLICY_AUTOMATIC);
			gtk_paned_add2(GTK_PANED(paned), sc_win);
#else
			GtkWidget* hbox = gtk_hbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(container), hbox);
			gtk_box_pack_start(GTK_BOX(hbox), left_vbox, TRUE, TRUE, 5);
			gtk_box_pack_start(GTK_BOX(hbox), right_vbox, TRUE, TRUE, 5);
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
			gtk_widget_set_vexpand(left_vbox, TRUE);
			gtk_widget_set_valign(right_vbox, GTK_ALIGN_START);
#endif
		}

		static GtkWidget* make_mappings_treeview(int port, ConfigData& cfg, GtkWidget* container)
		{
			GtkWidget* button;
			auto treeview = gtk_tree_view_new();
			cfg.treeview = GTK_TREE_VIEW(treeview);
			auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
			gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

			GtkCellRenderer* render = gtk_cell_renderer_text_new();

			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
														-1, "Name", render, "text", COL_NAME, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
														-1, "PS2", render, "text", COL_PS2, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
														-1, "PC", render, "text", COL_PC, NULL);

			gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(treeview), 0);

			gtk_tree_view_column_set_resizable(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0), TRUE);
			gtk_tree_view_column_set_resizable(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 1), TRUE);
			gtk_tree_view_column_set_resizable(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 2), TRUE);

			gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(cfg.store));
			g_object_unref(GTK_TREE_MODEL(cfg.store)); //treeview has its own ref

			GtkWidget* scwin = gtk_scrolled_window_new(NULL, NULL);
			gtk_container_add(GTK_CONTAINER(scwin), treeview);
			gtk_widget_set_size_request(GTK_WIDGET(scwin), 200, 100);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin), GTK_POLICY_AUTOMATIC,
										   GTK_POLICY_AUTOMATIC);
			gtk_box_pack_start(GTK_BOX(container), scwin, TRUE, TRUE, 5);

			button = gtk_button_new_with_label("Clear binding");
			gtk_box_pack_start(GTK_BOX(container), button, FALSE, FALSE, 5);
			g_object_set_data(G_OBJECT(button), CFG, &cfg);
			g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(clear_binding_clicked), reinterpret_cast<gpointer>(port));

			button = gtk_button_new_with_label("Clear All");
			gtk_box_pack_start(GTK_BOX(container), button, FALSE, FALSE, 5);
			g_object_set_data(G_OBJECT(button), CFG, &cfg);
			g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(clear_all_clicked), reinterpret_cast<gpointer>(port));
			return treeview;
		}

		static void load_devices_mappings(ConfigData& cfg, const int port, ApiCallbacks& apicbs)
		{
			int fd;
			apicbs.populate(cfg.joysticks);

			cfg.js_iter = cfg.joysticks.end();
			cfg.label = gtk_label_new("");
			cfg.store = gtk_list_store_new(NUM_COLS,
										   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
			cfg.cb = &apicbs;

			for (const auto& it : cfg.joysticks)
			{
				if ((fd = open(it.path.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
				{
					Console.Warning("USB: failed to open '%s'", it.path.c_str());
					continue;
				}

				ConfigMapping c(fd);
				LoadMappings(cfg.dev_type, port, it.id, cfg.max_buttons, cfg.max_axes, c);
				cfg.jsconf.push_back(std::make_pair(it.id, c));
			}

			refresh_store(&cfg);
		}

		int GtkPadConfigure(int port, const char* dev_type, const char* apititle, const char* apiname, GtkWindow* parent, ApiCallbacks& apicbs)
		{
			GtkWidget *ro_frame, *rs_cb;
			GtkWidget *right_vbox, *left_vbox;

			ConfigData cfg{};
			cfg.dev_type = dev_type;
			cfg.max_axes = 3;
			cfg.max_buttons = JOY_STEERING; // 16
			load_devices_mappings(cfg, port, apicbs);

			std::string path;
			LoadSetting(dev_type, port, apiname, N_JOYSTICK, path);

			cfg.use_hidraw_ff_pt = false;
			bool is_evdev = (strncmp(apiname, "evdev", 5) == 0);
			if (is_evdev)
			{
				LoadSetting(dev_type, port, apiname, N_HIDRAW_FF_PT, cfg.use_hidraw_ff_pt);
			}

			// ---------------------------
			const std::string title = std::string(port ? "Player One " : "Player Two ") + apititle;
			GtkWidget* dlg = make_dialog(parent, title);

			// ---------------------------
			GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			create_panes(dlg_area_box, left_vbox, right_vbox);
			make_mappings_treeview(port, cfg, left_vbox);

			// ---------------------------

			// Remapping
			{
				GtkWidget* table = gtk_table_new(5, 7, TRUE);
				gtk_container_add(GTK_CONTAINER(right_vbox), table);
				//GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default
				GtkAttachOptions opt = (GtkAttachOptions)(GTK_FILL);

				const char* button_labels[] = {
					"L2",
					"L1 / L",
					"R2",
					"R1 / R / Orange",
					"Left",
					"Up",
					"Right",
					"Down",
					"Square / X / Green",
					"Cross / A / Blue",
					"Circle / B / Red",
					"Triangle / Y / Yellow",
					"Select",
					"Start",
				};

				const Point button_pos[] = {
					{1, 0, JOY_L2},
					{1, 1, JOY_L1},
					{5, 0, JOY_R2},
					{5, 1, JOY_R1},
					{0, 3, JOY_LEFT},
					{1, 2, JOY_UP},
					{2, 3, JOY_RIGHT},
					{1, 4, JOY_DOWN},
					{4, 3, JOY_SQUARE},
					{5, 4, JOY_CROSS},
					{6, 3, JOY_CIRCLE},
					{5, 2, JOY_TRIANGLE},
					{3, 3, JOY_SELECT},
					{3, 2, JOY_START},
				};

				for (int i = 0; i < (int)countof(button_labels); i++)
				{
					GtkWidget* button = gtk_button_new_with_label(button_labels[i]);
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));

					g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(button_pos[i].type));
					g_object_set_data(G_OBJECT(button), CFG, &cfg);

					gtk_table_attach(GTK_TABLE(table), button,
									 0 + button_pos[i].x, 1 + button_pos[i].x,
									 0 + button_pos[i].y, 1 + button_pos[i].y,
									 opt, opt, 5, 1);
				}

				GtkWidget* hbox = gtk_hbox_new(false, 5);
				gtk_container_add(GTK_CONTAINER(right_vbox), hbox);

				GtkWidget* button = gtk_button_new_with_label("Steering");
				gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
				g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(JOY_STEERING));
				g_object_set_data(G_OBJECT(button), CFG, &cfg);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));

				button = gtk_button_new_with_label("Throttle");
				gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
				g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(JOY_THROTTLE));
				g_object_set_data(G_OBJECT(button), CFG, &cfg);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));

				button = gtk_button_new_with_label("Brake");
				gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
				g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(JOY_BRAKE));
				g_object_set_data(G_OBJECT(button), CFG, &cfg);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));

				gtk_box_pack_start(GTK_BOX(right_vbox), cfg.label, TRUE, TRUE, 5);
			}
			ro_frame = gtk_frame_new("Force feedback");
			gtk_box_pack_start(GTK_BOX(right_vbox), ro_frame, TRUE, FALSE, 5);

			//GtkWidget *frame_vbox = gtk_vbox_new (FALSE, 5);
			//gtk_container_add (GTK_CONTAINER (ro_frame), frame_vbox);

			const char* labels_buff[][2] = {{"Set gain", "Gain"}, {"Managed by game", "Autocenter strength"}};
			const char* ff_var_name[][2] = {{N_GAIN_ENABLED, N_GAIN}, {N_AUTOCENTER_MANAGED, N_AUTOCENTER}};
			GtkWidget* ff_scales[2];
			int32_t ff_enabled[2];

			GtkWidget* table = gtk_table_new(3, 2, TRUE);
			gtk_container_add(GTK_CONTAINER(ro_frame), table);
			gtk_table_set_homogeneous(GTK_TABLE(table), FALSE);
			GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default

			for (int i = 0; i < 2; i++)
			{
				if (LoadSetting(dev_type, port, apiname, ff_var_name[i][0], ff_enabled[i]))
					ff_enabled[i] = !!ff_enabled[i];
				else
					ff_enabled[i] = 1;

				GtkWidget* chk_btn = gtk_check_button_new_with_label(labels_buff[i][0]);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_btn), (gboolean)ff_enabled[i]);
				g_signal_connect(G_OBJECT(chk_btn), "toggled", G_CALLBACK(checkbox_toggled), reinterpret_cast<gboolean*>(&ff_enabled[i]));
				gtk_table_attach(GTK_TABLE(table), chk_btn,
								 2, 3,
								 0 + i, 1 + i,
								 GTK_FILL, GTK_SHRINK, 5, 1);

				GtkWidget* label = gtk_label_new(labels_buff[i][1]);
				gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
				gtk_table_attach(GTK_TABLE(table), label,
								 0, 1,
								 0 + i, 1 + i,
								 GTK_FILL, GTK_SHRINK, 5, 1);

				//ff_scales[i] = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 100, 1);
				ff_scales[i] = gtk_hscale_new_with_range(0, 100, 1);
				for (int v = 0; v <= 100; v += 10)
					gtk_scale_add_mark(GTK_SCALE(ff_scales[i]), v, GTK_POS_BOTTOM, nullptr);
				gtk_table_attach(GTK_TABLE(table), ff_scales[i],
								 1, 2,
								 0 + i, 1 + i,
								 opt, opt, 5, 1);

				int32_t var;
				if (LoadSetting(dev_type, port, apiname, ff_var_name[i][1], var))
				{
					var = std::min(100, std::max(0, var));
					gtk_range_set_value(GTK_RANGE(ff_scales[i]), var);
				}
				else
					gtk_range_set_value(GTK_RANGE(ff_scales[i]), 100);
			}

			if (is_evdev)
			{
				ro_frame = gtk_frame_new("Logitech wheel force feedback pass-through using hidraw");
				gtk_box_pack_start(GTK_BOX(right_vbox), ro_frame, FALSE, FALSE, 5);

				GtkWidget* frame_vbox = gtk_vbox_new(FALSE, 5);
				gtk_container_add(GTK_CONTAINER(ro_frame), frame_vbox);

				GtkWidget* chk_btn = gtk_check_button_new_with_label("Enable");
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_btn), (gboolean)cfg.use_hidraw_ff_pt);
				//g_object_set_data(G_OBJECT(chk_btn), CFG, &cfg);
				g_signal_connect(G_OBJECT(chk_btn), "toggled", G_CALLBACK(checkbox_toggled), reinterpret_cast<gboolean*>(&cfg.use_hidraw_ff_pt));
				gtk_box_pack_start(GTK_BOX(frame_vbox), chk_btn, FALSE, FALSE, 5);

				rs_cb = new_combobox("Device:", frame_vbox, true);

				const std::vector<uint16_t> whitelist{PAD_LG_FFB_WHITELIST};
				int idx = 0, sel_idx = 0;
				for (auto& it : cfg.joysticks)
				{
					if (!(it.input_id.vendor == PAD_VID && std::find(whitelist.begin(), whitelist.end(), it.input_id.product) != whitelist.end()))
						continue;

					std::stringstream str;
					str << it.name;
					if (!it.id.empty())
						str << " [" << it.id << "]";

					gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(rs_cb), str.str().c_str());
					if (path == it.path)
						sel_idx = idx;
					idx++;
				}

				g_object_set_data(G_OBJECT(rs_cb), CFG, &cfg);
				g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(joystick_changed), reinterpret_cast<gpointer>(port));
				gtk_combo_box_set_active(GTK_COMBO_BOX(rs_cb), sel_idx);
			}
			// ---------------------------
			gtk_widget_show_all(dlg);
			gint result = gtk_dialog_run(GTK_DIALOG(dlg));

			int ret = RESULT_OK;
			if (result == GTK_RESPONSE_OK)
			{
				if (cfg.js_iter != cfg.joysticks.end())
				{
					if (!SaveSetting(dev_type, port, apiname, N_JOYSTICK, cfg.js_iter->path))
						ret = RESULT_FAILED;
				}

				for (auto& it : cfg.jsconf)
					SaveMappings(dev_type, port, it.first, cfg.max_buttons, cfg.max_axes, it.second);

				if (is_evdev)
				{
					SaveSetting(dev_type, port, apiname, N_HIDRAW_FF_PT, cfg.use_hidraw_ff_pt);
				}
				for (int i = 0; i < 2; i++)
				{
					SaveSetting(dev_type, port, apiname, ff_var_name[i][0], ff_enabled[i]);
					int val = gtk_range_get_value(GTK_RANGE(ff_scales[i]));
					SaveSetting(dev_type, port, apiname, ff_var_name[i][1], val);
				}
			}
			else
				ret = RESULT_CANCELED;

			for (auto& it : cfg.jsconf)
				close(it.second.fd);

			gtk_widget_destroy(dlg);
			return ret;
		}

		GtkWidget* make_color_icon(uint32_t rgb)
		{
			GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 24, 24);
			guchar* data = gdk_pixbuf_get_pixels(pixbuf);

			for (size_t i = 0; i < 24 * 24; i++)
			{
				data[i * 4 + 0] = rgb & 0xFF;
				data[i * 4 + 1] = (rgb >> 8) & 0xFF;
				data[i * 4 + 2] = (rgb >> 16) & 0xFF;
				data[i * 4 + 3] = icon_buzz_24[i];
			}

			GtkWidget* w = gtk_image_new_from_pixbuf(pixbuf);
			g_object_unref(G_OBJECT(pixbuf));
			return w;
		}

		int GtkBuzzConfigure(int port, const char* dev_type, const char* apititle, const char* apiname, GtkWindow* parent, ApiCallbacks& apicbs)
		{
			GtkWidget *main_hbox, *right_vbox, *left_vbox;

			int fd;
			ConfigData cfg{};

			apicbs.populate(cfg.joysticks);

			cfg.js_iter = cfg.joysticks.end();
			cfg.label = gtk_label_new("");
			cfg.store = gtk_list_store_new(NUM_COLS,
										   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
			cfg.cb = &apicbs;
			cfg.dev_type = dev_type;
			cfg.max_axes = 0;
			cfg.max_buttons = 20;

			for (const auto& it : cfg.joysticks)
			{
				if ((fd = open(it.path.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
				{
					continue;
				}

				ConfigMapping c;
				c.fd = fd;
				LoadBuzzMappings(cfg.dev_type, port, it.id, c);
				cfg.jsconf.push_back(std::make_pair(it.id, c));
			}

			refresh_store(&cfg);

			// ---------------------------
			const std::string title = std::string("Buzz ") + apititle;
			GtkWidget* dlg = make_dialog(parent, title);

			// ---------------------------
			GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

			main_hbox = gtk_hbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(dlg_area_box), main_hbox);

			left_vbox = gtk_vbox_new(FALSE, 5);
			right_vbox = gtk_vbox_new(FALSE, 5);
			gtk_box_pack_start(GTK_BOX(main_hbox), left_vbox, TRUE, TRUE, 5);
			gtk_box_pack_start(GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, 5);

			make_mappings_treeview(port, cfg, left_vbox);

#if GTK_CHECK_VERSION(3, 0, 0)
			gtk_widget_set_vexpand(left_vbox, TRUE);
#endif

			// ---------------------------

			// Remapping
			{
				GtkWidget* table = gtk_table_new(5, 4, TRUE);
				gtk_container_add(GTK_CONTAINER(right_vbox), table);
				GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default

				static const char* button_labels[]{
					"Red",
					"Blue",
					"Orange",
					"Green",
					"Yellow",
				};

				static const Buzz buzz_btns[]{
					BUZZ_RED,
					BUZZ_BLUE,
					BUZZ_ORANGE,
					BUZZ_GREEN,
					BUZZ_YELLOW,
				};

				static const uint32_t icon_colors[]{
					0x0000FF,
					0xFF0000,
					0x0080FF,
					0x00FF00,
					0x00FFFF,
				};

				for (int j = 0; j < 4; j++)
				{
					for (int i = 0; i < (int)countof(button_labels); i++)
					{
						GtkWidget* button = gtk_button_new_with_label(button_labels[i]);

						GtkWidget* icon = make_color_icon(icon_colors[i]);
						gtk_button_set_image(GTK_BUTTON(button), icon);
						gtk_button_set_image_position(GTK_BUTTON(button), GTK_POS_LEFT);

						GList* children = gtk_container_get_children(GTK_CONTAINER(button));

						//Gtk 3.16+
						//gtk_label_set_xalign (GTK_WIDGET(children), 0.0)

						//gtk_misc_set_alignment (GTK_MISC (children->data), 0.0, 0.5);
						if (GTK_IS_ALIGNMENT(children->data))
							gtk_alignment_set(GTK_ALIGNMENT(children->data), 0.0f, 0.5f, 0.2f, 0.f);

						g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));

						g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(j * countof(buzz_btns) + buzz_btns[i]));
						g_object_set_data(G_OBJECT(button), CFG, &cfg);

						gtk_table_attach(GTK_TABLE(table), button,
										 j, 1 + j,
										 i + 1, 2 + i,
										 opt, opt, 5, 1);
					}

					gtk_table_attach(GTK_TABLE(table), gtk_label_new("Player 1"),
									 0, 1, 0, 1, opt, opt, 5, 1);
					gtk_table_attach(GTK_TABLE(table), gtk_label_new("Player 2"),
									 1, 2, 0, 1, opt, opt, 5, 1);
					gtk_table_attach(GTK_TABLE(table), gtk_label_new("Player 3"),
									 2, 3, 0, 1, opt, opt, 5, 1);
					gtk_table_attach(GTK_TABLE(table), gtk_label_new("Player 4"),
									 3, 4, 0, 1, opt, opt, 5, 1);
				}

				GtkWidget* hbox = gtk_hbox_new(false, 5);
				gtk_container_add(GTK_CONTAINER(right_vbox), hbox);

				gtk_box_pack_start(GTK_BOX(right_vbox), cfg.label, TRUE, TRUE, 5);
			}

			// ---------------------------
			gtk_widget_show_all(dlg);
			gint result = gtk_dialog_run(GTK_DIALOG(dlg));

			int ret = RESULT_OK;
			if (result == GTK_RESPONSE_OK)
			{
				if (cfg.js_iter != cfg.joysticks.end())
				{
					if (!SaveSetting(dev_type, port, apiname, N_JOYSTICK, cfg.js_iter->path))
						ret = RESULT_FAILED;
				}

				for (auto& it : cfg.jsconf)
					SaveBuzzMappings(dev_type, port, it.first, it.second);
			}
			else
				ret = RESULT_CANCELED;

			for (auto& it : cfg.jsconf)
				close(it.second.fd);

			gtk_widget_destroy(dlg);
			return ret;
		}

		int GtkKeyboardmaniaConfigure(int port, const char* dev_type, const char* apititle, const char* apiname, GtkWindow* parent, ApiCallbacks& apicbs)
		{
			GtkWidget *right_vbox, *left_vbox;

			ConfigData cfg{};
			cfg.dev_type = dev_type;
			cfg.max_buttons = 31;
			load_devices_mappings(cfg, port, apicbs);

			// ---------------------------
			const std::string title = std::string("Keyboardmania ") + apititle;
			GtkWidget* dlg = make_dialog(parent, title);

			// ---------------------------
			GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			create_panes(dlg_area_box, left_vbox, right_vbox);
			make_mappings_treeview(port, cfg, left_vbox);

			// ---------------------------
			// Remapping
			{
				GtkWidget* table = gtk_table_new(5, 14, TRUE);
				gtk_container_add(GTK_CONTAINER(right_vbox), table);
				GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default
#if GTK_CHECK_VERSION(3, 0, 0)
				gtk_widget_set_halign(table, GTK_ALIGN_START);
#endif

				struct keys
				{
					u32 index;
					bool sharp;
				};

				constexpr keys keys[]{
					{0, false},
					{1, true},
					{2, false},
					{3, true},
					{4, false},
					{5, false},
					{6, true},
					//{"padding", 7},
					{8, false},
					{9, true},
					{10, false},
					{11, true},
					{12, false},
					{13, false},
					//{"Select", 14},
					//{"padding", 15},
					{16, true},
					{17, false},
					{18, true},
					{19, false},
					{20, false},
					{21, true},
					//{"Start", 22},
					//{"padding", 23},
					{24, false},
					{25, true},
					{26, false},
					{27, true},
					{28, false},
					//{"Up", 29},
					//{"Down", 30},
				};

				int attached = 0;
				int voffset = 0;
				for (auto& key : keys)
				{
					GtkWidget* button = gtk_button_new_with_label(kbdmania_key_labels[key.index]);
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));
					g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(key.index));
					g_object_set_data(G_OBJECT(button), CFG, &cfg);

					// split into 2-by-2 rows
					if (attached > 6)
					{
						voffset = 2;
						attached = 0;
					}

					if (!key.sharp)
					{
						gtk_table_attach(GTK_TABLE(table), button,
										 attached * 2, 2 + attached * 2,
										 1 + voffset, 2 + voffset,
										 opt, opt, 5, 1);
						attached++;
					}
					else
						gtk_table_attach(GTK_TABLE(table), button,
										 attached * 2 - 1, attached * 2 + 2 - 1,
										 0 + voffset, 1 + voffset,
										 opt, opt, 5, 1);
				}

				GtkWidget *button, *frame_box, *frame;
				GtkWidget* frame_container = gtk_hbox_new(FALSE, 5);
				gtk_container_add(GTK_CONTAINER(right_vbox), frame_container);
#if GTK_CHECK_VERSION(3, 0, 0)
				gtk_widget_set_valign(frame_container, GTK_ALIGN_START);
#endif

				frame = gtk_frame_new("Buttons");
				{
					frame_box = gtk_hbox_new(FALSE, 5);
					gtk_container_add(GTK_CONTAINER(frame), frame_box);
					gtk_box_pack_start(GTK_BOX(frame_container), frame, FALSE, FALSE, 5);

					button = gtk_button_new_with_label("Start");
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));
					g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(22));
					g_object_set_data(G_OBJECT(button), CFG, &cfg);
					gtk_box_pack_start(GTK_BOX(frame_box), button, FALSE, FALSE, 5);

					button = gtk_button_new_with_label("Select");
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));
					g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(14));
					g_object_set_data(G_OBJECT(button), CFG, &cfg);
					gtk_box_pack_start(GTK_BOX(frame_box), button, FALSE, FALSE, 5);
				}

				frame = gtk_frame_new("Wheel");
				{
					frame_box = gtk_hbox_new(FALSE, 5);
					gtk_container_add(GTK_CONTAINER(frame), frame_box);
					gtk_box_pack_start(GTK_BOX(frame_container), frame, FALSE, FALSE, 5);

					button = gtk_button_new_with_label("Up");
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));
					g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(29));
					g_object_set_data(G_OBJECT(button), CFG, &cfg);
					gtk_box_pack_start(GTK_BOX(frame_box), button, FALSE, FALSE, 5);

					button = gtk_button_new_with_label("Down");
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked), reinterpret_cast<gpointer>(port));
					g_object_set_data(G_OBJECT(button), CONTROL, reinterpret_cast<gpointer>(30));
					g_object_set_data(G_OBJECT(button), CFG, &cfg);
					gtk_box_pack_start(GTK_BOX(frame_box), button, FALSE, FALSE, 5);
				}
			}

			gtk_box_pack_start(GTK_BOX(right_vbox), cfg.label, TRUE, TRUE, 5);

			// ---------------------------
			gtk_widget_show_all(dlg);
			gint result = gtk_dialog_run(GTK_DIALOG(dlg));

			int ret = RESULT_OK;
			if (result == GTK_RESPONSE_OK)
			{
				if (cfg.js_iter != cfg.joysticks.end())
				{
					if (!SaveSetting(dev_type, port, apiname, N_JOYSTICK, cfg.js_iter->path))
						ret = RESULT_FAILED;
				}

				for (auto& it : cfg.jsconf)
					SaveMappings(dev_type, port, it.first, cfg.max_buttons, 0, it.second);
			}
			else
				ret = RESULT_CANCELED;

			for (auto& it : cfg.jsconf)
				close(it.second.fd);

			gtk_widget_destroy(dlg);
			return ret;
		}

	} // namespace evdev
} // namespace usb_pad
