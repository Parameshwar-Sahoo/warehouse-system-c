#include "core/order_processor.h"
#include <string.h>
#include <time.h>

void order_processor_init(OrderProcessor* op, InventoryManager* inv) {
    if (!op) return;
    memset(op, 0, sizeof(OrderProcessor));
    op->inv = inv;
    pthread_mutex_init(&op->lock, NULL);
}

void order_processor_destroy(OrderProcessor* op) {
    if (!op) return;
    pthread_mutex_destroy(&op->lock);
}

bool order_processor_submit(OrderProcessor* op, const char* order_id, const char* dest, const char* sku, int qty) {
    if (!op || !order_id || !sku || op->order_count >= MAX_ORDERS) return false;

    pthread_mutex_lock(&op->lock);

    Order* o = &op->orders[op->order_count++];
    strncpy(o->order_id, order_id, sizeof(o->order_id) - 1);
    if (dest) strncpy(o->destination, dest, sizeof(o->destination) - 1);
    strncpy(o->sku, sku, sizeof(o->sku) - 1);
    o->quantity = qty;
    o->status = ORDER_PENDING;
    o->created_at = (uint64_t)time(NULL);

    pthread_mutex_unlock(&op->lock);
    return true;
}

bool order_processor_fulfill(OrderProcessor* op, const char* order_id) {
    if (!op || !order_id || !op->inv) return false;

    pthread_mutex_lock(&op->lock);
    bool result = false;

    for (size_t i = 0; i < op->order_count; ++i) {
        Order* o = &op->orders[i];
        if (strcmp(o->order_id, order_id) == 0) {
            if (o->status == ORDER_FULFILLED) {
                result = true;
                break;
            }

            uint32_t cleared_bay = 0;
            if (inventory_dispatch(op->inv, o->sku, &cleared_bay)) {
                o->status = ORDER_FULFILLED;
                o->allocated_bay = cleared_bay;
                op->fulfilled_count++;
                result = true;
            } else {
                o->status = ORDER_FAILED;
                result = false;
            }
            break;
        }
    }

    pthread_mutex_unlock(&op->lock);
    return result;
}

size_t order_processor_get_fulfilled_count(OrderProcessor* op) {
    if (!op) return 0;
    pthread_mutex_lock(&op->lock);
    size_t count = op->fulfilled_count;
    pthread_mutex_unlock(&op->lock);
    return count;
}
