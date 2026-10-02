#ifndef WMS_INVENTORY_MANAGER_H
#define WMS_INVENTORY_MANAGER_H

#include "storage_bay.h"
#include "warehouse_zone.h"
#include "driver/include/wms_ioctl.h"
#include <pthread.h>
#include <stddef.h>

#define TOTAL_BAYS 24
#define TOTAL_ZONES 3

typedef struct {
    StorageBay bays[TOTAL_BAYS];
    WarehouseZone zones[TOTAL_ZONES];
    pthread_mutex_t lock;
} InventoryManager;

void inventory_init(InventoryManager* inv);
void inventory_destroy(InventoryManager* inv);

/**
 * Ingest item from scan event.
 * Returns allocated bay ID (1..24) on success, or -1 on failure/full.
 */
int inventory_intake_from_scan(InventoryManager* inv, const wms_scan_event_t* scan);

/**
 * Dispatch item by SKU. Fails if bay is locked (safety interlock).
 * Returns true if fulfilled and records cleared bay ID, false otherwise.
 */
bool inventory_dispatch(InventoryManager* inv, const char* sku, uint32_t* out_cleared_bay);

bool inventory_set_bay_lock(InventoryManager* inv, uint32_t bay_id, bool lock);
float inventory_get_occupancy_rate(InventoryManager* inv);
size_t inventory_get_total_items(InventoryManager* inv);
StorageBay* inventory_get_bay(InventoryManager* inv, uint32_t bay_id);

#endif /* WMS_INVENTORY_MANAGER_H */
