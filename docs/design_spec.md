# Design Specification

## System Overview

This project implements software-first motion-control firmware for a simulated 2-axis stage. The current project runs as a host-side C demo executable and now includes a FreeRTOS-style task layer so the core firmware behavior can be tested before ARM, real FreeRTOS, and QEMU integration.

The current demo drives host-callable task-step functions that model `CommandTask`, `MotionTask`, `TelemetryTask`, and a fault-handling path. Those task steps send text commands through the command parser, update simulated 2-axis motion state, report telemetry, and handle stop/fault behavior. Future phases will connect those boundaries to an emulated ARM Cortex-M + real FreeRTOS environment with Python host tools.

The system is software-only but is designed to resemble a real embedded motion-control system.

## High-Level Architecture

```text
Host C Demo / Tests
        |
        v
FreeRTOS-style Task Layer
        |
        +--> Command Parser
        +--> RTOS-style Queue/Mutex/Event Flags
        |
        +--> Motion Controller
        +--> Fault Manager
        +--> Telemetry
```

## Current Module Design

The project keeps the core logic in plain C modules and wraps those modules with host-callable task-step functions. This keeps the firmware testable before real scheduler behavior is introduced.

### Command Parser

The Command Parser receives text commands from the host demo.

Responsibilities:

- Parse commands such as `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT`.
- Validate command arguments.
- Convert position/feedrate values into integer milli-units.
- Return stable parse error codes for invalid input.
- Leave workspace safety policy to application orchestration and motion bounds checks.

### Motion Controller

The Motion Controller owns simulated position and motion state.

Responsibilities:

- Accept movement requests from application orchestration.
- Track current and target `X`/`Y` positions.
- Update simulated position over time.
- Expose simulated travel bounds for target validation.
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
- Track travel bounds violations as `LIMIT_EXCEEDED`.
- Support future limit-switch and bounds-checking faults.

## Current FreeRTOS-Style Task Design

The current task layer uses one-step functions to model FreeRTOS tasks without starting a real scheduler. This allows CTest to verify task communication and fault behavior on the host.

### Command Task

The Command Task step receives one text command from the host interface.

Responsibilities:

- Accept incoming command text.
- Parse commands such as `PING`, `STATUS`, `MOVE`, `STOP`, `ESTOP`, and `CLEAR_FAULT`.
- Validate command arguments.
- Send valid commands into the command queue.
- Send invalid command parse errors to the telemetry/response path.

### Motion Task

The Motion Task step consumes at most one queued command and advances simulated position by a caller-provided tick duration.

Responsibilities:

- Accept command messages from the command queue.
- Track current and target `X`/`Y` positions.
- Update simulated position over time.
- Support stopped, moving, done, and fault states.
- Stop motion when requested by `STOP` or `ESTOP`.
- Set or clear task event bits such as `MOVING`, `FAULTED`, and `STOP_REQUESTED`.

### Telemetry Task

The Telemetry Task step periodically sends system status to the host.

Responsibilities:

- Read the current motion and fault snapshot through the task context.
- Report current firmware state.
- Report simulated `X` and `Y` positions.
- Report fault status.
- Report uptime or tick count.

### Fault Handling Path

Fault handling currently runs inside the task orchestration path rather than a separate `FaultTask`.

Responsibilities:

- Process `ESTOP`.
- Track fault flags through the fault manager and event group.
- Prevent new movement while in fault state.
- Process explicit fault recovery with `CLEAR_FAULT`.
- Support travel bounds faults with `LIMIT_EXCEEDED`.

### Future Sensor/Fault Tasks

Future firmware may split simulated sensor feedback and fault processing into dedicated tasks after real scheduler integration.

Possible responsibilities:

- Maintain simulated sensor values.
- Simulate limit-switch state.
- Inject sensor noise or faults.
- Escalate safety events through a fault queue or event group.

## Inter-Task Communication

The current host-side RTOS wrapper provides bounded queues, mutexes, and event groups. These wrappers are intentionally shaped like FreeRTOS primitives but do not run a real scheduler yet.

Current communication:

```text
command_queue      CommandTask step -> MotionTask step
state_mutex        Protects motion/fault snapshots in task-step tests
event_group        Tracks MOVING, FAULTED, and STOP_REQUESTED flags
```

Planned future communication:

```text
telemetry_queue    Command/Motion/Fault Tasks -> Telemetry Task
fault_queue        Command/Sensor Tasks -> Fault Task
```

The task layer currently protects orchestration-level access with a mutex while the existing motion and fault modules continue to own their internal state.

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
ERR LIMIT_EXCEEDED
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

### Travel Bounds

The MVP defines a simulated 2-axis workspace:

```text
X: 0 mm to 100 mm
Y: 0 mm to 100 mm
```

Targets outside this workspace are rejected before motion starts. The application orchestration layer sets `FAULT_REASON_LIMIT_EXCEEDED`, moves the motion controller into `FAULT`, and reports `ERR LIMIT_EXCEEDED`.

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

When a `MOVE` target exceeds the configured travel bounds:

- Current motion is not started.
- Fault reason is set to `LIMIT_EXCEEDED`.
- Motion state changes to `FAULT`.
- The host receives `ERR LIMIT_EXCEEDED`.

In stretch phases, additional fault sources may include:

- Simulated limit-switch trigger
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
- Travel bounds checks
- Fault state transitions
- Telemetry formatting
- RTOS queue, mutex, and event group behavior
- Task-layer command queueing, motion updates, telemetry snapshots, and fault recovery

Future unit test areas:

- PID controller
- Fault injection behavior
- Real FreeRTOS backend adapter behavior

### Integration Tests

The current Python host-demo smoke test runs the host demo executable and checks expected boot, command, telemetry, fault, and recovery output.

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
