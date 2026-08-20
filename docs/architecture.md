# Architecture

## System Summary

This project currently has three main parts:

- Host-side C firmware modules that simulate a 2-axis motion stage.
- A task layer that can run as host-callable steps or under the FreeRTOS POSIX scheduler demo.
- Python host tests that run the demo executables and check expected output.
- GitHub Actions CI that repeats build and test validation after code is pushed.

The firmware is designed like a real embedded motion controller, even though the hardware is simulated and no physical device is required.

## High-Level Flow

```text
Host Demo / Tests
        |
        v
Command Parser
        |
        v
FreeRTOS-style Task Layer
        |
        v
Motion state + telemetry queue + fault handling
```

## Current Task-Layer Components

```text
CommandTask step     Parses host commands and queues validated command messages
MotionTask step      Consumes command messages and updates simulated X/Y motion
TelemetryTask step   Drains queued responses and reports status snapshots
Fault path           Routes ESTOP, bounds faults, and recovery through helpers
```

These are implemented as host-callable step functions in `firmware/App/tasks/`. The FreeRTOS scheduler demo wraps the same task layer with real FreeRTOS task entry functions.

## Current Data Flow

```text
Raw command text
        |
        v
CommandTask step
        |
        v
command_queue
        |
        v
MotionTask step / fault path
        |
        v
Motion + fault state
        |
        v
telemetry_queue
        |
        v
TelemetryTask step
```

The task layer uses a command queue, telemetry queue, state mutex, and event bits for `MOVING`, `FAULTED`, and `STOP_REQUESTED`.

## Dependency Layout

```text
third_party/FreeRTOS-Kernel
```

The FreeRTOS kernel is present as a Git submodule for the real backend phase. The default host build still uses `firmware/App/rtos/rtos_port_host.c`, while the FreeRTOS scheduler build uses the POSIX simulator backend.

The real backend file is:

```text
firmware/App/rtos/rtos_port_freertos.c
```

The CMake option for backend selection is:

```text
USE_FREERTOS
```

## Main States

```text
IDLE
MOVING
STOPPED
DONE
FAULT
```

## Host Tools

```text
tests/hil/test_host_demo.py  Runs the current host-demo smoke test
tools/mc_cli.py              Future command sender
tools/log_telemetry.py       Future telemetry logger
sim/plant_sim.py             Future simulated hardware model
```

## Notes

The current version keeps simulation simple while validating the wrapper against real FreeRTOS primitives. Later versions can add real scheduler startup, QEMU execution, PID control, acceleration profiles, limit switches, and more detailed plant simulation.
