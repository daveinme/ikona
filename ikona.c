#include "ikona.h"
#include "importer.h"
#include "viewer.h"
#include "printer.h"

GtkWidget *label_copy_status;
GtkWidget *grid_container;
GtkWidget *page_label;
char src_folder[512] = "";
char base_dst_folder[512] = "";
char view_folder[512] = "";
char *image_files[10000];
int total_images = 0;
int current_page = 0;
int images_per_page = 9;

GList *print_list = NULL;
GtkWidget *print_list_box = NULL;
GtkWidget *printer_combo_box = NULL;
Printer *available_printers = NULL;
int printer_count = 0;

GtkWidget *global_stack = NULL;  // Stack per switchare tra tab


GtkWidget *create_view_page(GtkWidget *window);
GtkWidget *create_print_page();
GtkWidget *create_config_page(GtkWidget *window);

// Helper function to create icon buttons
GtkWidget *create_icon_button(const char *icon, const char *label) {
    GtkWidget *btn = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(box), 2);

    GtkWidget *icon_label = gtk_label_new(icon);
    gtk_label_set_markup(GTK_LABEL(icon_label), g_strdup_printf("<big>%s</big>", icon));

    GtkWidget *text_label = gtk_label_new(label);
    gtk_label_set_line_wrap(GTK_LABEL(text_label), TRUE);
    gtk_label_set_justify(GTK_LABEL(text_label), GTK_JUSTIFY_CENTER);

    gtk_box_pack_start(GTK_BOX(box), icon_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text_label, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(btn), box);
    return btn;
}

void on_add_to_print_list(GtkButton *button, gpointer data);
void on_remove_from_print_list(GtkButton *button, gpointer data);
void on_print_selected(GtkButton *button, gpointer data);

void on_drive_connected(GVolumeMonitor *monitor, GDrive *drive);
void on_drive_disconnected(GVolumeMonitor *monitor, GDrive *drive);

extern GtkWidget *secondary_viewer_window;
extern void secondary_viewer_display_page(void);
extern GtkWidget *create_secondary_viewer_window(void);

void on_open_secondary_viewer(GtkButton *button, gpointer data) {
    if (!secondary_viewer_window) {
        create_secondary_viewer_window();
    }
    secondary_viewer_display_page(); // Ensure it displays current images
    gtk_widget_show_all(secondary_viewer_window);
    gtk_window_present(GTK_WINDOW(secondary_viewer_window));
}

void *import_thread(void *data) {
    on_copy_jpg_auto(NULL, NULL);
    gtk_spinner_stop(GTK_SPINNER(data));
    return NULL;
}

void on_import_button_clicked(GtkButton *button, gpointer data) {
    gtk_spinner_start(GTK_SPINNER(data));
    g_thread_new("import", import_thread, data);
}


void *load_images_thread(void *data) {
    on_load_images(NULL, NULL);
    gtk_spinner_stop(GTK_SPINNER(data));
    return NULL;
}

void on_load_images_clicked(GtkButton *button, gpointer data) {
    gtk_spinner_start(GTK_SPINNER(data));
    g_thread_new("load-images", load_images_thread, data);
}


GtkWidget *create_config_page(GtkWidget *window) {
    GtkWidget *grid, *label_title, *label_src, *label_dst, *label_view, *btn_src, *btn_dst, *btn_view;
    GtkWidget *entry_src, *entry_dst, *entry_view;

    grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    label_title = gtk_label_new("Configurazione");
    gtk_grid_attach(GTK_GRID(grid), label_title, 0, 0, 2, 1);

    // Source folder
    label_src = gtk_label_new("Cartella Sorgente (Importa da):");
    gtk_grid_attach(GTK_GRID(grid), label_src, 0, 1, 2, 1);

    entry_src = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_src), src_folder);
    gtk_widget_set_sensitive(entry_src, FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry_src, 0, 2, 1, 1);

    btn_src = gtk_button_new_with_label("Sfoglia");
    g_signal_connect(btn_src, "clicked", G_CALLBACK(on_select_src_folder), window);
    gtk_grid_attach(GTK_GRID(grid), btn_src, 1, 2, 1, 1);

    // Destination folder
    label_dst = gtk_label_new("Cartella Destinazione (Salva in):");
    gtk_grid_attach(GTK_GRID(grid), label_dst, 0, 3, 2, 1);

    entry_dst = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_dst), base_dst_folder);
    gtk_widget_set_sensitive(entry_dst, FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry_dst, 0, 4, 1, 1);

    btn_dst = gtk_button_new_with_label("Sfoglia");
    g_signal_connect(btn_dst, "clicked", G_CALLBACK(on_select_dst_folder), window);
    gtk_grid_attach(GTK_GRID(grid), btn_dst, 1, 4, 1, 1);

    // View folder
    label_view = gtk_label_new("Cartella Visualizzazione (Libreria Foto):");
    gtk_grid_attach(GTK_GRID(grid), label_view, 0, 5, 2, 1);

    entry_view = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_view), view_folder);
    gtk_widget_set_sensitive(entry_view, FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry_view, 0, 6, 1, 1);

    btn_view = gtk_button_new_with_label("Sfoglia");
    g_signal_connect(btn_view, "clicked", G_CALLBACK(on_select_view_folder), window);
    gtk_grid_attach(GTK_GRID(grid), btn_view, 1, 6, 1, 1);

    return grid;
}


GtkWidget *create_import_page() {
    GtkWidget *grid, *label_title, *btn_src, *btn_dst, *btn_copy;
    GtkSpinner *spinner;

    grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    label_title = gtk_label_new("Importazione Automatica JPG");
    gtk_grid_attach(GTK_GRID(grid), label_title, 0, 0, 2, 1);

    btn_src = gtk_button_new_with_label("Seleziona Cartella Sorgente");
    g_signal_connect(btn_src, "clicked", G_CALLBACK(on_select_src_folder), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_src, 0, 1, 1, 1);

    btn_dst = gtk_button_new_with_label("Seleziona Cartella Base Destinazione");
    g_signal_connect(btn_dst, "clicked", G_CALLBACK(on_select_dst_folder), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_dst, 1, 1, 1, 1);

    spinner = GTK_SPINNER(gtk_spinner_new());
    btn_copy = gtk_button_new_with_label("Importa JPG");
    g_signal_connect(btn_copy, "clicked", G_CALLBACK(on_import_button_clicked), spinner);
    gtk_grid_attach(GTK_GRID(grid), btn_copy, 0, 2, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(spinner), 0, 3, 1, 1);

    label_copy_status = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), label_copy_status, 1, 3, 1, 1);

    return grid;
}


GtkWidget *create_view_page(GtkWidget *window) {
    GtkWidget *main_box, *top_box, *content_box, *sidebar, *bottom_box;
    GtkWidget *btn_view, *btn_all, *btn_selected, *btn_open_secondary;
    GtkWidget *btn_select_all, *btn_deselect_all, *btn_print;
    GtkWidget *scrolled, *btn_prev, *btn_next, *separator;

    // Main vertical box
    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 5);

    // Top bar (compact): Folder, All, Sel, Secondary viewer
    top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(top_box), 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(top_box), "top-bar");

    btn_view = gtk_button_new_with_label("📁 Cartella");
    gtk_widget_set_size_request(btn_view, 75, 32);
    g_signal_connect(btn_view, "clicked", G_CALLBACK(on_select_view_folder), window);
    gtk_box_pack_start(GTK_BOX(top_box), btn_view, FALSE, FALSE, 0);

    btn_all = gtk_button_new_with_label("☑ Tutti");
    gtk_widget_set_size_request(btn_all, 55, 32);
    g_signal_connect(btn_all, "clicked", G_CALLBACK(on_toggle_filter), NULL);
    gtk_box_pack_start(GTK_BOX(top_box), btn_all, FALSE, FALSE, 0);

    btn_selected = gtk_button_new_with_label("✓ Sel");
    gtk_widget_set_size_request(btn_selected, 55, 32);
    g_signal_connect(btn_selected, "clicked", G_CALLBACK(on_toggle_filter), NULL);
    gtk_box_pack_start(GTK_BOX(top_box), btn_selected, FALSE, FALSE, 0);

    btn_open_secondary = gtk_button_new_with_label("⬥ Confronto");
    gtk_widget_set_size_request(btn_open_secondary, 85, 32);
    g_signal_connect(btn_open_secondary, "clicked", G_CALLBACK(on_open_secondary_viewer), NULL);
    gtk_box_pack_end(GTK_BOX(top_box), btn_open_secondary, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), top_box, FALSE, FALSE, 0);

    // Content: Sidebar + Grid
    content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // Left sidebar (actions)
    sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(sidebar, 130, -1);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    btn_select_all = gtk_button_new_with_label("✓ Seleziona\nTutti");
    gtk_widget_set_size_request(btn_select_all, 110, 50);
    g_signal_connect(btn_select_all, "clicked", G_CALLBACK(on_select_all), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_select_all, FALSE, FALSE, 0);

    btn_deselect_all = gtk_button_new_with_label("⊘ Deseleziona\nTutti");
    gtk_widget_set_size_request(btn_deselect_all, 110, 50);
    g_signal_connect(btn_deselect_all, "clicked", G_CALLBACK(on_deselect_all), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_deselect_all, FALSE, FALSE, 0);

    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(sidebar), separator, FALSE, FALSE, 0);

    btn_print = gtk_button_new_with_label("🖨 Stampa");
    gtk_widget_set_size_request(btn_print, 110, 50);
    g_signal_connect(btn_print, "clicked", G_CALLBACK(on_add_to_print_list), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_print, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_box), sidebar, FALSE, FALSE, 0);

    // Center: Image grid (scrollable)
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(scrolled), grid_container);

    gtk_box_pack_start(GTK_BOX(content_box), scrolled, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), content_box, TRUE, TRUE, 0);

    // Bottom bar: Page navigation
    bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(bottom_box), 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(bottom_box), "bottom-bar");

    btn_prev = gtk_button_new_with_label("◀ Prec");
    gtk_widget_set_size_request(btn_prev, 60, 28);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_prev_page), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_box), btn_prev, FALSE, FALSE, 0);

    page_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(bottom_box), page_label, TRUE, FALSE, 0);

    btn_next = gtk_button_new_with_label("Succ ▶");
    gtk_widget_set_size_request(btn_next, 60, 28);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(on_next_page), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_box), btn_next, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), bottom_box, FALSE, FALSE, 0);

    return main_box;
}


void on_drive_connected(GVolumeMonitor *monitor, GDrive *drive) {
    GList *volumes = g_drive_get_volumes(drive);
    for (GList *l = volumes; l != NULL; l = l->next) {
        GVolume *volume = G_VOLUME(l->data);
        GMount *mount = g_volume_get_mount(volume);
        if (mount) {
            GFile *file = g_mount_get_root(mount);
            if (file) {
                char *path = g_file_get_path(file);
                if (path) {
                    strncpy(src_folder, path, sizeof(src_folder) - 1);
                    gtk_label_set_text(GTK_LABEL(label_copy_status), g_strdup_printf("Sorgente: %s", src_folder));
                    g_free(path);
                }
                g_object_unref(file);
            }
            g_object_unref(mount);
        }
    }
    g_list_free_full(volumes, g_object_unref);
}

void on_drive_disconnected(GVolumeMonitor *monitor, GDrive *drive);

void on_add_to_print_list(GtkButton *button, gpointer data);
void on_remove_from_print_list(GtkButton *button, gpointer data);
void on_print_selected(GtkButton *button, gpointer data);

GtkWidget *create_print_page() {
    GtkWidget *grid, *label_title, *scrolled_window, *hbox, *add_button, *remove_button, *print_button;
    GtkWidget *printer_label, *printer_hbox;

    grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    label_title = gtk_label_new("Print Images");
    gtk_grid_attach(GTK_GRID(grid), label_title, 0, 0, 2, 1);

    // Selezione stampante
    printer_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_grid_attach(GTK_GRID(grid), printer_hbox, 0, 1, 2, 1);

    printer_label = gtk_label_new("Printer:");
    gtk_box_pack_start(GTK_BOX(printer_hbox), printer_label, FALSE, FALSE, 0);

    printer_combo_box = gtk_combo_box_text_new();
    // Carica stampanti disponibili
    available_printers = get_available_printers(&printer_count);
    if (available_printers && printer_count > 0) {
        for (int i = 0; i < printer_count; i++) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(printer_combo_box),
                                     available_printers[i].name,
                                     available_printers[i].display_name);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(printer_combo_box), 0);
    } else {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(printer_combo_box), "none", "No printers found");
        gtk_combo_box_set_active(GTK_COMBO_BOX(printer_combo_box), 0);
        gtk_widget_set_sensitive(printer_combo_box, FALSE);
    }
    gtk_box_pack_start(GTK_BOX(printer_hbox), printer_combo_box, TRUE, TRUE, 0);

    print_list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(print_list_box), GTK_SELECTION_MULTIPLE);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), print_list_box);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 2, 2, 1);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_grid_attach(GTK_GRID(grid), hbox, 0, 3, 2, 1);

    add_button = gtk_button_new_with_label("Aggiungi a Lista Stampa");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_to_print_list), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), add_button, TRUE, TRUE, 0);

    remove_button = gtk_button_new_with_label("Rimuovi da Lista Stampa");
    g_signal_connect(remove_button, "clicked", G_CALLBACK(on_remove_from_print_list), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), remove_button, TRUE, TRUE, 0);

    print_button = gtk_button_new_with_label("Stampa Selezionate");
    g_signal_connect(print_button, "clicked", G_CALLBACK(on_print_selected), NULL);
    gtk_grid_attach(GTK_GRID(grid), print_button, 0, 4, 2, 1);

    return grid;
}


void on_add_to_print_list(GtkButton *button, gpointer data) {
    if (total_images > 0 && current_page * images_per_page < total_images) {
        int index = current_page * images_per_page; // Assuming the first image on the current page is the selected one, or just add all of them
        char *image_path = image_files[index];

        if (!g_list_find_custom(print_list, image_path, (GCompareFunc)g_strcmp0)) {
            print_list = g_list_append(print_list, g_strdup(image_path));
            GtkWidget *label = gtk_label_new(image_path);
            gtk_container_add(GTK_CONTAINER(print_list_box), label);
            gtk_widget_show_all(print_list_box);
            g_info("Added to print list: %s", image_path);
        }
    }
}

void on_remove_from_print_list(GtkButton *button, gpointer data) {
    GList *selected_rows = gtk_list_box_get_selected_rows(GTK_LIST_BOX(print_list_box));
    for (GList *l = selected_rows; l != NULL; l = l->next) {
        GtkListBoxRow *row = GTK_LIST_BOX_ROW(l->data);
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
        const char *image_path = gtk_label_get_text(GTK_LABEL(label));
        GList *node = g_list_find_custom(print_list, (gpointer)image_path, (GCompareFunc)g_strcmp0);
        if (node) {
            print_list = g_list_remove(print_list, node->data);
            g_free(node->data);
        }
        gtk_widget_destroy(GTK_WIDGET(row));
        g_info("Removed from print list: %s", image_path);
    }
    g_list_free(selected_rows);
}

void on_print_selected(GtkButton *button, gpointer data) {
    if (g_list_length(print_list) == 0) {
        g_warning("Print list is empty.");
        return;
    }

    // Ottieni stampante selezionata
    const gchar *selected_printer = gtk_combo_box_get_active_id(GTK_COMBO_BOX(printer_combo_box));
    if (!selected_printer || strcmp(selected_printer, "none") == 0) {
        g_warning("No printer selected.");
        return;
    }

    g_info("Printing %d images to printer: %s", (int)g_list_length(print_list), selected_printer);

    // Stampa ogni file
    int success_count = 0;
    for (GList *l = print_list; l != NULL; l = l->next) {
        const char *file_path = (char *)l->data;
        if (print_file(selected_printer, file_path) == 0) {
            g_info("Successfully sent to printer: %s", file_path);
            success_count++;
        } else {
            g_warning("Failed to print: %s", file_path);
        }
    }

    // Mostra risultato
    if (success_count == (int)g_list_length(print_list)) {
        g_info("All %d images sent to printer successfully!", success_count);
    } else {
        g_warning("Printed %d/%u images", success_count, (unsigned int)g_list_length(print_list));
    }
}


void on_drive_disconnected(GVolumeMonitor *monitor, GDrive *drive) {
    strncpy(src_folder, "", sizeof(src_folder) - 1);
    gtk_label_set_text(GTK_LABEL(label_copy_status), "Sorgente: ");
}

int main(int argc, char *argv[]) {
    GtkWidget *window;


    gtk_init(&argc, &argv);

    // Load CSS theme
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css_file = "./style.css";
    GError *error = NULL;

    g_info("Loading CSS from: %s", css_file);

    if (!gtk_css_provider_load_from_path(css_provider, css_file, &error)) {
        g_warning("Failed to load CSS: %s", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
    } else {
        g_info("CSS loaded successfully!");
        GdkDisplay *display = gdk_display_get_default();
        GdkScreen *screen = gdk_display_get_default_screen(display);
        gtk_style_context_add_provider_for_screen(screen,
                                                   GTK_STYLE_PROVIDER(css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(css_provider);

    GtkWidget *header_bar, *stack, *stack_sidebar, *box;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Ikona");
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);

    stack_sidebar = gtk_stack_sidebar_new();
    gtk_box_pack_start(GTK_BOX(box), stack_sidebar, FALSE, FALSE, 0);

    stack = gtk_stack_new();
    global_stack = stack;  // Assign global reference for tab switching
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(stack_sidebar), GTK_STACK(stack));
    gtk_box_pack_start(GTK_BOX(box), stack, TRUE, TRUE, 0);

    // Config page
    GtkWidget *config_grid = create_config_page(window);
    gtk_stack_add_titled(GTK_STACK(stack), config_grid, "config", "Config");

    // Import page
    GtkWidget *import_grid = create_import_page();
    gtk_stack_add_titled(GTK_STACK(stack), import_grid, "import", "Import");

    // View page
    GtkWidget *view_grid = create_view_page(window);
    gtk_stack_add_titled(GTK_STACK(stack), view_grid, "view", "View");

    // Print page
    GtkWidget *print_grid = create_print_page();
    gtk_stack_add_titled(GTK_STACK(stack), print_grid, "print", "Print");

    GVolumeMonitor *volume_monitor = g_volume_monitor_get();
    g_signal_connect(volume_monitor, "drive-connected", G_CALLBACK(on_drive_connected), NULL);
    g_signal_connect(volume_monitor, "drive-disconnected", G_CALLBACK(on_drive_disconnected), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
