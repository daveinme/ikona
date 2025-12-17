#include "ikona.h"
#include "importer.h"
#include "viewer.h"
#include "printer.h"
#include "icon_manager.h"

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct {
    char *path;
    int copies;
    GtkWidget *spin_button;
} PrintItem;

GtkWidget *label_copy_status;
GtkWidget *grid_container;
GtkWidget *page_label;
GtkWidget *label_auto_photo_count;  // Label for number of photos in Auto tab
char src_folder[512] = "";
char base_dst_folder[512] = "";
char view_folder[512] = "";
char *image_files[10000];
int total_images = 0;
int current_page = 0;
int images_per_page = 9;

/* Auto-Import SD configuration */
char sd_drive_letters[256] = "";  // Comma-separated drive letters (es: "E:,F:,G:")
gboolean auto_import_enabled = FALSE;
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

GtkWidget *global_stack = NULL;  // Stack per switchare tra tab
GtkWidget *global_sidebar_stack = NULL;  // Stack per sidebar dinamico
GtkWidget *import_spinner = NULL;  // Global spinner for import operations


GtkWidget *create_view_page(GtkWidget *window);
GtkWidget *create_print_page();
GtkWidget *create_config_page(GtkWidget *window);
GtkWidget *create_import_sidebar(void);
GtkWidget *create_view_sidebar(void);
GtkWidget *create_print_sidebar(void);
GtkWidget *create_empty_sidebar(void);
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

void on_tab_clicked(GtkButton *button, gpointer tab_name) {
    const char *tab = (const char *)tab_name;

    if (global_sidebar_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_sidebar_stack), tab);
    }

    if (global_stack) {
        if (strcmp(tab, "print") == 0) {
            gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "print");
        } else if (strcmp(tab, "config") != 0) {
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
}

void on_import_auto_tab(GtkButton *button, gpointer data) {
    if (import_sidebar_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(import_sidebar_stack), "auto");
    }
}

void on_open_secondary_viewer(GtkButton *button, gpointer data) {
    if (!secondary_viewer_window) {
        create_secondary_viewer_window();
    }
    secondary_viewer_display_page(); // Ensure it displays current images
    gtk_widget_show_all(secondary_viewer_window);
    gtk_window_present(GTK_WINDOW(secondary_viewer_window));
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

    btn_dst = create_icon_only_button(ICON_FOLDER_FILL, 18, "Seleziona cartella destinazione");
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

    btn_view = create_icon_only_button(ICON_FILE_IMAGE, 18, "Seleziona cartella visualizzazione");
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

    return grid;
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
                        dialog = gtk_message_dialog_new(NULL,
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
    GtkWidget *grid;
    GList *l;
    int row, col;

    /* Pulisci container */
    gtk_container_foreach(GTK_CONTAINER(print_grid_container),
                         (GtkCallback)gtk_widget_destroy, NULL);

    if (g_list_length(print_list) == 0) {
        GtkWidget *empty_label = gtk_label_new("Nessuna foto in lista stampa.\nAggiungi foto dalla tab View.");
        gtk_box_pack_start(GTK_BOX(print_grid_container), empty_label, TRUE, TRUE, 0);
        gtk_widget_show_all(print_grid_container);
        return;
    }

    /* Crea griglia 3 colonne */
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 15);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 15);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

    row = 0;
    col = 0;

    for (l = print_list; l != NULL; l = l->next) {
        PrintItem *item;
        GtkWidget *card;
        GtkWidget *vbox;
        GtkWidget *thumbnail;
        GtkWidget *label;
        GtkWidget *copies_hbox;
        GtkWidget *copies_label;
        GError *error;
        GdkPixbuf *pixbuf;

        item = (PrintItem *)l->data;

        /* Card per ogni foto */
        card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_ETCHED_IN);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

        /* Thumbnail 150x150 */
        error = NULL;
        pixbuf = gdk_pixbuf_new_from_file_at_scale(item->path, 150, 150, TRUE, &error);
        if (pixbuf) {
            thumbnail = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(pixbuf);
        } else {
            thumbnail = gtk_label_new("📷");
            if (error) g_error_free(error);
        }
        gtk_box_pack_start(GTK_BOX(vbox), thumbnail, FALSE, FALSE, 0);

        /* Nome file */
        label = gtk_label_new(g_path_get_basename(item->path));
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 20);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

        /* Copie */
        copies_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_halign(copies_hbox, GTK_ALIGN_CENTER);

        copies_label = gtk_label_new("Copie:");
        gtk_box_pack_start(GTK_BOX(copies_hbox), copies_label, FALSE, FALSE, 0);

        item->spin_button = gtk_spin_button_new_with_range(1, 99, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(item->spin_button), 1);
        gtk_widget_set_size_request(item->spin_button, 70, -1);
        gtk_box_pack_start(GTK_BOX(copies_hbox), item->spin_button, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), copies_hbox, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(card), vbox);
        gtk_grid_attach(GTK_GRID(grid), card, col, row, 1, 1);

        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }

    gtk_container_add(GTK_CONTAINER(print_grid_container), grid);
    gtk_widget_show_all(print_grid_container);
}

void on_add_to_print_list(GtkButton *button, gpointer data) {
    int i;
    int added_count;
    GList *l;

    if (selected_count == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
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
        image_path = image_files[img_idx];

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
    const gchar *selected_printer;
    GList *l;
    int total_prints;
    int total_files;
    int success_count;
    GtkWidget *dialog;
    char summary[512];
    gint response;
    int delay_ms;

    if (g_list_length(print_list) == 0) {
        dialog = gtk_message_dialog_new(NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       "Lista di stampa vuota!");
        gtk_window_set_title(GTK_WINDOW(dialog), "Attenzione");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    /* Ottieni stampante selezionata */
    selected_printer = gtk_combo_box_get_active_id(GTK_COMBO_BOX(printer_combo_box));
    if (!selected_printer || strcmp(selected_printer, "none") == 0) {
        dialog = gtk_message_dialog_new(NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       "Nessuna stampante selezionata!");
        gtk_window_set_title(GTK_WINDOW(dialog), "Attenzione");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    /* Calcola totale stampe */
    total_prints = 0;
    total_files = g_list_length(print_list);
    for (l = print_list; l != NULL; l = l->next) {
        PrintItem *item = (PrintItem *)l->data;
        int copies = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(item->spin_button));
        total_prints += copies;
    }

    /* PRIMO DIALOGO: Riepilogo */
    snprintf(summary, sizeof(summary),
             "Stampante: %s\n\n"
             "File da stampare: %d\n"
             "Totale stampe: %d\n\n"
             "Continuare?",
             selected_printer, total_files, total_prints);

    dialog = gtk_message_dialog_new(NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_YES_NO,
                                   "%s", summary);
    gtk_window_set_title(GTK_WINDOW(dialog), "Conferma Stampa");
    apply_print_dialog_style(dialog);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_YES) {
        g_info("Stampa annullata dall'utente");
        return;
    }

    /* SECONDO DIALOGO: Conferma finale */
    dialog = gtk_message_dialog_new(NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_YES_NO,
                                   "Sei sicuro di voler stampare %d copie?\n\n"
                                   "Questa azione non può essere annullata.",
                                   total_prints);
    gtk_window_set_title(GTK_WINDOW(dialog), "Conferma Finale");
    apply_print_dialog_style(dialog);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_YES) {
        g_info("Stampa annullata dall'utente (conferma finale)");
        return;
    }

    /* Crea dialogo di progresso */
    dialog = gtk_message_dialog_new(NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_NONE,
                                   "Stampa in corso...\n0/%d", total_prints);
    gtk_window_set_title(GTK_WINDOW(dialog), "Stampa");
    gtk_widget_show_all(dialog);

    success_count = 0;
    delay_ms = 500; /* 500ms di timeout tra stampe */

    g_info("Avvio stampa di %d file (%d copie totali) su: %s", total_files, total_prints, selected_printer);

    /* Stampa ogni file con timeout */
    for (l = print_list; l != NULL; l = l->next) {
        PrintItem *item;
        int copies;
        int copy_num;
        char progress_text[256];

        item = (PrintItem *)l->data;
        copies = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(item->spin_button));

        g_info("Stampa %s (%d copie)", item->path, copies);

        /* Stampa N copie con timeout */
        for (copy_num = 0; copy_num < copies; copy_num++) {
            int current_print;

            current_print = success_count + 1;

            /* Aggiorna dialogo progresso */
            snprintf(progress_text, sizeof(progress_text),
                    "Stampa in corso...\n%d/%d\n\n%s (copia %d/%d)",
                    current_print, total_prints,
                    g_path_get_basename(item->path),
                    copy_num + 1, copies);
            gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), progress_text);

            /* Processa eventi GTK per aggiornare UI */
            while (gtk_events_pending()) {
                gtk_main_iteration();
            }

            /* Stampa */
            if (print_file(selected_printer, item->path) == 0) {
                success_count++;
                g_info("Stampa riuscita: %s (copia %d/%d)", item->path, copy_num + 1, copies);
            } else {
                g_warning("Errore nella stampa di: %s (copia %d/%d)", item->path, copy_num + 1, copies);
            }

            /* Timeout tra stampe (eccetto ultima) */
            if (success_count < total_prints) {
                g_usleep(delay_ms * 1000);
            }
        }
    }

    gtk_widget_destroy(dialog);

    /* Mostra risultato finale */
    if (success_count == total_prints) {
        dialog = gtk_message_dialog_new(NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_INFO,
                                       GTK_BUTTONS_OK,
                                       "Stampa completata con successo!\n\n"
                                       "Stampe inviate: %d/%d",
                                       success_count, total_prints);
        gtk_window_set_title(GTK_WINDOW(dialog), "Successo");
        g_info("Tutte le %d stampe inviate con successo!", success_count);
    } else {
        dialog = gtk_message_dialog_new(NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       "Stampa completata con errori.\n\n"
                                       "Stampe riuscite: %d/%d",
                                       success_count, total_prints);
        gtk_window_set_title(GTK_WINDOW(dialog), "Attenzione");
        g_warning("Stampate %d/%d copie", success_count, total_prints);
    }

    apply_print_dialog_style(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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

    btn_prev = create_icon_only_button(ICON_ARROW_LEFT, 20, "Previous");
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_prev_page), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_box), btn_prev, FALSE, FALSE, 0);

    page_label = gtk_label_new("");
    gtk_widget_set_halign(page_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(bottom_box), page_label, TRUE, FALSE, 0);

    btn_next = create_icon_only_button(ICON_ARROW_RIGHT, 20, "Next");
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

GtkWidget *create_import_sidebar(void) {
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    GtkWidget *label = gtk_label_new("📥 Import");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 0);

    // Tabs: Manuale / Auto
    GtkWidget *tabs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *btn_manuale = gtk_button_new_with_label("Manuale");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_manuale), "flat");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_manuale, TRUE, TRUE, 0);

    GtkWidget *btn_auto = gtk_button_new_with_label("Auto");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_auto), "flat");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_auto, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(sidebar), tabs_box, FALSE, FALSE, 0);

    // Stack for Manuale/Auto content
    GtkWidget *import_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(import_stack), GTK_STACK_TRANSITION_TYPE_NONE);

    // Manuale content
    GtkWidget *manuale_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *lbl_src = gtk_label_new("Origine:");
    gtk_box_pack_start(GTK_BOX(manuale_box), lbl_src, FALSE, FALSE, 0);
    GtkWidget *btn_src = gtk_button_new_with_label("Sfoglia...");
    g_signal_connect(btn_src, "clicked", G_CALLBACK(on_select_src_folder), NULL);
    gtk_box_pack_start(GTK_BOX(manuale_box), btn_src, FALSE, FALSE, 0);

    GtkWidget *lbl_dst = gtk_label_new("Destinazione:");
    gtk_box_pack_start(GTK_BOX(manuale_box), lbl_dst, FALSE, FALSE, 0);
    GtkWidget *btn_dst = gtk_button_new_with_label("Sfoglia...");
    g_signal_connect(btn_dst, "clicked", G_CALLBACK(on_select_dst_folder), NULL);
    gtk_box_pack_start(GTK_BOX(manuale_box), btn_dst, FALSE, FALSE, 0);

    // Create spinner for import
    if (!import_spinner) {
        import_spinner = gtk_spinner_new();
    }
    gtk_box_pack_start(GTK_BOX(manuale_box), import_spinner, FALSE, FALSE, 0);

    label_copy_status = gtk_label_new("Sorgente: ");
    gtk_box_pack_start(GTK_BOX(manuale_box), label_copy_status, FALSE, FALSE, 0);

    GtkWidget *btn_importa = gtk_button_new_with_label("Importa");
    g_signal_connect(btn_importa, "clicked", G_CALLBACK(on_import_button_clicked), import_spinner);
    gtk_box_pack_end(GTK_BOX(manuale_box), btn_importa, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(import_stack), manuale_box, "manuale");

    // Auto content
    GtkWidget *auto_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    // Label showing number of photos to import
    label_auto_photo_count = gtk_label_new("0 Foto da importare");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_auto_photo_count), "info");
    gtk_box_pack_start(GTK_BOX(auto_box), label_auto_photo_count, FALSE, FALSE, 0);

    // Import button
    GtkWidget *btn_auto_import = gtk_button_new_with_label("Importa");
    gtk_widget_set_size_request(btn_auto_import, -1, 50);
    g_signal_connect(btn_auto_import, "clicked", G_CALLBACK(on_copy_jpg_auto), NULL);
    gtk_box_pack_start(GTK_BOX(auto_box), btn_auto_import, FALSE, FALSE, 0);

    // Progress bar (hidden until import starts)
    GtkWidget *progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Importazione...");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    gtk_widget_set_visible(progress_bar, FALSE);
    gtk_box_pack_start(GTK_BOX(auto_box), progress_bar, FALSE, FALSE, 0);

    // Update/Scan button
    GtkWidget *btn_scan = gtk_button_new_with_label("Aggiorna Scan");
    g_signal_connect(btn_scan, "clicked", G_CALLBACK(on_scan_sd_for_auto), NULL);
    gtk_box_pack_end(GTK_BOX(auto_box), btn_scan, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(import_stack), auto_box, "auto");

    gtk_box_pack_start(GTK_BOX(sidebar), import_stack, TRUE, TRUE, 0);

    // Save reference to import stack for tab switching
    import_sidebar_stack = import_stack;

    // Connect tab buttons
    g_signal_connect(btn_manuale, "clicked", G_CALLBACK(on_import_manual_tab), NULL);
    g_signal_connect(btn_auto, "clicked", G_CALLBACK(on_import_auto_tab), NULL);

    return sidebar;
}

void on_clear_view_clicked(GtkButton *button, gpointer data) {
    total_images = 0;
    current_page = 0;
    strcpy(view_folder, "");
    display_page();
}

GtkWidget *create_view_sidebar(void) {
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    GtkWidget *label = gtk_label_new("🖼 View");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 0);

    GtkWidget *btn_folder = gtk_button_new_with_label("Apri Cartella");
    g_signal_connect(btn_folder, "clicked", G_CALLBACK(on_select_view_folder), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_folder, FALSE, FALSE, 0);

    GtkWidget *btn_clear_view = gtk_button_new_with_label("Pulisci Visualizzazione");
    g_signal_connect(btn_clear_view, "clicked", G_CALLBACK(on_clear_view_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_clear_view, FALSE, FALSE, 0);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(sidebar), separator, FALSE, FALSE, 0);

    GtkWidget *btn_select_all = gtk_button_new_with_label("Seleziona Tutto");
    g_signal_connect(btn_select_all, "clicked", G_CALLBACK(on_select_all), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_select_all, FALSE, FALSE, 0);

    GtkWidget *btn_deselect_all = gtk_button_new_with_label("Deseleziona Tutto");
    g_signal_connect(btn_deselect_all, "clicked", G_CALLBACK(on_deselect_all), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_deselect_all, FALSE, FALSE, 0);

    GtkWidget *btn_toggle_filter = gtk_button_new_with_label("👁 Mostra Selezione");
    g_signal_connect(btn_toggle_filter, "clicked", G_CALLBACK(on_toggle_filter), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_toggle_filter, FALSE, FALSE, 0);

    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(sidebar), separator, FALSE, FALSE, 0);

    GtkWidget *btn_secondary = gtk_button_new_with_label("🪟 Finestra 2");
    g_signal_connect(btn_secondary, "clicked", G_CALLBACK(on_open_secondary_viewer), NULL);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_secondary, FALSE, FALSE, 0);

    GtkWidget *btn_print = gtk_button_new_with_label("Stampa");
    g_signal_connect(btn_print, "clicked", G_CALLBACK(on_add_to_print_list), NULL);
    gtk_box_pack_end(GTK_BOX(sidebar), btn_print, FALSE, FALSE, 0);

    return sidebar;
}

GtkWidget *create_print_sidebar(void) {
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");

    GtkWidget *label = gtk_label_new("🖨 Print");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "title");
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 0);

    GtkWidget *lbl_queue = gtk_label_new("Coda di Stampa");
    gtk_box_pack_start(GTK_BOX(sidebar), lbl_queue, FALSE, FALSE, 0);

    GtkWidget *btn_print_now = gtk_button_new_with_label("Stampa Ora");
    g_signal_connect(btn_print_now, "clicked", G_CALLBACK(on_print_selected), NULL);
    gtk_box_pack_end(GTK_BOX(sidebar), btn_print_now, FALSE, FALSE, 0);

    return sidebar;
}

void on_config_button_clicked(GtkButton *button, gpointer window) {
    GtkWidget *config_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(config_window), "Configurazione");
    gtk_window_set_transient_for(GTK_WINDOW(config_window), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(config_window), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(config_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(config_window), 400, 500);

    GtkWidget *config_grid = create_config_page(config_window);
    gtk_container_add(GTK_CONTAINER(config_window), config_grid);

    gtk_widget_show_all(config_window);
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    char *exe_dir;
    char icons_dir[1024];
    char css_path[1024];
    const char *css_locations[3];

    gtk_init(&argc, &argv);

    /* Get executable directory once */
    exe_dir = g_path_get_dirname(argv[0]);

    /* Initialize icon manager */
    snprintf(icons_dir, sizeof(icons_dir), "%s/icons", exe_dir);
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
    GtkWidget *btn_config, *btn_import, *btn_view, *btn_print;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Ikona - Image Manager");
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

    // NAVBAR
    tabs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(tabs_box), "top-bar");
    gtk_box_pack_start(GTK_BOX(main_vbox), tabs_box, FALSE, FALSE, 0);

    btn_import = gtk_button_new_with_label("📥 Import");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_import), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_import), GTK_RELIEF_NONE);
    g_signal_connect(btn_import, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"import");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_import, FALSE, FALSE, 12);

    btn_view = gtk_button_new_with_label("🖼 View");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_view), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_view), GTK_RELIEF_NONE);
    g_signal_connect(btn_view, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"view");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_view, FALSE, FALSE, 0);

    btn_print = gtk_button_new_with_label("🖨 Print");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_print), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_print), GTK_RELIEF_NONE);
    g_signal_connect(btn_print, "clicked", G_CALLBACK(on_tab_clicked), (gpointer)"print");
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_print, FALSE, FALSE, 0);

    btn_config = gtk_button_new_with_label("⚙ Config");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_config), "flat");
    gtk_button_set_relief(GTK_BUTTON(btn_config), GTK_RELIEF_NONE);
    g_signal_connect(btn_config, "clicked", G_CALLBACK(on_config_button_clicked), window);
    gtk_box_pack_start(GTK_BOX(tabs_box), btn_config, FALSE, FALSE, 0);

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

    // Grid page (shared for Import/View/Print)
    GtkWidget *grid_page = create_image_grid();
    gtk_stack_add_titled(GTK_STACK(stack), grid_page, "grid", "Grid");

    // Print page
    GtkWidget *print_page = create_print_page();
    gtk_stack_add_titled(GTK_STACK(stack), print_page, "print", "Print");

    // Add sidebars for each tab
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_import_sidebar(), "import", "Import");
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_view_sidebar(), "view", "View");
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_print_sidebar(), "print", "Print");
    gtk_stack_add_titled(GTK_STACK(sidebar_stack), create_empty_sidebar(), "config", "Config");

    // Set default tabs
    gtk_stack_set_visible_child_name(GTK_STACK(sidebar_stack), "import");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "grid");

    GVolumeMonitor *volume_monitor = g_volume_monitor_get();
    g_signal_connect(volume_monitor, "drive-connected", G_CALLBACK(on_drive_connected), NULL);
    g_signal_connect(volume_monitor, "drive-disconnected", G_CALLBACK(on_drive_disconnected), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    g_free(exe_dir);
    return 0;
}
