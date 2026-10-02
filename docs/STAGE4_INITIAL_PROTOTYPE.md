# Stage 4: Initial Implementation & Prototype Report

## 1. Prototype Milestone Overview
In Stage 4, the core modules designed in Stage 3 were implemented, establishing the physical and virtual bridge between the Linux Kernel Character Device Driver and the Modern C++ User Space Application:
- **Kernel Module**: `driver/wms_driver.c`, `driver/wms_driver.h`, `driver/include/wms_ioctl.h`
- **Hardware Abstraction Layer (HAL)**: `IDeviceDriver`, `LinuxCharDevice`, `SimulatedDevice`
- **Domain Core**: `StorageBay`, `WarehouseZone`, `InventoryManager`
- **Initial Verification Prototype**: End-to-end event intake pipeline and bay allocation.

---

## 2. Core Modules Implemented

### 2.1 Linux Character Device Driver (`/dev/wms_driver`)
The kernel driver was implemented as a Loadable Kernel Module (LKM):
- **Dynamic Character Device Registration**: Uses `alloc_chrdev_region()` to request a dynamic major number, followed by `cdev_init()` and `cdev_add()`.
- **Automatic Device File Generation**: Employs `class_create()` and `device_create()` to dynamically populate `/dev/wms_driver` without requiring manual `mknod`.
- **Ring Buffer Synchronization**:
  - Maintained a 64-slot circular FIFO buffer storing `wms_scan_event_t` records.
  - Protected read/write pointers (`head`, `tail`, `count`) with a kernel `mutex`.
  - Implemented a wait queue (`wait_queue_head_t read_wait`) allowing user-space processes calling `read()` or `poll()` to sleep until new scan events arrive.
- **IOCTL Interface**:
  - Implemented `WMS_IOCTL_GET_STATUS`, `WMS_IOCTL_SET_BAY_LOCK`, `WMS_IOCTL_GET_BAY_LOCK`, `WMS_IOCTL_SIMULATE_SCAN`, and `WMS_IOCTL_RESET_BUFFER`.

### 2.2 Hardware Abstraction Layer (HAL)
To guarantee high testability, fault tolerance, and multi-environment portability:
- **`IDeviceDriver`**: Abstract base class establishing the contract for hardware interaction.
- **`LinuxCharDevice`**: Concrete implementation utilizing Linux VFS system calls (`open()`, `read()`, `poll()`, `ioctl()`).
- **`SimulatedDevice`**: Thread-safe in-memory mock driver utilizing `std::mutex` and `std::condition_variable`. In environments without root privileges or with custom host kernels, the application transparently falls back to this simulator without changing a single line of business logic.

### 2.3 Warehouse Core Domain
- **`StorageBay`**: Encapsulates bay states (`EMPTY`, `OCCUPIED`, `LOCKED`, `MAINTENANCE`), weight constraints, and atomic item storage/retrieval.
- **`WarehouseZone`**: Partitions the facility into specialized physical zones (Zone A: Electronics, Zone B: Cold Storage, Zone C: General/Heavy Cargo).
- **`InventoryManager`**: Ingests `wms_scan_event_t` from the driver, categorizes cargo based on SKU prefixes and barcode patterns, computes the optimal bay, engages the electromagnetic bay lock during placement, and records stock.

---

## 3. Progressive Integration Flow
The integration pipeline was established as follows:
```
Hardware Scan / Injection ──> [Kernel Ring Buffer] 
                                    │ (Wait Queue wake-up)
                                    ▼
                         [LinuxCharDevice::readEvent] (via poll)
                                    │
                                    ▼
                       [C++ Worker Thread Loop]
                                    │
                                    ▼
                     [InventoryManager::intakeItem]
                                    │
                                    ├─► [HAL: setBayLock(ID, LOCKED)] (ioctl)
                                    ├─► [Bay State Updated to OCCUPIED]
                                    └─► [HAL: setBayLock(ID, UNLOCKED)] (ioctl)
```

---

## 4. Issues Encountered & Engineering Solutions

| # | Issue Identified | Root Cause | Engineering Solution |
|---|---|---|---|
| 1 | `class_create()` API divergence across Linux kernels | Linux kernel 6.4 removed the first `struct module *owner` argument from `class_create()`. | Added preprocessor conditional `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)` to support both legacy and modern kernels seamlessly. |
| 2 | WSL2 Kernel Vermagic Mismatch | WSL2 runs Microsoft's customized kernel (`6.18.33.2-microsoft-standard-WSL2`), while generic distro headers build against `7.0.0-generic`. | Implemented dual-mode HAL: C++ engine auto-detects if `/dev/wms_driver` can be opened; if unavailable, falls back to `SimulatedDevice`, while native Linux targets run the compiled `.ko`. |
| 3 | Read-side Busy Waiting | Polling without sleeping spikes CPU utilization to 100%. | Integrated kernel wait queues (`wait_event_interruptible`) and userspace `poll()` with timeout, ensuring zero idle CPU consumption. |
| 4 | Data Alignment across Kernel and Userspace | Structure padding differences between 32-bit/64-bit architectures or compilers. | Enforced `#pragma pack(push, 1)` on shared IOCTL structs (`wms_scan_event_t`, `wms_device_status_t`). |

---

## 5. Prototype Demonstration Evidence
During prototype verification:
1. Driver compilation (`make -C driver`) produced `wms_driver.ko` cleanly.
2. The C++ application ingested 7 simulated intake events across 3 distinct zones.
3. Bays #1, #2, #3 (Zone A), #9, #10 (Zone B), and #17, #18 (Zone C) were occupied accurately.
4. Active electromagnetic locks were set and read back via IOCTL.

---

## 6. Roadmap to Stage 5
With the core prototype functional, **Stage 5** focuses on:
- Complete test coverage (Unit, Integration, and Concurrency stress testing).
- POSIX Shared Memory telemetry broadcasting.
- Signal handling for graceful teardown.
- Bug fixing and performance benchmarking.
