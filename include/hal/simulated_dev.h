#ifndef WMS_SIMULATED_DEV_H
#define WMS_SIMULATED_DEV_H

#include "device_driver.h"
#include <pthread.h>

typedef struct {
    wms_scan_event_t queue[WMS_BUFFER_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;

    bool bay_locks[WMS_MAX_BAYS];
    uint32_t total_events;
    uint32_t dropped_events;

    bool is_connected;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} SimulatedDevContext;

DeviceDriver make_simulated_dev(SimulatedDevContext* ctx);
void simulated_dev_destroy(SimulatedDevContext* ctx);

#endif /* WMS_SIMULATED_DEV_H */
