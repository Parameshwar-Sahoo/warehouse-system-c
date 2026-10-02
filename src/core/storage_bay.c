#include "core/storage_bay.h"
#include <string.h>

void bay_init(StorageBay* bay, uint32_t id, const char* zone_id, float max_weight) {
    if (!bay) return;
    memset(bay, 0, sizeof(StorageBay));
    bay->id = id;
    if (zone_id) {
        strncpy(bay->zone_id, zone_id, sizeof(bay->zone_id) - 1);
    }
    bay->max_weight_kg = max_weight;
    bay->current_weight_kg = 0.0f;
    bay->is_locked = false;
    bay->is_occupied = false;
}

bool bay_can_accommodate(const StorageBay* bay, const Item* item) {
    if (!bay || !item) return false;
    if (bay->is_locked || bay->is_occupied) return false;
    return (item->weight_kg <= bay->max_weight_kg);
}

bool bay_deposit(StorageBay* bay, const Item* item) {
    if (!bay_can_accommodate(bay, item)) return false;
    bay->stored_item = *item;
    bay->current_weight_kg = item->weight_kg;
    bay->is_occupied = true;
    return true;
}

bool bay_retrieve(StorageBay* bay, Item* out_item) {
    if (!bay || !bay->is_occupied || bay->is_locked) return false;
    if (out_item) {
        *out_item = bay->stored_item;
    }
    bay_clear(bay);
    return true;
}

void bay_clear(StorageBay* bay) {
    if (!bay) return;
    memset(&bay->stored_item, 0, sizeof(Item));
    bay->current_weight_kg = 0.0f;
    bay->is_occupied = false;
}

void bay_set_lock(StorageBay* bay, bool locked) {
    if (!bay) return;
    bay->is_locked = locked;
}

BayState bay_get_state(const StorageBay* bay) {
    if (!bay) return BAY_EMPTY;
    if (bay->is_locked) return BAY_LOCKED;
    if (bay->is_occupied) return BAY_OCCUPIED;
    return BAY_EMPTY;
}
