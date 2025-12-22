#include "viewer.h"
#include "utils.h"
#include <dirent.h>
#include <string.h>
#include <gio/gio.h>

extern GtkWidget *grid_container;
extern GtkWidget *page_label;
extern char view_folder[512];
extern char *image_files[10000];
extern char *original_files[10000];
extern int total_images;
extern int current_page;
extern int images_per_page;

// Dimensioni celle griglia (calcolate in base allo schermo)
int CELL_WIDTH = 400;
int CELL_HEIGHT = 250;
int SECONDARY_CELL_WIDTH = 400;   // Default, will be calculated dynamically
int SECONDARY_CELL_HEIGHT = 250;  // Default, will be calculated dynamically

GtkWidget *secondary_viewer_window = NULL;
GtkWidget *secondary_viewer_grid_container = NULL;
GtkWidget *secondary_viewer_page_label = NULL;
GtkWidget *secondary_viewer_vbox = NULL;
GtkWidget *secondary_viewer_scrolled = NULL;
int secondary_viewer_current_page = 0;
int secondary_viewer_images_per_page = 9;

int selected_image_index = -1;

/* Forward declaration */
static void calculate_secondary_cell_dimensions(void);
int selected_images[10000];
int selected_count = 0;
gboolean show_only_selected = FALSE;

// Lista "frozen" per filtro "Solo Selezionate" - si aggiorna solo quando ripremi il pulsante filtro
int frozen_filter_images[10000];  // Indici delle foto da mostrare quando filtro è attivo
int frozen_filter_count = 0;      // Numero di foto nel filtro frozen

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

    // Popola la lista frozen con le foto ATTUALMENTE selezionate
    // Questa lista non si aggiorna fino a quando non ripremi questo pulsante
    frozen_filter_count = 0;
    for (int i = 0; i < selected_count; i++) {
        frozen_filter_images[frozen_filter_count] = selected_images[i];
        frozen_filter_count++;
    }

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

    // Quando filtro è attivo, usa la lista frozen (non selected_count che può cambiare)
    return frozen_filter_count;
}


void on_secondary_window_destroyed(GtkWidget *widget, gpointer data) {
    *(GtkWidget **)data = NULL;
}

/* Window resize handler - recalculate cell dimensions and refresh grid */
void on_secondary_window_resized(GtkWidget *widget, GtkAllocation *allocation, gpointer data) {
    static int last_width = 0;

    // Only recalculate if width changed significantly (avoid too many recalculations)
    if (allocation->width > 0 && abs(allocation->width - last_width) > 50) {
        last_width = allocation->width;
        calculate_secondary_cell_dimensions();
        secondary_viewer_display_page();  // Refresh grid with new dimensions
        g_debug("Secondary window resized to %dpx width", allocation->width);
    }
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

    // Aggiorna entrambi i monitor ricreando le griglie
    // (non modificare i bordi direttamente - il refresh garantisce coerenza)
    display_page();
    if (secondary_viewer_window && gtk_widget_get_visible(secondary_viewer_window)) {
        secondary_viewer_display_page();
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

    int row = 0, col = 0;

    if (show_only_selected) {
        /* Modalità filtro: mostra foto dalla lista FROZEN
           Lista si aggiorna SOLO quando ripremi "Solo Selezionate"
           Bordino si aggiorna in tempo reale controllando is_image_selected() */

        int start_idx = secondary_viewer_current_page * secondary_viewer_images_per_page;
        int end_idx = start_idx + secondary_viewer_images_per_page;
        if (end_idx > frozen_filter_count) end_idx = frozen_filter_count;

        for (int j = start_idx; j < end_idx; j++) {
            int i = frozen_filter_images[j];  // Indice reale in image_files[]

            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[i], SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Bordino verde SOLO per foto attualmente selezionate (aggiornamento real-time)
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
    } else {
        /* Modalità normale: mostra tutte le foto */
        int start_idx = secondary_viewer_current_page * secondary_viewer_images_per_page;
        int end_idx = start_idx + secondary_viewer_images_per_page;
        if (end_idx > total_images) end_idx = total_images;

        for (int i = start_idx; i < end_idx; i++) {
            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[i], SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Bordino verde SOLO per immagini selezionate
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

/* Calculate optimal cell dimensions based on WINDOW width (responsive, works on all monitors) */
static void calculate_secondary_cell_dimensions(void) {
    if (!secondary_viewer_window) {
        g_warning("Secondary viewer window not initialized yet");
        return;
    }

    // Get ACTUAL window width (not screen width) - this is the key to responsiveness
    int window_width = gtk_widget_get_allocated_width(secondary_viewer_window);

    // Fallback if window not yet allocated
    if (window_width <= 1) {
        window_width = 1200;  // Default window size fallback
    }

    // 3 columns layout with spacing
    // Margins: 40px left/right border + scrollbar space (15px)
    int num_columns = 3;
    int total_spacing = 40 + 15;  // Left/right margins + scrollbar
    int grid_gaps = (num_columns - 1) * 20;  // 20px between columns

    int available_width = window_width - total_spacing - grid_gaps;

    // Calculate cell width: use 70% of available width divided by 3 columns
    // This formula scales automatically from 19" to 50" monitors
    SECONDARY_CELL_WIDTH = (available_width * 80 ) / (100 * num_columns);

    // Maintain aspect ratio 3:2 for photos (landscape format)
    SECONDARY_CELL_HEIGHT = (SECONDARY_CELL_WIDTH * 2) / 3;

    // Ensure minimum size (no fixed cap - scales naturally to any monitor)
    if (SECONDARY_CELL_WIDTH < 120) SECONDARY_CELL_WIDTH = 120;
    SECONDARY_CELL_HEIGHT = (SECONDARY_CELL_WIDTH * 2) / 3;  // Re-calculate after minimum

    g_info("Secondary viewer: Window %dpx width → Cell size: %dx%d",
           window_width, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT);
}

GtkWidget *create_secondary_viewer_window(void) {
    secondary_viewer_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(secondary_viewer_window), "Secondary Viewer - Photos Only");
    gtk_window_set_default_size(GTK_WINDOW(secondary_viewer_window), 1200, 850);
    gtk_window_set_position(GTK_WINDOW(secondary_viewer_window), GTK_WIN_POS_CENTER_ALWAYS);

    /* Recalculate cell dimensions when window is resized (responsive design) */
    g_signal_connect(secondary_viewer_window, "size-allocate",
                     G_CALLBACK(on_secondary_window_resized), NULL);

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

// Helper per aggiornare la lista importazioni in base alla data selezionata
static void update_imports_list_by_date(GtkCalendar *calendar, GtkWidget *list_box) {
    guint year, month, day;
    char selected_date[64];

    gtk_calendar_get_date(calendar, &year, &month, &day);
    snprintf(selected_date, sizeof(selected_date), "%04d-%02d-%02d", year, month + 1, day);

    // Pulisci lista esistente
    gtk_container_foreach(GTK_CONTAINER(list_box), (GtkCallback)gtk_widget_destroy, NULL);

    char *thumbs_base = get_thumbnails_base_path();
    char *date_path = g_build_filename(thumbs_base, selected_date, NULL);

    // Verifica se esiste la cartella per questa data
    if (!g_file_test(date_path, G_FILE_TEST_IS_DIR)) {
        GtkWidget *no_imports = gtk_label_new("Nessuna importazione in questa data");
        gtk_widget_set_margin_top(no_imports, 20);
        gtk_container_add(GTK_CONTAINER(list_box), no_imports);
        gtk_widget_show(no_imports);
        g_free(date_path);
        return;
    }

    // Scansiona sottocartelle (001, 002, ecc.)
    GDir *import_dir = g_dir_open(date_path, 0, NULL);
    if (import_dir) {
        const char *import_num;
        while ((import_num = g_dir_read_name(import_dir)) != NULL) {
            if (import_num[0] == '.') continue;

            char *full_path = g_build_filename(date_path, import_num, NULL);

            if (!g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
                g_free(full_path);
                continue;
            }

            int photo_count = count_images_in_folder(full_path);

            char label[256];
            snprintf(label, sizeof(label), "📁 Import #%s (%d foto)", import_num, photo_count);

            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *label_widget = gtk_label_new(label);
            gtk_label_set_xalign(GTK_LABEL(label_widget), 0.0);
            gtk_widget_set_margin_start(label_widget, 10);
            gtk_widget_set_margin_end(label_widget, 10);
            gtk_widget_set_margin_top(label_widget, 8);
            gtk_widget_set_margin_bottom(label_widget, 8);
            gtk_container_add(GTK_CONTAINER(row), label_widget);

            g_object_set_data_full(G_OBJECT(row), "folder_path", g_strdup(full_path), g_free);

            gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);
            gtk_widget_show_all(row);
            g_free(full_path);
        }
        g_dir_close(import_dir);
    }
    g_free(date_path);
}

void on_select_view_folder(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Seleziona Foto Importate",
        GTK_WINDOW(data),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Annulla", GTK_RESPONSE_CANCEL,
        "Apri", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 450);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_hbox), 10);
    gtk_container_add(GTK_CONTAINER(content), main_hbox);

    // SINISTRA: Calendario
    GtkWidget *calendar_frame = gtk_frame_new("Seleziona Data");
    gtk_widget_set_size_request(calendar_frame, 280, -1);
    GtkWidget *calendar = gtk_calendar_new();
    gtk_container_add(GTK_CONTAINER(calendar_frame), calendar);
    gtk_box_pack_start(GTK_BOX(main_hbox), calendar_frame, FALSE, FALSE, 0);

    // DESTRA: Lista importazioni
    GtkWidget *list_frame = gtk_frame_new("Importazioni");
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_SINGLE);
    gtk_container_add(GTK_CONTAINER(scrolled), list_box);
    gtk_container_add(GTK_CONTAINER(list_frame), scrolled);
    gtk_box_pack_start(GTK_BOX(main_hbox), list_frame, TRUE, TRUE, 0);

    // Inizializza con data odierna
    update_imports_list_by_date(GTK_CALENDAR(calendar), list_box);

    // Callback per cambio data
    g_signal_connect(calendar, "day-selected",
                     G_CALLBACK(update_imports_list_by_date), list_box);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkListBoxRow *selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
        if (selected) {
            const char *folder_path = g_object_get_data(G_OBJECT(selected), "folder_path");
            if (folder_path) {
                strncpy(view_folder, folder_path, sizeof(view_folder) - 1);
                load_images_list();
                display_page();
            }
        }
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
        g_free(original_files[i]);  // NUOVO: cleanup array parallelo
    }
    total_images = 0;
    current_page = 0;

    // Reset stato selezioni quando si cambia cartella
    selected_count = 0;
    show_only_selected = FALSE;

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
            image_files[total_images] = g_strdup(file_path);  // Path thumbnail
            // NUOVO: Mappa thumbnail -> originale
            char *original_path = thumb_to_original_path(file_path);
            if (original_path) {
                original_files[total_images] = g_strdup(original_path);
            } else {
                // Fallback: se mapping fallisce, usa stesso path (compatibilità)
                original_files[total_images] = g_strdup(file_path);
            }
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

    int row = 0, col = 0;

    if (show_only_selected) {
        /* MONITOR PRINCIPALE - Modalità filtro: usa lista FROZEN
           Sincronizzato con monitor secondario (stessa sorgente dati) */

        int start_idx = current_page * images_per_page;
        int end_idx = start_idx + images_per_page;
        if (end_idx > frozen_filter_count) end_idx = frozen_filter_count;

        for (int j = start_idx; j < end_idx; j++) {
            int i = frozen_filter_images[j];  // Indice reale in image_files[]

            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[i], CELL_WIDTH, CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, CELL_WIDTH, CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Bordino verde SOLO per foto attualmente selezionate (aggiornamento real-time)
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
    } else {
        /* MONITOR PRINCIPALE - Modalità normale: tutte le foto */
        int start_idx = current_page * images_per_page;
        int end_idx = start_idx + images_per_page;
        if (end_idx > total_images) end_idx = total_images;

        for (int i = start_idx; i < end_idx; i++) {
            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(image_files[i], CELL_WIDTH, CELL_HEIGHT, TRUE, &error);
            if (pixbuf) {
                GtkWidget *event_box = gtk_event_box_new();
                gtk_event_box_set_above_child(GTK_EVENT_BOX(event_box), FALSE);

                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(image, GTK_ALIGN_CENTER);

                GtkWidget *clip_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(clip_box, CELL_WIDTH, CELL_HEIGHT);
                gtk_style_context_add_class(gtk_widget_get_style_context(clip_box), "image-cell-clip");

                // Bordino verde SOLO per immagini selezionate
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
