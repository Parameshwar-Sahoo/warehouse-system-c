#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "core/storage_bay.h"
#include "core/inventory_manager.h"
#include "core/order_processor.h"
#include "hal/simulated_dev.h"
#include "ipc/shared_memory.h"

void test_storage_bay(void) {
    printf("[TEST] Running StorageBay Unit Tests (C)...\n");

    StorageBay bay;
    bay_init(&bay, 1, "ZONE-A", 50.0f);
    assert(bay_get_state(&bay) == BAY_EMPTY);
    assert(!bay.is_occupied);

    // Overweight rejection
    Item heavy = { .sku = "SKU-HEAVY", .weight_kg = 60.0f, .quantity = 1 };
    assert(!bay_can_accommodate(&bay, &heavy));

    // Valid deposit
    Item fit = { .sku = "SKU-FIT", .weight_kg = 20.0f, .quantity = 1 };
    assert(bay_can_accommodate(&bay, &fit));
    assert(bay_deposit(&bay, &fit));
    assert(bay.is_occupied);
    assert(bay_get_state(&bay) == BAY_OCCUPIED);

    // Retrieval
    Item out_item;
    assert(bay_retrieve(&bay, &out_item));
    assert(strcmp(out_item.sku, "SKU-FIT") == 0);
    assert(!bay.is_occupied);
    assert(bay_get_state(&bay) == BAY_EMPTY);

    printf("[PASS] StorageBay Unit Tests Passed!\n");
}

void test_inventory_manager(void) {
    printf("[TEST] Running InventoryManager Unit Tests (C)...\n");

    InventoryManager inv;
    inventory_init(&inv);

    // Ingest Electronics -> Should route to Zone A (Bays 1..8)
    wms_scan_event_t scan1 = { .event_id = 1, .weight_kg = 12.0f, .source_gate_id = 1 };
    strcpy(scan1.barcode, "SKU-ELEC-77");
    int bay_id = inventory_intake_from_scan(&inv, &scan1);
    assert(bay_id >= 1 && bay_id <= 8);

    // Ingest Cold Storage -> Should route to Zone B (Bays 9..16)
    wms_scan_event_t scan2 = { .event_id = 2, .weight_kg = 40.0f, .source_gate_id = 1 };
    strcpy(scan2.barcode, "SKU-COLD-88");
    int bay_id2 = inventory_intake_from_scan(&inv, &scan2);
    assert(bay_id2 >= 9 && bay_id2 <= 16);

    // Verify stock
    assert(inventory_get_total_items(&inv) == 2);
    assert(inventory_get_occupancy_rate(&inv) > 8.0f);

    inventory_destroy(&inv);
    printf("[PASS] InventoryManager Unit Tests Passed!\n");
}

void test_order_fulfillment_safety(void) {
    printf("[TEST] Running OrderProcessor & Safety Interlock Tests (C)...\n");

    InventoryManager inv;
    inventory_init(&inv);
    OrderProcessor op;
    order_processor_init(&op, &inv);

    // Ingest cargo into Bay 1
    wms_scan_event_t scan = { .event_id = 10, .weight_kg = 15.0f, .source_gate_id = 1 };
    strcpy(scan.barcode, "SKU-SAFE-01");
    int bay = inventory_intake_from_scan(&inv, &scan);
    assert(bay > 0);

    // Lock Bay
    inventory_set_bay_lock(&inv, (uint32_t)bay, true);

    // Submit Order
    assert(order_processor_submit(&op, "ORD-TEST-1", "Client X", "SKU-SAFE-01", 1));

    // Try to fulfill while locked -> MUST FAIL (Safety Interlock)
    bool res1 = order_processor_fulfill(&op, "ORD-TEST-1");
    assert(!res1);
    assert(order_processor_get_fulfilled_count(&op) == 0);

    // Unlock Bay and retry
    inventory_set_bay_lock(&inv, (uint32_t)bay, false);
    bool res2 = order_processor_fulfill(&op, "ORD-TEST-1");
    assert(res2);
    assert(order_processor_get_fulfilled_count(&op) == 1);

    order_processor_destroy(&op);
    inventory_destroy(&inv);
    printf("[PASS] OrderProcessor & Safety Interlock Tests Passed!\n");
}

void test_hal_driver(void) {
    printf("[TEST] Running HAL Driver & Buffer Overflow Tests (C)...\n");

    SimulatedDevContext sim_ctx;
    DeviceDriver drv = make_simulated_dev(&sim_ctx);
    assert(driver_connect(&drv));
    assert(driver_is_connected(&drv));

    // Read empty timeout
    wms_scan_event_t ev;
    assert(!driver_read_event(&drv, &ev, 50));

    // Simulate scan
    wms_scan_event_t scan = { .event_id = 101, .weight_kg = 5.5f };
    strcpy(scan.barcode, "SKU-SIM-1");
    assert(driver_simulate_scan(&drv, &scan));

    wms_device_status_t st;
    assert(driver_get_status(&drv, &st));
    assert(st.buffer_occupancy == 1);

    // Read back
    assert(driver_read_event(&drv, &ev, 100));
    assert(strcmp(ev.barcode, "SKU-SIM-1") == 0);

    // Buffer Overflow test
    driver_reset_buffer(&drv);
    for (int i = 0; i < WMS_BUFFER_CAPACITY + 10; ++i) {
        wms_scan_event_t drop_ev = { .event_id = (uint32_t)i };
        driver_simulate_scan(&drv, &drop_ev);
    }

    assert(driver_get_status(&drv, &st));
    assert(st.buffer_occupancy == WMS_BUFFER_CAPACITY);
    assert(st.dropped_events == 10);

    driver_disconnect(&drv);
    simulated_dev_destroy(&sim_ctx);
    printf("[PASS] HAL Driver & Buffer Overflow Tests Passed!\n");
}

void test_posix_shm(void) {
    printf("[TEST] Running POSIX Shared Memory Tests (C)...\n");

    SharedMemoryManager master;
    assert(shm_init(&master, "/test_c_wms_shm", true));
    shm_update(&master, 50, 4, 12, 50.0f, "SKU-SHM-C");

    SharedMemoryManager client;
    assert(shm_init(&client, "/test_c_wms_shm", false));

    WmsSharedTelemetry t;
    assert(shm_read(&client, &t));
    assert(t.total_scans_received == 50);
    assert(t.active_bay_locks == 4);
    assert(t.total_items_stored == 12);
    assert(strcmp(t.last_scanned_barcode, "SKU-SHM-C") == 0);

    shm_cleanup(&client);
    shm_cleanup(&master);
    printf("[PASS] POSIX Shared Memory Tests Passed!\n");
}

int main(void) {
    printf("====================================================\n");
    printf("     WAREHOUSE SYSTEM (PURE C) COMPREHENSIVE SUITE   \n");
    printf("====================================================\n");

    test_storage_bay();
    test_inventory_manager();
    test_order_fulfillment_safety();
    test_hal_driver();
    test_posix_shm();

    printf("\n>>> ALL 5 TEST SUITES PASSED (100%% SUCCESS) <<<\n");
    return 0;
}
