# Test Procedure

## Purpose

This document lists the basic tests used to check the firmware during development. The current project is tested with CMake/CTest unit tests plus Python smoke tests for the host demo and FreeRTOS scheduler demo. Emulator-based tests are planned for later phases.

## Manual Tests

### Test 1: Firmware Startup

1. Build the project.
2. Run the host-side demo:

```bash
./build/mr_robo_demo
```

3. Confirm startup output appears.
4. Confirm telemetry begins or the firmware accepts commands.

Expected result: firmware is responsive.

### Test 1B: FreeRTOS Scheduler Startup

1. Build the FreeRTOS backend:

```bash
cmake -S . -B build-freertos -G Ninja -DUSE_FREERTOS=ON
cmake --build build-freertos
```

2. Run the scheduler demo:

```bash
./build-freertos/motion_freertos_scheduler_demo
```

Expected result: firmware boots, starts the scheduler, runs the scripted command flow, prints telemetry, and exits cleanly for the POSIX demo.

### Test 2: Ping Command

1. Send:

```text
PING
```

Expected result:

```text
OK PONG
```

### Test 3: Status Command

1. Send:

```text
STATUS
```

Expected result: firmware reports current state and position.

### Test 4: Move Command

1. Send:

```text
MOVE X=50 Y=20 F=600
```

Expected result: firmware accepts the command and simulated position changes over time.

### Test 5: Stop Command

1. Start a move.
2. Send:

```text
STOP
```

Expected result: motion stops without entering fault state.

### Test 6: Emergency Stop

1. Start a move.
2. Send:

```text
ESTOP
```

Expected result: motion stops and firmware enters `FAULT` state.

### Test 7: Clear Fault

1. Trigger emergency stop.
2. Send:

```text
CLEAR_FAULT
```

Expected result: firmware clears fault state and returns motion state to `IDLE`.

### Test 8: Travel Bounds Fault

1. Send a movement outside the simulated workspace:

```text
MOVE X=101 Y=20 F=25
```

Expected result: firmware rejects the move, reports `ERR LIMIT_EXCEEDED`, and enters `FAULT` state.

### Test 9: Invalid Command

1. Send:

```text
BANANA
```

Expected result: firmware returns `ERR_UNKNOWN_COMMAND`.

## Automated Tests

Run the current automated test suite with:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Current CTest suites:

- `telemetry`
- `motion_controller`
- `command_parser`
- `fault_manager`
- `rtos_port`
- `app_tasks`

The `motion_controller` suite includes travel bounds checks for valid minimum/maximum targets and invalid out-of-range targets.

The `rtos_port` suite verifies host-side queue, mutex, and event-group behavior.

The `app_tasks` suite verifies task-layer command queueing, movement, stop behavior, emergency-stop faulting, fault clearing, bounds faults, and telemetry snapshots.

Expected result for the default host build:

```text
6/6 tests passed
```

Run the FreeRTOS backend CTest suite with:

```bash
cmake -S . -B build-freertos -G Ninja -DUSE_FREERTOS=ON
cmake --build build-freertos
ctest --test-dir build-freertos --output-on-failure
```

Expected result for the FreeRTOS build:

```text
7/7 tests passed
```

Run the current host-demo integration test with:

```bash
python3 tests/hil/test_host_demo.py
```

Current integration test:

- `tests/hil/test_host_demo.py`

Expected result:

```text
host demo integration test passed
```

The host-demo integration test verifies that the built demo executable prints expected boot, command, telemetry, fault, and recovery output.

Run the FreeRTOS scheduler integration smoke test with:

```bash
DEMO_PATH=build-freertos/motion_freertos_scheduler_demo python3 tests/hil/test_freertos_scheduler_demo.py
```

Current scheduler integration test:

- `tests/hil/test_freertos_scheduler_demo.py`

Expected result:

```text
FreeRTOS scheduler demo integration test passed
```

The scheduler smoke test verifies that the FreeRTOS demo boots, processes the scripted command sequence in order, reaches `MOVING`, enters `FAULT` after `ESTOP`, clears the fault, reports `STATUS`, and exits before the timeout.

Future Python/emulator tests should cover:

- Emulator command-response behavior
- UART-style host communication
- Telemetry logging
- Fault injection
- QEMU firmware boot

## Current Regression Command Set

Run both automated validation paths before large refactors:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/hil/test_host_demo.py
```

Run the FreeRTOS backend validation path with:

```bash
cmake -S . -B build-freertos -G Ninja -DUSE_FREERTOS=ON
cmake --build build-freertos
ctest --test-dir build-freertos --output-on-failure
DEMO_PATH=build-freertos/mr_robo_demo python3 tests/hil/test_host_demo.py
DEMO_PATH=build-freertos/motion_freertos_scheduler_demo python3 tests/hil/test_freertos_scheduler_demo.py
```

Expected result:

```text
7/7 tests passed
host demo integration test passed
FreeRTOS scheduler demo integration test passed
```

## CI Validation

GitHub Actions runs the same core validation on push and pull request for both backend modes:

- checkout repository with submodules
- install CMake, Ninja, and Python
- configure the project with CMake for `USE_FREERTOS=OFF` and `USE_FREERTOS=ON`
- build the project
- run CTest
- run the Python host-demo smoke test
- run the Python FreeRTOS scheduler-demo smoke test for the `USE_FREERTOS=ON` backend

Current workflow file:

```text
.github/workflows/ci.yml
```

Expected result: the GitHub Actions run completes with a green check.
