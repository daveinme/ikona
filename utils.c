#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <sys/statvfs.h>
#include <unistd.h>
#endif

char* get_today_folder_path(const char *base_dst_folder) {
    static char folder_path[2048];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char date_str[20];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_info);
    snprintf(folder_path, sizeof(folder_path), "%s/%s", base_dst_folder, date_str);
    return folder_path;
}

int get_next_import_number(const char *base_dst_folder) {
    char *today_path = get_today_folder_path(base_dst_folder);
    int max_num = 0;

    DIR *dir = opendir(today_path);
    if (!dir) {
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", today_path, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            int num = atoi(entry->d_name);
            if (num > max_num) {
                max_num = num;
            }
        }
    }
    closedir(dir);

    return max_num + 1;
}

void create_import_folder(const char *import_path) {
#ifdef _WIN32
    mkdir(import_path);
#else
    mkdir(import_path, 0755);
#endif
}

/* Check if file is an image */
static int is_image_file(const char *filename) {
    const char *extensions[] = {".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".tif", NULL};

    char lower_name[256];
    strncpy(lower_name, filename, sizeof(lower_name) - 1);

    // Convert to lowercase
    for (int i = 0; lower_name[i]; i++) {
        if (lower_name[i] >= 'A' && lower_name[i] <= 'Z') {
            lower_name[i] = lower_name[i] - 'A' + 'a';
        }
    }

    for (int i = 0; extensions[i] != NULL; i++) {
        if (strstr(lower_name, extensions[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

/* Scan directory recursively and count image files */
int scan_and_count_images(const char *path) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    char full_path[1024];
    struct stat st;

    dir = opendir(path);
    if (!dir) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s%c%s", path, G_DIR_SEPARATOR, entry->d_name);

        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively scan subdirectory
                count += scan_and_count_images(full_path);
            } else if (S_ISREG(st.st_mode)) {
                // Check if it's an image file
                if (is_image_file(entry->d_name)) {
                    count++;
                }
            }
        }
    }

    closedir(dir);
    return count;
}

/* ========== NUOVO: Sistema Thumbnails ========== */

/* Ottiene directory app data platform-specific */
char* get_app_data_dir(void) {
    static char app_data_path[4096];
    static int initialized = 0;

    if (initialized) {
        return app_data_path;
    }

#ifdef _WIN32
    // Windows: Usa %APPDATA%/Ikona
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        snprintf(app_data_path, sizeof(app_data_path), "%s\\Ikona", appdata);
    } else {
        // Fallback a directory corrente
        snprintf(app_data_path, sizeof(app_data_path), ".\\Ikona");
    }
#else
    // Linux: Usa ~/.local/share/ikona
    const char *data_dir = g_get_user_data_dir();
    if (data_dir) {
        snprintf(app_data_path, sizeof(app_data_path), "%s/ikona", data_dir);
    } else {
        // Fallback a ~/.local/share/ikona
        const char *home = g_get_home_dir();
        snprintf(app_data_path, sizeof(app_data_path), "%s/.local/share/ikona", home);
    }
#endif

    initialized = 1;
    return app_data_path;
}

/* Percorso base per originali */
char* get_originals_base_path(void) {
    static char originals_path[4096];
    static int initialized = 0;

    if (initialized) {
        return originals_path;
    }

    char *app_data = get_app_data_dir();
    snprintf(originals_path, sizeof(originals_path), "%s%coriginals",
             app_data, G_DIR_SEPARATOR);

    initialized = 1;
    return originals_path;
}

/* Percorso base per thumbnails */
char* get_thumbnails_base_path(void) {
    static char thumbnails_path[4096];
    static int initialized = 0;

    if (initialized) {
        return thumbnails_path;
    }

    char *app_data = get_app_data_dir();
    snprintf(thumbnails_path, sizeof(thumbnails_path), "%s%cthumbnails",
             app_data, G_DIR_SEPARATOR);

    initialized = 1;
    return thumbnails_path;
}

/* Costruisce path con data */
char* get_dated_folder_path(const char *base_path, const char *date_str) {
    static char dated_path[4096];
    snprintf(dated_path, sizeof(dated_path), "%s%c%s",
             base_path, G_DIR_SEPARATOR, date_str);
    return dated_path;
}

/* Crea thumbnail da originale */
int create_thumbnail(const char *original_path, const char *thumbnail_path, int size) {
    GError *error = NULL;

    // Carica originale
    GdkPixbuf *original = gdk_pixbuf_new_from_file(original_path, &error);
    if (!original) {
        if (error) {
            g_warning("Failed to load image for thumbnail: %s", error->message);
            g_error_free(error);
        }
        return -1;
    }

    // Ottieni dimensioni originali
    int width = gdk_pixbuf_get_width(original);
    int height = gdk_pixbuf_get_height(original);
    int new_width, new_height;

    // Scala mantenendo aspect ratio
    if (width > height) {
        new_width = size;
        new_height = (height * size) / width;
        if (new_height == 0) new_height = 1;
    } else {
        new_height = size;
        new_width = (width * size) / height;
        if (new_width == 0) new_width = 1;
    }

    // Scala immagine
    GdkPixbuf *thumb = gdk_pixbuf_scale_simple(original, new_width, new_height,
                                               GDK_INTERP_BILINEAR);
    g_object_unref(original);

    if (!thumb) {
        g_warning("Failed to scale image for thumbnail");
        return -1;
    }

    // Salva come JPEG quality 85
    gboolean success = gdk_pixbuf_save(thumb, thumbnail_path, "jpeg",
                                       &error, "quality", "85", NULL);
    g_object_unref(thumb);

    if (!success) {
        if (error) {
            g_warning("Failed to save thumbnail: %s", error->message);
            g_error_free(error);
        }
        return -1;
    }

    return 0;
}

/* Mapping thumbnail -> originale */
char* thumb_to_original_path(const char *thumbnail_path) {
    if (!thumbnail_path) return NULL;

    static char original_path[4096];

    // Cerca "thumbnails" nel path e sostituiscilo con "originals"
    char *thumb_pos = strstr(thumbnail_path, "thumbnails");
    if (!thumb_pos) {
        // Path non contiene "thumbnails", ritorna NULL
        return NULL;
    }

    // Copia parte prima di "thumbnails"
    size_t prefix_len = thumb_pos - thumbnail_path;
    strncpy(original_path, thumbnail_path, prefix_len);
    original_path[prefix_len] = '\0';

    // Aggiungi "originals"
    strcat(original_path, "originals");

    // Aggiungi il resto del path (dopo "thumbnails")
    strcat(original_path, thumb_pos + strlen("thumbnails"));

    return original_path;
}

/* Mapping originale -> thumbnail */
char* original_to_thumb_path(const char *original_path) {
    if (!original_path) return NULL;

    static char thumb_path[4096];

    // Cerca "originals" nel path e sostituiscilo con "thumbnails"
    char *orig_pos = strstr(original_path, "originals");
    if (!orig_pos) {
        // Path non contiene "originals", ritorna NULL
        return NULL;
    }

    // Copia parte prima di "originals"
    size_t prefix_len = orig_pos - original_path;
    strncpy(thumb_path, original_path, prefix_len);
    thumb_path[prefix_len] = '\0';

    // Aggiungi "thumbnails"
    strcat(thumb_path, "thumbnails");

    // Aggiungi il resto del path (dopo "originals")
    strcat(thumb_path, orig_pos + strlen("originals"));

    return thumb_path;
}

/* Crea struttura directory app se non esiste */
int ensure_app_directories_exist(void) {
    char *app_data = get_app_data_dir();
    char *originals = get_originals_base_path();
    char *thumbnails = get_thumbnails_base_path();

    // Crea directory app data principale
    if (g_mkdir_with_parents(app_data, 0755) != 0) {
        g_critical("Failed to create app data directory: %s", app_data);
        return -1;
    }

    // Crea directory originals
    if (g_mkdir_with_parents(originals, 0755) != 0) {
        g_critical("Failed to create originals directory: %s", originals);
        return -1;
    }

    // Crea directory thumbnails
    if (g_mkdir_with_parents(thumbnails, 0755) != 0) {
        g_critical("Failed to create thumbnails directory: %s", thumbnails);
        return -1;
    }

    g_message("App directories initialized successfully");
    g_message("  App data: %s", app_data);
    g_message("  Originals: %s", originals);
    g_message("  Thumbnails: %s", thumbnails);

    return 0;
}

/* ========== Funzioni Helper per Conteggio Foto ========== */

/* Conta immagini in una cartella (ricorsivo) */
int count_photos_recursive(const char *path) {
    return scan_and_count_images(path);
}

/* Conta tutte le foto nella base path */
int count_all_photos(const char *base_path) {
    return scan_and_count_images(base_path);
}

/* Conta immagini in una specifica cartella */
int count_images_in_folder(const char *path) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(path);
    if (!dir) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Verifica se è un file immagine
        if (is_image_file(entry->d_name)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}
