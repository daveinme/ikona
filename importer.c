#include "importer.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gio/gio.h>

extern GtkWidget *label_copy_status;
extern GtkWidget *label_auto_photo_count;
extern GtkWidget *grid_container;
extern char src_folder[512];
extern char base_dst_folder[512];
extern char view_folder[512];
extern char sd_drive_letters[256];
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

/* Recursive function to scan directories and copy images */
static int scan_and_copy_images_internal(const char *source_dir, const char *dest_folder, int depth, int max_depth) {
    DIR *dir;
    struct dirent *entry;
    int count;
    char src_path[4096];
    char dst_path[4096];
    FILE *src_file;
    FILE *dst_file;
    char buffer[4096];
    size_t bytes;
    const char *ext;
    struct stat statbuf;

    /* Stop if we've gone too deep */
    if (depth > max_depth) {
        g_info("Skipping directory (max depth %d): %s", max_depth, source_dir);
        return 0;
    }

    dir = opendir(source_dir);
    if (!dir) {
        g_warning("Cannot open directory: %s", source_dir);
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
            count += scan_and_copy_images_internal(src_path, dest_folder, depth + 1, max_depth);
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

            snprintf(dst_path, sizeof(dst_path), "%s%c%s", dest_folder, G_DIR_SEPARATOR, entry->d_name);

            /* Check if file already exists, add suffix if needed */
            if (access(dst_path, F_OK) == 0) {
                char base_name[256];
                char *dot;
                int suffix;

                strncpy(base_name, entry->d_name, sizeof(base_name) - 1);
                dot = strrchr(base_name, '.');
                if (dot) *dot = '\0';

                suffix = 1;
                do {
                    snprintf(dst_path, sizeof(dst_path), "%s%c%s_%d%s",
                             dest_folder, G_DIR_SEPARATOR, base_name, suffix, ext);
                    suffix++;
                } while (access(dst_path, F_OK) == 0 && suffix < 1000);
            }

            src_file = fopen(src_path, "rb");
            if (src_file) {
                dst_file = fopen(dst_path, "wb");
                if (dst_file) {
                    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                        fwrite(buffer, 1, bytes, dst_file);
                    }
                    fclose(dst_file);
                    count++;
                    g_info("Copied: %s -> %s", entry->d_name, dst_path);
                } else {
                    g_warning("Failed to create destination file: %s", dst_path);
                    fclose(src_file);
                }
                fclose(src_file);
            } else {
                g_warning("Failed to open source file: %s", src_path);
            }
        }
    }

    closedir(dir);
    return count;
}

/* Wrapper function with default max depth */
static int scan_and_copy_images(const char *source_dir, const char *dest_folder) {
    /* Maximum depth: 5 levels is enough for most camera structures
     * e.g., E:\DCIM\100MSDCF\2024\December\Photos = 5 levels
     */
    int max_depth = 5;
    g_info("Starting recursive scan with max depth: %d", max_depth);
    return scan_and_copy_images_internal(source_dir, dest_folder, 0, max_depth);
}

void on_copy_jpg_auto(GtkButton *button, gpointer data) {
    char *today_path;
    int import_num;
    char import_folder[2048];
    int count;

    if (strlen(src_folder) == 0 || strlen(base_dst_folder) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "❌ Seleziona sorgente e destinazione!");
        return;
    }

    if (strncmp(base_dst_folder, src_folder, strlen(src_folder)) == 0) {
        GtkWidget *dialog;
        dialog = gtk_message_dialog_new(NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       "La cartella di destinazione non può essere una sottocartella della sorgente!");
        gtk_window_set_title(GTK_WINDOW(dialog), "Errore di importazione");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    g_info("Starting RECURSIVE import from '%s' to '%s'", src_folder, base_dst_folder);

    // Crea cartella con data se non esiste
    today_path = get_today_folder_path(base_dst_folder);
#ifdef _WIN32
    mkdir(today_path);
#else
    mkdir(today_path, 0755);
#endif

    // Ottieni numero importazione successivo
    import_num = get_next_import_number(base_dst_folder);
    snprintf(import_folder, sizeof(import_folder), "%s%c%03d", today_path, G_DIR_SEPARATOR, import_num);
    create_import_folder(import_folder);

    gtk_label_set_text(GTK_LABEL(label_copy_status), "🔍 Scansione ricorsiva (max 5 livelli)...");

    /* Scan recursively and copy all images */
    count = scan_and_copy_images(src_folder, import_folder);

    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   "Importazione #%03d completata!\n%d immagini copiate in:\n%s",
                                   import_num, count, import_folder);
    gtk_window_set_title(GTK_WINDOW(dialog), "Importazione Completata");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_info("Import #%03d completed. Copied %d images (recursive) to %s%c%03d", import_num, count, today_path, G_DIR_SEPARATOR, import_num);

    // Auto-open newly imported photos in View tab
    strncpy(view_folder, import_folder, sizeof(view_folder) - 1);
    load_images_list();
    display_page();
    if (global_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "import");
    }
}

/* Import from SD card (Auto) */
void on_import_sd_auto(GtkButton *button, gpointer data) {
    char sd_path[256];
    char *today_path;
    int import_num;
    char import_folder[2048];
    int count;
    char status[300];

    if (strlen(sd_drive_letters) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "⚠ Configurare la SD in Config!");
        return;
    }

    if (strlen(base_dst_folder) == 0) {
        gtk_label_set_text(GTK_LABEL(label_copy_status), "⚠ Configurare cartella destinazione!");
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

    // Create import folder
    today_path = get_today_folder_path(base_dst_folder);
#ifdef _WIN32
    mkdir(today_path);
#else
    mkdir(today_path, 0755);
#endif

    import_num = get_next_import_number(base_dst_folder);
    snprintf(import_folder, sizeof(import_folder), "%s%c%03d", today_path, G_DIR_SEPARATOR, import_num);
    create_import_folder(import_folder);

    // Count and copy
    count = scan_and_copy_images(sd_path, import_folder);

    snprintf(status, sizeof(status), "✓ Auto-Import #%03d completato!\n%d immagini copiate",
             import_num, count);
    gtk_label_set_text(GTK_LABEL(label_copy_status), status);

    // Show in View
    strncpy(view_folder, import_folder, sizeof(view_folder) - 1);
    load_images_list();
    display_page();
    if (global_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(global_stack), "grid");
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
