#include "core/warehouse_zone.h"
#include <string.h>

void zone_init(WarehouseZone* zone, const char* zone_id, const char* name, ItemCategory cat) {
    if (!zone) return;
    memset(zone, 0, sizeof(WarehouseZone));
    if (zone_id) strncpy(zone->zone_id, zone_id, sizeof(zone->zone_id) - 1);
    if (name) strncpy(zone->name, name, sizeof(zone->name) - 1);
    zone->category = cat;
    zone->bay_count = 0;
}

void zone_add_bay_id(WarehouseZone* zone, uint32_t bay_id) {
    if (!zone || zone->bay_count >= MAX_ZONE_BAYS) return;
    zone->bay_ids[zone->bay_count++] = bay_id;
}
