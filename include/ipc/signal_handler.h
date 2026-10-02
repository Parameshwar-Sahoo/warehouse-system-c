#ifndef WMS_SIGNAL_HANDLER_H
#define WMS_SIGNAL_HANDLER_H

#include <signal.h>
#include <stdbool.h>

void signals_init(void);
bool signals_is_shutdown_requested(void);
void signals_request_shutdown(void);

#endif /* WMS_SIGNAL_HANDLER_H */
