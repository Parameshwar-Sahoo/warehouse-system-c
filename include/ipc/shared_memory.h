#ifndef WMS_SHARED_MEMORY_H
#define WMS_SHARED_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

#define WMS_SHM_NAME "/wms_telemetry_shm"

#pragma pack(push, 1)
typedef struct {
    uint32_t process_id;
    uint32_t total_scans_received;
    uint32_t active_bay_locks;
    uint32_t total_items_stored;
    float    occupancy_rate;
    char     last_scanned_barcode[32];
    uint64_t last_heartbeat_epoch_ms;
    uint8_t  is_daemon_running;
} WmsSharedTelemetry;
#pragma pack(pop)

typedef struct {
    char shm_name[64];
    int shm_fd;
    WmsSharedTelemetry* shared_data;
    bool is_master;
} SharedMemoryManager;

bool shm_init(SharedMemoryManager* shm, const char* name, bool create_as_master);
void shm_update(SharedMemoryManager* shm, uint32_t scans, uint32_t locks, uint32_t items, float occupancy, const char* barcode);
bool shm_read(SharedMemoryManager* shm, WmsSharedTelemetry* out_telemetry);
void shm_cleanup(SharedMemoryManager* shm);

#endif /* WMS_SHARED_MEMORY_H */
