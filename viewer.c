#include "viewer.h"
#include <dirent.h>
#include <string.h>
#include <gio/gio.h>

extern GtkWidget *grid_container;
extern GtkWidget *page_label;
extern char view_folder[512];
extern char *image_files[10000];
extern int total_images;
extern int current_page;
extern int images_per_page;

GtkWidget *secondary_viewer_window = NULL;
GtkWidget *secondary_viewer_grid_container = NULL;
GtkWidget *secondary_viewer_page_label = NULL;
GtkWidget *secondary_viewer_vbox = NULL;
GtkWidget *secondary_viewer_scrolled = NULL;
int secondary_viewer_current_page = 0;
int secondary_viewer_images_per_page = 9;

int selected_image_index = -1;
int selected_images[10000];
int selected_count = 0;
gboolean show_only_selected = FALSE;

GtkCssProvider *global_css_provider = NULL;

void display_page(void);
void secondary_viewer_display_page(void);

gboolean is_image_selected(int index) {
    for (int i = 0; i < selected_count; i++) {
        if (selected_images[i] == index) {
            return TRUE;
        }
    }
    return FALSE;
}

void toggle_image_selection(int index) {
    for (int i = 0; i < selected_count; i++) {
        if (selected_images[i] == index) {
            // Rimuovi dalla selezione
            for (int j = i; j < selected_count - 1; j++) {
                selected_images[j] = selected_images[j + 1];
            }
            selected_count--;
            return;
        }
    }
    // Aggiungi alla selezione
    if (selected_count < 10000) {
        selected_images[selected_count] = index;
        selected_count++;
    }
}

void select_all_images(void) {
    selected_count = 0;
    for (int i = 0; i < total_images; i++) {
        selected_images[selected_count] = i;
        selected_count++;
    }
    current_page = 0;
    secondary_viewer_current_page = 0;
    display_page();
    if (secondary_viewer_window && gtk_widget_get_visible(secondary_viewer_window)) {
        secondary_viewer_display_page();
    }
}

void deselect_all_images(void) {
    selected_count = 0;
    current_page = 0;
    secondary_viewer_current_page = 0;
    display_page();
    if (secondary_viewer_window && gtk_widget_get_visible(secondary_viewer_window)) {
        secondary_viewer_display_page();
    }
}

void on_select_all(GtkButton *button, gpointer data) {
    select_all_images();
}

void on_deselect_all(GtkButton *button, gpointer data) {
    deselect_all_images();
}

void on_show_selected_only(GtkButton *button, gpointer data) {
    show_only_selected = TRUE;

    // Ripristina a pagina 0
    current_page = 0;
    secondary_viewer_current_page = 0;

    display_page();
    if (secondary_viewer_window && gtk_widget_get_visible(secondary_viewer_window)) {
        secondary_viewer_display_page();
    }
}

void on_show_all(GtkButton *button, gpointer data) {
    show_only_selected = FALSE;

    // Ripristina a pagina 0
    current_page = 0;
    secondary_viewer_current_page = 0;

    display_page();
    if (secondary_viewer_window && gtk_widget_get_visible(secondary_viewer_window)) {
        secondary_viewer_display_page();
    }
}

int get_filtered_image_index(int visible_index) {
    if (!show_only_selected) {
        return visible_index;
    }

    int count = 0;
    for (int i = 0; i < total_images; i++) {
        if (is_image_selected(i)) {
            if (count == visible_index) {
                return i;
            }
            count++;
        }
    }
    return -1;
}

int get_filtered_total_images(void) {
    if (!show_only_selected) {
        return total_images;
    }

    return selected_count;
}


void on_secondary_window_destroyed(GtkWidget *widget, gpointer data) {
    *(GtkWidget **)data = NULL;
}

gboolean on_secondary_viewer_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_F11) {
        static gboolean is_fullscreen = FALSE;
        if (is_fullscreen) {
            gtk_window_unfullscreen(GTK_WINDOW(secondary_viewer_window));
            is_fullscreen = FALSE;
        } else {
            gtk_window_fullscreen(GTK_WINDOW(secondary_viewer_window));
            is_fullscreen = TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

gboolean on_image_click(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    int image_index = GPOINTER_TO_INT(data);

    // Toggle selezione (aggiungi/togli)
    toggle_image_selection(image_index);
    selected_image_index = image_index;

    // In modalità "solo selezionate", non aggiornare automaticamente tutta la griglia
    // ma aggiorna solo il bordo della foto cliccata e il contatore
    if (show_only_selected) {
        // Ottieni il clip_box dall'event_box per aggiornare il bordo
        GtkWidget *clip_box = gtk_bin_get_child(GTK_BIN(widget));
        if (clip_box) {
            GtkStyleContext *context = gtk_widget_get_style_context(clip_box);
            if (is_image_selected(image_index)) {
                gtk_style_context_add_class(context, "image-cell-selected");
            } else {
                gtk_style_context_remove_class(context, "image-cell-selected");
            }
        }

        // Aggiorna il testo della paginazione con il nuovo conteggio
        int filtered_total = get_filtered_total_images();
        int total_pages = (filtered_total + images_per_page - 1) / images_per_page;
        char page_text[100];
        snprintf(page_text, sizeof(page_text), "Pagina %d/%d (%d immagini selezionate)",
                 current_page + 1, total_pages, filtered_total);
        gtk_label_set_text(GTK_LABEL(page_label), page_text);
    } else {
        display_page();
        if (secondary_viewer_window && gtk_widget_get_visible(secondary_viewer_window)) {
            secondary_viewer_display_page();
        }
    }

    return FALSE;
}


void secondary_viewer_display_page(void) {
    g_info("Displaying secondary viewer page %d", secondary_viewer_current_page);
    if (!secondary_viewer_grid_container) return; // Ensure grid container exists

    gtk_container_foreach(GTK_CONTAINER(secondary_viewer_grid_container),
                         (GtkCallback)gtk_widget_destroy, NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);

    // Pre-crea la struttura 3x3 con celle di dimensione fissa orizzontale ingrandite
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            GtkWidget *placeholder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_widget_set_size_request(placeholder, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
            gtk_grid_attach(GTK_GRID(grid), placeholder, col, row, 1, 1);
        }
    }

    int start_idx = secondary_viewer_current_page * secondary_viewer_images_per_page;
    int end_idx = start_idx + secondary_viewer_images_per_page;

    if (show_only_selected) {
        int visible_count = 0;  // Contatore per il posizionamento compatto
        int page_start = secondary_viewer_current_page * secondary_viewer_images_per_page;  // Prima immagine selezionata da mostrare

        // Salta le prime (page_start) immagini selezionate
        int images_skipped = 0;
        for (int i = 0; i < selected_count && visible_count < 9; i++) {
            if (images_skipped < page_start) {
                images_skipped++;
                continue;
            }

            int selected_idx = selected_images[i];  // Indice globale dell'immagine selezionata
            int col = visible_count % 3;
            int row = visible_count / 3;

            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[selected_idx], SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                // Contenitore con clipping per immagini - mantiene proporzioni cella fissa
                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Aggiungi bordo verde per immagini selezionate
                if (is_image_selected(selected_idx)) {
                    gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-selected");
                }

                gtk_container_add(GTK_CONTAINER(clip_box), image);
                gtk_container_add(GTK_CONTAINER(event_box), clip_box);
                gtk_grid_attach(GTK_GRID(grid), event_box, col, row, 1, 1);

                g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_image_click), GINT_TO_POINTER(selected_idx));
                gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);

                g_object_unref(pixbuf);
                visible_count++;
            } else {
                g_warning("Failed to load image file for secondary viewer '%s': %s", image_files[selected_idx], error->message);
                g_error_free(error);
            }
        }
    } else {
        if (end_idx > total_images) end_idx = total_images;

        int row = 0, col = 0;
        for (int i = start_idx; i < end_idx; i++) {
            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[i], SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                // Contenitore con clipping per immagini - mantiene proporzioni cella fissa
                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Aggiungi bordo verde per immagini selezionate
                if (is_image_selected(i)) {
                    gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-selected");
                }

                gtk_container_add(GTK_CONTAINER(clip_box), image);
                gtk_container_add(GTK_CONTAINER(event_box), clip_box);
                gtk_grid_attach(GTK_GRID(grid), event_box, col, row, 1, 1);

                g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_image_click), GINT_TO_POINTER(i));
                gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);

                g_object_unref(pixbuf);

                col++;
                if (col >= 3) {
                    col = 0;
                    row++;
                }
            } else {
                g_warning("Failed to load image file for secondary viewer '%s': %s", image_files[i], error->message);
                g_error_free(error);
            }
        }
    }

    gtk_container_add(GTK_CONTAINER(secondary_viewer_grid_container), grid);

    int filtered_total = get_filtered_total_images();
    int total_pages = (filtered_total + secondary_viewer_images_per_page - 1) / secondary_viewer_images_per_page;
    char page_text[100];
    snprintf(page_text, sizeof(page_text), "Pagina %d/%d (%d immagini%s)",
             secondary_viewer_current_page + 1, total_pages, filtered_total,
             show_only_selected ? " selezionate" : "");
    if (secondary_viewer_page_label) {
        gtk_label_set_text(GTK_LABEL(secondary_viewer_page_label), page_text);
    }
    gtk_widget_show_all(secondary_viewer_grid_container);
}


void on_secondary_viewer_next_page(GtkButton *button, gpointer data) {
    int filtered_total = get_filtered_total_images();
    int total_pages = (filtered_total + secondary_viewer_images_per_page - 1) / secondary_viewer_images_per_page;
    if (secondary_viewer_current_page < total_pages - 1) {
        secondary_viewer_current_page++;
        secondary_viewer_display_page();
        if (gtk_widget_get_visible(grid_container)) { // Check if main grid is visible
            current_page = secondary_viewer_current_page;
            display_page();
        }
    }
}

void on_secondary_viewer_prev_page(GtkButton *button, gpointer data) {
    if (secondary_viewer_current_page > 0) {
        secondary_viewer_current_page--;
        secondary_viewer_display_page();
        if (gtk_widget_get_visible(grid_container)) { // Check if main grid is visible
            current_page = secondary_viewer_current_page;
            display_page();
        }
    }
}

GtkWidget *create_secondary_viewer_window(void) {
    secondary_viewer_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(secondary_viewer_window), "Secondary Viewer - Photos Only");
    gtk_window_set_default_size(GTK_WINDOW(secondary_viewer_window), 1200, 850);
    gtk_window_set_position(GTK_WINDOW(secondary_viewer_window), GTK_WIN_POS_CENTER_ALWAYS);

    secondary_viewer_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(secondary_viewer_vbox), "secondary-viewer");
    gtk_container_add(GTK_CONTAINER(secondary_viewer_window), secondary_viewer_vbox);
    gtk_widget_set_hexpand(secondary_viewer_vbox, TRUE);
    gtk_widget_set_vexpand(secondary_viewer_vbox, TRUE);

    secondary_viewer_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(secondary_viewer_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_box_pack_start(GTK_BOX(secondary_viewer_vbox), secondary_viewer_scrolled, TRUE, TRUE, 0);

    secondary_viewer_grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(secondary_viewer_scrolled), secondary_viewer_grid_container);
    gtk_widget_set_hexpand(secondary_viewer_grid_container, TRUE);
    gtk_widget_set_vexpand(secondary_viewer_grid_container, TRUE);

    g_signal_connect(secondary_viewer_window, "destroy", G_CALLBACK(on_secondary_window_destroyed), &secondary_viewer_window);
    g_signal_connect(secondary_viewer_window, "key-press-event", G_CALLBACK(on_secondary_viewer_key_press), NULL);

    return secondary_viewer_window;
}

void on_select_view_folder(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Seleziona cartella da visualizzare",
        GTK_WINDOW(data),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Annulla", GTK_RESPONSE_CANCEL,
        "Seleziona", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        strncpy(view_folder, folder, sizeof(view_folder) - 1);
        g_free(folder);

        // Auto-load images from selected folder
        load_images_list();
        display_page();
    }

    gtk_widget_destroy(dialog);
}

void load_images_list(void) {
    DIR *dir;
    struct dirent *entry;
    char file_path[2048];
    const char *ext;
    int i;

    if (strlen(view_folder) == 0) {
        return;
    }

    g_info("Loading images from '%s'", view_folder);

    // Pulisci lista precedente
    for (i = 0; i < total_images; i++) {
        g_free(image_files[i]);
    }
    total_images = 0;
    current_page = 0;

    dir = opendir(view_folder);
    if (!dir) {
        g_warning("Failed to open view directory: %s", view_folder);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        snprintf(file_path, sizeof(file_path), "%s/%s", view_folder, entry->d_name);

        /* Check file extension - supporta JPG, JPEG, PNG, BMP, GIF, TIFF */
        ext = strrchr(entry->d_name, '.');
        if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 ||
                    g_ascii_strcasecmp(ext, ".jpeg") == 0 ||
                    g_ascii_strcasecmp(ext, ".png") == 0 ||
                    g_ascii_strcasecmp(ext, ".bmp") == 0 ||
                    g_ascii_strcasecmp(ext, ".gif") == 0 ||
                    g_ascii_strcasecmp(ext, ".tiff") == 0 ||
                    g_ascii_strcasecmp(ext, ".tif") == 0)) {
            image_files[total_images] = g_strdup(file_path);
            total_images++;
        }
    }

    closedir(dir);
    g_info("Found %d images", total_images);
    if (secondary_viewer_window) {
        secondary_viewer_current_page = 0;
        secondary_viewer_display_page();
    }
}

void display_page(void) {
    g_info("Displaying page %d", current_page);

    // Pulisci grid precedente
    gtk_container_foreach(GTK_CONTAINER(grid_container),
                         (GtkCallback)gtk_widget_destroy, NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 30);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 15);

    // Pre-crea la struttura 3x3 con celle di dimensione fissa orizzontale
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            GtkWidget *placeholder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_widget_set_size_request(placeholder, CELL_WIDTH, CELL_HEIGHT);
            gtk_grid_attach(GTK_GRID(grid), placeholder, col, row, 1, 1);
        }
    }

    int start_idx = current_page * images_per_page;
    int end_idx = start_idx + images_per_page;

    // Se filtriamo solo le selezionate, compatta le immagini da sinistra-alto
    if (show_only_selected) {
        int visible_count = 0;  // Contatore per il posizionamento compatto
        int page_start = current_page * images_per_page;  // Prima immagine selezionata da mostrare

        // Salta le prime (page_start) immagini selezionate
        int images_skipped = 0;
        for (int i = 0; i < selected_count && visible_count < 9; i++) {
            if (images_skipped < page_start) {
                images_skipped++;
                continue;
            }

            int selected_idx = selected_images[i];  // Indice globale dell'immagine selezionata
            int col = visible_count % 3;
            int row = visible_count / 3;

            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[selected_idx], SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                // Contenitore con clipping per immagini - mantiene proporzioni cella fissa
                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Aggiungi bordo verde per immagini selezionate
                if (is_image_selected(selected_idx)) {
                    gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-selected");
                }

                gtk_container_add(GTK_CONTAINER(clip_box), image);
                gtk_container_add(GTK_CONTAINER(event_box), clip_box);
                gtk_grid_attach(GTK_GRID(grid), event_box, col, row, 1, 1);

                g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_image_click), GINT_TO_POINTER(selected_idx));
                gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);

                g_object_unref(pixbuf);
                visible_count++;
            } else {
                g_warning("Failed to load image file '%s': %s", image_files[selected_idx], error->message);
                g_error_free(error);
            }
        }
    } else {
        // Modalità "All" - mostra tutte le immagini
        if (end_idx > total_images) end_idx = total_images;

        int row = 0, col = 0;
        for (int i = start_idx; i < end_idx; i++) {
            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[i], SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                // Contenitore con clipping per immagini - mantiene proporzioni cella fissa
                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Aggiungi bordo verde per immagini selezionate
                if (is_image_selected(i)) {
                    gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-selected");
                }

                gtk_container_add(GTK_CONTAINER(clip_box), image);
                gtk_container_add(GTK_CONTAINER(event_box), clip_box);
                gtk_grid_attach(GTK_GRID(grid), event_box, col, row, 1, 1);

                g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_image_click), GINT_TO_POINTER(i));
                gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK);

                g_object_unref(pixbuf);

                col++;
                if (col >= 3) {
                    col = 0;
                    row++;
                }
            } else {
                g_warning("Failed to load image file '%s': %s", image_files[i], error->message);
                g_error_free(error);
            }
        }

    }

    gtk_container_add(GTK_CONTAINER(grid_container), grid);

    // Aggiorna label pagina
    int filtered_total = get_filtered_total_images();
    int total_pages = (filtered_total + images_per_page - 1) / images_per_page;
    char page_text[100];
    snprintf(page_text, sizeof(page_text), "Pagina %d/%d (%d immagini%s)",
             current_page + 1, total_pages, filtered_total,
             show_only_selected ? " selezionate" : "");
    gtk_label_set_text(GTK_LABEL(page_label), page_text);

    gtk_widget_show_all(grid_container);
}

void on_next_page(GtkButton *button, gpointer data) {
    int filtered_total = get_filtered_total_images();
    int total_pages = (filtered_total + images_per_page - 1) / images_per_page;
    if (current_page < total_pages - 1) {
        current_page++;
        display_page();
        if (secondary_viewer_window) {
            secondary_viewer_current_page = current_page;
            secondary_viewer_display_page();
        }
    }
}

void on_prev_page(GtkButton *button, gpointer data) {
    if (current_page > 0) {
        current_page--;
        display_page();
        if (secondary_viewer_window) {
            secondary_viewer_current_page = current_page;
            secondary_viewer_display_page();
        }
    }
}

void on_load_images(GtkButton *button, gpointer data) {
    load_images_list();
    display_page();
}
