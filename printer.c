#include "printer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

Printer* get_available_printers(int *count) {
    *count = 0;
    Printer *printers = NULL;

    // Usa lpstat per ottenere la lista di stampanti
    // Estrae il nome della stampante dalla linea "printer <name> is ..."
    FILE *fp = popen("lpstat -p 2>/dev/null", "r");
    if (!fp) {
        g_warning("Failed to execute lpstat command");
        return NULL;
    }

    // Leggi la lista di stampanti
    char line[512];
    int capacity = 10;
    int current = 0;
    printers = (Printer *)malloc(sizeof(Printer) * capacity);

    while (fgets(line, sizeof(line), fp)) {
        // Rimuovi newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Estrai il nome della stampante dalla linea "printer <name> is ..."
        // o "la stampante <name> è ..." (italiano)
        char printer_name[256] = {0};

        // Prova pattern inglese "printer <name> is"
        if (sscanf(line, "printer %s is", printer_name) == 1) {
            // Trovato!
        }
        // Prova pattern italiano "la stampante <name> è"
        else if (sscanf(line, "la stampante %s è", printer_name) == 1) {
            // Trovato!
        }
        // Altre varianti locali potrebbero essere necessarie
        else {
            continue;
        }

        // Salta linee vuote
        if (strlen(printer_name) == 0) continue;

        // Ridimensiona array se necessario
        if (current >= capacity) {
            capacity *= 2;
            printers = (Printer *)realloc(printers, sizeof(Printer) * capacity);
        }

        printers[current].name = g_strdup(printer_name);
        printers[current].display_name = g_strdup(printer_name);
        current++;
    }

    pclose(fp);

    if (current == 0) {
        g_warning("No printers found");
        free(printers);
        return NULL;
    }

    *count = current;
    return printers;
}

void free_printers(Printer *printers, int count) {
    if (!printers) return;
    for (int i = 0; i < count; i++) {
        g_free(printers[i].name);
        g_free(printers[i].display_name);
    }
    free(printers);
}

int print_file(const char *printer_name, const char *file_path) {
    if (!printer_name || !file_path) {
        g_warning("Invalid printer name or file path");
        return -1;
    }

    // Usa comando 'lp' per stampare
    char command[512];
    snprintf(command, sizeof(command), "lp -d %s \"%s\" 2>/dev/null", printer_name, file_path);

    int result = system(command);

    if (result != 0) {
        g_warning("Failed to print file with lp command. Exit code: %d", result);
        return -1;
    }

    g_info("Print job sent successfully to printer: %s for file: %s", printer_name, file_path);
    return 0;
}
