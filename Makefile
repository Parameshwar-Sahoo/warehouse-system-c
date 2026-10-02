CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pthread -I. -Iinclude -Idriver/include -D_GNU_SOURCE -g
LDFLAGS = -pthread -lrt

SRC_CORE = src/core/storage_bay.c src/core/warehouse_zone.c src/core/inventory_manager.c src/core/order_processor.c
SRC_HAL  = src/hal/linux_chardev.c src/hal/simulated_dev.c
SRC_IPC  = src/ipc/shared_memory.c src/ipc/signal_handler.c
SRC_UI   = src/ui/terminal_ui.c
SRC_ALL  = $(SRC_CORE) $(SRC_HAL) $(SRC_IPC) $(SRC_UI)

OBJ = $(patsubst src/%.c, build/%.o, $(SRC_ALL))

.PHONY: all driver app test demo dashboard clean

all: driver app

driver:
	@echo "=== Building Linux Character Device Driver (C) ==="
	$(MAKE) -C driver

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

app: $(OBJ) build/main.o
	@mkdir -p bin
	@echo "=== Linking Warehouse System Executable (C) ==="
	$(CC) $(OBJ) build/main.o $(LDFLAGS) -o bin/warehouse_system

build/main.o: src/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

test: $(OBJ)
	@mkdir -p bin build
	@echo "=== Building & Running Comprehensive Test Suites (C) ==="
	$(CC) $(CFLAGS) $(OBJ) tests/test_runner.c $(LDFLAGS) -o bin/test_runner
	./bin/test_runner

demo: app
	@echo "=== Running Live System Demonstration (C) ==="
	./bin/warehouse_system --demo

dashboard:
	@echo "=== Launching Interactive Web Dashboard ==="
	@bash scripts/open_dashboard.sh || true

clean:
	@echo "=== Cleaning Project Artifacts ==="
	$(MAKE) -C driver clean || true
	rm -rf build bin
