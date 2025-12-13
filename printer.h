#ifndef PRINTER_H
#define PRINTER_H

typedef struct {
    char *name;
    char *display_name;
} Printer;

// Ottiene la lista di stampanti disponibili dal sistema
// Ritorna un array di Printer, e scrive il conteggio in *count
// Libera con free_printers()
Printer* get_available_printers(int *count);

// Libera la memoria allocata da get_available_printers()
void free_printers(Printer *printers, int count);

// Stampa un file su una stampante
// Ritorna 0 se successo, -1 se errore
int print_file(const char *printer_name, const char *file_path);

#endif // PRINTER_H
