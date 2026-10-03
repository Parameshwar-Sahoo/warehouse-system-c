#ifndef WMS_ORDER_PROCESSOR_H
#define WMS_ORDER_PROCESSOR_H

#include "inventory_manager.h"
#include <stdbool.h>

#define MAX_ORDERS 64

typedef enum {
    ORDER_PENDING,
    ORDER_PROCESSING,
    ORDER_FULFILLED,
    ORDER_FAILED
} OrderStatus;

typedef struct {
    char order_id[32];
    char destination[64];
    char sku[32];
    int quantity;
    OrderStatus status;
    uint32_t allocated_bay;
    uint64_t created_at;
} Order;

typedef struct {
    InventoryManager* inv;
    Order orders[MAX_ORDERS];
    size_t order_count;
    size_t fulfilled_count;
    pthread_mutex_t lock;
} OrderProcessor;

void order_processor_init(OrderProcessor* op, InventoryManager* inv);
void order_processor_destroy(OrderProcessor* op);
bool order_processor_submit(OrderProcessor* op, const char* order_id, const char* dest, const char* sku, int qty);
bool order_processor_fulfill(OrderProcessor* op, const char* order_id);
bool order_processor_fulfill_bay(OrderProcessor* op, const char* order_id, uint32_t bay_id);
size_t order_processor_get_fulfilled_count(OrderProcessor* op);

#endif /* WMS_ORDER_PROCESSOR_H */
