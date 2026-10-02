#ifndef WMS_ITEM_H
#define WMS_ITEM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CAT_GENERAL,
    CAT_ELECTRONICS,
    CAT_PERISHABLE,
    CAT_HAZARDOUS,
    CAT_FRAGILE
} ItemCategory;

typedef struct {
    char sku[32];
    char name[64];
    ItemCategory category;
    float weight_kg;
    int quantity;
    uint64_t registered_timestamp;
} Item;

static inline const char* item_category_to_string(ItemCategory cat) {
    switch (cat) {
    case CAT_GENERAL: return "General";
    case CAT_ELECTRONICS: return "Electronics";
    case CAT_PERISHABLE: return "Perishable";
    case CAT_HAZARDOUS: return "Hazardous";
    case CAT_FRAGILE: return "Fragile";
    default: return "Unknown";
    }
}

#endif /* WMS_ITEM_H */
