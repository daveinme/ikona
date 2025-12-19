#include "icon_manager.h"
#include <string.h>
#include <glib.h>

/* Cache delle icone caricate */
typedef struct {
    char *path;
    GdkPixbuf *pixbuf;
    int size;
} CachedIcon;

static char icons_base_dir[1024] = "";
static GHashTable *icon_cache = NULL;

/* Mappa enum → filename */
static const char* icon_filenames[] = {
    /* RemixIcon (mappate a custom) */
    [ICON_FOLDER_FILL] = "folder_icon.png",
    [ICON_FOLDER_LINE] = "folder_icon.png",
    [ICON_FILE_IMAGE] = "file_image_icon.png",
    [ICON_GALLERY_UPLOAD_FILL] = "gallery_upload_icon.png",
    [ICON_GALLERY_UPLOAD_LINE] = "gallery_upload_icon.png",
    [ICON_FLAG] = "flag_icon.png",
    [ICON_HOME_GEAR_FILL] = "home_gear_icon.png",
    [ICON_HOME_GEAR_LINE] = "home_gear_icon.png",
    [ICON_PRINTER] = "printer_icon.png",
    [ICON_ARROW_LEFT] = "arrow_left_icon.png",
    [ICON_ARROW_RIGHT] = "arrow_right_icon.png",
    [ICON_ARROW_LEFT_FILL] = "arrow_left_fill_icon.png",
    [ICON_ARROW_RIGHT_FILL] = "arrow_right_fill_icon.png",

    /* Navbar Icons */
    [ICON_IMPORTA] = "importa_icon.png",
    [ICON_FOTO] = "camera_icon-icons.com_72364.png",
    [ICON_STAMPA_NAVBAR] = "stampa_icon.png",
    [ICON_CONFIG] = "config_icon.png",

    /* Sidebar Importa */
    [ICON_IMPORTA_SIDEBAR] = "importa_sidebar_icon.png",
    [ICON_AGGIORNA] = "aggiorna_icon.png",

    /* Sidebar View */
    [ICON_SELEZIONA_TUTTO] = "seleziona_tutto_icon.png",
    [ICON_DESELEZIONA] = "deseleziona_icon.png",
    [ICON_EDITOR] = "editor_icon.png",
    [ICON_STAMPA_VIEW] = "stampa_wiev_icon.png",
    [ICON_MONITOR] = "monitor_icon.png",
    [ICON_VIEW_ALL] = "tutte_icon.png",
    [ICON_TRASH] = "pulisci_icon.png",
};

/* Genera chiave cache: icon_type_size */
static char* get_cache_key(IconType icon, int size) {
    static char key[64];
    snprintf(key, sizeof(key), "%d_%d", icon, size);
    return key;
}

/* Carica un'icona da file */
static GdkPixbuf* load_icon_from_file(IconType icon, int size) {
    char full_path[2048];
    GdkPixbuf *pixbuf;
    GError *error = NULL;

    if (icon < 0 || icon >= sizeof(icon_filenames)/sizeof(icon_filenames[0])) {
        g_warning("Invalid icon type: %d", icon);
        return NULL;
    }

    snprintf(full_path, sizeof(full_path), "%s/%s", icons_base_dir, icon_filenames[icon]);

    pixbuf = gdk_pixbuf_new_from_file_at_scale(full_path, size, size, TRUE, &error);
    if (!pixbuf) {
        g_warning("Failed to load icon '%s': %s", full_path, error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return NULL;
    }

    g_info("Loaded icon: %s (size: %d)", full_path, size);
    return pixbuf;
}

void icon_manager_init(const char *icons_dir) {
    if (!icons_dir) {
        g_warning("Icons directory not specified");
        return;
    }

    strncpy(icons_base_dir, icons_dir, sizeof(icons_base_dir) - 1);

    if (icon_cache) {
        g_hash_table_destroy(icon_cache);
    }

    icon_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify)g_object_unref);

    g_info("Icon manager initialized with directory: %s", icons_base_dir);
}

void icon_manager_cleanup(void) {
    if (icon_cache) {
        g_hash_table_destroy(icon_cache);
        icon_cache = NULL;
    }
    g_info("Icon manager cleaned up");
}

GdkPixbuf* get_icon_pixbuf(IconType icon, int size) {
    char *key;
    GdkPixbuf *pixbuf;

    if (!icon_cache) {
        g_warning("Icon manager not initialized");
        return NULL;
    }

    if (size <= 0) size = 24;  // Default size

    key = get_cache_key(icon, size);

    /* Controlla cache */
    pixbuf = g_hash_table_lookup(icon_cache, key);
    if (pixbuf) {
        return pixbuf;
    }

    /* Carica da file se non in cache */
    pixbuf = load_icon_from_file(icon, size);
    if (pixbuf) {
        g_hash_table_insert(icon_cache, g_strdup(key), pixbuf);
    }

    return pixbuf;
}

GtkWidget* create_button_with_icon(IconType icon, const char *label, int icon_size) {
    GtkWidget *button;
    GtkWidget *box;
    GtkWidget *image_widget;
    GtkWidget *label_widget;
    GdkPixbuf *pixbuf;

    if (icon_size <= 0) icon_size = 24;

    button = gtk_button_new();
    gtk_widget_set_name(button, "icon-button");
    gtk_widget_set_size_request(button, 40, 40);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 2);
    gtk_widget_set_margin_end(box, 2);
    gtk_widget_set_margin_top(box, 2);
    gtk_widget_set_margin_bottom(box, 2);

    /* Icona */
    pixbuf = get_icon_pixbuf(icon, icon_size);
    if (pixbuf) {
        image_widget = gtk_image_new_from_pixbuf(pixbuf);
    } else {
        image_widget = gtk_label_new("❌");  /* Fallback se icona non trovata */
    }
    gtk_widget_set_halign(image_widget, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), image_widget, FALSE, FALSE, 0);

    /* Testo */
    if (label && strlen(label) > 0) {
        label_widget = gtk_label_new(label);
        gtk_label_set_justify(GTK_LABEL(label_widget), GTK_JUSTIFY_CENTER);
        gtk_label_set_line_wrap(GTK_LABEL(label_widget), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(label_widget), 8);
        gtk_box_pack_start(GTK_BOX(box), label_widget, FALSE, FALSE, 0);
    }

    gtk_container_add(GTK_CONTAINER(button), box);
    return button;
}

GtkWidget* create_icon_only_button(IconType icon, int icon_size, const char *tooltip) {
    GtkWidget *button;
    GtkWidget *image_widget;
    GtkWidget *box;
    GdkPixbuf *pixbuf;

    if (icon_size <= 0) icon_size = 24;

    button = gtk_button_new();
    gtk_widget_set_name(button, "icon-only-button");
    gtk_widget_set_size_request(button, 40, 40);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 3);
    gtk_widget_set_margin_end(box, 3);
    gtk_widget_set_margin_top(box, 3);
    gtk_widget_set_margin_bottom(box, 3);

    pixbuf = get_icon_pixbuf(icon, icon_size);
    if (pixbuf) {
        image_widget = gtk_image_new_from_pixbuf(pixbuf);
    } else {
        image_widget = gtk_label_new("❌");
    }

    gtk_widget_set_halign(image_widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image_widget, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), image_widget, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(button), box);

    if (tooltip && strlen(tooltip) > 0) {
        gtk_widget_set_tooltip_text(button, tooltip);
    }

    return button;
}
