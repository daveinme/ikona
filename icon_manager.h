#ifndef ICON_MANAGER_H
#define ICON_MANAGER_H

#include <gtk/gtk.h>

/* Enum delle icone disponibili */
typedef enum {
    ICON_FOLDER_FILL,
    ICON_FOLDER_LINE,
    ICON_FILE_IMAGE,
    ICON_GALLERY_UPLOAD_FILL,
    ICON_GALLERY_UPLOAD_LINE,
    ICON_FLAG,
    ICON_HOME_GEAR_FILL,
    ICON_HOME_GEAR_LINE,
    ICON_PRINTER,
    ICON_ARROW_LEFT,
    ICON_ARROW_RIGHT,
    ICON_ARROW_LEFT_FILL,
    ICON_ARROW_RIGHT_FILL,
} IconType;

/* Inizializza il manager delle icone */
void icon_manager_init(const char *icons_dir);

/* Chiudi e libera risorse */
void icon_manager_cleanup(void);

/* Crea un bottone con icona e testo */
GtkWidget* create_button_with_icon(IconType icon, const char *label, int icon_size);

/* Crea un bottone con solo icona */
GtkWidget* create_icon_only_button(IconType icon, int icon_size, const char *tooltip);

/* Ottieni la GdkPixbuf di un'icona (per usi avanzati) */
GdkPixbuf* get_icon_pixbuf(IconType icon, int size);

#endif // ICON_MANAGER_H
