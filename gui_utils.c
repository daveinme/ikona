#include "gui_utils.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

GtkWidget* create_icon_button(const char *icon_path, const char *label) {
    GtkWidget *button;
    GtkWidget *box;
    GtkWidget *image = NULL;
    GtkWidget *label_widget;
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    button = gtk_button_new();
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(box), 5);

    /* Try to load icon from file */
    if (icon_path && g_file_test(icon_path, G_FILE_TEST_EXISTS)) {
        pixbuf = gdk_pixbuf_new_from_file_at_scale(icon_path, 20, 20, TRUE, &error);
        if (pixbuf) {
            image = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(pixbuf);
        } else {
            if (error) {
                g_error_free(error);
            }
        }
    }

    /* Add image if loaded, otherwise skip */
    if (image) {
        gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
    }

    /* Add label */
    label_widget = gtk_label_new(label);
    gtk_box_pack_start(GTK_BOX(box), label_widget, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(button), box);
    gtk_widget_show_all(button);

    return button;
}
