#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glib.h>

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
