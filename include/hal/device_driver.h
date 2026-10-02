#ifndef WMS_DEVICE_DRIVER_H
#define WMS_DEVICE_DRIVER_H

#include "driver/include/wms_ioctl.h"
#include <stdbool.h>

typedef struct DeviceDriverOps {
    bool (*connect)(void* ctx);
    void (*disconnect)(void* ctx);
    bool (*is_connected)(void* ctx);
    bool (*read_event)(void* ctx, wms_scan_event_t* out_event, int timeout_ms);
    bool (*set_bay_lock)(void* ctx, uint32_t bay_id, bool lock);
    bool (*get_bay_lock)(void* ctx, uint32_t bay_id, bool* out_locked);
    bool (*get_status)(void* ctx, wms_device_status_t* out_status);
    bool (*simulate_scan)(void* ctx, const wms_scan_event_t* event);
    bool (*reset_buffer)(void* ctx);
    const char* (*get_driver_name)(void* ctx);
} DeviceDriverOps;

typedef struct {
    const DeviceDriverOps* ops;
    void* ctx;
} DeviceDriver;

static inline bool driver_connect(DeviceDriver* d) { return d->ops->connect(d->ctx); }
static inline void driver_disconnect(DeviceDriver* d) { d->ops->disconnect(d->ctx); }
static inline bool driver_is_connected(DeviceDriver* d) { return d->ops->is_connected(d->ctx); }
static inline bool driver_read_event(DeviceDriver* d, wms_scan_event_t* out, int timeout) { return d->ops->read_event(d->ctx, out, timeout); }
static inline bool driver_set_bay_lock(DeviceDriver* d, uint32_t id, bool lk) { return d->ops->set_bay_lock(d->ctx, id, lk); }
static inline bool driver_get_bay_lock(DeviceDriver* d, uint32_t id, bool* lk) { return d->ops->get_bay_lock(d->ctx, id, lk); }
static inline bool driver_get_status(DeviceDriver* d, wms_device_status_t* st) { return d->ops->get_status(d->ctx, st); }
static inline bool driver_simulate_scan(DeviceDriver* d, const wms_scan_event_t* ev) { return d->ops->simulate_scan(d->ctx, ev); }
static inline bool driver_reset_buffer(DeviceDriver* d) { return d->ops->reset_buffer(d->ctx); }
static inline const char* driver_get_name(DeviceDriver* d) { return d->ops->get_driver_name(d->ctx); }

#endif /* WMS_DEVICE_DRIVER_H */
