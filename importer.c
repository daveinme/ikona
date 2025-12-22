#include "importer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gio/gio.h>

extern int import_buffer_size;  // Global buffer size from config (4KB, 64KB, 128KB, or 256KB)
extern GtkWidget *label_copy_status;
extern GtkWidget *label_auto_photo_count;
extern GtkWidget *grid_container;
extern GtkWidget *import_progress_bar;
extern GtkWidget *import_progress_bar_auto;
extern char src_folder[512];
extern char base_dst_folder[512];
extern char view_folder[512];
extern char sd_drive_letters[256];
extern GtkWidget *global_stack;
extern GtkWidget *main_window;
extern void load_images_list(void);
extern void display_page(void);
extern void show_toast_notification(const char *message);

/* Structure to track copy progress */
typedef struct {
    int total_files;
    int copied_files;
} CopyProgress;

/* Count total image files recursively */
static int count_image_files(const char *source_dir, int depth, int max_depth) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    char src_path[4096];
    const char *ext;
    struct stat statbuf;

    if (depth > max_depth) return 0;

    dir = opendir(source_dir);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.' ||
            strcmp(entry->d_name, "System Volume Information") == 0 ||
            strcmp(entry->d_name, "$RECYCLE.BIN") == 0 ||
            strcmp(entry->d_name, "RECYCLER") == 0 ||
            strcmp(entry->d_name, "RECYCLED") == 0 ||
            g_ascii_strcasecmp(entry->d_name, "Thumbs.db") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s%c%s", source_dir, G_DIR_SEPARATOR, entry->d_name);
        if (stat(src_path, &statbuf) != 0) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            count += count_image_files(src_path, depth + 1, max_depth);
            continue;
        }

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

/* Recursive function to scan directories and copy images */
static int scan_and_copy_images_internal(const char *source_dir, const char *dest_folder_original,
                                         const char *dest_folder_thumbnail, int depth, int max_depth,
                                         CopyProgress *progress, GtkWidget *progress_bar) {
    DIR *dir;
    struct dirent *entry;
    int count;
    char src_path[4096];
    char dst_path_original[4096];
    char dst_path_thumbnail[4096];
    FILE *src_file;
    FILE *dst_file;
    char *buffer;  // Dynamic buffer allocated on heap (configurable via settings)
    size_t bytes;
    const char *ext;
    struct stat statbuf;

    /* Allocate I/O buffer on heap to avoid stack overflow in recursive calls */
    buffer = (char*)malloc(import_buffer_size);
    if (!buffer) {
        g_critical("Failed to allocate %d bytes for I/O buffer", import_buffer_size);
        return 0;
    }
    g_debug("Allocated %d KB I/O buffer for import", import_buffer_size / 1024);

    /* Stop if we've gone too deep */
    if (depth > max_depth) {
        g_info("Skipping directory (max depth %d): %s", max_depth, source_dir);
        free(buffer);
        return 0;
    }

    dir = opendir(source_dir);
    if (!dir) {
        g_warning("Cannot open directory: %s", source_dir);
        free(buffer);
        return 0;
    }

    count = 0;

    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Skip hidden files and system folders */
        if (entry->d_name[0] == '.' ||
            strcmp(entry->d_name, "System Volume Information") == 0 ||
            strcmp(entry->d_name, "$RECYCLE.BIN") == 0 ||
            strcmp(entry->d_name, "RECYCLER") == 0 ||
            strcmp(entry->d_name, "RECYCLED") == 0 ||
            g_ascii_strcasecmp(entry->d_name, "Thumbs.db") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s%c%s", source_dir, G_DIR_SEPARATOR, entry->d_name);

        /* Get file info */
        if (stat(src_path, &statbuf) != 0) {
            continue;
        }

        /* If it's a directory, recurse into it */
        if (S_ISDIR(statbuf.st_mode)) {
            g_info("Scanning [depth %d]: %s", depth, entry->d_name);
            count += scan_and_copy_images_internal(src_path, dest_folder_original, dest_folder_thumbnail,
                                                   depth + 1, max_depth, progress, progress_bar);
            continue;
        }

        /* Check if it's an image file */
        ext = strrchr(entry->d_name, '.');
        if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 ||
                    g_ascii_strcasecmp(ext, ".jpeg") == 0 ||
                    g_ascii_strcasecmp(ext, ".png") == 0 ||
                    g_ascii_strcasecmp(ext, ".bmp") == 0 ||
                    g_ascii_strcasecmp(ext, ".gif") == 0 ||
                    g_ascii_strcasecmp(ext, ".tiff") == 0 ||
                    g_ascii_strcasecmp(ext, ".tif") == 0)) {

            snprintf(dst_path_original, sizeof(dst_path_original), "%s%c%s",
                     dest_folder_original, G_DIR_SEPARATOR, entry->d_name);
            snprintf(dst_path_thumbnail, sizeof(dst_path_thumbnail), "%s%c%s",
                     dest_folder_thumbnail, G_DIR_SEPARATOR, entry->d_name);

            /* Check if file already exists, add suffix if needed */
            if (access(dst_path_original, F_OK) == 0) {
                char base_name[256];
                char *dot;
                int suffix;

                strncpy(base_name, entry->d_name, sizeof(base_name) - 1);
                dot = strrchr(base_name, '.');
                if (dot) *dot = '\0';

                suffix = 1;
                do {
                    snprintf(dst_path_original, sizeof(dst_path_original), "%s%c%s_%d%s",
                             dest_folder_original, G_DIR_SEPARATOR, base_name, suffix, ext);
                    snprintf(dst_path_thumbnail, sizeof(dst_path_thumbnail), "%s%c%s_%d%s",
                             dest_folder_thumbnail, G_DIR_SEPARATOR, base_name, suffix, ext);
                    suffix++;
                } while (access(dst_path_original, F_OK) == 0 && suffix < 1000);
            }

            /* Copy original */
            src_file = fopen(src_path, "rb");
            if (src_file) {
                dst_file = fopen(dst_path_original, "wb");
                if (dst_file) {
                    while ((bytes = fread(buffer, 1, import_buffer_size, src_file)) > 0) {
                        fwrite(buffer, 1, bytes, dst_file);
                    }
                    fclose(dst_file);

                    /* NUOVO: Genera thumbnail */
                    if (create_thumbnail(dst_path_original, dst_path_thumbnail, 512) != 0) {
                        g_warning("Failed to create thumbnail for: %s", dst_path_original);
                        /* Non bloccare import, continua */
                    }

                    count++;

                    /* Update progress */
                    if (progress && progress_bar) {
                        progress->copied_files++;
                        double fraction = (double)progress->copied_files / progress->total_files;
                        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
                        char progress_text[64];
                        snprintf(progress_text, sizeof(progress_text), "%d / %d",
                               progress->copied_files, progress->total_files);
                        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progress_text);
                        /* Process GTK events to keep UI responsive */
                        while (gtk_events_pending()) gtk_main_iteration();
                    }

                    g_info("Copied: %s -> %s (+ thumbnail)", entry->d_name, dst_path_original);
                } else {
                    g_warning("Failed to create destination file: %s", dst_path_original);
                }
                /* Always close src_file, but only once */
                fclose(src_file);
            } else {
                g_warning("Failed to open source file: %s", src_path);
            }
        }
    }

    closedir(dir);
    free(buffer);
    return count;
}

/* Wrapper function with default max depth */
static int scan_and_copy_images(const char *source_dir, const char *dest_folder_original,
                                const char *dest_folder_thumbnail, GtkWidget *progress_bar) {
    /* Maximum depth: 5 levels is enough for most camera structures
     * e.g., E:\DCIM\100MSDCF\2024\December\Photos = 5 levels
     */
    int max_depth = 5;
    CopyProgress progress = {0, 0};

    /* First pass: count total files */
    g_info("Starting recursive scan with max depth: %d", max_depth);
    progress.total_files = count_image_files(source_dir, 0, max_depth);
    g_info("Found %d image files to copy", progress.total_files);

    if (progress.total_files == 0) {
        return 0;
    }

    /* Initialize progress bar */
    if (progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "0 / 0");
    }

    /* Second pass: copy files and update progress */
    return scan_and_copy_images_internal(source_dir, dest_folder_original, dest_folder_thumbnail,
                                        0, max_depth, &progress, progress_bar);
}

void on_copy_jpg_auto(GtkButton *button, gpointer data) {
    char *originals_base;
    char *thumbnails_base;
    char date_str[64];
    int import_num;
    char import_folder_original[4096];
    char import_folder_thumbnail[4096];
    int count;

    if (strlen(src_folder) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Seleziona sorgente!");
        return;
    }

    g_info("Starting RECURSIVE import from '%s'", src_folder);

    // Ottieni path base per originals e thumbnails
    originals_base = get_originals_base_path();
    thumbnails_base = get_thumbnails_base_path();

    // Ottieni data odierna
    time_t now = time(NULL);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&now));

    // Ottieni numero import progressivo
    import_num = get_next_import_number(originals_base);

    // Costruisci path completi per import odierno
    snprintf(import_folder_original, sizeof(import_folder_original),
             "%s%c%s%c%03d", originals_base, G_DIR_SEPARATOR, date_str, G_DIR_SEPARATOR, import_num);
    snprintf(import_folder_thumbnail, sizeof(import_folder_thumbnail),
             "%s%c%s%c%03d", thumbnails_base, G_DIR_SEPARATOR, date_str, G_DIR_SEPARATOR, import_num);

    // Crea directory se non esistono
    if (g_mkdir_with_parents(import_folder_original, 0755) != 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore creazione cartelle!");
        return;
    }
    if (g_mkdir_with_parents(import_folder_thumbnail, 0755) != 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore creazione cartelle!");
        return;
    }

    gtk_label_set_text(GTK_LABEL(label_copy_status), "🔍 Scansione ricorsiva (max 5 livelli)...");

    /* Scan recursively and copy all images + generate thumbnails */
    count = scan_and_copy_images(src_folder, import_folder_original, import_folder_thumbnail, import_progress_bar);

    g_info("Import #%03d completed. Copied %d images (+ thumbnails) to %s/%03d", import_num, count, date_str, import_num);

    // Show non-blocking notification
    char notification[512];
    snprintf(notification, sizeof(notification), "✓ Importazione #%03d completata!\n%d immagini copiate (+ thumbnail)", import_num, count);
    show_toast_notification(notification);

    // Auto-open newly imported photos in View tab (usa path thumbnails)
    strncpy(view_folder, import_folder_thumbnail, sizeof(view_folder) - 1);
    load_images_list();
    display_page();
    if (global_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "grid");
    }

    /* Reset progress bar */
    if (import_progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(import_progress_bar), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(import_progress_bar), "0 / 0");
    }
}

/* Import from SD card (Auto) */
void on_import_sd_auto(GtkButton *button, gpointer data) {
    char sd_path[256];
    char *originals_base;
    char *thumbnails_base;
    char date_str[64];
    int import_num;
    char import_folder_original[4096];
    char import_folder_thumbnail[4096];
    int count;
    char status[300];

    if (strlen(sd_drive_letters) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "⚠ Configurare la SD in Config!");
        return;
    }

    // Extract first drive letter
    strncpy(sd_path, sd_drive_letters, 255);
    char *comma = strchr(sd_path, ',');
    if (comma) *comma = '\0';

    // Trim whitespace
    char *p = sd_path;
    while (*p == ' ') p++;
    if (p != sd_path) strcpy(sd_path, p);

#ifdef _WIN32
    size_t len = strlen(sd_path);
    if (len > 0 && sd_path[len-1] != ':') strcat(sd_path, ":");
    strcat(sd_path, "\\");
#else
    strcat(sd_path, "/");
#endif

    // Ottieni path base per originals e thumbnails
    originals_base = get_originals_base_path();
    thumbnails_base = get_thumbnails_base_path();

    // Ottieni data odierna
    time_t now = time(NULL);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&now));

    // Ottieni numero import progressivo
    import_num = get_next_import_number(originals_base);

    // Costruisci path completi per import odierno
    snprintf(import_folder_original, sizeof(import_folder_original),
             "%s%c%s%c%03d", originals_base, G_DIR_SEPARATOR, date_str, G_DIR_SEPARATOR, import_num);
    snprintf(import_folder_thumbnail, sizeof(import_folder_thumbnail),
             "%s%c%s%c%03d", thumbnails_base, G_DIR_SEPARATOR, date_str, G_DIR_SEPARATOR, import_num);

    // Crea directory se non esistono
    if (g_mkdir_with_parents(import_folder_original, 0755) != 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore creazione cartelle!");
        return;
    }
    if (g_mkdir_with_parents(import_folder_thumbnail, 0755) != 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Errore creazione cartelle!");
        return;
    }

    // Count and copy
    count = scan_and_copy_images(sd_path, import_folder_original, import_folder_thumbnail, import_progress_bar_auto);

    snprintf(status, sizeof(status), "✓ Auto-Import #%03d completato!\n%d immagini copiate (+ thumbnail)",
             import_num, count);
    gtk_label_set_text(GTK_LABEL(label_copy_status), status);

    // Show in View (usa path thumbnails)
    strncpy(view_folder, import_folder_thumbnail, sizeof(view_folder) - 1);
    load_images_list();
    display_page();
    if (global_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "grid");
    }

    /* Reset progress bar */
    if (import_progress_bar_auto) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(import_progress_bar_auto), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(import_progress_bar_auto), "0 / 0");
    }
}

/* Scan SD card and count images */
void on_scan_sd_for_auto(GtkButton *button, gpointer data) {
    char sd_path[256];
    DIR *dir;
    int photo_count = 0;
    char status[300];

    // Get first SD drive letter from sd_drive_letters (e.g., "I:" or "I:,J:")
    if (strlen(sd_drive_letters) == 0) {
        gtk_label_set_text(GTK_LABEL(label_auto_photo_count), "0 Foto da importare");
        return;
    }

    // Extract first drive letter and trim whitespace
    strncpy(sd_path, sd_drive_letters, 255);
    char *comma = strchr(sd_path, ',');
    if (comma) {
        *comma = '\0';
    }

    // Trim whitespace
    char *p = sd_path;
    while (*p == ' ') p++;
    if (p != sd_path) {
        strcpy(sd_path, p);
    }

#ifdef _WIN32
    // On Windows, ensure format is "I:" and then add backslash
    size_t len = strlen(sd_path);
    if (len > 0 && sd_path[len-1] != ':') {
        strcat(sd_path, ":");
    }
    strcat(sd_path, "\\");
#else
    // On Linux, format path
    strcat(sd_path, "/");
#endif

    g_info("Scanning SD at path: %s", sd_path);

    // Try to open the directory
    dir = opendir(sd_path);
    if (!dir) {
        g_info("Failed to open directory: %s", sd_path);
        gtk_label_set_text(GTK_LABEL(label_auto_photo_count), "⚠ SD non rilevata");
        return;
    }

    // Count images recursively
    photo_count = scan_and_count_images(sd_path);
    closedir(dir);

    // Update label
    snprintf(status, sizeof(status), "%d Foto da importare", photo_count);
    gtk_label_set_text(GTK_LABEL(label_auto_photo_count), status);

    g_info("SD card scan: found %d images in %s", photo_count, sd_path);
}
