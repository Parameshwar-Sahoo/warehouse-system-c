#include "hal/linux_chardev.h"
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <string.h>

static bool lcd_connect(void* ctx) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c) return false;
    if (c->is_connected) return true;

    c->fd = open(c->device_path, O_RDWR | O_NONBLOCK);
    if (c->fd < 0) {
        c->fd = open(c->device_path, O_RDWR);
    }
    if (c->fd < 0) {
        return false;
    }
    c->is_connected = true;
    return true;
}

static void lcd_disconnect(void* ctx) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (c && c->is_connected && c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
        c->is_connected = false;
    }
}

static bool lcd_is_connected(void* ctx) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    return c && c->is_connected;
}

static bool lcd_read_event(void* ctx, wms_scan_event_t* out_event, int timeout_ms) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c || !c->is_connected || c->fd < 0) return false;

    struct pollfd pfd;
    pfd.fd = c->fd;
    pfd.events = POLLIN | POLLRDNORM;
    pfd.revents = 0;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) return false;

    if (pfd.revents & (POLLIN | POLLRDNORM)) {
        ssize_t bytes = read(c->fd, out_event, sizeof(wms_scan_event_t));
        return (bytes == sizeof(wms_scan_event_t));
    }
    return false;
}

static bool lcd_set_bay_lock(void* ctx, uint32_t bay_id, bool lock) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c || !c->is_connected || c->fd < 0) return false;

    wms_bay_lock_req_t req;
    req.bay_id = bay_id;
    req.lock_state = lock ? 1 : 0;
    return (ioctl(c->fd, WMS_IOCTL_SET_BAY_LOCK, &req) == 0);
}

static bool lcd_get_bay_lock(void* ctx, uint32_t bay_id, bool* out_locked) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c || !c->is_connected || c->fd < 0 || !out_locked) return false;

    wms_bay_lock_req_t req;
    req.bay_id = bay_id;
    req.lock_state = 0;
    if (ioctl(c->fd, WMS_IOCTL_GET_BAY_LOCK, &req) == 0) {
        *out_locked = (req.lock_state != 0);
        return true;
    }
    return false;
}

static bool lcd_get_status(void* ctx, wms_device_status_t* out_status) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c || !c->is_connected || c->fd < 0) return false;
    return (ioctl(c->fd, WMS_IOCTL_GET_STATUS, out_status) == 0);
}

static bool lcd_simulate_scan(void* ctx, const wms_scan_event_t* event) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c || !c->is_connected || c->fd < 0) return false;
    return (ioctl(c->fd, WMS_IOCTL_SIMULATE_SCAN, event) == 0);
}

static bool lcd_reset_buffer(void* ctx) {
    LinuxCharDevContext* c = (LinuxCharDevContext*)ctx;
    if (!c || !c->is_connected || c->fd < 0) return false;
    return (ioctl(c->fd, WMS_IOCTL_RESET_BUFFER) == 0);
}

static const char* lcd_get_driver_name(void* ctx) {
    (void)ctx;
    return "LinuxCharDevice (/dev/wms_driver)";
}

static const DeviceDriverOps g_linux_chardev_ops = {
    .connect = lcd_connect,
    .disconnect = lcd_disconnect,
    .is_connected = lcd_is_connected,
    .read_event = lcd_read_event,
    .set_bay_lock = lcd_set_bay_lock,
    .get_bay_lock = lcd_get_bay_lock,
    .get_status = lcd_get_status,
    .simulate_scan = lcd_simulate_scan,
    .reset_buffer = lcd_reset_buffer,
    .get_driver_name = lcd_get_driver_name
};

DeviceDriver make_linux_chardev(LinuxCharDevContext* ctx, const char* path) {
    memset(ctx, 0, sizeof(LinuxCharDevContext));
    ctx->fd = -1;
    strncpy(ctx->device_path, path ? path : WMS_DEVICE_PATH, sizeof(ctx->device_path) - 1);
    DeviceDriver d = { .ops = &g_linux_chardev_ops, .ctx = ctx };
    return d;
}
