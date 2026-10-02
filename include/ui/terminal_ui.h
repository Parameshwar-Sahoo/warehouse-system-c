#ifndef WMS_TERMINAL_UI_H
#define WMS_TERMINAL_UI_H

#include "core/inventory_manager.h"
#include "core/order_processor.h"
#include "hal/device_driver.h"
#include "ipc/shared_memory.h"

#define MAX_HISTORY_LINES 8

void ui_clear_screen(void);
void ui_render_header(void);
void ui_render_telemetry_bar(InventoryManager* inv, OrderProcessor* op, DeviceDriver* drv);
void ui_render_bay_grid(InventoryManager* inv);
void ui_render_event_log(char history[][128], size_t count);
void ui_render_menu(void);

#endif /* WMS_TERMINAL_UI_H */
