#include "print_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>

/* Get path to print log file */
static void get_log_path(char *path, size_t max_size) {
#ifdef _WIN32
    snprintf(path, max_size, "%s\\ikona_print_log.json", g_get_home_dir());
#else
    snprintf(path, max_size, "%s/.ikona_print_log.json", g_get_home_dir());
#endif
}

/* Get today's date in YYYY-MM-DD format */
static void get_today_date(char *date, size_t max_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(date, max_size, "%Y-%m-%d", tm_info);
}

/* Load entire log from JSON file into memory */
static GHashTable* load_log_table(void) {
    FILE *fp;
    char log_path[1024];
    char line[2048];
    GHashTable *table;

    get_log_path(log_path, sizeof(log_path));
    table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    fp = fopen(log_path, "r");
    if (!fp) {
        return table;  // Return empty table if file doesn't exist
    }

    /* Skip opening brace */
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        char date[32] = "";
        int count = 0;

        /* Remove whitespace and newline */
        line[strcspn(line, "\r\n")] = 0;

        /* Look for pattern: "YYYY-MM-DD": count, */
        if (sscanf(line, " \"%31[^\"]\" : %d", date, &count) == 2) {
            if (strlen(date) > 0 && count > 0) {
                g_hash_table_insert(table, g_strdup(date), GINT_TO_POINTER(count));
            }
        }
    }

    fclose(fp);
    return table;
}

/* Save hash table back to JSON file */
static void save_log_table(GHashTable *table) {
    FILE *fp;
    char log_path[1024];
    GHashTableIter iter;
    gpointer key, value;
    gboolean first = TRUE;

    get_log_path(log_path, sizeof(log_path));

    fp = fopen(log_path, "w");
    if (!fp) {
        g_warning("Cannot write to print log: %s", log_path);
        return;
    }

    fprintf(fp, "{\n");

    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const char *date = (const char *)key;
        int count = GPOINTER_TO_INT(value);

        if (!first) fprintf(fp, ",\n");
        fprintf(fp, "  \"%s\": %d", date, count);
        first = FALSE;
    }

    fprintf(fp, "\n}\n");
    fclose(fp);

    g_info("Print log saved to: %s", log_path);
}

/* Log a print event for today */
void log_print(int copies) {
    char today[32];
    GHashTable *table;
    gpointer current_count_ptr;
    int current_count;

    if (copies <= 0) return;

    get_today_date(today, sizeof(today));
    table = load_log_table();

    /* Get current count for today */
    current_count_ptr = g_hash_table_lookup(table, today);
    current_count = GPOINTER_TO_INT(current_count_ptr);

    /* Update count */
    g_hash_table_insert(table, g_strdup(today), GINT_TO_POINTER(current_count + copies));

    /* Save back to file */
    save_log_table(table);

    g_hash_table_destroy(table);

    g_info("Logged %d prints for %s", copies, today);
}

/* Get total prints for a specific date (YYYY-MM-DD format) */
int get_print_total_for_date(const char *date) {
    GHashTable *table;
    gpointer count_ptr;
    int count;

    if (!date || strlen(date) == 0) return 0;

    table = load_log_table();
    count_ptr = g_hash_table_lookup(table, date);
    count = GPOINTER_TO_INT(count_ptr);

    g_hash_table_destroy(table);

    return count;
}

/* Get all dates in the log as a GList of strings (must be freed by caller) */
GList* get_all_print_dates(void) {
    GHashTable *table;
    GHashTableIter iter;
    gpointer key, value;
    GList *dates = NULL;

    table = load_log_table();

    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const char *date = (const char *)key;
        dates = g_list_insert_sorted(dates, g_strdup(date), (GCompareFunc)g_strcmp0);
    }

    g_hash_table_destroy(table);

    return dates;
}

/* Clear all print logs with confirmation dialog */
void clear_print_log_with_dialog(void) {
    GtkWidget *dialog;
    GtkWindow *parent = NULL;
    gint response;
    char log_path[1024];

    /* Get parent window if available */
    GList *windows = gtk_window_list_toplevels();
    if (windows) {
        parent = GTK_WINDOW(windows->data);
        g_list_free(windows);
    }

    dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                                    "Cancella Resoconto Stampe?");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                            "Questa azione cancellerà tutti i dati di stampa. Non può essere annullata.");

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        get_log_path(log_path, sizeof(log_path));
        if (g_file_test(log_path, G_FILE_TEST_EXISTS)) {
            if (remove(log_path) == 0) {
                g_info("Print log cleared successfully");
                /* Show success dialog */
                GtkWidget *success_dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                                                                  GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                                  "Resoconto Cancellato");
                gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(success_dialog),
                                                        "Tutti i dati di stampa sono stati eliminati.");
                gtk_dialog_run(GTK_DIALOG(success_dialog));
                gtk_widget_destroy(success_dialog);
            } else {
                g_warning("Failed to clear print log");
            }
        }
    }
}
