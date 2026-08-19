# RTOS-Based Motion Control Firmware Simulator

## Overview

This project implements software-first embedded motion-control firmware for a simulated 2-axis stage, with host-side validation and a real FreeRTOS POSIX scheduler demo. The long-term path is ARM Cortex-M, QEMU integration, and hardware-style driver boundaries.

The goal is to learn and demonstrate embedded systems concepts without requiring physical hardware.

## Purpose

This project currently demonstrates:

- Embedded C
- CMake/CTest builds
- Motion planning
- State-machine control
- Telemetry formatting
- Fault handling and recovery
- Travel bounds checking
- FreeRTOS-style queues, mutexes, and event groups through a host-side RTOS wrapper
- Real FreeRTOS queue, mutex, event-group, and scheduler startup through `USE_FREERTOS=ON`
- Task-style application architecture for command, motion, telemetry, and fault behavior
- Static FreeRTOS task creation with command, motion, telemetry, and supervisor tasks
- GitHub Actions CI for automated build, unit-test, and smoke-test validation
- Unit-tested firmware modules

Planned future integrations include:

- ARM Cortex-M firmware builds
- UART command protocols
- QEMU-based embedded simulation
- Python-based firmware testing
- PID/control-loop concepts

## System Concept

The current project runs in two software-only modes:

- a host-side C demo that steps through the task layer directly
- a FreeRTOS POSIX scheduler demo that creates real FreeRTOS tasks and runs a scripted command flow

The planned target flow is a Python host program sending commands to firmware running inside QEMU, where the firmware behaves like it is running on a real microcontroller.

Example command:

```text
MOVE X=50 Y=20 F=600
```

The firmware parses the command, plans motion, updates simulated motor state, handles faults, and sends telemetry back to the host.

Example telemetry:

```text
OK MOVE QUEUED
T ms=100 state=MOVING x=10.2 y=4.1 fault=0
T ms=500 state=DONE x=50.0 y=20.0 fault=0
```

Current MVP behavior is available through a host-side demo executable:

```bash
./build/mr_robo_demo
```

The demo boots Mr. Robo, parses commands, simulates movement, enforces travel bounds, triggers `ESTOP`, clears the fault with `CLEAR_FAULT`, and prints telemetry.

The scheduler-backed FreeRTOS demo is available when building with `USE_FREERTOS=ON`:

```bash
./build-freertos/motion_freertos_scheduler_demo
```

This demo uses real FreeRTOS task creation and scheduler startup through the POSIX simulator port while keeping the same parser, motion, fault, and telemetry modules.

## Current Architecture

```text
Host Demo / CTest / Python Smoke Tests
        |
        v
Command Parser
        |
        v
FreeRTOS-style Task Layer
        |
        +--> Command queue
        +--> State mutex
        +--> Event flags
        |
        v
Motion Controller + Fault Manager + Telemetry
        |
        v
Host RTOS Wrapper or Real FreeRTOS POSIX Backend
```

The default build uses the host RTOS wrapper so tests stay fast and hardware-free. The real FreeRTOS kernel is tracked as a third-party Git submodule under `third_party/FreeRTOS-Kernel`, and `USE_FREERTOS=ON` builds the wrapper API and scheduler demo against real FreeRTOS queue, mutex, event-group, task, and scheduler APIs through the POSIX simulator port.

## Planned Architecture

```text
Python CLI / Tests
        |
        v
UART Command Interface
        |
        v
Real FreeRTOS Firmware in QEMU or on ARM Cortex-M
        |
        v
Motion Planner + PID + Fault Handling
        |
        v
Python Plant Simulator
```

## MVP Features

- Build and run a host-side C demo that exercises the firmware modules.
- Implement a UART-style command parser.
- Support basic commands:
  - `PING`
  - `STATUS`
  - `MOVE`
  - `STOP`
  - `ESTOP`
  - `CLEAR_FAULT`
- Simulate 2-axis position movement.
- Generate periodic telemetry.
- Simulate emergency-stop behavior, travel bounds faults, and explicit fault recovery.
- Add C unit tests for telemetry, command parsing, motion behavior, fault handling, RTOS wrappers, and task-level command flow.
- Add Python smoke tests for the host demo and FreeRTOS scheduler demo.
- Build a real FreeRTOS scheduler demo using static task creation.

## Stretch Features

- PID position-control loop.
- Trapezoidal acceleration profile.
- Telemetry CSV logging.
- Simple plotting or dashboard tool.
- GDB debugging guide.
- Fault-injection test suite.

## Repository Structure

```text
docs/       Requirements, design docs, verification plans
firmware/   Embedded C/C++ firmware modules
sim/        Python plant/motion simulator
third_party/ External dependencies such as FreeRTOS-Kernel
tools/      Python CLI and telemetry tools
tests/      C unit tests and Python host-demo integration tests
logs/       Captured telemetry logs
media/      Demo images, GIFs, or videos
.github/    GitHub Actions CI workflows
```

## Current Status

Current phase: Implementation Phase 5 - RTOS communication refinement.

Completed so far:

- Requirements and design documentation.
- Modular C implementations for telemetry, command parsing, motion control, and fault handling.
- Travel bounds checking for the simulated 2-axis workspace.
- Host-side `mr_robo_demo` executable.
- Host-side RTOS wrapper for queues, mutexes, and event groups.
- Task-style application layer with command, motion, telemetry, and fault-handling steps.
- Host demo refactored to drive the task layer instead of duplicating command orchestration.
- GitHub Actions CI workflow for CMake build, CTest, and Python smoke testing.
- FreeRTOS-Kernel added as a Git submodule under `third_party/FreeRTOS-Kernel`.
- CMake option `USE_FREERTOS` added for backend selection.
- `firmware/App/rtos/rtos_port_freertos.c` maps the project RTOS wrapper to real FreeRTOS APIs.
- CI validates both `USE_FREERTOS=OFF` and `USE_FREERTOS=ON` build/test paths.
- FreeRTOS scheduler demo creates command, motion, telemetry, and supervisor tasks with `xTaskCreateStatic`.
- CMake/CTest unit tests for telemetry, command parser, motion controller, fault manager, RTOS port, and app tasks.
- Python host-demo integration smoke test.
- Python FreeRTOS scheduler-demo smoke test.

Next implementation steps:

- Move telemetry through a queue or stream-style path so reporting is decoupled from task logic.
- Clarify the fault handling path as the scheduler layer grows.
- Keep the existing host backend and tests as the stable regression path.

Current validation status:

```text
Host build: 6/6 CTest suites passing
FreeRTOS build: 7/7 CTest suites passing
Python host-demo smoke test passing
Python FreeRTOS scheduler-demo smoke test passing
Both host and FreeRTOS scheduler builds passing
```

## Learning Goals

By completing this project, I will understand how RTOS-based firmware is structured, how embedded tasks communicate, how motion-control software is tested, and how Python can be used to automate embedded firmware validation.

## Resume Keywords

`C`, `CMake`, `CTest`, `GitHub Actions`, `CI/CD`, `FreeRTOS`, `ARM GCC`, `embedded systems`, `motion control`, `state machines`, `fault handling`, `telemetry`, `firmware testing`, `RTOS queues`, `mutexes`, `event groups`, `task architecture`, `static task creation`, `scheduler startup`

Planned keywords after future integration: `QEMU`, `UART`, `Python`, `ARM Cortex-M`, `PID control`
