#include "wms_driver.h"
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);

static struct wms_dev_state *wms_dev = NULL;

static int wms_open(struct inode *inode, struct file *filp)
{
    filp->private_data = wms_dev;
    pr_info("wms_driver: device opened successfully by PID %d\n", current->pid);
    return 0;
}

static int wms_release(struct inode *inode, struct file *filp)
{
    pr_info("wms_driver: device released by PID %d\n", current->pid);
    return 0;
}

static ssize_t wms_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct wms_dev_state *dev = filp->private_data;
    wms_scan_event_t event;
    int ret;

    if (!dev)
        return -ENODEV;

    if (count < sizeof(wms_scan_event_t))
        return -EINVAL;

    /* Non-blocking mode check */
    if (filp->f_flags & O_NONBLOCK) {
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;
        if (dev->count == 0) {
            mutex_unlock(&dev->lock);
            return -EAGAIN;
        }
    } else {
        /* Wait until an event is available or closing */
        ret = wait_event_interruptible(dev->read_wait, (dev->count > 0) || dev->is_closing);
        if (ret != 0)
            return -ERESTARTSYS;

        if (dev->is_closing)
            return -ESHUTDOWN;

        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;
    }

    if (dev->count == 0) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    /* Pop event from ring buffer */
    event = dev->ring_buffer[dev->tail];
    dev->tail = (dev->tail + 1) % WMS_BUFFER_CAPACITY;
    dev->count--;

    mutex_unlock(&dev->lock);

    /* Transfer event to user space */
    if (copy_to_user(buf, &event, sizeof(wms_scan_event_t)))
        return -EFAULT;

    return sizeof(wms_scan_event_t);
}

static int internal_enqueue_event(struct wms_dev_state *dev, const wms_scan_event_t *ev)
{
    if (dev->count >= WMS_BUFFER_CAPACITY) {
        dev->dropped_events++;
        pr_warn("wms_driver: ring buffer overflow, event dropped! Total dropped: %u\n",
                dev->dropped_events);
        return -ENOSPC;
    }

    dev->ring_buffer[dev->head] = *ev;
    dev->head = (dev->head + 1) % WMS_BUFFER_CAPACITY;
    dev->count++;
    dev->total_events++;

    /* Wake up waiting reader threads */
    wake_up_interruptible(&dev->read_wait);
    return 0;
}

static ssize_t wms_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct wms_dev_state *dev = filp->private_data;
    wms_scan_event_t event;
    int ret;

    if (!dev)
        return -ENODEV;

    if (count < sizeof(wms_scan_event_t))
        return -EINVAL;

    if (copy_from_user(&event, buf, sizeof(wms_scan_event_t)))
        return -EFAULT;

    if (event.timestamp_ns == 0)
        event.timestamp_ns = ktime_get_real_ns();

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    ret = internal_enqueue_event(dev, &event);
    mutex_unlock(&dev->lock);

    if (ret < 0)
        return ret;

    return sizeof(wms_scan_event_t);
}

static unsigned int wms_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct wms_dev_state *dev = filp->private_data;
    __poll_t mask = 0;

    if (!dev)
        return EPOLLERR;

    poll_wait(filp, &dev->read_wait, wait);

    mutex_lock(&dev->lock);
    if (dev->count > 0)
        mask |= (EPOLLIN | EPOLLRDNORM);
    if (dev->count < WMS_BUFFER_CAPACITY)
        mask |= (EPOLLOUT | EPOLLWRNORM);
    if (dev->is_closing)
        mask |= EPOLLHUP;
    mutex_unlock(&dev->lock);

    return mask;
}

static long wms_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct wms_dev_state *dev = filp->private_data;
    int ret = 0;

    if (!dev)
        return -ENODEV;

    if (_IOC_TYPE(cmd) != WMS_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case WMS_IOCTL_GET_STATUS: {
        wms_device_status_t status;
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        status.total_events_logged = dev->total_events;
        status.buffer_occupancy = (uint32_t)dev->count;
        status.buffer_capacity = WMS_BUFFER_CAPACITY;
        status.dropped_events = dev->dropped_events;
        status.active_bay_locks = dev->active_locks_count;
        status.device_ready = dev->is_closing ? 0 : 1;

        mutex_unlock(&dev->lock);

        if (copy_to_user((void __user *)arg, &status, sizeof(status)))
            return -EFAULT;
        break;
    }

    case WMS_IOCTL_SET_BAY_LOCK: {
        wms_bay_lock_req_t req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        if (req.bay_id >= WMS_MAX_BAYS)
            return -EINVAL;

        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        uint8_t prev = dev->bay_locks[req.bay_id];
        dev->bay_locks[req.bay_id] = (req.lock_state ? 1 : 0);

        if (!prev && dev->bay_locks[req.bay_id])
            dev->active_locks_count++;
        else if (prev && !dev->bay_locks[req.bay_id]) {
            if (dev->active_locks_count > 0)
                dev->active_locks_count--;
        }

        pr_info("wms_driver: bay #%u lock set to %s (active locks: %u)\n",
                req.bay_id, dev->bay_locks[req.bay_id] ? "LOCKED" : "UNLOCKED",
                dev->active_locks_count);

        mutex_unlock(&dev->lock);
        break;
    }

    case WMS_IOCTL_GET_BAY_LOCK: {
        wms_bay_lock_req_t req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        if (req.bay_id >= WMS_MAX_BAYS)
            return -EINVAL;

        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        req.lock_state = dev->bay_locks[req.bay_id];
        mutex_unlock(&dev->lock);

        if (copy_to_user((void __user *)arg, &req, sizeof(req)))
            return -EFAULT;
        break;
    }

    case WMS_IOCTL_SIMULATE_SCAN: {
        wms_scan_event_t event;
        if (copy_from_user(&event, (void __user *)arg, sizeof(event)))
            return -EFAULT;

        if (event.timestamp_ns == 0)
            event.timestamp_ns = ktime_get_real_ns();

        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        ret = internal_enqueue_event(dev, &event);
        mutex_unlock(&dev->lock);
        break;
    }

    case WMS_IOCTL_RESET_BUFFER: {
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;

        dev->head = 0;
        dev->tail = 0;
        dev->count = 0;
        dev->dropped_events = 0;
        pr_info("wms_driver: ring buffer and drop counters reset\n");

        mutex_unlock(&dev->lock);
        break;
    }

    default:
        ret = -ENOTTY;
        break;
    }

    return ret;
}

static struct file_operations wms_fops = {
    .owner          = THIS_MODULE,
    .open           = wms_open,
    .release        = wms_release,
    .read           = wms_read,
    .write          = wms_write,
    .poll           = wms_poll,
    .unlocked_ioctl = wms_ioctl,
};

static int __init wms_driver_init(void)
{
    int ret;

    pr_info("wms_driver: initializing character device module in C...\n");

    wms_dev = kzalloc(sizeof(struct wms_dev_state), GFP_KERNEL);
    if (!wms_dev) {
        pr_err("wms_driver: failed to allocate memory for device state\n");
        return -ENOMEM;
    }

    mutex_init(&wms_dev->lock);
    init_waitqueue_head(&wms_dev->read_wait);

    /* Allocate dynamic major/minor number */
    ret = alloc_chrdev_region(&wms_dev->dev_num, 0, 1, WMS_DEVICE_NAME);
    if (ret < 0) {
        pr_err("wms_driver: alloc_chrdev_region failed with error %d\n", ret);
        kfree(wms_dev);
        return ret;
    }

    /* Initialize cdev */
    cdev_init(&wms_dev->cdev, &wms_fops);
    wms_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&wms_dev->cdev, wms_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("wms_driver: cdev_add failed with error %d\n", ret);
        unregister_chrdev_region(wms_dev->dev_num, 1);
        kfree(wms_dev);
        return ret;
    }

    /* Create class */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    wms_dev->dev_class = class_create("wms_class");
#else
    wms_dev->dev_class = class_create(THIS_MODULE, "wms_class");
#endif

    if (IS_ERR(wms_dev->dev_class)) {
        pr_err("wms_driver: failed to create device class\n");
        ret = PTR_ERR(wms_dev->dev_class);
        cdev_del(&wms_dev->cdev);
        unregister_chrdev_region(wms_dev->dev_num, 1);
        kfree(wms_dev);
        return ret;
    }

    /* Create device node /dev/wms_driver */
    wms_dev->device = device_create(wms_dev->dev_class, NULL, wms_dev->dev_num, NULL, WMS_DEVICE_NAME);
    if (IS_ERR(wms_dev->device)) {
        pr_err("wms_driver: failed to create device node\n");
        ret = PTR_ERR(wms_dev->device);
        class_destroy(wms_dev->dev_class);
        cdev_del(&wms_dev->cdev);
        unregister_chrdev_region(wms_dev->dev_num, 1);
        kfree(wms_dev);
        return ret;
    }

    pr_info("wms_driver: loaded successfully with Major %d, Minor %d -> /dev/%s\n",
            MAJOR(wms_dev->dev_num), MINOR(wms_dev->dev_num), WMS_DEVICE_NAME);

    return 0;
}

static void __exit wms_driver_exit(void)
{
    pr_info("wms_driver: unloading module...\n");

    if (wms_dev) {
        /* Wake any blocked readers */
        mutex_lock(&wms_dev->lock);
        wms_dev->is_closing = 1;
        wake_up_interruptible_all(&wms_dev->read_wait);
        mutex_unlock(&wms_dev->lock);

        device_destroy(wms_dev->dev_class, wms_dev->dev_num);
        class_destroy(wms_dev->dev_class);
        cdev_del(&wms_dev->cdev);
        unregister_chrdev_region(wms_dev->dev_num, 1);
        kfree(wms_dev);
        wms_dev = NULL;
    }

    pr_info("wms_driver: unloaded successfully\n");
}

module_init(wms_driver_init);
module_exit(wms_driver_exit);
