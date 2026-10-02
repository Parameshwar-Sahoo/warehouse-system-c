#ifndef WMS_STORAGE_BAY_H
#define WMS_STORAGE_BAY_H

#include "item.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BAY_EMPTY,
    BAY_OCCUPIED,
    BAY_LOCKED,
    BAY_MAINTENANCE
} BayState;

typedef struct StorageBay {
    uint32_t id;
    char zone_id[16];
    float max_weight_kg;
    float current_weight_kg;
    bool is_locked;
    bool is_occupied;
    Item stored_item;
} StorageBay;

void bay_init(StorageBay* bay, uint32_t id, const char* zone_id, float max_weight);
bool bay_can_accommodate(const StorageBay* bay, const Item* item);
bool bay_deposit(StorageBay* bay, const Item* item);
bool bay_retrieve(StorageBay* bay, Item* out_item);
void bay_clear(StorageBay* bay);
void bay_set_lock(StorageBay* bay, bool locked);
BayState bay_get_state(const StorageBay* bay);

#endif /* WMS_STORAGE_BAY_H */
