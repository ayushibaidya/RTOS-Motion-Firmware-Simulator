# Architecture

## System Summary

This project currently has three main parts:

- Host-side C firmware modules that simulate a 2-axis motion stage.
- A FreeRTOS-style task layer that models task communication without a real scheduler.
- Python host tests that run the demo executable and check expected output.

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
Motion state + telemetry + fault handling
```

## Current Task-Layer Components

```text
CommandTask step     Parses host commands and queues validated command messages
MotionTask step      Consumes command messages and updates simulated X/Y motion
TelemetryTask step   Reports status snapshots to host output
Fault path           Handles ESTOP, bounds faults, and explicit recovery
```

These are implemented as host-callable step functions in `firmware/App/tasks/`. They are not real FreeRTOS tasks yet.

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
TelemetryTask step
```

The task layer uses a command queue, a state mutex, and event bits for `MOVING`, `FAULTED`, and `STOP_REQUESTED`.

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

The current version keeps simulation simple. Later versions can add real FreeRTOS scheduling, QEMU execution, PID control, acceleration profiles, limit switches, and more detailed plant simulation.
