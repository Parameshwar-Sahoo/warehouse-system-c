#include "ipc/shared_memory.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

bool shm_init(SharedMemoryManager* shm, const char* name, bool create_as_master) {
    if (!shm) return false;
    memset(shm, 0, sizeof(SharedMemoryManager));
    strncpy(shm->shm_name, name ? name : WMS_SHM_NAME, sizeof(shm->shm_name) - 1);
    shm->is_master = create_as_master;

    if (create_as_master) {
        shm_unlink(shm->shm_name);
        shm->shm_fd = shm_open(shm->shm_name, O_CREAT | O_RDWR, 0666);
        if (shm->shm_fd < 0) return false;

        if (ftruncate(shm->shm_fd, sizeof(WmsSharedTelemetry)) != 0) {
            close(shm->shm_fd);
            shm->shm_fd = -1;
            return false;
        }
    } else {
        shm->shm_fd = shm_open(shm->shm_name, O_RDONLY, 0666);
        if (shm->shm_fd < 0) return false;
    }

    int prot = create_as_master ? (PROT_READ | PROT_WRITE) : PROT_READ;
    shm->shared_data = (WmsSharedTelemetry*)mmap(NULL, sizeof(WmsSharedTelemetry), prot, MAP_SHARED, shm->shm_fd, 0);

    if (shm->shared_data == MAP_FAILED) {
        shm->shared_data = NULL;
        close(shm->shm_fd);
        shm->shm_fd = -1;
        return false;
    }

    if (create_as_master) {
        memset(shm->shared_data, 0, sizeof(WmsSharedTelemetry));
        shm->shared_data->process_id = (uint32_t)getpid();
        shm->shared_data->is_daemon_running = 1;
        shm->shared_data->last_heartbeat_epoch_ms = (uint64_t)time(NULL) * 1000;
    }

    return true;
}

void shm_update(SharedMemoryManager* shm, uint32_t scans, uint32_t locks, uint32_t items, float occupancy, const char* barcode) {
    if (!shm || !shm->shared_data || !shm->is_master) return;

    shm->shared_data->total_scans_received = scans;
    shm->shared_data->active_bay_locks = locks;
    shm->shared_data->total_items_stored = items;
    shm->shared_data->occupancy_rate = occupancy;
    if (barcode) {
        strncpy(shm->shared_data->last_scanned_barcode, barcode, sizeof(shm->shared_data->last_scanned_barcode) - 1);
    }
    shm->shared_data->last_heartbeat_epoch_ms = (uint64_t)time(NULL) * 1000;
}

bool shm_read(SharedMemoryManager* shm, WmsSharedTelemetry* out_telemetry) {
    if (!shm || !shm->shared_data || !out_telemetry) return false;
    *out_telemetry = *shm->shared_data;
    return true;
}

void shm_cleanup(SharedMemoryManager* shm) {
    if (!shm) return;
    if (shm->shared_data) {
        if (shm->is_master) {
            shm->shared_data->is_daemon_running = 0;
        }
        munmap(shm->shared_data, sizeof(WmsSharedTelemetry));
        shm->shared_data = NULL;
    }
    if (shm->shm_fd >= 0) {
        close(shm->shm_fd);
        shm->shm_fd = -1;
    }
    if (shm->is_master) {
        shm_unlink(shm->shm_name);
        shm->is_master = false;
    }
}
