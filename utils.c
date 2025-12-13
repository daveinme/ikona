#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

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
    mkdir(import_path, 0755);
}
