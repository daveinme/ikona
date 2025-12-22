#ifndef UTILS_H
#define UTILS_H

#include <glib.h>

// Funzioni esistenti
char* get_today_folder_path(const char *base_dst_folder);
int get_next_import_number(const char *base_dst_folder);
void create_import_folder(const char *import_path);
int scan_and_count_images(const char *path);

// NUOVO: Path management per cartella app data
char* get_app_data_dir(void);
char* get_originals_base_path(void);
char* get_thumbnails_base_path(void);
char* get_dated_folder_path(const char *base_path, const char *date_str);

// NUOVO: Thumbnail generation
int create_thumbnail(const char *original_path, const char *thumbnail_path, int size);

// NUOVO: Path mapping thumbnail <-> originale
char* thumb_to_original_path(const char *thumbnail_path);
char* original_to_thumb_path(const char *original_path);

// NUOVO: Creazione directory app
int ensure_app_directories_exist(void);

// NUOVO: Gestione spazio disco
gint64 get_available_disk_space(const char *path);
gint64 get_folder_size(const char *path);
int estimate_importable_photos(gint64 available_bytes, int avg_photo_size_mb);
void format_bytes(gint64 bytes, char *buffer, size_t buffer_size);

// NUOVO: Cancellazione foto
int delete_photos_in_date_range(const char *date_from, const char *date_to,
                                int *deleted_count, gint64 *freed_space);
int delete_directory_recursive(const char *path);
int count_photos_recursive(const char *path);
int count_all_photos(const char *base_path);
int count_images_in_folder(const char *path);

#endif // UTILS_H
