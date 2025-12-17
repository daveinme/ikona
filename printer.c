#include "printer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#ifdef _WIN32
#include <windows.h>
#include <winspool.h>
#include <shellapi.h>
#include <wingdi.h>
#endif

Printer* get_available_printers(int *count) {
    Printer *printers;

    *count = 0;
    printers = NULL;

#ifdef _WIN32
    /* Windows: usa EnumPrinters API */
    DWORD needed, returned;
    PRINTER_INFO_4 *printer_info;
    DWORD i;

    needed = 0;
    returned = 0;

    /* Prima chiamata per ottenere la dimensione necessaria */
    EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, NULL, 0, &needed, &returned);

    if (needed == 0) {
        g_warning("No printers found on Windows");
        return NULL;
    }

    printer_info = (PRINTER_INFO_4 *)malloc(needed);
    if (!printer_info) {
        g_warning("Failed to allocate memory for printer info");
        return NULL;
    }

    /* Seconda chiamata per ottenere effettivamente le stampanti */
    if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4,
                      (LPBYTE)printer_info, needed, &needed, &returned)) {
        g_warning("Failed to enumerate printers");
        free(printer_info);
        return NULL;
    }

    if (returned == 0) {
        g_warning("No printers available");
        free(printer_info);
        return NULL;
    }

    printers = (Printer *)malloc(sizeof(Printer) * returned);

    for (i = 0; i < returned; i++) {
        printers[i].name = g_strdup(printer_info[i].pPrinterName);
        printers[i].display_name = g_strdup(printer_info[i].pPrinterName);
    }

    *count = returned;
    free(printer_info);
    return printers;

#else
    /* Unix/Linux: usa lpstat */
    FILE *fp;
    char line[512];
    int capacity;
    int current;

    fp = popen("lpstat -p 2>/dev/null", "r");
    if (!fp) {
        g_warning("Failed to execute lpstat command");
        return NULL;
    }

    capacity = 10;
    current = 0;
    printers = (Printer *)malloc(sizeof(Printer) * capacity);

    while (fgets(line, sizeof(line), fp)) {
        size_t len;
        char printer_name[256];

        /* Rimuovi newline */
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        memset(printer_name, 0, sizeof(printer_name));

        /* Prova pattern inglese "printer <name> is" */
        if (sscanf(line, "printer %s is", printer_name) == 1) {
            /* Trovato! */
        }
        /* Prova pattern italiano "la stampante <name> è" */
        else if (sscanf(line, "la stampante %s è", printer_name) == 1) {
            /* Trovato! */
        }
        else {
            continue;
        }

        /* Salta linee vuote */
        if (strlen(printer_name) == 0) continue;

        /* Ridimensiona array se necessario */
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
#endif
}

void free_printers(Printer *printers, int count) {
    int i;

    if (!printers) return;

    for (i = 0; i < count; i++) {
        g_free(printers[i].name);
        g_free(printers[i].display_name);
    }
    free(printers);
}

int print_file(const char *printer_name, const char *file_path) {
#ifdef _WIN32
    char abs_path[MAX_PATH];
    HINSTANCE exec_result;
    WIN32_FIND_DATA find_data;
    HANDLE find_handle;
#else
    char command[512];
    int result;
#endif

    if (!printer_name || !file_path) {
        g_warning("Invalid printer name or file path");
        return -1;
    }

#ifdef _WIN32
    g_info("Stampa su Windows: printer='%s', file='%s'", printer_name, file_path);

    /* Converti a percorso assoluto */
    if (GetFullPathName(file_path, sizeof(abs_path), abs_path, NULL) == 0) {
        g_warning("Impossibile convertire percorso assoluto: %s", file_path);
        return -1;
    }

    /* Controlla se il file esiste */
    find_handle = FindFirstFile(abs_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        g_warning("File non trovato: %s", abs_path);
        return -1;
    }
    FindClose(find_handle);

    g_info("File trovato: %s", abs_path);

    /* Attendi un po' prima di stampare (per stampanti WiFi) */
    g_info("Attesa 2 secondi prima di inviare il job (per stampanti WiFi)");
    Sleep(2000);

    /* Metodo 1: ShellExecute con "printto" - NESSUN DIALOG */
    g_info("Tentativo 1 - ShellExecute printto senza dialog");
    g_info("Nome stampante esatto: '%s'", printer_name);
    g_info("File: '%s'", abs_path);

    /* Usa ShellExecute con verbo "printto" con solo il nome della stampante */
    exec_result = ShellExecute(NULL, "printto", abs_path, (LPCSTR)printer_name, NULL, SW_HIDE);

    g_info("ShellExecute printto risultato: %p", exec_result);

    if ((intptr_t)exec_result > 32) {
        g_info("Stampa inviata in coda con successo a: %s", printer_name);
        Sleep(1000);  /* Attesa per assicurare che il job arrivi alla coda */
        return 0;
    }

    /* Se il primo metodo fallisce, prova con "print" */
    g_warning("ShellExecute printto fallito (errore %p), provo print...", exec_result);

    exec_result = ShellExecute(NULL, "print", abs_path, NULL, NULL, SW_HIDE);

    g_info("ShellExecute print risultato: %p", exec_result);

    if ((intptr_t)exec_result > 32) {
        g_info("Stampa inviata con successo tramite print");
        Sleep(1000);
        return 0;
    }

    /* Se entrambi falliscono */
    g_warning("Tutti i metodi falliti (ultimo errore %p)", exec_result);
    return -1;

#else
    /* Unix/Linux: usa comando 'lp' */
    snprintf(command, sizeof(command), "lp -d %s \"%s\" 2>/dev/null", printer_name, file_path);

    result = system(command);

    if (result != 0) {
        g_warning("Failed to print file with lp command. Exit code: %d", result);
        return -1;
    }

    g_info("Print job sent successfully to printer: %s for file: %s", printer_name, file_path);
    return 0;
#endif
}
