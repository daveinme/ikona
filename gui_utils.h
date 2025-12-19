#ifndef GUI_UTILS_H
#define GUI_UTILS_H

#include <gtk/gtk.h>

/* Helper function to create icon button from file path */
GtkWidget* create_icon_button(const char *icon_path, const char *label);

#endif // GUI_UTILS_H
