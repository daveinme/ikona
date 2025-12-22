#include "ikona.h"
#include "importer.h"
#include "viewer.h"
#include "printer.h"
#include "icon_manager.h"
#include "editor.h"
#include "print_log.h"
#include "utils.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

GtkWidget *main_window = NULL;

GtkWidget *label_copy_status;
GtkWidget *grid_container;
GtkWidget *page_label;
GtkWidget *label_auto_photo_count;  // Label for number of photos in Auto tab
char src_folder[512] = "";
char base_dst_folder[512] = "";
char view_folder[512] = "";
char *image_files[10000];          // Path thumbnails
char *original_files[10000];       // Path originali (mapping parallelo)
int total_images = 0;
int current_page = 0;
int images_per_page = 9;

/* Auto-Import SD configuration */
char sd_drive_letters[256] = "";  // Comma-separated drive letters (es: "E:,F:,G:")
gboolean auto_import_enabled = FALSE;
int import_buffer_size = 65536;   // Default 64KB (4KB=4096, 64KB=65536, 128KB=131072, 256KB=262144)
GtkWidget *entry_sd_letters = NULL;
GtkWidget *check_auto_import = NULL;
GtkWidget *import_radio_manual = NULL;
GtkWidget *import_radio_auto = NULL;

GList *print_list = NULL;
GtkWidget *print_list_box = NULL;
GtkWidget *print_grid_container = NULL;
GtkWidget *printer_combo_box = NULL;
Printer *available_printers = NULL;
int printer_count = 0;
char default_printer[256] = "";  // Default printer name

GtkWidget *global_stack = NULL;  // Stack per switchare tra tab
GtkWidget *global_sidebar_stack = NULL;  // Stack per sidebar dinamico
GtkWidget *import_spinner = NULL;  // Global spinner for import operations
GtkWidget *import_progress_bar = NULL;  // Progress bar for manual imports
GtkWidget *import_progress_bar_auto = NULL;  // Progress bar for auto imports
GtkWidget *report_page_widget = NULL;  // Report page widget (needs refresh)
char global_icons_dir[1024] = "";  // Global icons directory path


GtkWidget *create_view_page(GtkWidget *window);
GtkWidget *create_print_page();
GtkWidget *create_config_page(GtkWidget *window);
GtkWidget *create_report_page(void);
GtkWidget *create_import_sidebar(void);
GtkWidget *create_view_sidebar(void);
GtkWidget *create_print_sidebar(void);
GtkWidget *create_empty_sidebar(void);
GtkWidget *create_config_sidebar(void);
GtkWidget *create_report_sidebar(void);
GtkWidget *create_image_grid(void);

/* Auto-Import functions */
void save_config(void);
void load_config(void);
void on_sd_config_changed(GtkWidget *widget, gpointer data);
gboolean check_if_drive_is_monitored(const char *drive_path);


void on_add_to_print_list(GtkButton *button, gpointer data);
void on_remove_from_print_list(GtkButton *button, gpointer data);
void on_print_selected(GtkButton *button, gpointer data);

void on_drive_connected(GVolumeMonitor *monitor, GDrive *drive);
void on_drive_disconnected(GVolumeMonitor *monitor, GDrive *drive);
void on_clear_view_clicked(GtkButton *button, gpointer data);

extern GtkWidget *secondary_viewer_window;
extern void secondary_viewer_display_page(void);
extern GtkWidget *create_secondary_viewer_window(void);

/* Toast notification per avvisi standard */
static gboolean toast_timeout(gpointer data) {
    GtkWidget *toast = (GtkWidget *)data;
    gtk_widget_destroy(toast);
    return FALSE;
}

void show_toast_notification(const char *message) {
    GtkWidget *toast_window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_decorated(GTK_WINDOW(toast_window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(toast_window), TRUE);
    gtk_widget_set_opacity(toast_window, 0.95);

    GtkWidget *label = gtk_label_new(message);
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_bottom(label, 10);
    gtk_widget_set_margin_start(label, 20);
    gtk_widget_set_margin_end(label, 20);

    gtk_container_add(GTK_CONTAINER(toast_window), label);
    gtk_widget_show_all(toast_window);

    /* Posiziona in basso al centro */
    GdkDisplay *display = gdk_display_get_default();
    if (display) {
        GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
        if (monitor) {
            GdkRectangle geometry;
            gdk_monitor_get_geometry(monitor, &geometry);
            gtk_window_move(GTK_WINDOW(toast_window),
                            geometry.x + (geometry.width / 2) - 150,
                            geometry.y + geometry.height - 100);
        }
    }

    /* Scompare dopo 3 secondi */
    g_timeout_add(3000, toast_timeout, toast_window);
}

void on_print_button_clicked(GtkButton *button, gpointer data) {
    on_add_to_print_list(button, data);
    GtkWidget *print_window = create_print_window(global_icons_dir);
    gtk_window_maximize(GTK_WINDOW(print_window));
}

void on_tab_clicked(GtkButton *button, gpointer tab_name) {
    const char *tab = (const char *)tab_name;

    if (global_sidebar_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_sidebar_stack), tab);
    }

    if (global_stack) {
        if (strcmp(tab, "config") == 0) {
            gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "config");
        } else if (strcmp(tab, "report") == 0) {
            /* Ricrea il report ogni volta che viene visualizzato */
            if (report_page_widget) {
                gtk_widget_destroy(report_page_widget);
                report_page_widget = NULL;
            }
            report_page_widget = create_report_page();
            gtk_stack_add_titled(GTK_STACK(global_stack), report_page_widget, "report", "Report");
            gtk_widget_show_all(report_page_widget);
            gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "report");
        } else {
            gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "grid");
        }
    }
}

void on_import_tab_clicked(GtkButton *button, gpointer tab_name) {
    // This will be used to switch between Manuale/Auto in the Import sidebar
    // Implementation will be added when sidebar is properly structured
}

GtkWidget *import_sidebar_stack = NULL;

void on_import_manual_tab(GtkButton *button, gpointer data) {
    if (import_sidebar_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(import_sidebar_stack), "manuale");
    }

    /* Aggiorna stili dei pulsanti */
    GtkWidget *btn_auto = (GtkWidget *)g_object_get_data(G_OBJECT(button), "btn_auto");
    gtk_style_context_remove_class(gtk_widget_get_style_context(btn_auto), "suggested-action");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(button)), "suggested-action");
}

void on_import_auto_tab(GtkButton *button, gpointer data) {
    if (import_sidebar_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(import_sidebar_stack), "auto");
    }

    /* Aggiorna stili dei pulsanti */
    GtkWidget *btn_manuale = (GtkWidget *)g_object_get_data(G_OBJECT(button), "btn_manuale");
    gtk_style_context_remove_class(gtk_widget_get_style_context(btn_manuale), "suggested-action");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(button)), "suggested-action");
}

void on_open_secondary_viewer(GtkButton *button, gpointer data) {
    if (!secondary_viewer_window) {
        create_secondary_viewer_window();
    }
    secondary_viewer_display_page(); // Ensure it displays current images
    gtk_widget_show_all(secondary_viewer_window);
    gtk_window_present(GTK_WINDOW(secondary_viewer_window));
}

void on_edit_image_clicked(GtkButton *button, gpointer data) {
    if (selected_image_index != -1 && image_files[selected_image_index] != NULL) {
        create_photo_editor_window(image_files[selected_image_index], global_icons_dir);
    } else {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_INFO, // Added message type
                                       GTK_BUTTONS_OK,
                                       "Seleziona un'immagine prima di modificarla.");
        gtk_window_set_title(GTK_WINDOW(dialog), "Nessuna immagine selezionata");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

/* Save configuration to file */
void save_config(void) {
    FILE *fp;
    char config_path[1024];

#ifdef _WIN32
    snprintf(config_path, sizeof(config_path), "%s\\ikona_config.ini", g_get_home_dir());
#else
    snprintf(config_path, sizeof(config_path), "%s/.ikona_config", g_get_home_dir());
#endif

    fp = fopen(config_path, "w");
    if (!fp) {
        g_warning("Cannot save config file: %s", config_path);
        return;
    }

    fprintf(fp, "[AutoImport]\n");
    fprintf(fp, "enabled=%d\n", auto_import_enabled ? 1 : 0);
    fprintf(fp, "drive_letters=%s\n", sd_drive_letters);
    fprintf(fp, "\n[Performance]\n");
    fprintf(fp, "buffer_size=%d\n", import_buffer_size);
    fprintf(fp, "\n[Folders]\n");
    fprintf(fp, "src=%s\n", src_folder);
    fprintf(fp, "dst=%s\n", base_dst_folder);
    fprintf(fp, "view=%s\n", view_folder);

    fclose(fp);
    g_info("Configuration saved to: %s", config_path);
}

/* Load configuration from file */
void load_config(void) {
    FILE *fp;
    char config_path[1024];
    char line[512];

#ifdef _WIN32
    snprintf(config_path, sizeof(config_path), "%s\\ikona_config.ini", g_get_home_dir());
#else
    snprintf(config_path, sizeof(config_path), "%s/.ikona_config", g_get_home_dir());
#endif

    fp = fopen(config_path, "r");
    if (!fp) {
        g_info("No config file found, using defaults");
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *equals;

        /* Remove newline */
        line[strcspn(line, "\r\n")] = 0;

        /* Skip empty lines and sections */
        if (line[0] == '\0' || line[0] == '[' || line[0] == '#') continue;

        equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        char *key = line;
        char *value = equals + 1;

        if (strcmp(key, "enabled") == 0) {
            auto_import_enabled = (atoi(value) == 1);
        } else if (strcmp(key, "drive_letters") == 0) {
            strncpy(sd_drive_letters, value, sizeof(sd_drive_letters) - 1);
        } else if (strcmp(key, "buffer_size") == 0) {
            int size = atoi(value);
            // Validate buffer size (must be 4KB, 64KB, 128KB, or 256KB)
            if (size == 4096 || size == 65536 || size == 131072 || size == 262144) {
                import_buffer_size = size;
            }
        } else if (strcmp(key, "src") == 0) {
            strncpy(src_folder, value, sizeof(src_folder) - 1);
        } else if (strcmp(key, "dst") == 0) {
            strncpy(base_dst_folder, value, sizeof(base_dst_folder) - 1);
        } else if (strcmp(key, "view") == 0) {
            strncpy(view_folder, value, sizeof(view_folder) - 1);
        }
    }

    fclose(fp);
    g_info("Configuration loaded from: %s", config_path);
}

/* Callback when SD config changes */
void on_sd_config_changed(GtkWidget *widget, gpointer data) {
    const char *text;

    if (GTK_IS_ENTRY(widget)) {
        text = gtk_entry_get_text(GTK_ENTRY(widget));
        strncpy(sd_drive_letters, text, sizeof(sd_drive_letters) - 1);
    } else if (GTK_IS_TOGGLE_BUTTON(widget)) {
        auto_import_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    }

    save_config();
}

/* Callback when buffer size changes */
void on_buffer_size_changed(GtkComboBox *combo, gpointer data) {
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) {
        int size = atoi(id);
        if (size == 4096 || size == 65536 || size == 131072 || size == 262144) {
            import_buffer_size = size;
            save_config();
            g_info("Buffer size changed to: %d bytes (%d KB)", size, size / 1024);
        }
    }
}

/* Check if a drive path matches monitored letters */
gboolean check_if_drive_is_monitored(const char *drive_path) {
    char *letters_copy;
    char *token;
    gboolean found;

    if (!auto_import_enabled || strlen(sd_drive_letters) == 0) {
        return FALSE;
    }

    if (!drive_path) {
        return FALSE;
    }

    found = FALSE;
    letters_copy = g_strdup(sd_drive_letters);
    token = strtok(letters_copy, ",;: ");

    while (token != NULL) {
        char normalized[16];
        char drive_letter[8];

        /* Normalize token (uppercase, add : if missing) */
        snprintf(normalized, sizeof(normalized), "%c:", g_ascii_toupper(token[0]));
        g_info("Checking against normalized drive letter: %s", normalized);

        /* Extract drive letter from path (e.g., "E:\\" -> "E:") */
#ifdef _WIN32
        if (strlen(drive_path) >= 2 && drive_path[1] == ':') {
            snprintf(drive_letter, sizeof(drive_letter), "%c:", g_ascii_toupper(drive_path[0]));
            g_info("Drive path: %s, Extracted drive letter: %s", drive_path, drive_letter);

            if (strcmp(normalized, drive_letter) == 0) {
                found = TRUE;
                break;
            }
        }
#endif
        token = strtok(NULL, ",;: ");
    }

    g_free(letters_copy);
    return found;
}

/* Find first connected SD with configured letter */
gboolean find_and_set_sd_source(void) {
    char *letters_copy;
    char *token;
    gboolean found;

    if (strlen(sd_drive_letters) == 0) {
        return FALSE;
    }

    found = FALSE;
    letters_copy = g_strdup(sd_drive_letters);
    token = strtok(letters_copy, ",;: ");

    while (token != NULL) {
        char drive_path[16];
        char test_path[32];

#ifdef _WIN32
        /* Build drive path (e.g., "E:\") */
        snprintf(drive_path, sizeof(drive_path), "%c:\\", g_ascii_toupper(token[0]));
        snprintf(test_path, sizeof(test_path), "%c:\\", g_ascii_toupper(token[0]));

        /* Check if drive exists */
        if (GetDriveTypeA(test_path) != DRIVE_NO_ROOT_DIR &&
            GetDriveTypeA(test_path) != DRIVE_UNKNOWN) {
            strncpy(src_folder, drive_path, sizeof(src_folder) - 1);
            g_info("Found SD card at: %s", src_folder);
            found = TRUE;
            break;
        }
#else
        /* On Linux, we'd check /media or /mnt */
        snprintf(drive_path, sizeof(drive_path), "/media/%s", token);
        if (access(drive_path, F_OK) == 0) {
            strncpy(src_folder, drive_path, sizeof(src_folder) - 1);
            g_info("Found SD card at: %s", src_folder);
            found = TRUE;
            break;
        }
#endif
        token = strtok(NULL, ",;: ");
    }

    g_free(letters_copy);
    return found;
}

void *import_thread(void *data) {
    gboolean is_auto_mode;

    /* Check which radio button is selected */
    if (import_radio_auto && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(import_radio_auto))) {
        is_auto_mode = TRUE;
    } else {
        is_auto_mode = FALSE;
    }

    /* Check if destination folder is configured */
    if (strlen(base_dst_folder) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status),
            "❌ ERRORE: Cartella destinazione non configurata!\n"
            "Vai in Config e seleziona la cartella destinazione.");
        gtk_spinner_stop(GTK_SPINNER(data));
        return NULL;
    }

    if (is_auto_mode) {
        /* Auto mode: find SD automatically */
        g_info("Auto-import mode: searching for configured SD cards...");

        /* Check if SD letters are configured */
        if (strlen(sd_drive_letters) == 0) {
            gtk_label_set_text(GTK_LABEL(label_copy_status),
                "❌ ERRORE: Nessuna lettera SD configurata!\n"
                "Vai in Config → Auto-Import e configura le lettere (es: E,F,G)");
            gtk_spinner_stop(GTK_SPINNER(data));
            return NULL;
        }

        if (!find_and_set_sd_source()) {
            gtk_label_set_text(GTK_LABEL(label_copy_status),
                g_strdup_printf("❌ Nessuna SD trovata!\n"
                               "Collega una SD con lettere: %s", sd_drive_letters));
            gtk_spinner_stop(GTK_SPINNER(data));
            return NULL;
        }
        gtk_label_set_text(GTK_LABEL(label_copy_status),
            g_strdup_printf("📸 SD trovata: %s\nImportazione in corso...", src_folder));
    } else {
        /* Manual mode: check if source folder is selected */
        if (strlen(src_folder) == 0) {
            gtk_label_set_text(GTK_LABEL(label_copy_status),
                "❌ ERRORE: Cartella sorgente non selezionata!\n"
                "Clicca 'Seleziona Cartella Sorgente' prima di importare.");
            gtk_spinner_stop(GTK_SPINNER(data));
            return NULL;
        }
        g_info("Manual import mode: using selected folder");
    }

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
    GtkWidget *grid, *label_title, *label_dst, *label_view, *btn_dst, *btn_view;
    GtkWidget *entry_dst, *entry_view;

    grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    label_title = gtk_label_new("Configurazione");
    gtk_grid_attach(GTK_GRID(grid), label_title, 0, 0, 2, 1);

    // Destination folder
    label_dst = gtk_label_new("Cartella Destinazione (Salva in):");
    gtk_grid_attach(GTK_GRID(grid), label_dst, 0, 1, 2, 1);

    entry_dst = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_dst), base_dst_folder);
    gtk_widget_set_sensitive(entry_dst, FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry_dst, 0, 2, 1, 1);

    btn_dst = create_icon_only_button(ICON_FOLDER_FILL, 18, "Destinazione");
    gtk_widget_set_name(GTK_WIDGET(btn_dst), "btn-dst");
    g_signal_connect(btn_dst, "clicked", G_CALLBACK(on_select_dst_folder), window);
    gtk_widget_set_size_request(btn_dst, 40, 32);
    gtk_grid_attach(GTK_GRID(grid), btn_dst, 1, 2, 1, 1);

    // View folder
    label_view = gtk_label_new("Cartella Visualizzazione (Libreria Foto):");
    gtk_grid_attach(GTK_GRID(grid), label_view, 0, 3, 2, 1);

    entry_view = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_view), view_folder);
    gtk_widget_set_sensitive(entry_view, FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry_view, 0, 4, 1, 1);

    btn_view = create_icon_only_button(ICON_FILE_IMAGE, 18, "Libreria");
    gtk_widget_set_name(GTK_WIDGET(btn_view), "btn-view");
    g_signal_connect(btn_view, "clicked", G_CALLBACK(on_select_view_folder), window);
    gtk_widget_set_size_request(btn_view, 40, 32);
    gtk_grid_attach(GTK_GRID(grid), btn_view, 1, 4, 1, 1);

    /* Separator */
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), separator, 0, 5, 2, 1);

    /* Auto-Import SD section */
    GtkWidget *label_auto_import = gtk_label_new("<b>Auto-Import da SD Card</b>");
    gtk_label_set_use_markup(GTK_LABEL(label_auto_import), TRUE);
    gtk_widget_set_halign(label_auto_import, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_auto_import, 0, 6, 2, 1);

    check_auto_import = gtk_check_button_new_with_label("Abilita auto-import quando vengono collegate SD");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_import), auto_import_enabled);
    g_signal_connect(check_auto_import, "toggled", G_CALLBACK(on_sd_config_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), check_auto_import, 0, 7, 2, 1);

    GtkWidget *label_drives = gtk_label_new("Lettere drive da monitorare (es: E,F,G):");
    gtk_widget_set_halign(label_drives, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_drives, 0, 8, 2, 1);

    entry_sd_letters = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_sd_letters), sd_drive_letters);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_sd_letters), "E,F,G");
    g_signal_connect(entry_sd_letters, "changed", G_CALLBACK(on_sd_config_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), entry_sd_letters, 0, 9, 2, 1);

    GtkWidget *label_help = gtk_label_new("<small><i>Quando colleghi una SD con queste lettere, ti verrà chiesto se vuoi importare le foto.</i></small>");
    gtk_label_set_use_markup(GTK_LABEL(label_help), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(label_help), TRUE);
    gtk_widget_set_halign(label_help, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_help, 0, 10, 2, 1);

    /* Separator */
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), separator2, 0, 11, 2, 1);

    /* Performance section */
    GtkWidget *label_performance = gtk_label_new("<b>Prestazioni Import</b>");
    gtk_label_set_use_markup(GTK_LABEL(label_performance), TRUE);
    gtk_widget_set_halign(label_performance, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_performance, 0, 12, 2, 1);

    GtkWidget *label_buffer = gtk_label_new("Dimensione buffer I/O:");
    gtk_widget_set_halign(label_buffer, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_buffer, 0, 13, 1, 1);

    GtkWidget *combo_buffer = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_buffer), "4096", "4 KB (Sicuro, lento)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_buffer), "65536", "64 KB (Bilanciato) ⭐");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_buffer), "131072", "128 KB (Veloce)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_buffer), "262144", "256 KB (Massimo, richiede RAM)");

    // Set current value
    char current_id[16];
    snprintf(current_id, sizeof(current_id), "%d", import_buffer_size);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_buffer), current_id);

    g_signal_connect(combo_buffer, "changed", G_CALLBACK(on_buffer_size_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), combo_buffer, 1, 13, 1, 1);

    GtkWidget *label_buffer_help = gtk_label_new("<small><i>Buffer più grandi = import più veloce, ma usano più RAM.\n64 KB è il valore consigliato per la maggior parte dei PC.</i></small>");
    gtk_label_set_use_markup(GTK_LABEL(label_buffer_help), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(label_buffer_help), TRUE);
    gtk_widget_set_halign(label_buffer_help, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_buffer_help, 0, 14, 2, 1);

    return grid;
}


GtkWidget *create_report_page(void) {
    GtkWidget *vbox, *label_title, *scrolled, *report_box, *separator;
    GtkWidget *label_help, *clear_button, *hbox_buttons;
    GList *dates, *l;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);

    label_title = gtk_label_new("Report Stampe Giornaliere");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_title), "title");
    gtk_box_pack_start(GTK_BOX(vbox), label_title, FALSE, FALSE, 0);

    label_help = gtk_label_new("Riepilogo delle stampe per data:");
    gtk_widget_set_halign(label_help, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), label_help, FALSE, FALSE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);

    report_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(report_box), 10);

    /* Get all dates from print log */
    dates = get_all_print_dates();

    if (dates == NULL || g_list_length(dates) == 0) {
        GtkWidget *empty_label = gtk_label_new("Nessuna stampa registrata ancora.");
        gtk_widget_set_halign(empty_label, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(empty_label, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(report_box), empty_label, TRUE, TRUE, 0);
    } else {
        /* Display each date with count */
        for (l = dates; l != NULL; l = l->next) {
            const char *date = (const char *)l->data;
            int count = get_print_total_for_date(date);

            GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);

            GtkWidget *label_date = gtk_label_new(date);
            gtk_widget_set_halign(label_date, GTK_ALIGN_START);
            gtk_widget_set_size_request(label_date, 120, -1);
            gtk_box_pack_start(GTK_BOX(hbox), label_date, FALSE, FALSE, 0);

            GtkWidget *label_count = gtk_label_new(
                g_strdup_printf("📄 %d copie", count));
            gtk_widget_set_halign(label_count, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(hbox), label_count, FALSE, FALSE, 0);

            gtk_box_pack_start(GTK_BOX(report_box), hbox, FALSE, FALSE, 0);

            if (l->next != NULL) {
                GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
                gtk_box_pack_start(GTK_BOX(report_box), sep, FALSE, FALSE, 0);
            }
        }

        /* Free dates list */
        g_list_free_full(dates, g_free);
    }

    gtk_container_add(GTK_CONTAINER(scrolled), report_box);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

    /* Bottom buttons */
    hbox_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(hbox_buttons), TRUE);

    clear_button = gtk_button_new_with_label("🗑 Cancella Log");
    g_signal_connect(clear_button, "clicked", G_CALLBACK(clear_print_log_with_dialog), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), clear_button, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

    return vbox;
}

GtkWidget *create_import_page() {
    // Return just the image grid - sidebar is handled globally
    return create_image_grid();
}


GtkWidget *create_view_page(GtkWidget *window) {
    // Return just the image grid - sidebar is handled globally
    return create_image_grid();
}


void on_drive_connected(GVolumeMonitor *monitor, GDrive *drive) {
    GList *volumes;
    GList *l;
    GtkWidget *dialog;
    gint response;

    volumes = g_drive_get_volumes(drive);
    for (l = volumes; l != NULL; l = l->next) {
        GVolume *volume;
        GMount *mount;
        GFile *file;
        char *path;

        volume = G_VOLUME(l->data);
        mount = g_volume_get_mount(volume);
        if (mount) {
            file = g_mount_get_root(mount);
            if (file) {
                path = g_file_get_path(file);
                if (path) {
                    /* Update src_folder regardless */
                    strncpy(src_folder, path, sizeof(src_folder) - 1);

                    /* Check if this is a monitored SD card */
                    if (check_if_drive_is_monitored(path)) {
                        g_info("Detected monitored SD card: %s", path);

                        /* Show confirmation dialog */
                        dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                            GTK_DIALOG_MODAL,
                            GTK_MESSAGE_QUESTION,
                            GTK_BUTTONS_YES_NO,
                            "SD Card rilevata: %s\n\nVuoi importare automaticamente le foto?",
                            path);

                        gtk_window_set_title(GTK_WINDOW(dialog), "Auto-Import SD");
                        response = gtk_dialog_run(GTK_DIALOG(dialog));
                        gtk_widget_destroy(dialog);

                        if (response == GTK_RESPONSE_YES) {
                            g_info("User confirmed auto-import, starting...");

                            /* Switch to Import tab */
                            if (global_stack) {
                                gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "import");
                            }

                            /* Update status label */
                            if (label_copy_status) {
                                gtk_label_set_text(GTK_LABEL(label_copy_status),
                                    g_strdup_printf("Auto-Import da: %s", path));
                            }

                            /* Start import automatically */
                            on_copy_jpg_auto(NULL, NULL);
                        } else {
                            g_info("User cancelled auto-import");
                        }
                    } else {
                        /* Not a monitored drive, just update label */
                        if (label_copy_status) {
                            gtk_label_set_text(GTK_LABEL(label_copy_status),
                                g_strdup_printf("Sorgente: %s", path));
                        }
                    }

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

/* Helper: applica stile print-dialog ai messagedialog */
void apply_print_dialog_style(GtkWidget *dialog) {
    gtk_style_context_add_class(gtk_widget_get_style_context(dialog), "print-dialog");
}

GtkWidget *create_print_page() {
    GtkWidget *main_vbox, *label_title, *scrolled_window, *hbox, *add_button, *remove_button, *print_button;
    GtkWidget *printer_label, *printer_hbox;
    int i;

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 20);

    label_title = gtk_label_new("Stampa Immagini");
    gtk_box_pack_start(GTK_BOX(main_vbox), label_title, FALSE, FALSE, 0);

    /* Selezione stampante */
    printer_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), printer_hbox, FALSE, FALSE, 0);

    printer_label = gtk_label_new("Stampante:");
    gtk_box_pack_start(GTK_BOX(printer_hbox), printer_label, FALSE, FALSE, 0);

    printer_combo_box = gtk_combo_box_text_new();
    /* Carica stampanti disponibili */
    available_printers = get_available_printers(&printer_count);
    if (available_printers && printer_count > 0) {
        for (i = 0; i < printer_count; i++) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(printer_combo_box),
                                     available_printers[i].name,
                                     available_printers[i].display_name);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(printer_combo_box), 0);
    } else {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(printer_combo_box), "none", "Nessuna stampante trovata");
        gtk_combo_box_set_active(GTK_COMBO_BOX(printer_combo_box), 0);
        gtk_widget_set_sensitive(printer_combo_box, FALSE);
    }
    gtk_box_pack_start(GTK_BOX(printer_hbox), printer_combo_box, TRUE, TRUE, 0);

    /* Griglia per le foto in stampa */
    print_grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), print_grid_container);
    gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);

    /* Pulsanti */
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

    add_button = gtk_button_new_with_label("Aggiungi Selezionate");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_to_print_list), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), add_button, TRUE, TRUE, 0);

    remove_button = gtk_button_new_with_label("Rimuovi Tutte");
    g_signal_connect(remove_button, "clicked", G_CALLBACK(on_remove_from_print_list), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), remove_button, TRUE, TRUE, 0);

    print_button = gtk_button_new_with_label("🖨 Stampa Tutto");
    g_signal_connect(print_button, "clicked", G_CALLBACK(on_print_selected), NULL);
    gtk_box_pack_start(GTK_BOX(main_vbox), print_button, FALSE, FALSE, 0);

    return main_vbox;
}


void refresh_print_grid(void) {
    /* Update print window if it's open */
    extern void refresh_print_window_grid(void);
    refresh_print_window_grid();
}

void on_add_to_print_list(GtkButton *button, gpointer data) {
    int i;
    int added_count;
    GList *l;

    if (selected_count == 0) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       "Nessuna immagine selezionata!");
        gtk_window_set_title(GTK_WINDOW(dialog), "Attenzione");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    added_count = 0;

    /* Aggiungi tutte le immagini selezionate */
    for (i = 0; i < selected_count; i++) {
        int img_idx;
        char *image_path;
        gboolean already_exists;
        PrintItem *item;

        img_idx = selected_images[i];
        // NUOVO: Usa path ORIGINALE (non thumbnail) per stampa
        image_path = original_files[img_idx];

        /* Controlla se già esiste */
        already_exists = FALSE;
        for (l = print_list; l != NULL; l = l->next) {
            PrintItem *existing = (PrintItem *)l->data;
            if (g_strcmp0(existing->path, image_path) == 0) {
                already_exists = TRUE;
                break;
            }
        }

        if (already_exists) {
            continue;
        }

        /* Crea PrintItem */
        item = g_new(PrintItem, 1);
        item->path = g_strdup(image_path);
        item->copies = 1;
        item->spin_button = NULL;

        print_list = g_list_append(print_list, item);
        added_count++;
    }

    refresh_print_grid();
    g_info("Aggiunte %d immagini alla lista di stampa", added_count);
}

void on_remove_from_print_list(GtkButton *button, gpointer data) {
    GList *l;

    if (g_list_length(print_list) == 0) {
        return;
    }

    /* Rimuovi tutti gli item */
    for (l = print_list; l != NULL; l = l->next) {
        PrintItem *item = (PrintItem *)l->data;
        g_free(item->path);
        g_free(item);
    }

    g_list_free(print_list);
    print_list = NULL;

    refresh_print_grid();
    g_info("Lista stampa svuotata");
}

void on_print_selected(GtkButton *button, gpointer data) {
    GList *l;
    const char *printer_name;
    int total_printed = 0;
    int total_failed = 0;
    int total_items = 0;
    int total_copies = 0;
    gint response;
    GtkWidget *dialog;

    /* NUOVO: Cancella timeout di pulizia automatica (stiamo stampando) */
    cancel_print_list_cleanup_timeout();

    if (!print_list) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
        dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       "Nessuna immagine nella lista di stampa!");
        gtk_window_set_title(GTK_WINDOW(dialog), "Attenzione");
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    printer_name = get_selected_printer_from_window();
    if (!printer_name || strlen(printer_name) == 0) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
        dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       "Seleziona una stampante!");
        gtk_window_set_title(GTK_WINDOW(dialog), "Errore");
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    /* Conta il totale di foto e copie */
    for (l = print_list; l != NULL; l = l->next) {
        PrintItem *item = (PrintItem *)l->data;
        int copies = 1;

        /* Leggi il numero di copie dallo spinner se disponibile */
        if (item->spin_button) {
            copies = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(item->spin_button));
        } else {
            copies = item->copies > 0 ? item->copies : 1;
        }

        total_items++;
        total_copies += copies;
    }

    /* Dialog di riepilogo con conferma */
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
    dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_OK_CANCEL,
                                   "Stampa %d foto (%d copie totali)?",
                                   total_items, total_copies);
    gtk_window_set_title(GTK_WINDOW(dialog), "Riepilogo Stampa");
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    /* Se l'utente clicca Annulla, non fare nulla */
    if (response != GTK_RESPONSE_OK) {
        return;
    }

    /* Stampa ogni elemento nella lista */
    for (l = print_list; l != NULL; l = l->next) {
        PrintItem *item = (PrintItem *)l->data;
        int copies = 1;

        /* Leggi il numero di copie dallo spinner se disponibile */
        if (item->spin_button) {
            copies = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(item->spin_button));
        } else {
            copies = item->copies > 0 ? item->copies : 1;
        }

        int i;

        g_info("Stampa file: %s (%d copie)", item->path, copies);

        for (i = 0; i < copies; i++) {
            int result = print_file(printer_name, item->path);
            if (result == 0) {
                total_printed++;
            } else {
                total_failed++;
            }
        }
    }

    /* Log print to history (only if successful) */
    if (total_printed > 0) {
        log_print(total_printed);
    }

    /* Toast notification di conferma finale */
    char toast_message[256];
    snprintf(toast_message, sizeof(toast_message),
             "✓ Stampe completate: %d inviate - %d errori",
             total_printed, total_failed);
    show_toast_notification(toast_message);

    g_info("Stampa completata: %d stampe inviate, %d errori", total_printed, total_failed);

    /* NUOVO: Pulisci lista stampa dopo completamento */
    on_remove_from_print_list(NULL, NULL);
}


void on_drive_disconnected(GVolumeMonitor *monitor, GDrive *drive) {
    strncpy(src_folder, "", sizeof(src_folder) - 1);
    gtk_label_set_text(GTK_LABEL(label_copy_status), "Sorgente: ");
}

/* Image grid creation - reusable for all pages with photos */
GtkWidget *create_image_grid(void) {
    static GtkWidget *grid_vbox = NULL;

    // Create grid structure only once
    if (grid_vbox) {
        return grid_vbox;
    }

    GtkWidget *scrolled, *bottom_box, *btn_prev, *btn_next;

    // Vertical box to hold grid + pagination
    grid_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(grid_vbox, TRUE);
    gtk_widget_set_vexpand(grid_vbox, TRUE);

    // Scrolled window with grid
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    // Create grid_container only once
    grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid_container), 16);
    gtk_widget_set_name(grid_container, "grid-container");
    gtk_style_context_add_class(gtk_widget_get_style_context(grid_container), "grid-container");
    gtk_widget_set_halign(grid_container, GTK_ALIGN_FILL);
    gtk_widget_set_vexpand(grid_container, FALSE);

    gtk_container_add(GTK_CONTAINER(scrolled), grid_container);
    gtk_box_pack_start(GTK_BOX(grid_vbox), scrolled, TRUE, TRUE, 0);

    // Bottom pagination bar
    bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(bottom_box), 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(bottom_box), "bottom-bar");

    btn_prev = create_icon_only_button(ICON_ARROW_LEFT, 20, "Precedente");
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_prev_page), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_box), btn_prev, FALSE, FALSE, 0);

    page_label = gtk_label_new("");
    gtk_widget_set_halign(page_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(bottom_box), page_label, TRUE, FALSE, 0);

    btn_next = create_icon_only_button(ICON_ARROW_RIGHT, 20, "Successiva");
    g_signal_connect(btn_next, "clicked", G_CALLBACK(on_next_page), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_box), btn_next, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(grid_vbox), bottom_box, FALSE, FALSE, 0);

    return grid_vbox;
}

/* Sidebar creation functions */
GtkWidget *create_empty_sidebar(void) {
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");
    return sidebar;
}

GtkWidget *create_config_sidebar(void) {
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    GtkWidget *label = gtk_label_new("Configurazione");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 0);

    return sidebar;
}

GtkWidget *create_report_sidebar(void) {
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    GtkWidget *label = gtk_label_new("Report");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 0);

    return sidebar;
}

GtkWidget *create_import_sidebar(void) {
    /* 1. Contenitore principale */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    /* 2. Titolo */
    GtkWidget *label = gtk_label_new("Importa Foto");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 10);

    /* 3. Selettore Tab (Manuale / Auto) */
    GtkWidget *tabs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(tabs_box), "linked"); // Unisce i pulsanti visivamente

    GtkWidget *btn_auto = gtk_button_new_with_label("Auto");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_auto), "flat");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_auto), "suggested-action");
    gtk_widget_set_name(GTK_WIDGET(btn_auto), "btn-auto");

    GtkWidget *btn_manuale = gtk_button_new_with_label("Manuale");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_manuale), "flat");
    gtk_widget_set_name(GTK_WIDGET(btn_manuale), "btn-manuale");

    gtk_box_pack_start(GTK_BOX(tabs_box), btn_auto, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_manuale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), tabs_box, FALSE, FALSE, 15);

    /* Salva riferimenti ai pulsanti per gestire l'attivo */
    g_object_set_data(G_OBJECT(btn_auto), "btn_manuale", btn_manuale);
    g_object_set_data(G_OBJECT(btn_manuale), "btn_auto", btn_auto);

    /* 4. Stack per il contenuto */
    GtkWidget *import_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(import_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    /* --- CONTENUTO AUTO (creato per primo = default) --- */
    GtkWidget *auto_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    label_auto_photo_count = gtk_label_new("0 Foto da importare");
    gtk_box_pack_start(GTK_BOX(auto_box), label_auto_photo_count, FALSE, FALSE, 10);

    GtkWidget *btn_scan = create_icon_only_button(ICON_AGGIORNA, 20, "Aggiorna SD");
    g_signal_connect(btn_scan, "clicked", G_CALLBACK(on_scan_sd_for_auto), NULL);
    gtk_box_pack_start(GTK_BOX(auto_box), btn_scan, FALSE, FALSE, 0);

    import_progress_bar_auto = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(import_progress_bar_auto), TRUE);
    gtk_widget_set_margin_top(import_progress_bar_auto, 10);
    gtk_box_pack_start(GTK_BOX(auto_box), import_progress_bar_auto, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(import_stack), auto_box, "auto");

    /* --- CONTENUTO MANUALE --- */
    GtkWidget *manuale_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_box_pack_start(GTK_BOX(manuale_box), gtk_label_new("Origine:"), FALSE, FALSE, 0);
    GtkWidget *btn_src = gtk_button_new_with_label("Sfoglia...");
    g_signal_connect(btn_src, "clicked", G_CALLBACK(on_select_src_folder), NULL);
    gtk_box_pack_start(GTK_BOX(manuale_box), btn_src, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(manuale_box), gtk_label_new("Destinazione:"), FALSE, FALSE, 0);
    GtkWidget *btn_dst = gtk_button_new_with_label("Sfoglia...");
    g_signal_connect(btn_dst, "clicked", G_CALLBACK(on_select_dst_folder), NULL);
    gtk_box_pack_start(GTK_BOX(manuale_box), btn_dst, FALSE, FALSE, 0);

    if (!import_spinner) import_spinner = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(manuale_box), import_spinner, FALSE, FALSE, 5);

    label_copy_status = gtk_label_new("In attesa...");
    gtk_box_pack_start(GTK_BOX(manuale_box), label_copy_status, FALSE, FALSE, 0);

    import_progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(import_progress_bar), TRUE);
    gtk_widget_set_margin_top(import_progress_bar, 10);
    gtk_box_pack_start(GTK_BOX(manuale_box), import_progress_bar, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(import_stack), manuale_box, "manuale");

    /* Impacchettiamo lo stack al centro */
    gtk_box_pack_start(GTK_BOX(sidebar), import_stack, TRUE, TRUE, 0);

    /* 5. PULSANTE "IMPORTA" FISSO AI PIEDI */
    GtkWidget *btn_importa_final = create_icon_only_button(ICON_IMPORTA_SIDEBAR, 20, "IMPORTA");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_importa_final), "suggested-action");
    g_signal_connect(btn_importa_final, "clicked", G_CALLBACK(on_import_button_clicked), import_spinner);
    
    // Lo mettiamo in fondo alla sidebar
    gtk_box_pack_end(GTK_BOX(sidebar), btn_importa_final, FALSE, FALSE, 0);

    // Salvataggio riferimenti e segnali tab
    import_sidebar_stack = import_stack;
    g_signal_connect(btn_manuale, "clicked", G_CALLBACK(on_import_manual_tab), NULL);
    g_signal_connect(btn_auto, "clicked", G_CALLBACK(on_import_auto_tab), NULL);

    /* Imposta "Auto" come tab visibile di default */
    gtk_stack_set_visible_child_name(GTK_STACK(import_stack), "auto");

    return sidebar;
}

void on_clear_view_clicked(GtkButton *button, gpointer data) {
    total_images = 0;
    current_page = 0;
    strcpy(view_folder, "");
    display_page();
}
GtkWidget *create_view_sidebar(void) {
    /* Contenitore principale verticale */
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(main_container), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_container), "sidebar");

    /* 1. GRIGLIA SUPERIORE (8 pulsanti) */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    
    // Titolo
    GtkWidget *label = gtk_label_new("Libreria Foto");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 2, 1);

    // Definizione e collegamento dei primi 8 pulsanti
    GtkWidget *btn_folder = create_icon_only_button(ICON_FOLDER_FILL, 20, "Apri");
    g_signal_connect(btn_folder, "clicked", G_CALLBACK(on_select_view_folder), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_folder, 0, 1, 1, 1);

    GtkWidget *btn_clear_view = create_icon_only_button(ICON_TRASH, 20, "Pulisci");
    g_signal_connect(btn_clear_view, "clicked", G_CALLBACK(on_clear_view_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_clear_view, 1, 1, 1, 1);

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), sep1, 0, 2, 2, 1);

    GtkWidget *btn_select_all = create_icon_only_button(ICON_SELEZIONA_TUTTO, 20, "Tutti");
    g_signal_connect(btn_select_all, "clicked", G_CALLBACK(on_select_all), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_select_all, 0, 3, 1, 1);

    GtkWidget *btn_deselect_all = create_icon_only_button(ICON_DESELEZIONA, 20, "Nessuno");
    g_signal_connect(btn_deselect_all, "clicked", G_CALLBACK(on_deselect_all), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_deselect_all, 1, 3, 1, 1);

    GtkWidget *btn_show_selected = create_icon_only_button(ICON_AGGIORNA, 20, "Ref");
    g_signal_connect(btn_show_selected, "clicked", G_CALLBACK(on_show_selected_only), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_show_selected, 0, 4, 1, 1);

    GtkWidget *btn_show_all = create_icon_only_button(ICON_VIEW_ALL, 20, "Tutte");
    g_signal_connect(btn_show_all, "clicked", G_CALLBACK(on_show_all), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_show_all, 1, 4, 1, 1);

    GtkWidget *btn_edit = create_icon_only_button(ICON_EDITOR, 20, "Mod.");
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_edit_image_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_edit, 0, 5, 1, 1);

    GtkWidget *btn_secondary = create_icon_only_button(ICON_MONITOR, 20, "Client");
    g_signal_connect(btn_secondary, "clicked", G_CALLBACK(on_open_secondary_viewer), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_secondary, 1, 5, 1, 1);

    // Aggiungiamo tutte le classi "square-button"
    GtkWidget *all_btns[] = {btn_folder, btn_clear_view, btn_select_all, btn_deselect_all, btn_show_selected, btn_show_all, btn_edit, btn_secondary};
    for(int i=0; i<8; i++) gtk_style_context_add_class(gtk_widget_get_style_context(all_btns[i]), "square-button");

    /* Impacchettiamo la griglia in alto */
    gtk_box_pack_start(GTK_BOX(main_container), grid, FALSE, FALSE, 0);

    /* 2. PULSANTE STAMPA AI PIEDI (Copyright eliminato) */
    GtkWidget *btn_print = create_icon_only_button(ICON_STAMPA_VIEW, 20, "Stampa");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_print), "square-button");
    g_signal_connect(btn_print, "clicked", G_CALLBACK(on_print_button_clicked), NULL);
    
    // pack_end lo spinge in fondo alla sidebar
    gtk_box_pack_end(GTK_BOX(main_container), btn_print, FALSE, FALSE, 0);

    return main_container;
}

GtkWidget *create_print_sidebar(void) {
    /* Contenitore principale verticale */
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(main_container), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_container), "sidebar");

    /* 1. GRIGLIA SUPERIORE (Informazioni e Titoli) */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    // Titolo della sezione (Riga 0)
    GtkWidget *label = gtk_label_new("🖨 STAMPA");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 2, 1);

    // Etichetta Coda di Stampa (Riga 1)
    GtkWidget *lbl_queue = gtk_label_new("Coda di Stampa");
    gtk_widget_set_halign(lbl_queue, GTK_ALIGN_START); // Allinea a sinistra
    gtk_grid_attach(GTK_GRID(grid), lbl_queue, 0, 1, 2, 1);

    /* Impacchettiamo la griglia in alto */
    gtk_box_pack_start(GTK_BOX(main_container), grid, FALSE, FALSE, 0);

    /* 2. PULSANTE "STAMPA ORA" AI PIEDI */
    GtkWidget *btn_print_now = gtk_button_new_with_label("STAMPA ORA");
    g_signal_connect(btn_print_now, "clicked", G_CALLBACK(on_print_selected), NULL);
    
    // Lo rendiamo speciale con la classe dei tuoi pulsanti oro
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_print_now), "suggested-action");
    
    // pack_end lo spinge in fondo
    gtk_box_pack_end(GTK_BOX(main_container), btn_print_now, FALSE, FALSE, 0);

    return main_container;
}

void on_config_button_clicked(GtkButton *button, gpointer window) {
    on_tab_clicked(button, (gpointer)"config");
}

/* Splash screen data for timeout callback */
typedef struct {
    GtkWidget *splash_window;
    GtkWidget *main_window;
} SplashData;

static gboolean splash_timeout_callback(gpointer user_data) {
    SplashData *data = (SplashData *)user_data;
    gtk_widget_destroy(data->splash_window);
    gtk_widget_show_all(data->main_window);
    gtk_window_maximize(GTK_WINDOW(data->main_window));
    g_free(data);
    return FALSE;
}

int main(int argc, char *argv[]) {
    char *exe_dir;
    char icons_dir[1024];
    char css_path[1024];
    const char *css_locations[3];

    gtk_init(&argc, &argv);

    /* NUOVO: Scala dimensioni griglia in base allo schermo */
    GdkScreen *screen = gdk_screen_get_default();
    if (screen) {
        int screen_width = gdk_screen_get_width(screen);
        if (screen_width < 1400) {  // Schermi piccoli (19")
            extern int CELL_WIDTH, CELL_HEIGHT, SECONDARY_CELL_WIDTH, SECONDARY_CELL_HEIGHT;
            CELL_WIDTH = 320;
            CELL_HEIGHT = 200;
            SECONDARY_CELL_WIDTH = 420;
            SECONDARY_CELL_HEIGHT = 260;
            g_message("Schermo piccolo rilevato (%dpx): griglia scalata", screen_width);
        }
    }

    /* NUOVO: Inizializza directory applicazione */
    if (ensure_app_directories_exist() != 0) {
        GtkWidget *error_dialog = gtk_message_dialog_new(NULL,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Impossibile creare le directory dell'applicazione.\n"
            "Verificare i permessi di scrittura.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return 1;
    }

    /* Log percorsi per debug */
    g_message("App data directory: %s", get_app_data_dir());
    g_message("Originals: %s", get_originals_base_path());
    g_message("Thumbnails: %s", get_thumbnails_base_path());

    /* Get executable directory once */
    exe_dir = g_path_get_dirname(argv[0]);

    /* Initialize icon manager */
    snprintf(icons_dir, sizeof(icons_dir), "%s/icons", exe_dir);
    strncpy(global_icons_dir, icons_dir, sizeof(global_icons_dir) - 1);
    icon_manager_init(icons_dir);

    /* Load configuration from file */
    load_config();

    /* Load CSS theme */
    GtkCssProvider *css_provider;
    GError *error = NULL;
    int i;
    gboolean css_loaded;

    css_provider = gtk_css_provider_new();
    error = NULL;
    css_loaded = FALSE;

    /* Build CSS path from executable directory */
    snprintf(css_path, sizeof(css_path), "%s/style.css", exe_dir);

    /* Prova diverse posizioni */
    css_locations[0] = css_path;          /* Nella directory dell'exe */
    css_locations[1] = ".\\style.css";    /* Directory corrente */
    css_locations[2] = "..\\style.css";   /* Directory parent */

    for (i = 0; i < 3; i++) {
        g_info("Trying to load CSS from: %s", css_locations[i]);

        if (gtk_css_provider_load_from_path(css_provider, css_locations[i], &error)) {
            g_info("CSS loaded successfully from: %s", css_locations[i]);
            css_loaded = TRUE;
            break;
        } else {
            if (error) {
                g_warning("Failed to load CSS from %s: %s", css_locations[i], error->message);
                g_error_free(error);
                error = NULL;
            }
        }
    }

    if (css_loaded) {
        GdkDisplay *display = gdk_display_get_default();
        GdkScreen *screen = gdk_display_get_default_screen(display);
        gtk_style_context_add_provider_for_screen(screen,
                                                   GTK_STYLE_PROVIDER(css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } else {
        g_warning("Could not load CSS from any location!");
    }

    g_object_unref(css_provider);

    GtkWidget *header_bar, *stack, *main_vbox, *tabs_box;
    GtkWidget *btn_config, *btn_import, *btn_view;

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1200, 800);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Ikona - Image Manager");
    gtk_window_set_titlebar(GTK_WINDOW(main_window), header_bar);

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), main_vbox);

    // NAVBAR
    tabs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(tabs_box), "top-bar");
    gtk_box_pack_start(GTK_BOX(main_vbox), tabs_box, FALSE, FALSE, 0);

    btn_import = create_button_with_icon(ICON_IMPORTA, "Importa", 37);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_import), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_import), GTK_RELIEF_NONE);
    g_signal_connect(btn_import, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"import");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_import, FALSE, FALSE, 12);

    btn_view = create_button_with_icon(ICON_FOTO, "Foto", 37);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_view), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_view), GTK_RELIEF_NONE);
    g_signal_connect(btn_view, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"view");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_view, FALSE, FALSE, 0);

    btn_config = create_button_with_icon(ICON_CONFIG, "Config", 37);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_config), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_config), GTK_RELIEF_NONE);
    g_signal_connect(btn_config, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"config");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_config, FALSE, FALSE, 0);

    GtkWidget *btn_report = create_button_with_icon(ICON_REPORT, "Report", 37);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_report), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_report), GTK_RELIEF_NONE);
    g_signal_connect(btn_report, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"report");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_report, FALSE, FALSE, 0);

    // MAIN CONTENT AREA with sidebar + stack
    GtkWidget *main_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(main_content, TRUE);
    gtk_widget_set_vexpand(main_content, TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), main_content, TRUE, TRUE, 0);

    // Stack for sidebar (changes per tab)
    GtkWidget *sidebar_stack = gtk_stack_new();
    global_sidebar_stack = sidebar_stack;
    gtk_widget_set_size_request(sidebar_stack, 140, -1);
    gtk_stack_set_transition_type(GTK_STACK(sidebar_stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_box_pack_start(GTK_BOX(main_content), sidebar_stack, FALSE, FALSE, 0);

    // Stack for main content (pages)
    stack = gtk_stack_new();
    global_stack = stack;
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_widget_set_hexpand(stack, TRUE);
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_box_pack_start(GTK_BOX(main_content), stack, TRUE, TRUE, 0);

    // Grid page (shared for Import/View)
    GtkWidget *grid_page = create_image_grid();
    gtk_stack_add_titled(GTK_STACK(stack), grid_page, "grid", "Grid");

    // Config page
    GtkWidget *config_page = create_config_page(main_window);
    gtk_stack_add_titled(GTK_STACK(stack), config_page, "config", "Config");

    // Report page will be created on demand in on_tab_clicked()

    // Add sidebars for each tab
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_import_sidebar(), "import", "Import");
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_view_sidebar(), "view", "View");
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_config_sidebar(), "config", "Config");
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_report_sidebar(), "report", "Report");

    // Set default tabs
    gtk_stack_set_visible_child_name(GTK_STACK(sidebar_stack), "import");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "grid");

    GVolumeMonitor *volume_monitor = g_volume_monitor_get();
    g_signal_connect(volume_monitor, "drive-connected", G_CALLBACK(on_drive_connected), NULL);
    g_signal_connect(volume_monitor, "drive-disconnected", G_CALLBACK(on_drive_disconnected), NULL);

    /* Create and show splash screen */
    GtkWidget *splash_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(splash_window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
    gtk_window_set_decorated(GTK_WINDOW(splash_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(splash_window), GTK_WIN_POS_CENTER);

    char splash_path[2048];
    snprintf(splash_path, sizeof(splash_path), "%s/ikona-splash.png", global_icons_dir);

    GError *splash_error = NULL;
    GdkPixbuf *splash_pixbuf = gdk_pixbuf_new_from_file(splash_path, &splash_error);

    if (splash_pixbuf) {
        GtkWidget *splash_image = gtk_image_new_from_pixbuf(splash_pixbuf);
        gtk_container_add(GTK_CONTAINER(splash_window), splash_image);
        g_object_unref(splash_pixbuf);
    } else {
        GtkWidget *splash_label = gtk_label_new("Ikona");
        gtk_container_add(GTK_CONTAINER(splash_window), splash_label);
        if (splash_error) g_error_free(splash_error);
    }

    gtk_widget_show_all(splash_window);

    /* Show main window after 2 seconds */
    SplashData *splash_data = g_malloc(sizeof(SplashData));
    splash_data->splash_window = splash_window;
    splash_data->main_window = main_window;

    g_timeout_add(2000, splash_timeout_callback, splash_data);

    gtk_main();

    g_free(exe_dir);
    return 0;
}
