# Stage 1: Project Introduction & Problem Definition

## 1. Project Title
**Warehouse Management System (WMS) Using a Linux Character Device Driver and Modern C++**

---

## 2. Project Executive Summary
In modern industrial logistics, automated warehousing, and supply chain hubs (Industry 4.0), hundreds of IoT peripherals—such as barcode/RFID scanners, load-cell weight sensors, conveyor diverters, automated guided vehicles (AGVs), and bay locking solenoids—generate high-frequency telemetry and require low-latency actuation. 

Conventional pure userspace warehouse applications interact with peripherals via slow, polled serial ports or unbuffered socket streams. This introduces kernel-to-user context switching overhead, potential packet drops during high bursts, and unreliable concurrency control.

This project designs and implements an end-to-end, hybrid industrial **Warehouse Management System (WMS)** bridging **Linux Kernel Space** and **Modern C++ User Space**:
1. **Linux Kernel Character Device Driver (`/dev/wms_driver`)**: Implements an event-driven hardware abstraction layer with interrupt/scan simulation, a thread-safe circular ring buffer, wait queues for non-blocking and asynchronous I/O (`poll`/`epoll`), and custom `ioctl` commands for hardware telemetry and bay lock control.
2. **System Programming & Modern C++ Application Layer**: Implements a robust multi-threaded warehouse controller featuring POSIX shared memory, real-time signal handling, concurrent inventory management, automated order dispatching, and an interactive real-time visual terminal dashboard.

---

## 3. Problem Statement & Motivation

### 3.1 The Problem
Traditional warehouse IT stacks often exhibit the following limitations:
- **High-Latency Sensor Ingestion**: User-space polling of hardware sensors consumes excessive CPU cycles and risks dropping scan events during peak intake operations.
- **Lack of Atomic Device Control**: When multiple warehouse operator stations attempt to actuate physical storage bays (locking/unlocking) or read weight scales concurrently, race conditions and inconsistent states occur without kernel-enforced synchronization.
- **Disconnection Between Hardware and Business Logic**: Typical enterprise software treats hardware as an afterthought, creating fragile glue scripts that fail under load or crash silently when hardware buffers overflow.

### 3.2 The Solution
By implementing a dedicated **Linux Character Device Driver**, the operating system kernel directly manages hardware event buffering, hardware status queries, and physical locking primitives:
- The driver buffers scanner inputs into a kernel-space ring buffer protected by kernel mutexes.
- The user-space C++ daemon uses asynchronous notifications (`select`/`poll`/`epoll`) or blocking wait-queues to read events instantaneously upon arrival with zero idle CPU burn.
- Custom `ioctl` (Input/Output Control) syscalls provide atomic querying of sensor data, tare/calibration of scales, and actuation of physical bay locks.

---

## 4. Project Objectives

1. **Kernel Driver Development**:
   - Design and build a Linux Character Device Driver using dynamic major/minor allocation (`alloc_chrdev_region`).
   - Implement core file operations: `open()`, `release()`, `read()`, `write()`, `unlocked_ioctl()`, and `poll()`.
   - Maintain a synchronized ring buffer using kernel wait queues (`wait_event_interruptible`, `wake_up_interruptible`) and mutexes.
   - Support hardware actuation commands and sensor queries via structured `ioctl` calls.

2. **System Programming & Concurrency**:
   - Utilize POSIX System V/POSIX APIs: Shared Memory (`shm_open`, `mmap`) to broadcast real-time telemetry across multi-process warehouse terminals.
   - Implement robust POSIX Signal Handling (`SIGINT`, `SIGTERM`, `SIGHUP`) for graceful resource deallocation and dynamic config reload.
   - Multi-threaded worker pool using `std::thread`, `std::mutex`, and `std::condition_variable` for parallel order processing.

3. **Domain & Application Architecture (Modern C++17/20)**:
   - Object-Oriented design modeling Warehouse Zones (Aisle, Rack, Shelf, Bay), Items (SKU, batch, weight, temperature sensitivity), and Dispatch Orders.
   - Clean Hardware Abstraction Layer (HAL) isolating user-space business logic from kernel file descriptors, complete with a hardware simulation fallback mode.
   - Interactive, ANSI-colored real-time console dashboard for warehouse supervisors.

4. **Academic & Engineering Rigor**:
   - Adhere to a strict 6-stage lifecycle: Requirements & PRD $\rightarrow$ UML & Architecture $\rightarrow$ Prototype $\rightarrow$ Verification & Performance $\rightarrow$ Final Delivery.

---

## 5. Project Scope

### In-Scope
- **Kernel Space**:
  - Loadable Kernel Module (LKM) targeting Linux kernel $5.x$ / $6.x$.
  - Character device registration under `/dev/wms_driver`.
  - Kernel memory management (kmalloc, kfree, copy_to_user, copy_from_user).
  - IOCTL command interface (`WMS_IOCTL_GET_STATUS`, `WMS_IOCTL_SET_BAY_LOCK`, `WMS_IOCTL_TRIGGER_SCAN`, `WMS_IOCTL_RESET_BUFFER`).
- **User Space**:
  - C++17 application compiled with GCC/Clang and CMake.
  - Multi-threaded event listener integrating `poll()` on `/dev/wms_driver`.
  - Inventory management logic (stock intake, automated item allocation, order picking, capacity tracking).
  - POSIX shared memory state publishing for external inspector processes.
  - Signal handling for graceful shutdown.
  - Unit tests and automated integration test harness.

### Out-of-Scope (Future Expansion)
- Physical PCB soldering / bare-metal microcontroller firmware (simulated via driver injection and virtual hardware generator).
- Remote cloud ERP synchronization (SAP/Oracle WMS cloud sync) — mocked via local JSON persistence.

---

## 6. Expected Outcomes & Real-World Applications

### 6.1 Expected Outcomes
- A fully functional, loadable Linux Kernel Module (`wms_driver.ko`) demonstrating kernel synchronization primitives and device file operations.
- A compiled C++ Warehouse Management System executable (`wms_daemon` and `wms_cli`) capable of real-time event processing and stock dispatching.
- Measurable performance: sub-millisecond event dispatch latency and zero data loss across concurrent item scans.
- Complete documentation suite, UML diagrams, test reports, and an automated deployment pipeline.

### 6.2 Industry Applications
- **Automated Fulfillment Centers (e.g., Amazon, DHL, FedEx)**: High-speed barcode/RFID conveyor sorting and automated bin placement.
- **Cold Storage Logistics**: Kernel-level temperature/humidity threshold monitoring triggering urgent hardware alarms and bay isolation.
- **Smart Manufacturing Inventory**: Just-In-Time (JIT) parts tracking with automated inventory deduction upon sensor gate traversal.

---

## 7. Roadmap to Stage 2
With the project foundation and objectives established, **Stage 2** will detail the **Project Requirements Document (PRD)**, enumerating explicit Functional Requirements (FR), Non-Functional Requirements (NFR), interface specifications, and the project timeline.
