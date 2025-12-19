#ifndef PRINTER_H
#define PRINTER_H

#include <gtk/gtk.h>

typedef struct {
    char *name;
    char *display_name;
} Printer;

typedef struct {
    char *path;
    int copies;
    GtkWidget *spin_button;
} PrintItem;

// Ottiene la lista di stampanti disponibili dal sistema
// Ritorna un array di Printer, e scrive il conteggio in *count
// Libera con free_printers()
Printer* get_available_printers(int *count);

// Libera la memoria allocata da get_available_printers()
void free_printers(Printer *printers, int count);

// Stampa un file su una stampante
// Ritorna 0 se successo, -1 se errore
int print_file(const char *printer_name, const char *file_path);

// Crea la finestra mobile di stampa
GtkWidget* create_print_window(const char *icons_dir);

// Ottiene il nome della stampante selezionata nella finestra stampa
const char* get_selected_printer_from_window(void);

// Aggiorna la griglia di stampa nella finestra stampa
void refresh_print_window_grid(void);

#endif // PRINTER_H
