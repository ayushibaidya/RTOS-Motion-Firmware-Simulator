# FreeRTOS Task Architecture

## Purpose

This document defines the first FreeRTOS-style architecture for the motion-control firmware simulator.

The goal is to move from a single host-side control loop toward a task-based embedded design while preserving the existing command parser, motion controller, fault manager, telemetry module, and tests.

## Current Project Stage

The current project is a host-side C simulator with a FreeRTOS-style task layer, CI validation, and a real FreeRTOS kernel dependency prepared as a submodule. It supports:

- Command parsing for `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT`
- Simulated X/Y motion using milli-millimeter units
- Fault handling for emergency stop and out-of-bounds targets
- Telemetry output for timestamp, state, position, and fault status
- Unit tests for core modules, RTOS wrappers, and task-layer flow
- A Python host demo smoke test
- GitHub Actions validation on push and pull request
- FreeRTOS-Kernel tracked under `third_party/FreeRTOS-Kernel`

The FreeRTOS-style task boundaries and communication primitives have been introduced on the host. The real FreeRTOS dependency is present, but the project does not run a real FreeRTOS scheduler yet.

## Task Overview

| Task | Responsibility | Input | Output |
|---|---|---|---|
| `CommandTask` | Owns raw command intake and parsing | Text command line | Parsed `command_t` message |
| `MotionTask` | Owns periodic motion updates and normal motion commands | Parsed command messages and periodic tick | Updated motion state |
| `TelemetryTask` | Owns periodic status publishing | Motion/fault state snapshot | Telemetry line |
| Fault handling path | Owns safety transitions and recovery behavior | `ESTOP`, invalid target, internal fault, `CLEAR_FAULT` | Updated fault state and motion state |

## High-Level Data Flow

```text
Host or UART command text
        |
        v
CommandTask
        |
        v
Command Queue
        |
        v
MotionTask / Fault handling path
        |
        v
Shared motion + fault state
        |
        v
TelemetryTask
        |
        v
Telemetry output
```

## FreeRTOS Primitives To Use

| Primitive | Planned Use | Why It Exists |
|---|---|---|
| Queue | Pass parsed commands from `CommandTask` to `MotionTask` | Tasks should communicate through bounded messages instead of shared globals |
| Queue or stream buffer | Pass telemetry text or telemetry snapshots | Telemetry output should not block motion/control logic |
| Mutex | Protect shared motion and fault state | Multiple tasks may read or update state after FreeRTOS scheduling is introduced |
| Event group | Track flags such as `MOVING`, `FAULTED`, and `STOP_REQUESTED` | System-level flags should be visible without tightly coupling modules |

## Task Responsibilities

### CommandTask

`CommandTask` receives one command line at a time, parses it using the existing command parser, and sends valid parsed commands to the command queue.

It should not directly move the stage, format telemetry, or mutate motion internals.

### MotionTask

`MotionTask` receives parsed commands from the command queue and applies motion behavior.

It owns normal command execution such as:

- `MOVE`
- `STOP`
- `STATUS` requests that need motion state

It also runs the periodic motion update step using a fixed time interval.

### TelemetryTask

`TelemetryTask` periodically reads the current motion and fault state, builds a telemetry status snapshot, and sends telemetry output.

It should use existing telemetry formatting instead of duplicating output strings.

### Fault Handling Path

Fault handling may begin as helper functions called by `MotionTask`, then later become a separate `FaultTask` if the design needs it.

This path owns safety behavior:

- `ESTOP` immediately sets fault state
- Out-of-bounds `MOVE` sets fault state
- `MOVE` is blocked while faulted
- `CLEAR_FAULT` clears the latched fault and returns the system to `IDLE`
- `STOP` stops motion without faulting

## Shared State

The following state will need protection once multiple tasks can access it:

- Current X/Y position
- Target X/Y position
- Motion state
- Feedrate
- Fault active flag
- Fault reason

Initial implementation should keep state inside the existing modules and add protection at the task/orchestration layer.

## Implementation Slices

### Completed Slice 1: RTOS Wrapper

A small RTOS abstraction layer runs on the host:

```text
firmware/App/rtos/
  rtos_port.h
  rtos_port_host.c
```

This layer provides:

- Queue init/send/receive
- Mutex init/lock/unlock
- Event bit set/clear/get

This layer is covered by the `rtos_port` CTest suite.

### Completed Slice 2: Task Layer

Task-level application files have been added:

```text
firmware/App/tasks/
  app_tasks.h
  app_tasks.c
  app_messages.h
```

These files define:

- Command message types
- Task initialization
- One-step host-callable versions of each task
- Event bits for `MOVING`, `FAULTED`, and `STOP_REQUESTED`

This layer is covered by the `app_tasks` CTest suite.

### Completed Slice 3: Main Refactor

`firmware/Core/main.c` now uses `app_tasks` instead of duplicating command orchestration logic.

### Completed Slice 4: CI/CD

GitHub Actions now builds the host project, runs CTest, and runs the Python host-demo smoke test on push and pull request.

### Current Slice 5: Real FreeRTOS Backend Preparation

FreeRTOS-Kernel is now tracked as a Git submodule:

```text
third_party/FreeRTOS-Kernel
```

The project has a CMake backend-selection option:

```text
USE_FREERTOS
```

The real backend placeholder is:

```text
firmware/App/rtos/rtos_port_freertos.c
```

The next implementation step is to map the wrapper APIs in `rtos_port.h` to real FreeRTOS queue, semaphore, and event-group APIs without breaking the default host backend.

## Testing Strategy

Existing tests must remain valid.

New tests should be added incrementally:

- RTOS queue behavior
- RTOS mutex behavior
- RTOS event bit behavior
- Command queue handoff
- `ESTOP` fault transition through the task path
- `CLEAR_FAULT` recovery through the task path
- Motion update through the task path
- Host-demo regression after `main.c` uses the task layer
- CI regression on push and pull request
- Future `USE_FREERTOS=ON` backend build test

## Known Limitations

- This project does not yet run a real FreeRTOS scheduler.
- `rtos_port_freertos.c` is currently a backend placeholder and is not the default host backend.
- `USE_FREERTOS=ON` is not yet wired to a complete FreeRTOS target.
- The current simulator does not interact with physical motors or sensors.
- Timing is simulated using fixed tick values.
- Hardware-specific validation is deferred until QEMU or physical hardware support is added.

## Next Decision

The host-side wrapper path has been selected and implemented, the host demo uses the task layer, CI is active, and the FreeRTOS dependency is available. The next decision is how to complete the real FreeRTOS backend:

1. Select the first real backend target or portable layer.
2. Add the required `FreeRTOSConfig.h`.
3. Wire `USE_FREERTOS=ON` to compile `rtos_port_freertos.c`.
4. Keep `USE_FREERTOS=OFF` as the stable host-test path.
