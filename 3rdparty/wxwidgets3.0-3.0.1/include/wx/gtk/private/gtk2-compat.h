///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/compat.h
// Purpose:     Compatibility code for older GTK+ versions
// Author:      Vaclav Slavik
// Created:     2011-03-25
// Copyright:   (c) 2011 Vaclav Slavik <vslavik@fastmail.fm>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_COMPAT_H_
#define _WX_GTK_PRIVATE_COMPAT_H_

// ----------------------------------------------------------------------------
// Implementations of new accessors for older GTK+ versions
// ----------------------------------------------------------------------------

// GTK+ deprecated direct access to struct members and some other stuff,
// replacing them with simple accessor functions. These aren't available in
// older versions, though, so we have to provide them for compatibility.
//
// Note: wx_ prefix is used to avoid symbol conflicts at runtime
//
// Note 2: We support building against newer GTK+ version and using an older
//         one at runtime, so we must provide our implementations of these
//         functions even if GTK_CHECK_VERSION would indicate the function is
//         already available in GTK+.

#ifndef __WXGTK3__

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.8

static inline GtkWidget* wx_gtk_scrolled_window_get_hscrollbar(GtkScrolledWindow* scrolled_window)
{
    return scrolled_window->hscrollbar;
}
#define gtk_scrolled_window_get_hscrollbar wx_gtk_scrolled_window_get_hscrollbar

static inline GtkWidget* wx_gtk_scrolled_window_get_vscrollbar(GtkScrolledWindow* scrolled_window)
{
    return scrolled_window->vscrollbar;
}
#define gtk_scrolled_window_get_vscrollbar wx_gtk_scrolled_window_get_vscrollbar

// ----------------------------------------------------------------------------
// the following were introduced in GLib 2.10

static inline gpointer wx_g_object_ref_sink(gpointer object)
{
    g_object_ref(object);
    gtk_object_sink(GTK_OBJECT(object));
    return object;
}
#define g_object_ref_sink wx_g_object_ref_sink

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.12

static inline void wx_gtk_about_dialog_set_program_name(GtkAboutDialog* about, const gchar* name)
{
    gtk_about_dialog_set_name(about, name);
}
#define gtk_about_dialog_set_program_name wx_gtk_about_dialog_set_program_name

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.14

static inline gdouble wx_gtk_adjustment_get_lower(GtkAdjustment* adjustment)
{
    return adjustment->lower;
}
#define gtk_adjustment_get_lower wx_gtk_adjustment_get_lower

static inline gdouble wx_gtk_adjustment_get_page_increment(GtkAdjustment* adjustment)
{
    return adjustment->page_increment;
}
#define gtk_adjustment_get_page_increment wx_gtk_adjustment_get_page_increment

static inline gdouble wx_gtk_adjustment_get_page_size(GtkAdjustment* adjustment)
{
    return adjustment->page_size;
}
#define gtk_adjustment_get_page_size wx_gtk_adjustment_get_page_size

static inline gdouble wx_gtk_adjustment_get_step_increment(GtkAdjustment* adjustment)
{
    return adjustment->step_increment;
}
#define gtk_adjustment_get_step_increment wx_gtk_adjustment_get_step_increment

static inline gdouble wx_gtk_adjustment_get_upper(GtkAdjustment* adjustment)
{
    return adjustment->upper;
}
#define gtk_adjustment_get_upper wx_gtk_adjustment_get_upper

static inline void wx_gtk_adjustment_set_page_size(GtkAdjustment* adjustment, gdouble page_size)
{
    adjustment->page_size = page_size;
}
#define gtk_adjustment_set_page_size wx_gtk_adjustment_set_page_size

static inline GtkWidget* wx_gtk_color_selection_dialog_get_color_selection(GtkColorSelectionDialog* csd)
{
    return csd->colorsel;
}
#define gtk_color_selection_dialog_get_color_selection wx_gtk_color_selection_dialog_get_color_selection

static inline GtkWidget* wx_gtk_dialog_get_content_area(GtkDialog* dialog)
{
    return dialog->vbox;
}
#define gtk_dialog_get_content_area wx_gtk_dialog_get_content_area

static inline GtkWidget* wx_gtk_dialog_get_action_area(GtkDialog* dialog)
{
    return dialog->action_area;
}
#define gtk_dialog_get_action_area wx_gtk_dialog_get_action_area

static inline guint16 wx_gtk_entry_get_text_length(GtkEntry* entry)
{
    return entry->text_length;
}
#define gtk_entry_get_text_length wx_gtk_entry_get_text_length

static inline const guchar* wx_gtk_selection_data_get_data(GtkSelectionData* selection_data)
{
    return selection_data->data;
}
#define gtk_selection_data_get_data wx_gtk_selection_data_get_data

static inline GdkAtom wx_gtk_selection_data_get_data_type(GtkSelectionData* selection_data)
{
    return selection_data->type;
}
#define gtk_selection_data_get_data_type wx_gtk_selection_data_get_data_type

static inline gint wx_gtk_selection_data_get_format(GtkSelectionData* selection_data)
{
    return selection_data->format;
}
#define gtk_selection_data_get_format wx_gtk_selection_data_get_format

static inline gint wx_gtk_selection_data_get_length(GtkSelectionData* selection_data)
{
    return selection_data->length;
}
#define gtk_selection_data_get_length wx_gtk_selection_data_get_length

static inline GdkAtom wx_gtk_selection_data_get_target(GtkSelectionData* selection_data)
{
    return selection_data->target;
}
#define gtk_selection_data_get_target wx_gtk_selection_data_get_target

static inline GdkWindow* wx_gtk_widget_get_window(GtkWidget* widget)
{
    return widget->window;
}
#define gtk_widget_get_window wx_gtk_widget_get_window

static inline GtkWidget* wx_gtk_window_get_default_widget(GtkWindow* window)
{
    return window->default_widget;
}
#define gtk_window_get_default_widget wx_gtk_window_get_default_widget

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.16

static inline GdkAtom wx_gtk_selection_data_get_selection(GtkSelectionData* selection_data)
{
    return selection_data->selection;
}
#define gtk_selection_data_get_selection wx_gtk_selection_data_get_selection

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.18

static inline void wx_gtk_cell_renderer_get_alignment(GtkCellRenderer* cell, gfloat* xalign, gfloat* yalign)
{
    *xalign = cell->xalign;
    *yalign = cell->yalign;
}
#define gtk_cell_renderer_get_alignment wx_gtk_cell_renderer_get_alignment

static inline void wx_gtk_cell_renderer_get_padding(GtkCellRenderer* cell, gint* xpad, gint* ypad)
{
    *xpad = cell->xpad;
    *ypad = cell->ypad;
}
#define gtk_cell_renderer_get_padding wx_gtk_cell_renderer_get_padding

static inline void wx_gtk_widget_get_allocation(GtkWidget* widget, GtkAllocation* allocation)
{
    *allocation = widget->allocation;
}
#define gtk_widget_get_allocation wx_gtk_widget_get_allocation

inline gboolean wx_gtk_widget_get_has_window(GtkWidget *widget)
{
    return !GTK_WIDGET_NO_WINDOW(widget);
}
#define gtk_widget_get_has_window wx_gtk_widget_get_has_window


inline gboolean wx_gtk_widget_get_has_grab(GtkWidget *widget)
{
    return GTK_WIDGET_HAS_GRAB(widget);
}
#define gtk_widget_get_has_grab wx_gtk_widget_get_has_grab


inline gboolean wx_gtk_widget_get_visible(GtkWidget *widget)
{
    return GTK_WIDGET_VISIBLE(widget);
}
#define gtk_widget_get_visible wx_gtk_widget_get_visible


inline gboolean wx_gtk_widget_get_sensitive(GtkWidget *widget)
{
    return GTK_WIDGET_SENSITIVE(widget);
}
#define gtk_widget_get_sensitive wx_gtk_widget_get_sensitive


inline gboolean wx_gtk_widget_is_drawable(GtkWidget *widget)
{
    return GTK_WIDGET_DRAWABLE(widget);
}
#define gtk_widget_is_drawable wx_gtk_widget_is_drawable


inline gboolean wx_gtk_widget_get_can_focus(GtkWidget *widget)
{
    return GTK_WIDGET_CAN_FOCUS(widget);
}
#define gtk_widget_get_can_focus wx_gtk_widget_get_can_focus

inline void wx_gtk_widget_set_can_focus(GtkWidget *widget, gboolean can)
{
    if ( can )
        GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
    else
        GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_FOCUS);
}
#define gtk_widget_set_can_focus wx_gtk_widget_set_can_focus


inline gboolean wx_gtk_widget_get_can_default(GtkWidget *widget)
{
    return GTK_WIDGET_CAN_DEFAULT(widget);
}
#define gtk_widget_get_can_default wx_gtk_widget_get_can_default

inline void wx_gtk_widget_set_can_default(GtkWidget *widget, gboolean can)
{
    if ( can )
        GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_DEFAULT);
    else
        GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_DEFAULT);
}
#define gtk_widget_set_can_default wx_gtk_widget_set_can_default


inline gboolean wx_gtk_widget_has_default(GtkWidget *widget)
{
    return GTK_WIDGET_HAS_DEFAULT(widget);
}
#define gtk_widget_has_default wx_gtk_widget_has_default


inline GtkStateType wx_gtk_widget_get_state(GtkWidget *widget)
{
    return (GtkStateType)GTK_WIDGET_STATE(widget);
}
#define gtk_widget_get_state wx_gtk_widget_get_state


inline gboolean wx_gtk_widget_get_double_buffered(GtkWidget *widget)
{
    return GTK_WIDGET_DOUBLE_BUFFERED(widget);
}
#define gtk_widget_get_double_buffered wx_gtk_widget_get_double_buffered

static inline gboolean wx_gtk_widget_has_grab(GtkWidget* widget)
{
    return GTK_WIDGET_HAS_GRAB(widget);
}
#define gtk_widget_has_grab wx_gtk_widget_has_grab

static inline void wx_gtk_widget_set_allocation(GtkWidget* widget, const GtkAllocation* allocation)
{
    widget->allocation = *allocation;
}
#define gtk_widget_set_allocation wx_gtk_widget_set_allocation

static inline gboolean wx_gtk_widget_is_toplevel(GtkWidget* widget)
{
    return GTK_WIDGET_TOPLEVEL(widget);
}
#define gtk_widget_is_toplevel wx_gtk_widget_is_toplevel

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.20

inline gboolean wx_gtk_widget_get_realized(GtkWidget *widget)
{
    return GTK_WIDGET_REALIZED(widget);
}
#define gtk_widget_get_realized wx_gtk_widget_get_realized


inline gboolean wx_gtk_widget_get_mapped(GtkWidget *widget)
{
    return GTK_WIDGET_MAPPED(widget);
}
#define gtk_widget_get_mapped wx_gtk_widget_get_mapped

static inline void wx_gtk_widget_get_requisition(GtkWidget* widget, GtkRequisition* requisition)
{
    *requisition = widget->requisition;
}
#define gtk_widget_get_requisition wx_gtk_widget_get_requisition

static inline GdkWindow* wx_gtk_entry_get_text_window(GtkEntry* entry)
{
    return entry->text_area;
}
#define gtk_entry_get_text_window wx_gtk_entry_get_text_window

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.22

static inline GdkWindow* wx_gtk_button_get_event_window(GtkButton* button)
{
    return button->event_window;
}
#define gtk_button_get_event_window wx_gtk_button_get_event_window

static inline GdkDragAction wx_gdk_drag_context_get_actions(GdkDragContext* context)
{
    return context->actions;
}
#define gdk_drag_context_get_actions wx_gdk_drag_context_get_actions

static inline GdkDragAction wx_gdk_drag_context_get_selected_action(GdkDragContext* context)
{
    return context->action;
}
#define gdk_drag_context_get_selected_action wx_gdk_drag_context_get_selected_action

static inline GdkDragAction wx_gdk_drag_context_get_suggested_action(GdkDragContext* context)
{
    return context->suggested_action;
}
#define gdk_drag_context_get_suggested_action wx_gdk_drag_context_get_suggested_action

static inline GList* wx_gdk_drag_context_list_targets(GdkDragContext* context)
{
    return context->targets;
}
#define gdk_drag_context_list_targets wx_gdk_drag_context_list_targets

static inline gint wx_gdk_visual_get_depth(GdkVisual* visual)
{
    return visual->depth;
}
#define gdk_visual_get_depth wx_gdk_visual_get_depth

static inline gboolean wx_gtk_window_has_group(GtkWindow* window)
{
    return window->group != NULL;
}
#define gtk_window_has_group wx_gtk_window_has_group

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 2.24

static inline GdkDisplay* wx_gdk_window_get_display(GdkWindow* window)
{
    return gdk_drawable_get_display(window);
}
#define gdk_window_get_display wx_gdk_window_get_display

static inline GdkScreen* wx_gdk_window_get_screen(GdkWindow* window)
{
    return gdk_drawable_get_screen(window);
}
#define gdk_window_get_screen wx_gdk_window_get_screen

static inline gint wx_gdk_window_get_height(GdkWindow* window)
{
    int h;
    gdk_drawable_get_size(window, NULL, &h);
    return h;
}
#define gdk_window_get_height wx_gdk_window_get_height

static inline gint wx_gdk_window_get_width(GdkWindow* window)
{
    int w;
    gdk_drawable_get_size(window, &w, NULL);
    return w;
}
#define gdk_window_get_width wx_gdk_window_get_width

#if GTK_CHECK_VERSION(2,10,0)
static inline void wx_gdk_cairo_set_source_window(cairo_t* cr, GdkWindow* window, gdouble x, gdouble y)
{
    gdk_cairo_set_source_pixmap(cr, window, x, y);
}
#define gdk_cairo_set_source_window wx_gdk_cairo_set_source_window
#endif

// ----------------------------------------------------------------------------
// the following were introduced in GTK+ 3.0

static inline void wx_gdk_window_get_geometry(GdkWindow* window, gint* x, gint* y, gint* width, gint* height)
{
    gdk_window_get_geometry(window, x, y, width, height, NULL);
}
#define gdk_window_get_geometry wx_gdk_window_get_geometry

static inline GtkWidget* wx_gtk_tree_view_column_get_button(GtkTreeViewColumn* tree_column)
{
    return tree_column->button;
}
#define gtk_tree_view_column_get_button wx_gtk_tree_view_column_get_button

static inline GtkWidget* wx_gtk_box_new(GtkOrientation orientation, gint spacing)
{
    GtkWidget* widget;
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        widget = gtk_hbox_new(false, spacing);
    else
        widget = gtk_vbox_new(false, spacing);
    return widget;
}
#define gtk_box_new wx_gtk_box_new

static inline GtkWidget* wx_gtk_button_box_new(GtkOrientation orientation)
{
    GtkWidget* widget;
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        widget = gtk_hbutton_box_new();
    else
        widget = gtk_vbutton_box_new();
    return widget;
}
#define gtk_button_box_new wx_gtk_button_box_new

static inline GtkWidget* wx_gtk_scale_new(GtkOrientation orientation, GtkAdjustment* adjustment)
{
    GtkWidget* widget;
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        widget = gtk_hscale_new(adjustment);
    else
        widget = gtk_vscale_new(adjustment);
    return widget;
}
#define gtk_scale_new wx_gtk_scale_new

static inline GtkWidget* wx_gtk_scrollbar_new(GtkOrientation orientation, GtkAdjustment* adjustment)
{
    GtkWidget* widget;
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        widget = gtk_hscrollbar_new(adjustment);
    else
        widget = gtk_vscrollbar_new(adjustment);
    return widget;
}
#define gtk_scrollbar_new wx_gtk_scrollbar_new

static inline GtkWidget* wx_gtk_separator_new(GtkOrientation orientation)
{
    GtkWidget* widget;
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        widget = gtk_hseparator_new();
    else
        widget = gtk_vseparator_new();
    return widget;
}
#define gtk_separator_new wx_gtk_separator_new

static inline void wx_gtk_widget_get_preferred_height(GtkWidget* widget, gint* minimum, gint* natural)
{
    GtkRequisition req;
    gtk_widget_size_request(widget, &req);
    if (minimum)
        *minimum = req.height;
    if (natural)
        *natural = req.height;
}
#define gtk_widget_get_preferred_height wx_gtk_widget_get_preferred_height

static inline void wx_gtk_widget_get_preferred_width(GtkWidget* widget, gint* minimum, gint* natural)
{
    GtkRequisition req;
    gtk_widget_size_request(widget, &req);
    if (minimum)
        *minimum = req.width;
    if (natural)
        *natural = req.width;
}
#define gtk_widget_get_preferred_width wx_gtk_widget_get_preferred_width

static inline void wx_gtk_widget_get_preferred_size(GtkWidget* widget, GtkRequisition* minimum, GtkRequisition* natural)
{
    GtkRequisition* req = minimum;
    if (req == NULL)
        req = natural;
    gtk_widget_size_request(widget, req);
}
#define gtk_widget_get_preferred_size wx_gtk_widget_get_preferred_size

#endif // !__WXGTK3__
#endif // _WX_GTK_PRIVATE_COMPAT_H_
