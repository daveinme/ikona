#include "printer.h"
#include "icon_manager.h"
#include "gui_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>

#ifdef _WIN32
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include <wingdi.h>
#endif

Printer* get_available_printers(int *count) {
    Printer *printers;

    *count = 0;
    printers = NULL;

#ifdef _WIN32
    /* Windows: usa EnumPrinters API */
    DWORD needed, returned;
    PRINTER_INFO_4 *printer_info;
    DWORD i;

    needed = 0;
    returned = 0;

    /* Prima chiamata per ottenere la dimensione necessaria */
    EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, NULL, 0, &needed, &returned);

    if (needed == 0) {
        g_warning("No printers found on Windows");
        return NULL;
    }

    printer_info = (PRINTER_INFO_4 *)malloc(needed);
    if (!printer_info) {
        g_warning("Failed to allocate memory for printer info");
        return NULL;
    }

    /* Seconda chiamata per ottenere effettivamente le stampanti */
    if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4,
                      (LPBYTE)printer_info, needed, &needed, &returned)) {
        g_warning("Failed to enumerate printers");
        free(printer_info);
        return NULL;
    }

    if (returned == 0) {
        g_warning("No printers available");
        free(printer_info);
        return NULL;
    }

    printers = (Printer *)malloc(sizeof(Printer) * returned);

    for (i = 0; i < returned; i++) {
        printers[i].name = g_strdup(printer_info[i].pPrinterName);
        printers[i].display_name = g_strdup(printer_info[i].pPrinterName);
    }

    *count = returned;
    free(printer_info);
    return printers;

#else
    /* Unix/Linux: usa lpstat */
    FILE *fp;
    char line[512];
    int capacity;
    int current;

    fp = popen("lpstat -p 2>/dev/null", "r");
    if (!fp) {
        g_warning("Failed to execute lpstat command");
        return NULL;
    }

    capacity = 10;
    current = 0;
    printers = (Printer *)malloc(sizeof(Printer) * capacity);

    while (fgets(line, sizeof(line), fp)) {
        size_t len;
        char printer_name[256];

        /* Rimuovi newline */
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        memset(printer_name, 0, sizeof(printer_name));

        /* Prova pattern inglese "printer <name> is" */
        if (sscanf(line, "printer %s is", printer_name) == 1) {
            /* Trovato! */
        }
        /* Prova pattern italiano "la stampante <name> è" */
        else if (sscanf(line, "la stampante %s è", printer_name) == 1) {
            /* Trovato! */
        }
        else {
            continue;
        }

        /* Salta linee vuote */
        if (strlen(printer_name) == 0) continue;

        /* Ridimensiona array se necessario */
        if (current >= capacity) {
            capacity *= 2;
            printers = (Printer *)realloc(printers, sizeof(Printer) * capacity);
        }

        printers[current].name = g_strdup(printer_name);
        printers[current].display_name = g_strdup(printer_name);
        current++;
    }

    pclose(fp);

    if (current == 0) {
        g_warning("No printers found");
        free(printers);
        return NULL;
    }

    *count = current;
    return printers;
#endif
}

void free_printers(Printer *printers, int count) {
    int i;

    if (!printers) return;

    for (i = 0; i < count; i++) {
        g_free(printers[i].name);
        g_free(printers[i].display_name);
    }
    free(printers);
}

int print_file(const char *printer_name, const char *file_path) {
#ifdef _WIN32
    char abs_path[MAX_PATH];
    HINSTANCE exec_result;
    WIN32_FIND_DATA find_data;
    HANDLE find_handle;
#else
    char command[512];
    int result;
#endif

    if (!printer_name || !file_path) {
        g_warning("Invalid printer name or file path");
        return -1;
    }

#ifdef _WIN32
    g_info("Stampa su Windows: printer='%s', file='%s'", printer_name, file_path);

    /* Converti a percorso assoluto */
    if (GetFullPathName(file_path, sizeof(abs_path), abs_path, NULL) == 0) {
        g_warning("Impossibile convertire percorso assoluto: %s", file_path);
        return -1;
    }

    /* Controlla se il file esiste */
    find_handle = FindFirstFile(abs_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        g_warning("File non trovato: %s", abs_path);
        return -1;
    }
    FindClose(find_handle);

    g_info("File trovato: %s", abs_path);

    /* Attendi un po' prima di stampare (per stampanti WiFi) */
    g_info("Attesa 2 secondi prima di inviare il job (per stampanti WiFi)");
    Sleep(2000);

    /* Usa ShellExecute con verbo "print" */
    g_info("ShellExecute print: printer='%s', file='%s'", printer_name, abs_path);

    exec_result = ShellExecute(NULL, "print", abs_path, NULL, NULL, SW_HIDE);

    g_info("ShellExecute print risultato: %p", exec_result);

    if ((intptr_t)exec_result > 32) {
        g_info("Stampa inviata con successo a: %s", printer_name);
        Sleep(1000);
        return 0;
    }

    /* Se fallisce */
    g_warning("ShellExecute fallito (errore %p)", exec_result);
    return -1;

#else
    /* Unix/Linux: usa comando 'lp' */
    snprintf(command, sizeof(command), "lp -d %s \"%s\" 2>/dev/null", printer_name, file_path);

    result = system(command);

    if (result != 0) {
        g_warning("Failed to print file with lp command. Exit code: %d", result);
        return -1;
    }

    g_info("Print job sent successfully to printer: %s for file: %s", printer_name, file_path);
    return 0;
#endif
}

/* External variables from ikona.c */
extern GList *print_list;
extern GtkWidget *print_grid_container;
extern Printer *available_printers;
extern int printer_count;
extern char default_printer[];

extern void on_remove_from_print_list(GtkButton *button, gpointer data);
extern void on_print_selected(GtkButton *button, gpointer data);
extern void refresh_print_grid(void);

static GtkWidget *print_window = NULL;
static GtkWidget *printer_combo_box = NULL;
static GtkWidget *print_grid_container_local = NULL;
static guint print_list_cleanup_timeout_id = 0;  // NUOVO: ID timeout per pulizia automatica

// NUOVO: Callback timeout per pulire la lista dopo 30 secondi
static gboolean cleanup_print_list_timeout(gpointer data) {
    g_info("Timeout scaduto: pulizia automatica lista stampa");
    on_remove_from_print_list(NULL, NULL);
    print_list_cleanup_timeout_id = 0;
    return G_SOURCE_REMOVE;  // Rimuovi timeout dopo esecuzione
}

// NUOVO: Cancella timeout se attivo (chiamato quando si stampa)
void cancel_print_list_cleanup_timeout(void) {
    if (print_list_cleanup_timeout_id > 0) {
        g_source_remove(print_list_cleanup_timeout_id);
        print_list_cleanup_timeout_id = 0;
        g_info("Timeout pulizia lista stampa cancellato");
    }
}

static void on_print_window_destroyed(GtkWidget *widget, gpointer data) {
    print_window = NULL;
    printer_combo_box = NULL;
    print_grid_container_local = NULL;

    // NUOVO: Avvia timeout di 30 secondi per pulire la lista se non stampato
    if (print_list_cleanup_timeout_id > 0) {
        g_source_remove(print_list_cleanup_timeout_id);
    }
    print_list_cleanup_timeout_id = g_timeout_add_seconds(30, cleanup_print_list_timeout, NULL);
    g_info("Finestra stampa chiusa: pulizia lista programmata tra 30 secondi");
}

static void refresh_print_grid_local(void) {
    GtkWidget *grid;
    GList *l;
    int row, col;

    if (!print_grid_container_local || !GTK_IS_CONTAINER(print_grid_container_local)) {
        return;
    }

    /* Pulisci container */
    gtk_container_foreach(GTK_CONTAINER(print_grid_container_local),
                         (GtkCallback)gtk_widget_destroy, NULL);

    if (g_list_length(print_list) == 0) {
        GtkWidget *empty_label = gtk_label_new("Nessuna foto in lista stampa.\nAggiungi foto dalla tab View.");
        gtk_box_pack_start(GTK_BOX(print_grid_container_local), empty_label, TRUE, TRUE, 0);
        if (GTK_IS_WIDGET(print_grid_container_local)) {
            gtk_widget_show_all(print_grid_container_local);
        }
        return;
    }

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

        card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_ETCHED_IN);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

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

        label = gtk_label_new(g_path_get_basename(item->path));
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 20);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

        copies_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_halign(copies_hbox, GTK_ALIGN_CENTER);

        copies_label = gtk_label_new("Copie:");
        gtk_style_context_add_class(gtk_widget_get_style_context(copies_label), "title");
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

    gtk_box_pack_start(GTK_BOX(print_grid_container_local), grid, TRUE, TRUE, 0);

    if (GTK_IS_WIDGET(print_grid_container_local)) {
        gtk_widget_show_all(print_grid_container_local);
    }
}

GtkWidget* create_print_window(const char *icons_dir) {
    GtkWidget *main_vbox, *label_title, *scrolled_window, *print_button, *clean_button;
    GtkWidget *printer_label, *printer_hbox, *bottom_box;
    int i;

    if (print_window) {
        gtk_window_present(GTK_WINDOW(print_window));
        return print_window;
    }

    print_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(print_window), "Ikona - Stampa");
    gtk_window_set_default_size(GTK_WINDOW(print_window), 1000, 700);
    gtk_window_set_position(GTK_WINDOW(print_window), GTK_WIN_POS_CENTER);
    g_signal_connect(print_window, "destroy", G_CALLBACK(on_print_window_destroyed), NULL);

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 20);
    gtk_container_add(GTK_CONTAINER(print_window), main_vbox);

    label_title = gtk_label_new("Stampa Immagini");
    gtk_box_pack_start(GTK_BOX(main_vbox), label_title, FALSE, FALSE, 0);

    /* Selezione stampante */
    printer_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), printer_hbox, FALSE, FALSE, 0);

    printer_label = gtk_label_new("Stampante:");
    gtk_box_pack_start(GTK_BOX(printer_hbox), printer_label, FALSE, FALSE, 0);

    printer_combo_box = gtk_combo_box_text_new();
    available_printers = get_available_printers(&printer_count);
    if (available_printers && printer_count > 0) {
        int default_idx = 0;
        for (i = 0; i < printer_count; i++) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(printer_combo_box),
                                     available_printers[i].name,
                                     available_printers[i].display_name);
            /* Track index of default printer */
            if (strlen(default_printer) > 0 && strcmp(available_printers[i].name, default_printer) == 0) {
                default_idx = i;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(printer_combo_box), default_idx);
    } else {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(printer_combo_box), "none", "Nessuna stampante trovata");
        gtk_combo_box_set_active(GTK_COMBO_BOX(printer_combo_box), 0);
        gtk_widget_set_sensitive(printer_combo_box, FALSE);
    }
    gtk_box_pack_start(GTK_BOX(printer_hbox), printer_combo_box, TRUE, TRUE, 0);

    /* Griglia per le foto in stampa - CREA NUOVO CONTAINER LOCALE */
    print_grid_container_local = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), print_grid_container_local);
    gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);

    /* Bottom bar con pulsanti */
    bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(bottom_box), 10);

    clean_button = create_icon_only_button(ICON_TRASH, 20, "Pulisci");
    g_signal_connect(clean_button, "clicked", G_CALLBACK(on_remove_from_print_list), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_box), clean_button, FALSE, FALSE, 0);

    print_button = gtk_button_new_with_label("🖨 Stampa Tutto");
    g_signal_connect(print_button, "clicked", G_CALLBACK(on_print_selected), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_box), print_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_vbox), bottom_box, FALSE, FALSE, 0);

    gtk_widget_show_all(print_window);
    refresh_print_grid_local();

    return print_window;
}

const char* get_selected_printer_from_window(void) {
    if (!printer_combo_box) return NULL;
    return gtk_combo_box_get_active_id(GTK_COMBO_BOX(printer_combo_box));
}

void refresh_print_window_grid(void) {
    /* Aggiorna solo se la finestra stampa è aperta */
    if (print_window && print_grid_container_local) {
        refresh_print_grid_local();
    }
}
