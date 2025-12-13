#include "importer.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gio/gio.h>

extern GtkWidget *label_copy_status;
extern char src_folder[512];
extern char base_dst_folder[512];
extern char view_folder[512];
extern GtkWidget *global_stack;
extern void load_images_list(void);
extern void display_page(void);

void on_select_src_folder(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Seleziona cartella sorgente",
        GTK_WINDOW(data),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Annulla", GTK_RESPONSE_CANCEL,
        "Seleziona", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        strncpy(src_folder, folder, sizeof(src_folder) - 1);
        gtk_label_set_text(GTK_LABEL(label_copy_status),
                          g_strdup_printf("Sorgente: %s", src_folder));
        g_free(folder);
    }

    gtk_widget_destroy(dialog);
}

void on_select_dst_folder(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Seleziona cartella base destinazione",
        GTK_WINDOW(data),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Annulla", GTK_RESPONSE_CANCEL,
        "Seleziona", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        strncpy(base_dst_folder, folder, sizeof(base_dst_folder) - 1);
        gtk_label_set_text(GTK_LABEL(label_copy_status),
                          g_strdup_printf("Destinazione base: %s", base_dst_folder));
        g_free(folder);
    }

    gtk_widget_destroy(dialog);
}

void on_copy_jpg_auto(GtkButton *button, gpointer data) {
    if (strlen(src_folder) == 0 || strlen(base_dst_folder) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Seleziona sorgente e destinazione!");
        return;
    }

    g_info("Starting import from '%s' to '%s'", src_folder, base_dst_folder);

    // Crea cartella con data se non esiste
    char *today_path = get_today_folder_path(base_dst_folder);
    mkdir(today_path, 0755);

    // Ottieni numero importazione successivo
    int import_num = get_next_import_number(base_dst_folder);
    char import_folder[2048];
    snprintf(import_folder, sizeof(import_folder), "%s/%03d", today_path, import_num);
    create_import_folder(import_folder);

    DIR *dir = opendir(src_folder);
    if (!dir) {
        g_warning("Failed to open source directory: %s", src_folder);
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore apertura cartella!");
        return;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        char src_path[4096];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_folder, entry->d_name);

        char *content_type = g_content_type_guess(src_path, NULL, 0, NULL);
        if (content_type && g_str_has_prefix(content_type, "image/jpeg")) {
            char dst_path[4096];
            snprintf(dst_path, sizeof(dst_path), "%s/%s", import_folder, entry->d_name);

            FILE *src = fopen(src_path, "rb");
            if (src) {
                FILE *dst = fopen(dst_path, "wb");
                if (dst) {
                    char buffer[4096];
                    size_t bytes;
                    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                        fwrite(buffer, 1, bytes, dst);
                    }
                    fclose(dst);
                    count++;
                } else {
                    g_warning("Failed to create destination file: %s", dst_path);
                    gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore creazione file destinazione!");
                    fclose(src);
                    continue;
                }
                fclose(src);
            } else {
                g_warning("Failed to open source file: %s", src_path);
                gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore apertura file sorgente!");
                continue;
            }
        }
        g_free(content_type);
    }

    closedir(dir);

    char status[300];
    snprintf(status, sizeof(status), "✓ Importazione #%03d completata!\n%d immagini copiate in:\n%s/%03d",
             import_num, count, today_path, import_num);
    gtk_label_set_text(GTK_LABEL(label_copy_status), status);
    g_info("Import #%03d completed. Copied %d images to %s/%03d", import_num, count, today_path, import_num);

    // Auto-open newly imported photos in View tab
    strncpy(view_folder, import_folder, sizeof(view_folder) - 1);
    load_images_list();
    display_page();
    if (global_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "view");
    }
}
