#include "ui/terminal_ui.h"
#include <stdio.h>

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"

void ui_clear_screen(void) {
    printf("\033[2J\033[1;1H");
}

void ui_render_header(void) {
    printf("%s%s", ANSI_BOLD, ANSI_CYAN);
    printf("========================================================================================\n");
    printf("              WAREHOUSE SYSTEM (Pure C & Linux CharDev Kernel Driver)                   \n");
    printf("========================================================================================\n");
    printf("%s", ANSI_RESET);
}

void ui_render_telemetry_bar(InventoryManager* inv, OrderProcessor* op, DeviceDriver* drv) {
    wms_device_status_t status = {0};
    driver_get_status(drv, &status);

    float occ = inventory_get_occupancy_rate(inv);
    size_t total_items = inventory_get_total_items(inv);
    size_t fulfilled = order_processor_get_fulfilled_count(op);

    printf("%s[HARDWARE TELEMETRY & SYSTEM STATE]%s\n", ANSI_BOLD, ANSI_RESET);
    printf(" Driver: %s%s%s | Connected: %s%s%s | Active Locks: %s%u%s | Total Scans: %s%u%s\n",
           ANSI_YELLOW, driver_get_name(drv), ANSI_RESET,
           driver_is_connected(drv) ? ANSI_GREEN : ANSI_RED,
           driver_is_connected(drv) ? "YES" : "NO", ANSI_RESET,
           ANSI_RED, status.active_bay_locks, ANSI_RESET,
           ANSI_CYAN, status.total_events_logged, ANSI_RESET);

    printf(" Buffer Occupancy: %u/%u | Dropped Events: %s%u%s | Total Items: %zu | Wh Occupancy: %.1f%%\n",
           status.buffer_occupancy, status.buffer_capacity,
           status.dropped_events > 0 ? ANSI_RED : ANSI_GREEN,
           status.dropped_events, ANSI_RESET,
           total_items, occ);

    printf(" Orders Fulfilled: %s%zu%s | System Mode: POSIX C11 Architecture\n",
           ANSI_GREEN, fulfilled, ANSI_RESET);
    printf("----------------------------------------------------------------------------------------\n");
}

void ui_render_bay_grid(InventoryManager* inv) {
    printf("%s[STORAGE BAY RACKS LAYOUT - 24 BAYS]%s\n", ANSI_BOLD, ANSI_RESET);
    printf(" Legend: %s[FREE] %s%s[OCCUPIED] %s%s[LOCKED] %s\n\n",
           ANSI_GREEN, ANSI_RESET, ANSI_YELLOW, ANSI_RESET, ANSI_RED, ANSI_RESET);

    for (uint32_t i = 1; i <= TOTAL_BAYS; ++i) {
        StorageBay* bay = inventory_get_bay(inv, i);
        if (!bay) continue;

        printf("[Bay %2u ", bay->id);
        if (bay->is_locked) {
            printf("%sLOCKED  %s", ANSI_RED, ANSI_RESET);
        } else if (bay->is_occupied) {
            printf("%sOCCUPIED%s", ANSI_YELLOW, ANSI_RESET);
        } else {
            printf("%sEMPTY   %s", ANSI_GREEN, ANSI_RESET);
        }
        printf("] ");

        if (i % 4 == 0) {
            printf("\n");
        }
    }
    printf("\n----------------------------------------------------------------------------------------\n");
}

void ui_render_event_log(char history[][128], size_t count) {
    printf("%s[LIVE HARDWARE SCAN STREAM / EVENT LOG]%s\n", ANSI_BOLD, ANSI_RESET);
    if (count == 0) {
        printf("  (No events recorded yet. Ready for scans...)\n");
    } else {
        size_t start = (count > 5) ? count - 5 : 0;
        for (size_t i = start; i < count; ++i) {
            printf("  * %s\n", history[i]);
        }
    }
    printf("----------------------------------------------------------------------------------------\n");
}

void ui_render_menu(void) {
    printf("%s[OPERATIONS MENU]%s\n", ANSI_BOLD, ANSI_RESET);
    printf(" 1) Scan Intake (Trigger Barcode/RFID scan)\n");
    printf(" 2) Dispatch Order (Pick item from bay)\n");
    printf(" 3) Toggle Electromagnetic Bay Lock (ioctl)\n");
    printf(" 4) View Bay Detailed Diagnostics\n");
    printf(" 5) Reset Driver Ring Buffer & Stats (ioctl)\n");
    printf(" 6) Read POSIX Shared Memory Telemetry\n");
    printf(" 7) Refresh Dashboard\n");
    printf(" 0) Exit / Graceful Shutdown\n");
    printf("Select option: ");
    fflush(stdout);
}
