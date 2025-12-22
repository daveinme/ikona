#ifndef VIEWER_H
#define VIEWER_H

#include <gtk/gtk.h>

#define THUMBNAIL_SIZE 800

extern int CELL_WIDTH;
extern int CELL_HEIGHT;
extern int SECONDARY_CELL_WIDTH;
extern int SECONDARY_CELL_HEIGHT;

extern gboolean show_only_selected;
extern GtkCssProvider *global_css_provider;
extern int selected_images[10000];
extern int selected_count;
extern int selected_image_index; // Added
extern char *image_files[10000]; // Added

void on_select_view_folder(GtkButton *button, gpointer data);
void on_load_images(GtkButton *button, gpointer data);
void on_next_page(GtkButton *button, gpointer data);
void on_prev_page(GtkButton *button, gpointer data);
void on_show_selected_only(GtkButton *button, gpointer data);
void on_show_all(GtkButton *button, gpointer data);
void on_select_all(GtkButton *button, gpointer data);
void on_deselect_all(GtkButton *button, gpointer data);
void load_images_list(void);
void display_page(void);

#endif // VIEWER_H
