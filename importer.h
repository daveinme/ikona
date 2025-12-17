#ifndef IMPORTER_H
#define IMPORTER_H

#include <gtk/gtk.h>

void on_select_src_folder(GtkButton *button, gpointer data);
void on_select_dst_folder(GtkButton *button, gpointer data);
void on_copy_jpg_auto(GtkButton *button, gpointer data);
void on_scan_sd_for_auto(GtkButton *button, gpointer data);

#endif // IMPORTER_H
