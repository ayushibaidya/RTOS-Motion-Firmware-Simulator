# Requirements

## Project Goal

Build software-first embedded firmware for a simulated 2-axis motion stage. The current project shall run as a host-side C demo with a FreeRTOS-style task architecture, real FreeRTOS wrapper backend, and CI validation, while the active integration phase adds real scheduler task startup for future ARM Cortex-M emulation, UART-style host communication, and QEMU-based validation.

## MVP Requirements

### REQ-001: Firmware Boot

The MVP firmware shall build and run as a host-side C demo executable. Future integration shall target an emulated ARM Cortex-M environment.

### REQ-002: RTOS-Style Task Structure

The firmware shall keep command handling, motion control, telemetry, and fault handling in separate C modules. The current FreeRTOS-style phase shall map these responsibilities into host-callable task-step functions before real FreeRTOS scheduling is introduced.

### REQ-003: UART Command Interface

The MVP firmware shall accept text-based commands through the host-side C demo. Future phases shall accept UART-style commands from a host-side Python tool.

Required commands:

- `PING`
- `STATUS`
- `MOVE X=<value> Y=<value> F=<feedrate>`
- `STOP`
- `ESTOP`
- `CLEAR_FAULT`

### REQ-004: Command Responses

The firmware shall respond to commands using simple text responses.

Required response types:

- `OK`
- `ERR`
- Telemetry messages beginning with `T`

### REQ-005: Motion Simulation

The firmware shall track simulated 2-axis position state for `X` and `Y`.

The firmware shall update position over time when a valid `MOVE` command is received.

The firmware shall reject movement targets outside the simulated travel area.

### REQ-006: Telemetry

The firmware shall periodically report motion state, simulated position, and fault status.

Example:

```text
T ms=100 state=MOVING x=10.2 y=4.1 fault=0
```

### REQ-007: Fault Handling

The firmware shall support emergency-stop behavior.

When `ESTOP` is received:

- Motion shall stop.
- Fault state shall become active.
- New movement shall be blocked while the fault is active.

The firmware shall support explicit fault recovery with `CLEAR_FAULT`.

When `CLEAR_FAULT` is received:

- Fault state shall be cleared.
- Fault reason shall return to `NONE`.
- Motion state shall be allowed to return to `IDLE`.

When a target exceeds the simulated travel bounds:

- Fault state shall become active.
- Fault reason shall become `LIMIT_EXCEEDED`.
- Motion state shall enter `FAULT`.
- The command response shall report `ERR LIMIT_EXCEEDED`.

### REQ-008: Host Demo

The MVP shall provide a host-side executable that demonstrates boot, command parsing, simulated movement, telemetry, emergency stop, and fault clearing.

### REQ-009: Unit Tests

The MVP shall include CMake/CTest unit tests for:

- telemetry formatting
- command parsing
- motion state updates
- fault handling
- RTOS wrapper queue, mutex, and event-group behavior
- task-layer command flow, motion updates, telemetry snapshots, and fault recovery

### REQ-010: RTOS Wrapper Layer

The project shall provide a host-side RTOS abstraction layer that models:

- bounded queues
- mutex lock/unlock behavior
- event bit set/clear/get behavior

The wrapper shall avoid heap allocation and use caller-owned static storage.

### REQ-011: Task-Layer Application Flow

The project shall provide task-style application functions for:

- parsing raw command text and queueing validated commands
- consuming command messages and updating motion/fault state
- publishing telemetry snapshots

The task layer shall preserve existing safety behavior for `ESTOP`, `STOP`, `CLEAR_FAULT`, and out-of-bounds `MOVE` commands.

### REQ-012: CI/CD Validation

The project shall include a GitHub Actions workflow that:

- checks out the repository with submodules
- configures the project with CMake and Ninja
- builds the host-side C targets
- runs CTest unit suites
- runs the Python host-demo smoke test

### REQ-013: FreeRTOS Dependency Preparation

The project shall track the real FreeRTOS kernel as a third-party Git submodule under `third_party/FreeRTOS-Kernel`.

The default host build shall continue to use `rtos_port_host.c`.

The CMake build shall expose a `USE_FREERTOS` option that selects `rtos_port_freertos.c` and real FreeRTOS kernel sources when enabled.

### REQ-014: Real FreeRTOS Backend Wrapper

When `USE_FREERTOS=ON`, the project shall build the RTOS wrapper against real FreeRTOS APIs for:

- queue creation, send, receive, count, empty, and full checks
- mutex creation, lock, and unlock
- event group creation, set, clear, and get operations

This requirement verifies FreeRTOS primitive integration, not full scheduler-driven task execution.

## Current MVP Definition of Done

The current MVP is complete when:

- The host-side demo executable builds successfully.
- The firmware modules initialize correctly.
- `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT` are parsed.
- Simulated movement updates position over time.
- Telemetry reports uptime, state, position, and fault status.
- `ESTOP` activates fault state.
- `CLEAR_FAULT` clears fault state.
- Out-of-bounds moves activate `LIMIT_EXCEEDED` fault behavior.
- Unit tests pass for telemetry, command parser, motion controller, fault manager, RTOS port, and app tasks.
- The Python host-demo smoke test passes.
- GitHub Actions CI runs the same build, CTest, and Python smoke validation on push and pull request.
- `USE_FREERTOS=ON` builds and passes the same CTest and Python smoke validation through the real FreeRTOS backend wrapper.

## Current Validation Status

```text
6/6 CTest suites passing for host backend
6/6 CTest suites passing for FreeRTOS backend
Python host-demo smoke test passing for both demo builds
```

## Future Requirements

Future phases should add:

- Real FreeRTOS task scheduling
- QEMU-based ARM Cortex-M execution
- UART-style host communication
- Python CLI and telemetry logger
- PID/control-loop logic
- simulated limit-switch faults
