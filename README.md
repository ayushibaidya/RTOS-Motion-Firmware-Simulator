# RTOS-Based Motion Control Firmware Simulator

## Overview

This project implements software-first embedded motion-control firmware for a simulated 2-axis stage, with a planned path toward ARM Cortex-M, real FreeRTOS scheduling, and QEMU integration.

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
- Task-style application architecture for command, motion, telemetry, and fault behavior
- GitHub Actions CI for automated build, unit-test, and smoke-test validation
- Unit-tested firmware modules

Planned future integrations include:

- Complete real FreeRTOS backend selection with `USE_FREERTOS=ON`
- FreeRTOS task creation and scheduler startup
- ARM Cortex-M firmware builds
- UART command protocols
- QEMU-based embedded simulation
- Python-based firmware testing
- PID/control-loop concepts

## System Concept

The current project runs as a host-side C demo and includes a host-testable FreeRTOS-style task layer. The planned target flow is a Python host program sending commands to firmware running inside QEMU, where the firmware behaves like it is running on a real microcontroller.

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

## Current Architecture

```text
Host Demo / CTest / Python Smoke Test
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
```

The current RTOS layer is a host-side simulation of FreeRTOS concepts. It does not run a real scheduler yet.

The real FreeRTOS kernel is now tracked as a third-party Git submodule under `third_party/FreeRTOS-Kernel`. The default build still uses the host RTOS wrapper until the real backend is implemented and selected.

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
- Add a Python host-demo smoke test.

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

Current phase: Implementation Phase 3 - real FreeRTOS backend preparation.

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
- CMake option `USE_FREERTOS` added for future backend selection.
- Placeholder `firmware/App/rtos/rtos_port_freertos.c` created for the real FreeRTOS backend.
- CMake/CTest unit tests for telemetry, command parser, motion controller, fault manager, RTOS port, and app tasks.
- Python host-demo integration smoke test.

Next implementation steps:

- Implement `rtos_port_freertos.c` by mapping the wrapper API to real FreeRTOS queue, semaphore, and event-group APIs.
- Update CMake so `USE_FREERTOS=OFF` uses `rtos_port_host.c` and `USE_FREERTOS=ON` uses `rtos_port_freertos.c`.
- Add the required FreeRTOS configuration and target/portable layer before starting the real scheduler.

Current validation status:

```text
6/6 CTest suites passing
Python host-demo smoke test passing
```

## Learning Goals

By completing this project, I will understand how RTOS-based firmware is structured, how embedded tasks communicate, how motion-control software is tested, and how Python can be used to automate embedded firmware validation.

## Resume Keywords

`C`, `CMake`, `CTest`, `GitHub Actions`, `CI/CD`, `ARM GCC`, `embedded systems`, `motion control`, `state machines`, `fault handling`, `telemetry`, `firmware testing`, `RTOS-style queues`, `mutexes`, `event groups`, `task architecture`

Planned keywords after future integration: `FreeRTOS scheduler`, `QEMU`, `UART`, `Python`, `ARM Cortex-M`
