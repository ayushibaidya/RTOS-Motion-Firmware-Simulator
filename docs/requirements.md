# Requirements

## Project Goal

Build software-first embedded firmware for a simulated 2-axis motion stage. The current project shall run as a host-side C demo with a FreeRTOS-style task architecture, while future phases shall target ARM Cortex-M emulation, real FreeRTOS task scheduling, UART-style host communication, and QEMU-based validation.

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

## Current Validation Status

```text
6/6 CTest suites passing
Python host-demo smoke test passing
```

## Future Requirements

Future phases should add:

- GitHub Actions CI/CD for build, CTest, and Python smoke testing
- Real FreeRTOS task scheduling
- QEMU-based ARM Cortex-M execution
- UART-style host communication
- Python CLI and telemetry logger
- PID/control-loop logic
- simulated limit-switch faults
