#ifndef WMS_DRIVER_H
#define WMS_DRIVER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include "include/wms_ioctl.h"

#define DRIVER_AUTHOR "Individual Capstone Project - Linux System Programming & Device Drivers"
#define DRIVER_DESC   "Warehouse System (WMS) Event & Telemetry Character Driver in C"
#define DRIVER_VERSION "1.0"

/**
 * Internal device state representation
 */
struct wms_dev_state {
    dev_t dev_num;
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;
    struct mutex lock;
    wait_queue_head_t read_wait;
    
    /* Event Ring Buffer */
    wms_scan_event_t ring_buffer[WMS_BUFFER_CAPACITY];
    size_t head;       /* Next write position */
    size_t tail;       /* Next read position */
    size_t count;      /* Current number of queued events */
    
    /* Device Telemetry */
    uint32_t total_events;
    uint32_t dropped_events;
    
    /* Bay Lock Bitmask/Array */
    uint8_t bay_locks[WMS_MAX_BAYS];
    uint32_t active_locks_count;
    
    /* Closing flag to wake pending readers */
    int is_closing;
};

#endif /* WMS_DRIVER_H */
