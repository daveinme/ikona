#include "editor.h"
#include "icon_manager.h"
#include "gui_utils.h"
#include "utils.h"
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-enum-types.h>

// External references from ikona.c and viewer.c
extern char *image_files[10000];
extern char *original_files[10000];
extern int selected_images[10000];
extern int selected_count;
extern int selected_image_index;

static GtkWidget *editor_window = NULL;
static GdkPixbuf *original_pixbuf = NULL;
static GdkPixbuf *current_pixbuf = NULL;
static GtkWidget *image_display = NULL;
static char *current_image_path = NULL;

// UI widgets - removed overlay and event_box for simpler scrolling

// Filter values structure
typedef struct {
    int brightness;   // -100 to +100
} FilterValues;

static FilterValues filter_values = {0};
static GtkWidget *label_brightness = NULL;
static gboolean is_applying_filter = FALSE;

// Canvas control
static GtkWidget *canvas_fixed = NULL;

// Multi-image editing
static int current_edit_index = 0;  // Index in selected_images array
static GtkWidget *image_counter_label = NULL;

// Forward declarations
static void update_image_display(void);
static void update_filter_label(GtkWidget *label, int value);

static void on_editor_window_destroyed(GtkWidget *widget, gpointer data) {
    if (original_pixbuf) {
        g_object_unref(original_pixbuf);
        original_pixbuf = NULL;
    }
    if (current_pixbuf) {
        g_object_unref(current_pixbuf);
        current_pixbuf = NULL;
    }
    if (current_image_path) {
        g_free(current_image_path);
        current_image_path = NULL;
    }
    editor_window = NULL;
    canvas_fixed = NULL;
}

static gboolean on_editor_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_F11) {
        static gboolean is_fullscreen = FALSE;
        if (is_fullscreen) {
            gtk_window_unfullscreen(GTK_WINDOW(editor_window));
            is_fullscreen = FALSE;
        } else {
            gtk_window_fullscreen(GTK_WINDOW(editor_window));
            is_fullscreen = TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

static void update_image_display(void) {
    if (!current_pixbuf || !image_display || !canvas_fixed) {
        return;
    }

    int img_width = gdk_pixbuf_get_width(current_pixbuf);
    int img_height = gdk_pixbuf_get_height(current_pixbuf);

    // Get canvas dimensions
    int window_width, window_height;
    gtk_window_get_size(GTK_WINDOW(editor_window), &window_width, &window_height);
    int canvas_width = window_width - 260;
    int canvas_height = window_height - 20;

    // Calculate fit-to-canvas scale
    double scale_w = (double)canvas_width / img_width;
    double scale_h = (double)canvas_height / img_height;
    double scale = (scale_w < scale_h) ? scale_w : scale_h;

    int scaled_width = (int)(img_width * scale);
    int scaled_height = (int)(img_height * scale);

    GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple(current_pixbuf,
                                                       scaled_width,
                                                       scaled_height,
                                                       GDK_INTERP_BILINEAR);

    // Center image in canvas
    int pos_x = (canvas_width - scaled_width) / 2;
    int pos_y = (canvas_height - scaled_height) / 2;

    gtk_fixed_move(GTK_FIXED(canvas_fixed), image_display, pos_x, pos_y);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_display), scaled_pixbuf);

    g_object_unref(scaled_pixbuf);
}

// Load image at current index
static void load_current_image(void) {
    if (current_edit_index < 0 || current_edit_index >= selected_count) {
        return;
    }

    int global_idx = selected_images[current_edit_index];
    // NUOVO: Usa path ORIGINALE (non thumbnail) per editing
    const char *image_path = original_files[global_idx];

    g_print("Loading image %d/%d: %s\n", current_edit_index + 1, selected_count, image_path);

    // Free previous pixbufs
    if (original_pixbuf) {
        g_object_unref(original_pixbuf);
        original_pixbuf = NULL;
    }
    if (current_pixbuf && current_pixbuf != original_pixbuf) {
        g_object_unref(current_pixbuf);
        current_pixbuf = NULL;
    }
    if (current_image_path) {
        g_free(current_image_path);
    }

    current_image_path = g_strdup(image_path);

    // Load new image (ORIGINALE a piena risoluzione)
    GError *error = NULL;
    original_pixbuf = gdk_pixbuf_new_from_file(image_path, &error);
    if (!original_pixbuf) {
        g_warning("Failed to load image '%s': %s", image_path, error->message);
        g_error_free(error);
        return;
    }
    current_pixbuf = g_object_ref(original_pixbuf);

    // Reset filters
    filter_values.brightness = 0;
    update_filter_label(label_brightness, 0);

    // Update counter label
    if (image_counter_label) {
        char counter_text[64];
        snprintf(counter_text, sizeof(counter_text), "Image %d of %d",
                 current_edit_index + 1, selected_count);
        gtk_label_set_text(GTK_LABEL(image_counter_label), counter_text);
    }

    update_image_display();
}

static void on_editor_window_size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer data) {
    update_image_display();
}

// Utility function to clamp pixel values
static inline guchar clamp_pixel(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (guchar)value;
}

// Apply brightness filter
static void apply_brightness(GdkPixbuf *pixbuf, int brightness) {
    if (brightness == 0) return;

    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    // Map brightness from [-100, +100] to [-128, +127]
    int adjustment = (int)(brightness * 1.27);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            guchar *p = pixels + y * rowstride + x * n_channels;
            p[0] = clamp_pixel(p[0] + adjustment); // R
            p[1] = clamp_pixel(p[1] + adjustment); // G
            p[2] = clamp_pixel(p[2] + adjustment); // B
            // Keep alpha channel unchanged if present
        }
    }
}

// Apply all filters to the image
static void apply_all_filters(void) {
    if (is_applying_filter) {
        g_print("DEBUG: apply_all_filters - già in esecuzione, skip\n");
        return;
    }
    if (!original_pixbuf) {
        g_print("DEBUG: apply_all_filters - original_pixbuf è NULL\n");
        return;
    }

    g_print("DEBUG: apply_all_filters - START (brightness=%d)\n", filter_values.brightness);
    is_applying_filter = TRUE;

    // Start from original image
    GdkPixbuf *result = gdk_pixbuf_copy(original_pixbuf);
    g_print("DEBUG: Copiato original_pixbuf (%dx%d)\n",
            gdk_pixbuf_get_width(result), gdk_pixbuf_get_height(result));

    // Apply brightness filter
    apply_brightness(result, filter_values.brightness);
    g_print("DEBUG: Applicato brightness\n");

    // Replace current pixbuf
    if (current_pixbuf && current_pixbuf != original_pixbuf) {
        g_object_unref(current_pixbuf);
    }
    current_pixbuf = result;
    g_print("DEBUG: Sostituito current_pixbuf\n");

    // Update display
    update_image_display();
    g_print("DEBUG: apply_all_filters - DONE\n");

    is_applying_filter = FALSE;
}

// Update filter label with current value
static void update_filter_label(GtkWidget *label, int value) {
    char text[16];
    snprintf(text, sizeof(text), "%+d", value);
    gtk_label_set_text(GTK_LABEL(label), text);
}

// Brightness callbacks
#define FILTER_STEP 5

static void on_brightness_inc(GtkButton *button, gpointer data) {
    filter_values.brightness = (filter_values.brightness + FILTER_STEP > 100) ?
                                100 : filter_values.brightness + FILTER_STEP;
    g_print("DEBUG: on_brightness_inc - nuovo valore: %d\n", filter_values.brightness);
    update_filter_label(label_brightness, filter_values.brightness);
    apply_all_filters();
}

static void on_brightness_dec(GtkButton *button, gpointer data) {
    filter_values.brightness = (filter_values.brightness - FILTER_STEP < -100) ?
                                -100 : filter_values.brightness - FILTER_STEP;
    update_filter_label(label_brightness, filter_values.brightness);
    apply_all_filters();
}

// Reset filters on current image
static void on_reset_filters(GtkButton *button, gpointer data) {
    filter_values.brightness = 0;
    update_filter_label(label_brightness, 0);

    if (current_pixbuf && current_pixbuf != original_pixbuf) {
        g_object_unref(current_pixbuf);
    }
    current_pixbuf = g_object_ref(original_pixbuf);
    update_image_display();
}

// Navigation callbacks
static void on_previous_image(GtkButton *button, gpointer data) {
    if (current_edit_index > 0) {
        current_edit_index--;
        load_current_image();
    }
}

static void on_next_image(GtkButton *button, gpointer data) {
    if (current_edit_index < selected_count - 1) {
        current_edit_index++;
        load_current_image();
    }
}

// Cancel callback - close without saving
static void on_cancel(GtkButton *button, gpointer data) {
    g_print("DEBUG: Cancel - chiusura senza salvare\n");
    // Just close the window, changes are discarded
    gtk_widget_destroy(editor_window);
}

// Save & Next callback
static void on_save_and_next(GtkButton *button, gpointer data) {
    if (!current_pixbuf || !current_image_path) {
        g_print("ERROR: Cannot save - pixbuf or path NULL\n");
        return;
    }

    // Detect format from extension
    const char *ext = strrchr(current_image_path, '.');
    const char *format = "jpeg";

    if (ext != NULL) {
        if (g_ascii_strcasecmp(ext, ".png") == 0) {
            format = "png";
        } else if (g_ascii_strcasecmp(ext, ".jpg") == 0 || g_ascii_strcasecmp(ext, ".jpeg") == 0) {
            format = "jpeg";
        }
    }

    // Save image
    GError *error = NULL;
    gboolean success;

    if (strcmp(format, "png") == 0) {
        success = gdk_pixbuf_save(current_pixbuf, current_image_path, "png", &error, NULL);
    } else {
        success = gdk_pixbuf_save(current_pixbuf, current_image_path, "jpeg", &error, "quality", "95", NULL);
    }

    if (!success) {
        g_warning("Error saving image '%s': %s", current_image_path, error->message);
        g_error_free(error);
        return;
    }

    // NUOVO: Rigenera thumbnail dopo salvataggio originale
    char *thumb_path = original_to_thumb_path(current_image_path);
    if (thumb_path) {
        if (create_thumbnail(current_image_path, thumb_path, 512) != 0) {
            g_warning("Failed to update thumbnail for: %s", current_image_path);
        }
        // Note: thumb_path è static, non serve free
    }

    g_print("Image %d/%d saved: %s\n", current_edit_index + 1, selected_count, current_image_path);

    // Move to next or close
    if (current_edit_index < selected_count - 1) {
        current_edit_index++;
        load_current_image();
    } else {
        g_print("All images processed, closing editor\n");
        gtk_widget_destroy(editor_window);
    }
}

// Save All & Close callback
static void on_save_all_and_close(GtkButton *button, gpointer data) {
    // Save current image first
    if (current_pixbuf && current_image_path) {
        const char *ext = strrchr(current_image_path, '.');
        const char *format = "jpeg";

        if (ext && g_ascii_strcasecmp(ext, ".png") == 0) {
            format = "png";
        }

        GError *error = NULL;
        gboolean success;

        if (strcmp(format, "png") == 0) {
            success = gdk_pixbuf_save(current_pixbuf, current_image_path, "png", &error, NULL);
        } else {
            success = gdk_pixbuf_save(current_pixbuf, current_image_path, "jpeg", &error, "quality", "95", NULL);
        }

        if (success) {
            g_print("Saved image %d/%d\n", current_edit_index + 1, selected_count);

            // NUOVO: Rigenera thumbnail dopo salvataggio originale
            char *thumb_path = original_to_thumb_path(current_image_path);
            if (thumb_path) {
                if (create_thumbnail(current_image_path, thumb_path, 512) != 0) {
                    g_warning("Failed to update thumbnail for: %s", current_image_path);
                }
            }
        }
    }

    gtk_widget_destroy(editor_window);
}

GtkWidget *create_photo_editor_window(const char *image_path, const char *icons_dir) {
    if (editor_window) {
        gtk_window_present(GTK_WINDOW(editor_window));
        return editor_window;
    }

    // Initialize for multi-image editing
    filter_values.brightness = 0;

    // Find current image in selected_images array
    current_edit_index = 0;
    for (int i = 0; i < selected_count; i++) {
        if (selected_images[i] == selected_image_index) {
            current_edit_index = i;
            break;
        }
    }

    editor_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(editor_window), "Ikona Photo Editor");
    gtk_window_set_default_size(GTK_WINDOW(editor_window), 1280, 800);
    gtk_window_set_position(GTK_WINDOW(editor_window), GTK_WIN_POS_CENTER_ALWAYS);

    // Image will be loaded by load_current_image() at the end
    current_image_path = NULL;
    original_pixbuf = NULL;
    current_pixbuf = NULL;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(editor_window), main_box);

    // Fixed canvas area (no scrollbars, fixed size)
    canvas_fixed = gtk_fixed_new();
    gtk_widget_set_hexpand(canvas_fixed, TRUE);
    gtk_widget_set_vexpand(canvas_fixed, TRUE);

    // Add black background to canvas
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "#canvas { background-color: #2b2b2b; }", -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(canvas_fixed);
    gtk_style_context_add_provider(context,
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_widget_set_name(canvas_fixed, "canvas");

    gtk_box_pack_start(GTK_BOX(main_box), canvas_fixed, TRUE, TRUE, 0);

    image_display = gtk_image_new();
    gtk_fixed_put(GTK_FIXED(canvas_fixed), image_display, 0, 0);

    // Controls sidebar
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(controls_box, 250, -1); // Fixed width for controls
    gtk_widget_set_margin_start(controls_box, 10);
    gtk_widget_set_margin_end(controls_box, 10);
    gtk_widget_set_margin_top(controls_box, 10);
    gtk_widget_set_margin_bottom(controls_box, 10);
    gtk_box_pack_start(GTK_BOX(main_box), controls_box, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Editing Controls</b>");
    gtk_box_pack_start(GTK_BOX(controls_box), label, FALSE, FALSE, 0);

    // Separator
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), separator1, FALSE, FALSE, 5);

    // Brightness control
    GtkWidget *brightness_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);

    GtkWidget *brightness_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *brightness_name_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(brightness_name_label), "<b>Brightness</b>");
    gtk_widget_set_halign(brightness_name_label, GTK_ALIGN_START);
    label_brightness = gtk_label_new("0");
    gtk_widget_set_halign(label_brightness, GTK_ALIGN_END);

    gtk_box_pack_start(GTK_BOX(brightness_header), brightness_name_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(brightness_header), label_brightness, FALSE, FALSE, 0);

    GtkWidget *brightness_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    char brightness_dec_icon_path[1024];
    char brightness_inc_icon_path[1024];
    snprintf(brightness_dec_icon_path, sizeof(brightness_dec_icon_path), "%s/brightness_dec_icon.png", icons_dir);
    snprintf(brightness_inc_icon_path, sizeof(brightness_inc_icon_path), "%s/brightness_inc_icon.png", icons_dir);
    GtkWidget *btn_brightness_dec = create_icon_button(brightness_dec_icon_path, "➖ Diminuisci");
    GtkWidget *btn_brightness_inc = create_icon_button(brightness_inc_icon_path, "➕ Aumenta");
    gtk_widget_set_size_request(btn_brightness_dec, 50, 30);
    gtk_widget_set_size_request(btn_brightness_inc, 50, 30);
    g_signal_connect(btn_brightness_dec, "clicked", G_CALLBACK(on_brightness_dec), NULL);
    g_signal_connect(btn_brightness_inc, "clicked", G_CALLBACK(on_brightness_inc), NULL);
    gtk_box_pack_start(GTK_BOX(brightness_buttons), btn_brightness_dec, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(brightness_buttons), btn_brightness_inc, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(brightness_vbox), brightness_header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brightness_vbox), brightness_buttons, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls_box), brightness_vbox, FALSE, FALSE, 0);

    // Separator
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), separator2, FALSE, FALSE, 5);

    // Reset button
    char reset_icon_path[1024];
    snprintf(reset_icon_path, sizeof(reset_icon_path), "%s/reset_icon.png", icons_dir);
    GtkWidget *button_reset = create_icon_button(reset_icon_path, "🔄 Reset Filtri");
    g_signal_connect(button_reset, "clicked", G_CALLBACK(on_reset_filters), NULL);
    gtk_box_pack_start(GTK_BOX(controls_box), button_reset, FALSE, FALSE, 0);

    // Separator
    GtkWidget *separator3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), separator3, FALSE, FALSE, 5);

    // Rotation controls (disabled)
    GtkWidget *rotation_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(controls_box), rotation_box, FALSE, FALSE, 0);

    char rotate_left_icon_path[1024];
    char rotate_right_icon_path[1024];
    snprintf(rotate_left_icon_path, sizeof(rotate_left_icon_path), "%s/rotate_left_icon.png", icons_dir);
    snprintf(rotate_right_icon_path, sizeof(rotate_right_icon_path), "%s/rotate_right_icon.png", icons_dir);
    GtkWidget *button_rotate_left = create_icon_button(rotate_left_icon_path, "⬅️ Ruota Sinistra");
    gtk_widget_set_sensitive(button_rotate_left, FALSE);
    gtk_box_pack_start(GTK_BOX(rotation_box), button_rotate_left, TRUE, TRUE, 0);

    GtkWidget *button_rotate_right = create_icon_button(rotate_right_icon_path, "➡️ Ruota Destra");
    gtk_widget_set_sensitive(button_rotate_right, FALSE);
    gtk_box_pack_start(GTK_BOX(rotation_box), button_rotate_right, TRUE, TRUE, 0);

    // Separator
    GtkWidget *separator4 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), separator4, FALSE, FALSE, 5);

    // Image counter
    image_counter_label = gtk_label_new("Image 1 of 1");
    gtk_box_pack_start(GTK_BOX(controls_box), image_counter_label, FALSE, FALSE, 0);

    // Navigation buttons
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    char prev_icon_path[1024];
    char next_icon_path[1024];
    snprintf(prev_icon_path, sizeof(prev_icon_path), "%s/prev_icon.png", icons_dir);
    snprintf(next_icon_path, sizeof(next_icon_path), "%s/next_icon.png", icons_dir);
    GtkWidget *button_prev = create_icon_button(prev_icon_path, "⬅️ Precedente");
    g_signal_connect(button_prev, "clicked", G_CALLBACK(on_previous_image), NULL);
    gtk_box_pack_start(GTK_BOX(nav_box), button_prev, TRUE, TRUE, 0);

    GtkWidget *button_next = create_icon_button(next_icon_path, "Successivo ➡️");
    g_signal_connect(button_next, "clicked", G_CALLBACK(on_next_image), NULL);
    gtk_box_pack_start(GTK_BOX(nav_box), button_next, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(controls_box), nav_box, FALSE, FALSE, 0);

    // Separator before save
    GtkWidget *separator5 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), separator5, FALSE, FALSE, 5);

    // Save buttons
    char save_next_icon_path[1024];
    snprintf(save_next_icon_path, sizeof(save_next_icon_path), "%s/save_next_icon.png", icons_dir);
    GtkWidget *button_save_next = create_icon_button(save_next_icon_path, "💾 Salva & Successivo");
    g_signal_connect(button_save_next, "clicked", G_CALLBACK(on_save_and_next), NULL);
    gtk_box_pack_start(GTK_BOX(controls_box), button_save_next, FALSE, FALSE, 0);

    GtkWidget *save_actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    char cancel_icon_path[1024];
    char save_all_icon_path[1024];
    snprintf(cancel_icon_path, sizeof(cancel_icon_path), "%s/cancel_icon.png", icons_dir);
    snprintf(save_all_icon_path, sizeof(save_all_icon_path), "%s/save_all_icon.png", icons_dir);
    GtkWidget *button_cancel = create_icon_button(cancel_icon_path, "❌ Annulla");
    g_signal_connect(button_cancel, "clicked", G_CALLBACK(on_cancel), NULL);
    gtk_box_pack_start(GTK_BOX(save_actions_box), button_cancel, TRUE, TRUE, 0);

    GtkWidget *button_save_all = create_icon_button(save_all_icon_path, "💾 Salva Tutti");
    g_signal_connect(button_save_all, "clicked", G_CALLBACK(on_save_all_and_close), NULL);
    gtk_box_pack_start(GTK_BOX(save_actions_box), button_save_all, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(controls_box), save_actions_box, FALSE, FALSE, 0);

    g_signal_connect(editor_window, "destroy", G_CALLBACK(on_editor_window_destroyed), NULL);
    g_signal_connect(editor_window, "key-press-event", G_CALLBACK(on_editor_key_press), NULL);
    g_signal_connect(editor_window, "size-allocate", G_CALLBACK(on_editor_window_size_allocate), NULL);

    gtk_widget_show_all(editor_window);

    // Load first image in selection
    load_current_image();

    return editor_window;
}
