# FreeRTOS Task Architecture

## Purpose

This document defines the first FreeRTOS-style architecture for the motion-control firmware simulator.

The goal is to move from a single host-side control loop toward a task-based embedded design while preserving the existing command parser, motion controller, fault manager, telemetry module, and tests.

## Current Project Stage

The current project is a host-side C simulator with a FreeRTOS-style task layer. It supports:

- Command parsing for `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT`
- Simulated X/Y motion using milli-millimeter units
- Fault handling for emergency stop and out-of-bounds targets
- Telemetry output for timestamp, state, position, and fault status
- Unit tests for core modules, RTOS wrappers, and task-layer flow
- A Python host demo smoke test

The FreeRTOS-style task boundaries and communication primitives have been introduced on the host. The project does not run a real FreeRTOS scheduler yet.

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

### Next Slice: Main Refactor

The next implementation slice should refactor `firmware/Core/main.c` so the host demo uses `app_tasks` instead of duplicating command orchestration logic.

After that, add CI/CD and then start the real FreeRTOS backend/scheduler integration.

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
- Host-demo regression after `main.c` is refactored to use the task layer

## Known Limitations

- This project does not yet run a real FreeRTOS scheduler.
- The current simulator does not interact with physical motors or sensors.
- Timing is simulated using fixed tick values.
- Hardware-specific validation is deferred until QEMU or physical hardware support is added.

## Next Decision

The host-side wrapper path has been selected and implemented. The next decision is when to replace the host wrappers with a real FreeRTOS backend:

1. After `main.c` uses the task layer, and
2. After CI confirms the host tests and smoke test stay green.
