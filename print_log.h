#ifndef PRINT_LOG_H
#define PRINT_LOG_H

#include <time.h>

/* Log a print event for today */
void log_print(int copies);

/* Get total prints for a specific date (YYYY-MM-DD format) */
int get_print_total_for_date(const char *date);

/* Get all dates in the log as a GList of strings (must be freed) */
#include <glib.h>
GList* get_all_print_dates(void);

/* Clear all print logs with confirmation dialog */
void clear_print_log_with_dialog(void);

#endif // PRINT_LOG_H
