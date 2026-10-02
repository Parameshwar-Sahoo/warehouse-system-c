#include "hal/simulated_dev.h"
#include <string.h>
#include <time.h>

static bool sim_connect(void* ctx) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c) return false;
    c->is_connected = true;
    return true;
}

static void sim_disconnect(void* ctx) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c) return;
    pthread_mutex_lock(&c->lock);
    c->is_connected = false;
    pthread_cond_broadcast(&c->cond);
    pthread_mutex_unlock(&c->lock);
}

static bool sim_is_connected(void* ctx) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    return c && c->is_connected;
}

static bool sim_read_event(void* ctx, wms_scan_event_t* out_event, int timeout_ms) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c || !c->is_connected) return false;

    pthread_mutex_lock(&c->lock);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    while (c->count == 0 && c->is_connected) {
        int res = pthread_cond_timedwait(&c->cond, &c->lock, &ts);
        if (res != 0) {
            pthread_mutex_unlock(&c->lock);
            return false;
        }
    }

    if (!c->is_connected || c->count == 0) {
        pthread_mutex_unlock(&c->lock);
        return false;
    }

    *out_event = c->queue[c->tail];
    c->tail = (c->tail + 1) % WMS_BUFFER_CAPACITY;
    c->count--;

    pthread_mutex_unlock(&c->lock);
    return true;
}

static bool sim_set_bay_lock(void* ctx, uint32_t bay_id, bool lock) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c || bay_id >= WMS_MAX_BAYS) return false;

    pthread_mutex_lock(&c->lock);
    c->bay_locks[bay_id] = lock;
    pthread_mutex_unlock(&c->lock);
    return true;
}

static bool sim_get_bay_lock(void* ctx, uint32_t bay_id, bool* out_locked) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c || bay_id >= WMS_MAX_BAYS || !out_locked) return false;

    pthread_mutex_lock(&c->lock);
    *out_locked = c->bay_locks[bay_id];
    pthread_mutex_unlock(&c->lock);
    return true;
}

static bool sim_get_status(void* ctx, wms_device_status_t* out_status) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c || !out_status) return false;

    pthread_mutex_lock(&c->lock);
    out_status->total_events_logged = c->total_events;
    out_status->buffer_occupancy = (uint32_t)c->count;
    out_status->buffer_capacity = WMS_BUFFER_CAPACITY;
    out_status->dropped_events = c->dropped_events;

    uint32_t locks = 0;
    for (int i = 0; i < WMS_MAX_BAYS; ++i) {
        if (c->bay_locks[i]) locks++;
    }
    out_status->active_bay_locks = locks;
    out_status->device_ready = c->is_connected ? 1 : 0;

    pthread_mutex_unlock(&c->lock);
    return true;
}

static bool sim_simulate_scan(void* ctx, const wms_scan_event_t* event) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c || !event) return false;

    pthread_mutex_lock(&c->lock);

    if (c->count >= WMS_BUFFER_CAPACITY) {
        c->dropped_events++;
        pthread_mutex_unlock(&c->lock);
        return false;
    }

    c->queue[c->head] = *event;
    c->head = (c->head + 1) % WMS_BUFFER_CAPACITY;
    c->count++;
    c->total_events++;

    pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->lock);
    return true;
}

static bool sim_reset_buffer(void* ctx) {
    SimulatedDevContext* c = (SimulatedDevContext*)ctx;
    if (!c) return false;

    pthread_mutex_lock(&c->lock);
    c->head = 0;
    c->tail = 0;
    c->count = 0;
    c->dropped_events = 0;
    pthread_mutex_unlock(&c->lock);
    return true;
}

static const char* sim_get_driver_name(void* ctx) {
    (void)ctx;
    return "SimulatedDevice (HAL In-Memory Driver in C)";
}

static const DeviceDriverOps g_sim_dev_ops = {
    .connect = sim_connect,
    .disconnect = sim_disconnect,
    .is_connected = sim_is_connected,
    .read_event = sim_read_event,
    .set_bay_lock = sim_set_bay_lock,
    .get_bay_lock = sim_get_bay_lock,
    .get_status = sim_get_status,
    .simulate_scan = sim_simulate_scan,
    .reset_buffer = sim_reset_buffer,
    .get_driver_name = sim_get_driver_name
};

DeviceDriver make_simulated_dev(SimulatedDevContext* ctx) {
    memset(ctx, 0, sizeof(SimulatedDevContext));
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);
    ctx->is_connected = false;

    DeviceDriver d = { .ops = &g_sim_dev_ops, .ctx = ctx };
    return d;
}

void simulated_dev_destroy(SimulatedDevContext* ctx) {
    if (!ctx) return;
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);
}
