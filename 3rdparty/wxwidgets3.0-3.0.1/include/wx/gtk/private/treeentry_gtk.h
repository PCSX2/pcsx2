/* ///////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/treeentry_gtk.h
// Purpose:     GtkTreeEntry - a string/userdata combo for use with treeview
// Author:      Ryan Norton
// Copyright:   (c) 2006 Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifndef _WX_GTK_TREE_ENTRY_H_
#define _WX_GTK_TREE_ENTRY_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h> /* for gpointer and gchar* etc. */

#define WX_TYPE_TREE_ENTRY          wx_tree_entry_get_type()
#define WX_TREE_ENTRY(obj)          G_TYPE_CHECK_INSTANCE_CAST(obj, wx_tree_entry_get_type(), wxTreeEntry)
#define WX_IS_TREE_ENTRY(obj)       G_TYPE_CHECK_INSTANCE_TYPE(obj, wx_tree_entry_get_type())

typedef struct _wxTreeEntry wxTreeEntry;

typedef void (*wxTreeEntryDestroy)(wxTreeEntry* entry, void* context);

struct _wxTreeEntry
{
    GObject parent;                     /* object instance */
    gchar*  label;                      /* label - always copied by this object except on get */
    gchar*  collate_key;                /* collate key used for string comparisons/sorting */
    gpointer userdata;                  /* untouched userdata */
    wxTreeEntryDestroy destroy_func;    /* called upon destruction - use for freeing userdata etc. */
    gpointer destroy_func_data;         /* context passed to destroy_func */
};

wxTreeEntry* wx_tree_entry_new(void);

GType wx_tree_entry_get_type(void);

char* wx_tree_entry_get_collate_key(wxTreeEntry* entry);

char* wx_tree_entry_get_label(wxTreeEntry* entry);

void* wx_tree_entry_get_userdata(wxTreeEntry* entry);

void wx_tree_entry_set_label(wxTreeEntry* entry, const char* label);

void wx_tree_entry_set_userdata(wxTreeEntry* entry, void* userdata);

void wx_tree_entry_set_destroy_func(wxTreeEntry* entry,
                                        wxTreeEntryDestroy destroy_func,
                                        gpointer destroy_func_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WX_GTK_TREE_ENTRY_H_ */
