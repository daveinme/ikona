#ifndef UTILS_H
#define UTILS_H

char* get_today_folder_path(const char *base_dst_folder);
int get_next_import_number(const char *base_dst_folder);
void create_import_folder(const char *import_path);
int scan_and_count_images(const char *path);

#endif // UTILS_H
