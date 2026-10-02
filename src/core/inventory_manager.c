#include "core/inventory_manager.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static ItemCategory deduce_category(const char* barcode) {
    if (strstr(barcode, "ELEC") || strstr(barcode, "TECH")) return CAT_ELECTRONICS;
    if (strstr(barcode, "COLD") || strstr(barcode, "FOOD")) return CAT_PERISHABLE;
    if (strstr(barcode, "HAZ") || strstr(barcode, "CHEM")) return CAT_HAZARDOUS;
    return CAT_GENERAL;
}

static const char* deduce_name(const char* barcode) {
    if (strstr(barcode, "ELEC")) return "Microcontroller Board";
    if (strstr(barcode, "COLD")) return "Vaccine Vial Pack";
    if (strstr(barcode, "HAZ")) return "Chemical Solvent Drum";
    return "Palletized Container";
}

void inventory_init(InventoryManager* inv) {
    if (!inv) return;
    pthread_mutex_init(&inv->lock, NULL);

    // Initialize 3 zones
    zone_init(&inv->zones[0], "ZONE-A", "Electronics & High-Tech", CAT_ELECTRONICS);
    zone_init(&inv->zones[1], "ZONE-B", "Cold Storage & Vaccines", CAT_PERISHABLE);
    zone_init(&inv->zones[2], "ZONE-C", "General Ambient & Heavy", CAT_GENERAL);

    // Initialize 24 bays (Bays 1-8 Zone A, 9-16 Zone B, 17-24 Zone C)
    for (uint32_t i = 1; i <= 8; ++i) {
        bay_init(&inv->bays[i - 1], i, "ZONE-A", 100.0f);
        zone_add_bay_id(&inv->zones[0], i);
    }
    for (uint32_t i = 9; i <= 16; ++i) {
        bay_init(&inv->bays[i - 1], i, "ZONE-B", 250.0f);
        zone_add_bay_id(&inv->zones[1], i);
    }
    for (uint32_t i = 17; i <= 24; ++i) {
        bay_init(&inv->bays[i - 1], i, "ZONE-C", 500.0f);
        zone_add_bay_id(&inv->zones[2], i);
    }
}

void inventory_destroy(InventoryManager* inv) {
    if (!inv) return;
    pthread_mutex_destroy(&inv->lock);
}

int inventory_intake_from_scan(InventoryManager* inv, const wms_scan_event_t* scan) {
    if (!inv || !scan) return -1;

    pthread_mutex_lock(&inv->lock);

    Item item;
    memset(&item, 0, sizeof(Item));
    strncpy(item.sku, scan->barcode, sizeof(item.sku) - 1);
    strncpy(item.name, deduce_name(scan->barcode), sizeof(item.name) - 1);
    item.category = deduce_category(scan->barcode);
    item.weight_kg = (scan->weight_kg > 0.0f) ? scan->weight_kg : 10.0f;
    item.quantity = 1;
    item.registered_timestamp = (uint64_t)time(NULL);

    StorageBay* target_bay = NULL;

    // Phase 1: Search within matching category zone
    for (int z = 0; z < TOTAL_ZONES; ++z) {
        if (inv->zones[z].category == item.category) {
            for (size_t b = 0; b < inv->zones[z].bay_count; ++b) {
                uint32_t bay_id = inv->zones[z].bay_ids[b];
                StorageBay* bay = &inv->bays[bay_id - 1];
                if (bay_can_accommodate(bay, &item)) {
                    target_bay = bay;
                    break;
                }
            }
        }
        if (target_bay) break;
    }

    // Phase 2: Fallback to any available bay with sufficient capacity
    if (!target_bay) {
        for (int i = 0; i < TOTAL_BAYS; ++i) {
            if (bay_can_accommodate(&inv->bays[i], &item)) {
                target_bay = &inv->bays[i];
                break;
            }
        }
    }

    int result = -1;
    if (target_bay) {
        if (bay_deposit(target_bay, &item)) {
            result = (int)target_bay->id;
        }
    }

    pthread_mutex_unlock(&inv->lock);
    return result;
}

bool inventory_dispatch(InventoryManager* inv, const char* sku, uint32_t* out_cleared_bay) {
    if (!inv || !sku) return false;

    pthread_mutex_lock(&inv->lock);
    bool fulfilled = false;

    for (int i = 0; i < TOTAL_BAYS; ++i) {
        StorageBay* bay = &inv->bays[i];
        if (bay->is_occupied) {
            if (strcmp(bay->stored_item.sku, sku) == 0) {
                // Safety Interlock check: Cannot pick from physically locked bays!
                if (!bay->is_locked) {
                    if (out_cleared_bay) {
                        *out_cleared_bay = bay->id;
                    }
                    bay_clear(bay);
                    fulfilled = true;
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&inv->lock);
    return fulfilled;
}

bool inventory_set_bay_lock(InventoryManager* inv, uint32_t bay_id, bool lock) {
    if (!inv || bay_id < 1 || bay_id > TOTAL_BAYS) return false;

    pthread_mutex_lock(&inv->lock);
    bay_set_lock(&inv->bays[bay_id - 1], lock);
    pthread_mutex_unlock(&inv->lock);
    return true;
}

float inventory_get_occupancy_rate(InventoryManager* inv) {
    if (!inv) return 0.0f;
    pthread_mutex_lock(&inv->lock);

    size_t occ = 0;
    for (int i = 0; i < TOTAL_BAYS; ++i) {
        if (inv->bays[i].is_occupied) occ++;
    }

    float rate = ((float)occ / (float)TOTAL_BAYS) * 100.0f;
    pthread_mutex_unlock(&inv->lock);
    return rate;
}

size_t inventory_get_total_items(InventoryManager* inv) {
    if (!inv) return 0;
    pthread_mutex_lock(&inv->lock);

    size_t count = 0;
    for (int i = 0; i < TOTAL_BAYS; ++i) {
        if (inv->bays[i].is_occupied) count++;
    }

    pthread_mutex_unlock(&inv->lock);
    return count;
}

StorageBay* inventory_get_bay(InventoryManager* inv, uint32_t bay_id) {
    if (!inv || bay_id < 1 || bay_id > TOTAL_BAYS) return NULL;
    return &inv->bays[bay_id - 1];
}
