#ifndef WMS_LINUX_CHARDEV_H
#define WMS_LINUX_CHARDEV_H

#include "device_driver.h"

typedef struct {
    char device_path[64];
    int fd;
    bool is_connected;
} LinuxCharDevContext;

DeviceDriver make_linux_chardev(LinuxCharDevContext* ctx, const char* path);

#endif /* WMS_LINUX_CHARDEV_H */
