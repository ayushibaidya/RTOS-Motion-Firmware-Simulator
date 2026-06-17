# Design Specification

## System Overview

This project implements software-first motion-control firmware for a simulated 2-axis stage. The current MVP runs as a host-side C demo executable so the core firmware modules can be built and tested before ARM, FreeRTOS, and QEMU integration.

The current demo sends text commands into the command parser, updates simulated 2-axis motion state, reports telemetry, and handles stop/fault behavior. Future phases will move the same module boundaries into an emulated ARM Cortex-M + FreeRTOS environment with Python host tools.

The system is software-only but is designed to resemble a real embedded motion-control system.

## High-Level Architecture

```text
Host C Demo / Tests
        |
        v
Command Parser
        |
        v
Application Orchestration
        |
        +--> Motion Controller
        +--> Fault Manager
        +--> Telemetry
```

## Current MVP Module Design

The current MVP uses plain C modules instead of RTOS tasks. This keeps the core logic testable before scheduler behavior is introduced.

### Command Parser

The Command Parser receives text commands from the host demo.

Responsibilities:

- Parse commands such as `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT`.
- Validate command arguments.
- Convert position/feedrate values into integer milli-units.
- Return stable parse error codes for invalid input.

### Motion Controller

The Motion Controller owns simulated position and motion state.

Responsibilities:

- Accept movement requests from application orchestration.
- Track current and target `X`/`Y` positions.
- Update simulated position over time.
- Support stopped, moving, done, and fault states.
- Stop motion when requested by `STOP` or `ESTOP`.

### Telemetry

The Telemetry module formats system status for the host.

Responsibilities:

- Report current firmware state.
- Report simulated `X` and `Y` positions.
- Report fault status.
- Report uptime or tick count.
- Send command responses such as `OK` and `ERR`.

### Fault Manager

The Fault Manager tracks unsafe or stopped states.

Responsibilities:

- Process `ESTOP`.
- Track fault flags and fault reasons.
- Allow explicit fault recovery through `CLEAR_FAULT`.
- Support future limit-switch and bounds-checking faults.

## Future FreeRTOS Task Design

Future firmware shall use multiple FreeRTOS tasks to separate responsibilities.

### Command Task

The Command Task receives text commands from the host interface.

Responsibilities:

- Read incoming UART-style command data.
- Parse commands such as `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT`.
- Validate command arguments.
- Send valid motion commands to the Motion Task.
- Send invalid commands to the telemetry/response path as errors.

### Motion Task

The Motion Task owns the current motion command and simulated position state.

Responsibilities:

- Accept movement requests from the Command Task.
- Track current and target `X`/`Y` positions.
- Update simulated position over time.
- Support stopped, moving, done, and fault states.
- Stop motion when requested by `STOP` or `ESTOP`.

### Sensor/State Task

The Sensor/State Task represents simulated feedback from sensors.

Responsibilities:

- Maintain simulated sensor values.
- Provide position feedback for telemetry and future PID logic.
- Optionally simulate limit-switch state.
- Optionally inject sensor noise or faults in stretch phases.

### Telemetry Task

The Telemetry Task periodically sends system status to the host.

Responsibilities:

- Report current firmware state.
- Report simulated `X` and `Y` positions.
- Report fault status.
- Report uptime or tick count.
- Send command responses such as `OK` and `ERR`.

### Fault Task

The Fault Task handles unsafe or stopped states.

Responsibilities:

- Process `ESTOP`.
- Track fault flags.
- Prevent new movement while in fault state.
- Process explicit fault recovery.
- Support future limit-switch and bounds-checking faults.

## Inter-Task Communication

FreeRTOS queues shall be used to pass messages between tasks in a future RTOS integration phase.

Planned queues:

```text
command_queue      Command Task -> Motion Task
telemetry_queue    Command/Motion/Fault Tasks -> Telemetry Task
fault_queue        Command/Sensor Tasks -> Fault Task
```

The Motion Task should own motion state to avoid multiple tasks modifying position directly.

Shared state should be minimized. If shared state is needed, it should be protected using a mutex or copied through a queue.

## Command Protocol

Commands are text-based and newline-terminated.

Example:

```text
MOVE X=50 Y=20 F=600
```

### Supported MVP Commands

| Command | Purpose |
|---|---|
| `PING` | Check whether firmware is responsive |
| `STATUS` | Request current firmware state |
| `MOVE X=<value> Y=<value> F=<feedrate>` | Move to a target position |
| `STOP` | Stop current motion without entering fault |
| `ESTOP` | Emergency stop and enter fault state |
| `CLEAR_FAULT` | Clear active fault state and allow motion recovery |

### Command Responses

| Response | Meaning |
|---|---|
| `OK` | Command accepted |
| `ERR` | Command rejected |
| `T` | Telemetry message |

Examples:

```text
OK PONG
OK MOVE QUEUED
OK FAULT CLEARED
ERR_UNKNOWN_COMMAND
ERR_INVALID_ARGUMENT
ERR FAULT_ACTIVE
```

## Motion Model

The MVP motion model shall simulate position changes over time.

State variables:

```text
current_x
current_y
target_x
target_y
feedrate
motion_state
```

Required motion states:

```text
IDLE
MOVING
STOPPED
DONE
FAULT
```

For the MVP, the motion model may use a simple constant-rate update.

Future versions may replace this with a trapezoidal acceleration profile.

## Telemetry Format

Telemetry shall be text-based and machine-readable.

Example:

```text
T ms=100 state=MOVING x=10.2 y=4.1 fault=0
```

Fields:

| Field | Meaning |
|---|---|
| `ms` | System uptime in milliseconds |
| `state` | Current motion/controller state |
| `x` | Simulated X position |
| `y` | Simulated Y position |
| `fault` | Fault flag, `0` or `1` |

## Fault Handling

The firmware shall support emergency stop behavior.

When `ESTOP` is received:

- Current motion stops immediately.
- State changes to `FAULT`.
- Fault flag is set.
- New motion commands are rejected while fault is active.

When `CLEAR_FAULT` is received:

- Fault manager state is cleared.
- Motion fault state is cleared.
- Motion state is allowed to return to `IDLE`.
- New movement commands may be accepted again.

In stretch phases, additional fault sources may include:

- Simulated limit-switch trigger
- Invalid target position
- Command timeout
- Sensor mismatch
- Position error above threshold

## Future Host-Side Python Tools

Future host tools shall interact with the emulated firmware.

Planned tools:

```text
tools/mc_cli.py
tools/log_telemetry.py
sim/plant_sim.py
```

### `mc_cli.py`

Sends user commands to the firmware and prints responses.

Example:

```bash
python tools/mc_cli.py ping
python tools/mc_cli.py status
python tools/mc_cli.py move --x 50 --y 20 --feed 600
python tools/mc_cli.py estop
```

### `log_telemetry.py`

Captures telemetry output and stores it in a log file.

Example:

```bash
python tools/log_telemetry.py --seconds 10 --out logs/run.csv
```

### `plant_sim.py`

Represents future simulated hardware behavior.

Possible responsibilities:

- Simulate encoder readings.
- Simulate limit switches.
- Simulate position noise.
- Simulate sensor faults.

## Testing Strategy

Testing shall be split into unit tests and integration tests.

### Unit Tests

Unit tests shall validate pure logic that does not require the emulator.

Current unit test areas:

- Command parser
- Motion state update logic
- Fault state transitions
- Telemetry formatting

Future unit test areas:

- PID controller
- Motion bounds checking
- Fault injection behavior

### Integration Tests

Future integration tests shall run the firmware in an emulator and communicate with it from Python.

Planned integration checks:

- `PING` returns `OK PONG`.
- Invalid command returns `ERR_UNKNOWN_COMMAND`.
- `MOVE` changes simulated position over time.
- `STATUS` returns current state.
- `STOP` stops motion.
- `ESTOP` enters fault state.
- `CLEAR_FAULT` clears fault state.
- New `MOVE` command is rejected after `ESTOP`.

## Design Constraints

- No physical hardware is required.
- The project should remain portable across macOS/Linux where possible.
- Firmware logic should be separated into small modules.
- Host-side tools should be scriptable from the command line.
- Documentation should clearly distinguish simulation from physical hardware validation.

## Future Hardware Path

Although the MVP is software-only, the design should leave room for future hardware migration.

Potential future target:

- STM32 NUCLEO board
- UART over USB
- GPIO/PWM stepper control
- I2C encoder or IMU
- Limit switches
- Hardware-in-the-loop validation
