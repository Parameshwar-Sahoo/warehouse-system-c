#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include "core/inventory_manager.h"
#include "core/order_processor.h"
#include "hal/device_driver.h"
#include "hal/linux_chardev.h"
#include "hal/simulated_dev.h"
#include "ipc/shared_memory.h"
#include "ipc/signal_handler.h"
#include "ui/terminal_ui.h"

typedef struct {
    DeviceDriver* driver;
    InventoryManager* inv;
    OrderProcessor* op;
    SharedMemoryManager* shm;
    char event_history[20][128];
    size_t event_count;
    pthread_mutex_t history_lock;
    volatile bool worker_running;
} AppContext;

static void push_event(AppContext* app, const char* msg) {
    pthread_mutex_lock(&app->history_lock);
    if (app->event_count < 20) {
        strncpy(app->event_history[app->event_count++], msg, 127);
    } else {
        for (int i = 0; i < 19; ++i) {
            strcpy(app->event_history[i], app->event_history[i + 1]);
        }
        strncpy(app->event_history[19], msg, 127);
    }
    pthread_mutex_unlock(&app->history_lock);
}

static void* worker_thread_fn(void* arg) {
    AppContext* app = (AppContext*)arg;
    wms_scan_event_t event;

    while (app->worker_running && !signals_is_shutdown_requested()) {
        if (driver_read_event(app->driver, &event, 200)) {
            if (event.source_gate_id == 1) { // Intake gate
                int allocated_bay = inventory_intake_from_scan(app->inv, &event);
                if (allocated_bay > 0) {
                    // Momentary electromagnetic lock during placement
                    driver_set_bay_lock(app->driver, (uint32_t)allocated_bay, true);
                    usleep(50000); // 50ms
                    driver_set_bay_lock(app->driver, (uint32_t)allocated_bay, false);

                    char buf[128];
                    snprintf(buf, sizeof(buf), "INTAKE: %s (%.1f kg) -> Bay #%d",
                             event.barcode, event.weight_kg, allocated_bay);
                    push_event(app, buf);
                } else {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "INTAKE REJECTED: Capacity full for %s", event.barcode);
                    push_event(app, buf);
                }
            } else if (event.source_gate_id == 2) { // Dispatch gate
                uint32_t cleared_bay = 0;
                if (inventory_dispatch(app->inv, event.barcode, &cleared_bay)) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "DISPATCH: %s retrieved from Bay #%u", event.barcode, cleared_bay);
                    push_event(app, buf);
                }
            }

            // Broadcast telemetry to POSIX Shared Memory
            wms_device_status_t dev_status;
            driver_get_status(app->driver, &dev_status);
            shm_update(app->shm, dev_status.total_events_logged, dev_status.active_bay_locks,
                       (uint32_t)inventory_get_total_items(app->inv),
                       inventory_get_occupancy_rate(app->inv), event.barcode);
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    bool demo_mode = false;
    bool daemon_mode = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--demo") == 0) {
            demo_mode = true;
        } else if (strcmp(argv[i], "--daemon") == 0 || strcmp(argv[i], "-d") == 0) {
            daemon_mode = true;
        }
    }

    signals_init();
    printf("[WMS C Initialization] Starting Warehouse System in C...\n");

    // 1. Initialize HAL & Driver
    LinuxCharDevContext lcd_ctx;
    SimulatedDevContext sim_ctx;
    DeviceDriver driver = make_linux_chardev(&lcd_ctx, WMS_DEVICE_PATH);

    if (driver_connect(&driver)) {
        printf("[HAL] Successfully connected to Linux Character Device: %s\n", WMS_DEVICE_PATH);
    } else {
        printf("[HAL Warning] Could not open %s (Driver not loaded or permissions required).\n", WMS_DEVICE_PATH);
        printf("[HAL] Seamlessly activating In-Memory HAL Driver in C.\n");
        driver = make_simulated_dev(&sim_ctx);
        driver_connect(&driver);
    }

    // 2. Initialize Core Domain
    InventoryManager inventory;
    inventory_init(&inventory);

    OrderProcessor order_processor;
    order_processor_init(&order_processor, &inventory);

    // 3. Initialize POSIX Shared Memory
    SharedMemoryManager shm;
    if (shm_init(&shm, WMS_SHM_NAME, true)) {
        printf("[IPC] POSIX Shared Memory active at %s\n", WMS_SHM_NAME);
    }

    // 4. Application Context & Background Worker Thread
    AppContext app;
    memset(&app, 0, sizeof(AppContext));
    app.driver = &driver;
    app.inv = &inventory;
    app.op = &order_processor;
    app.shm = &shm;
    app.worker_running = true;
    pthread_mutex_init(&app.history_lock, NULL);

    pthread_t worker;
    pthread_create(&worker, NULL, worker_thread_fn, &app);

    // 5. Execution Modes
    if (demo_mode) {
        printf("\n>>> RUNNING AUTOMATED STAGE DEMONSTRATION (PURE C) <<<\n\n");

        printf("[Step 1] Ingesting test cargo into warehouse...\n");
        const char* skus[] = {
            "SKU-ELEC-101", "SKU-ELEC-102", "SKU-COLD-201", "SKU-COLD-202",
            "SKU-GEN-301", "SKU-GEN-302", "SKU-HAZ-401"
        };
        float weights[] = { 15.0f, 18.5f, 45.0f, 60.0f, 120.0f, 95.0f, 25.0f };

        for (int i = 0; i < 7; ++i) {
            wms_scan_event_t ev;
            memset(&ev, 0, sizeof(wms_scan_event_t));
            ev.event_id = i + 1;
            strncpy(ev.barcode, skus[i], sizeof(ev.barcode) - 1);
            ev.weight_kg = weights[i];
            ev.source_gate_id = 1;
            driver_simulate_scan(&driver, &ev);
            usleep(100000);
        }

        usleep(500000); // 500ms

        ui_render_header();
        ui_render_telemetry_bar(&inventory, &order_processor, &driver);
        ui_render_bay_grid(&inventory);
        pthread_mutex_lock(&app.history_lock);
        ui_render_event_log(app.event_history, app.event_count);
        pthread_mutex_unlock(&app.history_lock);

        // Step 2: IOCTL Locking
        printf("\n[Step 2] Testing IOCTL Electromagnetic Bay Locks...\n");
        driver_set_bay_lock(&driver, 1, true);
        driver_set_bay_lock(&driver, 9, true);
        inventory_set_bay_lock(&inventory, 1, true);
        inventory_set_bay_lock(&inventory, 9, true);

        wms_device_status_t status_after;
        driver_get_status(&driver, &status_after);
        printf("-> Active Bay Locks reported by driver: %u\n", status_after.active_bay_locks);

        // Step 3: Order Fulfillment & Safety Interlock
        printf("\n[Step 3] Submitting Customer Order...\n");
        order_processor_submit(&order_processor, "ORD-9901", "Hub Chicago", "SKU-ELEC-101", 1);
        printf("-> Order ORD-9901 submitted for SKU-ELEC-101.\n");
        printf("-> Attempting order fulfillment while Bay #1 is locked...\n");

        bool fulfilled = order_processor_fulfill(&order_processor, "ORD-9901");
        printf("-> Fulfillment blocked as expected (%s): Goods cannot be picked from locked bays!\n",
               fulfilled ? "UNEXPECTED" : "SAFETY INTERLOCK ACTIVE");

        printf("-> Disengaging Bay Locks via IOCTL...\n");
        driver_set_bay_lock(&driver, 1, false);
        driver_set_bay_lock(&driver, 9, false);
        inventory_set_bay_lock(&inventory, 1, false);
        inventory_set_bay_lock(&inventory, 9, false);

        printf("-> Retrying order fulfillment with unlocked bays...\n");
        fulfilled = order_processor_fulfill(&order_processor, "ORD-9901");
        printf("-> Fulfillment result: %s\n", fulfilled ? "SUCCESSFUL" : "FAILED");

        // Step 4: POSIX Shared Memory
        printf("\n[Step 4] Querying POSIX Shared Memory Telemetry...\n");
        WmsSharedTelemetry telem;
        if (shm_read(&shm, &telem)) {
            printf("-> SHM PID: %u\n", telem.process_id);
            printf("-> Total Scans: %u\n", telem.total_scans_received);
            printf("-> Active Locks: %u\n", telem.active_bay_locks);
            printf("-> Warehouse Occupancy: %.1f%%\n", telem.occupancy_rate);
            printf("-> Last Barcode: %s\n", telem.last_scanned_barcode);
        }

        printf("\n>>> DEMO SEQUENCE COMPLETED SUCCESSFULLY (PURE C) <<<\n");
    } else if (daemon_mode) {
        printf("[Daemon] Running in headless background mode. Press Ctrl+C to terminate.\n");
        while (!signals_is_shutdown_requested()) {
            usleep(500000);
        }
    } else {
        // Interactive UI Mode
        int choice = -1;
        while (!signals_is_shutdown_requested() && choice != 0) {
            ui_clear_screen();
            ui_render_header();
            ui_render_telemetry_bar(&inventory, &order_processor, &driver);
            ui_render_bay_grid(&inventory);
            pthread_mutex_lock(&app.history_lock);
            ui_render_event_log(app.event_history, app.event_count);
            pthread_mutex_unlock(&app.history_lock);
            ui_render_menu();

            if (scanf("%d", &choice) != 1) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
                continue;
            }

            switch (choice) {
            case 1: { // Scan intake
                char barcode[32];
                float weight;
                printf("Enter Item Barcode (e.g. SKU-ELEC-101, SKU-COLD-201, SKU-GEN-301): ");
                scanf("%31s", barcode);
                printf("Enter Item Weight (kg): ");
                scanf("%f", &weight);

                wms_scan_event_t ev;
                memset(&ev, 0, sizeof(wms_scan_event_t));
                ev.event_id = 100;
                strncpy(ev.barcode, barcode, sizeof(ev.barcode) - 1);
                ev.weight_kg = weight;
                ev.source_gate_id = 1;

                driver_simulate_scan(&driver, &ev);
                printf("[+] Scan injected into driver.\n");
                usleep(300000);
                break;
            }
            case 2: { // Dispatch
                char sku[32];
                printf("Enter SKU to dispatch: ");
                scanf("%31s", sku);
                order_processor_submit(&order_processor, "ORD-MANUAL", "Client Direct", sku, 1);
                if (order_processor_fulfill(&order_processor, "ORD-MANUAL")) {
                    printf("[+] Order fulfilled and item dispatched!\n");
                } else {
                    printf("[-] Order failed: Item not in stock or bay locked.\n");
                }
                usleep(1000000);
                break;
            }
            case 3: { // Toggle bay lock
                uint32_t bay_id;
                printf("Enter Bay ID (1 - 24): ");
                scanf("%u", &bay_id);
                StorageBay* b = inventory_get_bay(&inventory, bay_id);
                if (b) {
                    bool new_lock = !b->is_locked;
                    driver_set_bay_lock(&driver, bay_id, new_lock);
                    inventory_set_bay_lock(&inventory, bay_id, new_lock);
                    printf("[+] Bay #%u lock set to: %s\n", bay_id, new_lock ? "LOCKED" : "UNLOCKED");
                } else {
                    printf("[-] Invalid Bay ID!\n");
                }
                usleep(1000000);
                break;
            }
            case 4: { // Bay diagnostics
                uint32_t bay_id;
                printf("Enter Bay ID (1 - 24): ");
                scanf("%u", &bay_id);
                StorageBay* b = inventory_get_bay(&inventory, bay_id);
                if (b) {
                    printf("\n--- Bay #%u Diagnostics ---\n", b->id);
                    printf("Zone: %s\n", b->zone_id);
                    printf("Max Weight: %.1f kg | Current Weight: %.1f kg\n", b->max_weight_kg, b->current_weight_kg);
                    printf("Status: %s\n", b->is_locked ? "LOCKED" : (b->is_occupied ? "OCCUPIED" : "EMPTY"));
                    if (b->is_occupied) {
                        printf("Stored SKU: %s (%s)\n", b->stored_item.sku, b->stored_item.name);
                        printf("Category: %s\n", item_category_to_string(b->stored_item.category));
                    }
                    printf("\nPress Enter to return...");
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF);
                    getchar();
                }
                break;
            }
            case 5: { // Reset driver buffer
                driver_reset_buffer(&driver);
                printf("[+] Driver buffer reset successfully.\n");
                usleep(700000);
                break;
            }
            case 6: { // Read SHM
                WmsSharedTelemetry t;
                if (shm_read(&shm, &t)) {
                    printf("\n--- POSIX Shared Memory Telemetry ---\n");
                    printf("PID: %u | Scans: %u | Active Locks: %u\n", t.process_id, t.total_scans_received, t.active_bay_locks);
                    printf("Total Items: %u | Occupancy Rate: %.1f%%\n", t.total_items_stored, t.occupancy_rate);
                    printf("Last Barcode: %s\n", t.last_scanned_barcode);
                    printf("\nPress Enter to return...");
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF);
                    getchar();
                }
                break;
            }
            case 7:
                break;
            case 0:
                printf("Exiting...\n");
                break;
            default:
                break;
            }
        }
    }

    // 6. Graceful Cleanup
    printf("[WMS Shutdown] Stopping worker thread and cleaning up...\n");
    app.worker_running = false;
    pthread_join(worker, NULL);

    for (uint32_t i = 1; i <= TOTAL_BAYS; ++i) {
        driver_set_bay_lock(&driver, i, false);
    }
    shm_cleanup(&shm);
    driver_disconnect(&driver);
    simulated_dev_destroy(&sim_ctx);
    inventory_destroy(&inventory);
    order_processor_destroy(&order_processor);
    pthread_mutex_destroy(&app.history_lock);

    printf("[WMS Shutdown] Clean shutdown complete. Goodbye!\n");
    return 0;
}
