#include "importer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

/* Funzioni esterne definite in altri file del progetto */
extern void show_toast_notification(const char *message);
extern void load_images_list(void);
extern void display_page(void);
extern char *get_originals_base_path(void);
extern char *get_thumbnails_base_path(void);
extern int get_next_import_number(const char *base_path);
extern int create_thumbnail(const char *src, const char *dst, int size);

extern int import_buffer_size;
extern GtkWidget *label_copy_status;
extern GtkWidget *label_auto_photo_count;
extern GtkWidget *import_progress_bar;
extern GtkWidget *import_progress_bar_auto;
extern char src_folder[512];
extern char base_dst_folder[512];
extern char view_folder[512];
extern char sd_drive_letters[256];
extern GtkWidget *global_stack;

/* Struttura per il progresso */
typedef struct {
    int total_files;
    int copied_files;
} CopyProgress;

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

/* --- DEFINIZIONE FUNZIONI INTERNE --- */

/* 1. Conta immagini nella cartella (NON ricorsivo) */
static int count_image_files(const char *source_dir) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    const char *ext;

    dir = opendir(source_dir);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Salta nascosti e . / ..

        ext = strrchr(entry->d_name, '.');
        if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 ||
                    g_ascii_strcasecmp(ext, ".jpeg") == 0 ||
                    g_ascii_strcasecmp(ext, ".png") == 0 ||
                    g_ascii_strcasecmp(ext, ".bmp") == 0 ||
                    g_ascii_strcasecmp(ext, ".gif") == 0 ||
                    g_ascii_strcasecmp(ext, ".tiff") == 0 ||
                    g_ascii_strcasecmp(ext, ".tif") == 0)) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

/* 2. Copia immagini nella cartella (NON ricorsivo) */
static int scan_and_copy_images_internal(const char *source_dir, 
                                         const char *dest_folder_original,
                                         const char *dest_folder_thumbnail, 
                                         CopyProgress *progress, 
                                         GtkWidget *progress_bar) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    char src_path[4096], dst_path_original[4096], dst_path_thumbnail[4096];
    FILE *src_file, *dst_file;
    char *buffer;
    size_t bytes;
    const char *ext;
    struct stat statbuf;

    buffer = (char*)malloc(import_buffer_size);
    if (!buffer) return 0;

    dir = opendir(source_dir);
    if (!dir) {
        free(buffer);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        snprintf(src_path, sizeof(src_path), "%s%c%s", source_dir, G_DIR_SEPARATOR, entry->d_name);
        if (stat(src_path, &statbuf) != 0 || S_ISDIR(statbuf.st_mode)) continue;

        ext = strrchr(entry->d_name, '.');
        if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 || g_ascii_strcasecmp(ext, ".jpeg") == 0 ||
                    g_ascii_strcasecmp(ext, ".png") == 0 || g_ascii_strcasecmp(ext, ".bmp") == 0 ||
                    g_ascii_strcasecmp(ext, ".gif") == 0 || g_ascii_strcasecmp(ext, ".tiff") == 0 ||
                    g_ascii_strcasecmp(ext, ".tif") == 0)) {

            snprintf(dst_path_original, sizeof(dst_path_original), "%s%c%s", dest_folder_original, G_DIR_SEPARATOR, entry->d_name);
            snprintf(dst_path_thumbnail, sizeof(dst_path_thumbnail), "%s%c%s", dest_folder_thumbnail, G_DIR_SEPARATOR, entry->d_name);

            // Gestione duplicati semplice
            if (access(dst_path_original, F_OK) == 0) {
                snprintf(dst_path_original, sizeof(dst_path_original), "%s%c_copy_%s", dest_folder_original, G_DIR_SEPARATOR, entry->d_name);
            }

            src_file = fopen(src_path, "rb");
            if (src_file) {
                dst_file = fopen(dst_path_original, "wb");
                if (dst_file) {
                    while ((bytes = fread(buffer, 1, import_buffer_size, src_file)) > 0) fwrite(buffer, 1, bytes, dst_file);
                    fclose(dst_file);
                    create_thumbnail(dst_path_original, dst_path_thumbnail, 512);
                    count++;

                    if (progress && progress_bar) {
                        progress->copied_files++;
                        double fraction = (double)progress->copied_files / progress->total_files;
                        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%d / %d", progress->copied_files, progress->total_files);
                        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), buf);
                        while (gtk_events_pending()) gtk_main_iteration();
                    }
                }
                fclose(src_file);
            }
        }
    }
    closedir(dir);
    free(buffer);
    return count;
}

/* 3. Wrapper principale */
static int scan_and_copy_images(const char *source_dir, const char *dest_folder_original,
                                const char *dest_folder_thumbnail, GtkWidget *progress_bar) {
    CopyProgress progress = {0, 0};
    
    progress.total_files = count_image_files(source_dir);
    if (progress.total_files == 0) return 0;

    if (progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    }

    return scan_and_copy_images_internal(source_dir, dest_folder_original, dest_folder_thumbnail, &progress, progress_bar);
}

/* --- CALLBACKS GTK --- */

void on_copy_jpg_auto(GtkButton *button, gpointer data) {
    char *originals_base, *thumbnails_base;
    char date_str[64], import_folder_original[4096], import_folder_thumbnail[4096];
    int import_num, count;

    if (strlen(src_folder) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Seleziona sorgente!");
        return;
    }

    originals_base = get_originals_base_path();
    thumbnails_base = get_thumbnails_base_path();
    time_t now = time(NULL);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&now));
    import_num = get_next_import_number(originals_base);

    snprintf(import_folder_original, sizeof(import_folder_original), "%s%c%s%c%03d", originals_base, G_DIR_SEPARATOR, date_str, G_DIR_SEPARATOR, import_num);
    snprintf(import_folder_thumbnail, sizeof(import_folder_thumbnail), "%s%c%s%c%03d", thumbnails_base, G_DIR_SEPARATOR, date_str, G_DIR_SEPARATOR, import_num);

    g_mkdir_with_parents(import_folder_original, 0755);
    g_mkdir_with_parents(import_folder_thumbnail, 0755);

    count = scan_and_copy_images(src_folder, import_folder_original, import_folder_thumbnail, import_progress_bar);

    char notification[512];
    snprintf(notification, sizeof(notification), "✓ Importazione #%03d completata: %d immagini", import_num, count);
    show_toast_notification(notification);

    strncpy(view_folder, import_folder_thumbnail, sizeof(view_folder) - 1);
    load_images_list();
    display_page();
    if (global_stack) gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "grid");
}

/* Callback per la scansione SD */
void on_scan_sd_for_auto(GtkButton *button, gpointer data) {
    //char full_sd_path[1024];
    int photo_count = 0;
    char status[300];

    // Se src_folder è caricata dal .ini ed è "F:\DCIM\100MSDCF"
    if (strlen(src_folder) > 0) {
        photo_count = count_image_files(src_folder);
    } else {
        // Fallback sulla lettera drive se src_folder è vuota
        // (Qui però conterebbe solo i file nella root)
        photo_count = 0; 
    }

    snprintf(status, sizeof(status), "%d Foto da importare", photo_count);
    gtk_label_set_text(GTK_LABEL(label_auto_photo_count), status);
}

// Nota: on_import_sd_auto segue la stessa logica di on_copy_jpg_auto usando sd_path