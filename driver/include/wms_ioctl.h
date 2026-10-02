#ifndef WMS_IOCTL_H
#define WMS_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#endif

#define WMS_DEVICE_NAME "wms_driver"
#define WMS_DEVICE_PATH "/dev/" WMS_DEVICE_NAME
#define WMS_MAX_BARCODE_LEN 32
#define WMS_MAX_BAYS 128
#define WMS_BUFFER_CAPACITY 64

#pragma pack(push, 1)

/**
 * Scan event representing incoming barcode/RFID scan and weight reading
 */
typedef struct {
    uint32_t event_id;                     /* Monotonically increasing scan event ID */
    uint64_t timestamp_ns;                 /* Timestamp in nanoseconds */
    char     barcode[WMS_MAX_BARCODE_LEN]; /* Scanned SKU or Pallet barcode */
    float    weight_kg;                    /* Measured weight from load cell */
    uint8_t  source_gate_id;               /* 1 = Intake Gate, 2 = Dispatch Gate */
    uint8_t  checksum;                     /* XOR checksum for data integrity */
} wms_scan_event_t;

/**
 * Overall driver telemetry and status structure
 */
typedef struct {
    uint32_t total_events_logged;          /* Total events processed since driver load */
    uint32_t buffer_occupancy;             /* Current items in the ring buffer */
    uint32_t buffer_capacity;              /* Total capacity of ring buffer */
    uint32_t dropped_events;               /* Count of dropped events due to overflow */
    uint32_t active_bay_locks;             /* Count of currently engaged electromagnetic locks */
    uint8_t  device_ready;                 /* 1 if healthy, 0 if fault state */
} wms_device_status_t;

/**
 * Storage bay lock actuation request
 */
typedef struct {
    uint32_t bay_id;                       /* Target bay identifier (0 to WMS_MAX_BAYS - 1) */
    uint8_t  lock_state;                   /* 1 = Lock engaged, 0 = Lock released */
} wms_bay_lock_req_t;

#pragma pack(pop)

/* IOCTL Definitions using 'W' magic number */
#define WMS_IOC_MAGIC 'W'

/* Read current driver status and statistics */
#define WMS_IOCTL_GET_STATUS      _IOR(WMS_IOC_MAGIC, 1, wms_device_status_t)

/* Actuate electromagnetic lock on a specific warehouse bay */
#define WMS_IOCTL_SET_BAY_LOCK    _IOW(WMS_IOC_MAGIC, 2, wms_bay_lock_req_t)

/* Query current lock state of a specific bay */
#define WMS_IOCTL_GET_BAY_LOCK    _IOWR(WMS_IOC_MAGIC, 3, wms_bay_lock_req_t)

/* Inject a simulated scan event directly into the kernel buffer */
#define WMS_IOCTL_SIMULATE_SCAN   _IOW(WMS_IOC_MAGIC, 4, wms_scan_event_t)

/* Reset the driver ring buffer and clear event counters */
#define WMS_IOCTL_RESET_BUFFER    _IO(WMS_IOC_MAGIC, 5)

#endif /* WMS_IOCTL_H */
