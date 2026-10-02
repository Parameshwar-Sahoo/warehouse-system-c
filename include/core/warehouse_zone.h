#ifndef WMS_WAREHOUSE_ZONE_H
#define WMS_WAREHOUSE_ZONE_H

#include "item.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_ZONE_BAYS 16

typedef struct {
    char zone_id[16];
    char name[64];
    ItemCategory category;
    uint32_t bay_ids[MAX_ZONE_BAYS];
    size_t bay_count;
} WarehouseZone;

void zone_init(WarehouseZone* zone, const char* zone_id, const char* name, ItemCategory cat);
void zone_add_bay_id(WarehouseZone* zone, uint32_t bay_id);

#endif /* WMS_WAREHOUSE_ZONE_H */
