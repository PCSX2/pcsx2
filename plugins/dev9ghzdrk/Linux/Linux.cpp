/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
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

#include <stdio.h>

#include <gtk/gtk.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#include "Config.h"
#include "../DEV9.h"
#include "pcap.h"
#include "pcap_io.h"
#include "net.h"

static GtkBuilder * builder;

void SysMessage(char *fmt, ...) {
    va_list list;
    char tmp[512];

    va_start(list,fmt);
    vsprintf(tmp,fmt,list);
    va_end(list);

    GtkWidget *dialog = gtk_message_dialog_new (NULL,
                        GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        "%s", tmp);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_hide(dialog);
}

void OnInitDialog() {
    char *dev;
    gint idx = 0;
    static int initialized = 0;

    LoadConf();

    if( initialized )
        return;

    gtk_combo_box_text_append_text((GtkComboBoxText *)gtk_builder_get_object(builder,"IDC_BAYTYPE"),"Expansion");
    gtk_combo_box_text_append_text((GtkComboBoxText *)gtk_builder_get_object(builder,"IDC_BAYTYPE"),"PC Card");
    for (int i=0; i<pcap_io_get_dev_num(); i++) {
        dev = pcap_io_get_dev_name(i);
        gtk_combo_box_text_append_text((GtkComboBoxText *)gtk_builder_get_object(builder,"IDC_ETHDEV"), dev);
        if (strcmp(dev, config.Eth) == 0) {
        gtk_combo_box_set_active((GtkComboBox *)gtk_builder_get_object(builder,"IDC_ETHDEV"),idx);
        }
        idx++;
    }
    gtk_entry_set_text ((GtkEntry *)gtk_builder_get_object(builder,"IDC_HDDFILE"), config.Hdd);
    gtk_toggle_button_set_active ((GtkToggleButton *)gtk_builder_get_object(builder,"IDC_ETHENABLED"),
                  config.ethEnable);
    gtk_toggle_button_set_active ((GtkToggleButton *)gtk_builder_get_object(builder,"IDC_HDDENABLED"),
                  config.hddEnable);

    initialized = 1;
}

void OnOk() {

    char* ptr = gtk_combo_box_text_get_active_text((GtkComboBoxText *)gtk_builder_get_object(builder,"IDC_ETHDEV"));
    strcpy(config.Eth, ptr);

    strcpy(config.Hdd, gtk_entry_get_text ((GtkEntry *)gtk_builder_get_object(builder,"IDC_HDDFILE")));

    config.ethEnable = gtk_toggle_button_get_active ((GtkToggleButton *)gtk_builder_get_object(builder,"IDC_ETHENABLED"));
    config.hddEnable = gtk_toggle_button_get_active ((GtkToggleButton *)gtk_builder_get_object(builder,"IDC_HDDENABLED"));

    SaveConf();

}

/* Simple GTK+2 variant of gtk_builder_add_from_resource() */
static guint builder_add_from_resource(GtkBuilder *builder
    , const gchar *resource_path
    , GError **error)
{
    GBytes *data;
    const gchar *buffer;
    gsize buffer_length;
    guint ret;

    g_assert(error && *error == NULL);

    data = g_resources_lookup_data(resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
    if (data == NULL) {
        return 0;
    }

    buffer_length = 0;
    buffer = (const gchar *)g_bytes_get_data(data, &buffer_length);
    g_assert(buffer != NULL);

    ret = gtk_builder_add_from_string(builder, buffer, buffer_length, error);

    g_bytes_unref(data);

    return ret;
}

EXPORT_C_(void)
DEV9configure() {

    gtk_init (NULL, NULL);
    GError *error = NULL;
    builder = gtk_builder_new();
    if (!builder_add_from_resource(builder, "/net/pcsx2/dev9ghzdrk/Linux/dev9ghzdrk.ui", &error)) {
        g_warning("Could not build config ui: %s", error->message);
        g_error_free(error);
        g_object_unref(G_OBJECT(builder));
    }
    GtkDialog *dlg = GTK_DIALOG (gtk_builder_get_object(builder, "IDD_CONFDLG"));
    OnInitDialog();
    gint result = gtk_dialog_run (dlg);
    switch(result) {
    case -5: //IDOK
    OnOk();
    break;
    case -6: //IDCANCEL
    break;
    }
    gtk_widget_hide (GTK_WIDGET(dlg));

}

void __attribute__((constructor)) DllMain() {
    //gtk_builder_add_from_file(builder, "dev9ghzdrk.ui", NULL);
    //builder = gtk_build_new_from_resource( "/net/pcsx2/dev9ghzdrk/dev9ghzdrk.ui" );
}

NetAdapter* GetNetAdapter()
{
    NetAdapter* na;
    na = new PCAPAdapter();

    if (!na->isInitialised())
    {
            delete na;
            return 0;
    }
    return na;
}
s32  _DEV9open()
{
    NetAdapter* na=GetNetAdapter();
    if (!na)
    {
        emu_printf("Failed to GetNetAdapter()\n");
        config.ethEnable = false;
    }
    else
    {
        InitNet(na);
    }
    return 0;
}

void _DEV9close() {
    TermNet();
}
