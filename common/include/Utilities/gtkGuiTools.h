/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

// ----------------------------------------------------------------------------
// gtkGuiTools.h
//
// This file is meant to contain utility classes for users of GTK, for purposes
// of GTK 2/3 compatibility, and other helpful routines to help avoid repeatedly
// implementing the same code.
//
// ----------------------------------------------------------------------------

#include <gtk/gtk.h>
#include <cstring>

// They've gotten rid of the GtkHBox and GtkVBox in GTK3 in favor of using the 
// more general GtkBox and supplying an orientation. While this is probably a 
// move in the right direction, for compatability, it's easier to define our
// own hbox and vbox routines that invoke the old or the new version, depending
// on what it's built for.
//

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

static GtkWidget *ps_gtk_hbox_new(int padding = 5)
{
#if GTK_MAJOR_VERSION < 3
    return gtk_hbox_new(false, padding);
#else
    return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, padding);
#endif
}

static GtkWidget *ps_gtk_vbox_new(int padding = 5)
{
#if GTK_MAJOR_VERSION < 3
    return gtk_vbox_new(false, padding);
#else
    return gtk_box_new(GTK_ORIENTATION_VERTICAL, padding);
#endif
}

// Similarly, GtkHScale and GtkVScale make way for GtkScale.
static GtkWidget *ps_gtk_hscale_new_with_range(double g_min, double g_max, int g_step = 5)
{
#if GTK_MAJOR_VERSION < 3
    return gtk_hscale_new_with_range(g_min, g_max, g_step);
#else
    return gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, g_min, g_max, g_step);
#endif
}

static GtkWidget *ps_gtk_vscale_new_with_range(double g_min, double g_max, int g_step = 5)
{
#if GTK_MAJOR_VERSION < 3
    return gtk_vscale_new_with_range(g_min, g_max, g_step);
#else
    return gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, g_min, g_max, g_step);
#endif
}

// And so on and so forth...
static GtkWidget *ps_gtk_hseparator_new()
{
#if GTK_MAJOR_VERSION < 3
    return gtk_hseparator_new();
#else
    return gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
#endif
}

static GtkWidget *ps_gtk_vseparator_new()
{
#if GTK_MAJOR_VERSION < 3
    return gtk_vseparator_new();
#else
    return gtk_separator_new(GTK_ORIENTATION_VERTICAL);
#endif
}

// These two routines have been rewritten over and over. May as well include a copy.
// Renaming so as not to interfere with existing versions.
static void pcsx2_message(const char *fmt, ...)
{
    va_list list;
    char msg[512];

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    if (msg[strlen(msg) - 1] == '\n')
        msg[strlen(msg) - 1] = 0;

    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    "%s", msg);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void pcsx2_message(const wchar_t *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    wxString msg;
    msg.PrintfV(fmt, list);
    va_end(list);

    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    "%s", msg.ToUTF8().data());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
