#ifndef VIEWER_H
#define VIEWER_H

#include <gtk/gtk.h>

#define CELL_WIDTH 400
#define CELL_HEIGHT 250
#define SECONDARY_CELL_WIDTH 520
#define SECONDARY_CELL_HEIGHT 325
#define THUMBNAIL_SIZE 800

extern gboolean show_only_selected;
extern GtkCssProvider *global_css_provider;
extern int selected_images[10000];
extern int selected_count;

void on_select_view_folder(GtkButton *button, gpointer data);
void on_load_images(GtkButton *button, gpointer data);
void on_next_page(GtkButton *button, gpointer data);
void on_prev_page(GtkButton *button, gpointer data);
void on_toggle_filter(GtkButton *button, gpointer data);
void on_select_all(GtkButton *button, gpointer data);
void on_deselect_all(GtkButton *button, gpointer data);
void load_images_list(void);
void display_page(void);

#endif // VIEWER_H
