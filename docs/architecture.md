# Architecture

## System Summary

This project has two main parts:

- Firmware running on an emulated ARM Cortex-M target.
- Python host tools that send commands, run tests, and collect telemetry.

The firmware is designed like a real embedded motion controller, even though the hardware is simulated.

## High-Level Flow

```text
Python CLI / Tests
        |
        v
UART-style commands
        |
        v
FreeRTOS firmware in emulator
        |
        v
Motion state + telemetry + fault handling
```

## Firmware Tasks

```text
Command Task    Parses host commands
Motion Task     Updates simulated X/Y motion
Sensor Task     Provides simulated feedback state
Telemetry Task  Reports status to host
Fault Task      Handles stop and emergency-stop behavior
```

## Planned Data Flow

```text
Command Task -> command_queue -> Motion Task
Motion Task  -> telemetry_queue -> Telemetry Task
Fault Task   -> telemetry_queue -> Telemetry Task
```

The Motion Task owns the main motion state so that position updates are controlled in one place.

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
tools/mc_cli.py        Send commands
tools/log_telemetry.py Log firmware telemetry
sim/plant_sim.py       Future simulated hardware model
```

## Notes

The first version will keep simulation simple. Later versions can add PID control, acceleration profiles, limit switches, and more detailed plant simulation.
