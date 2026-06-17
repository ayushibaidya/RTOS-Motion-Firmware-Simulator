# Verification Plan

## Purpose

This document defines how the project will prove that the firmware meets the requirements.

## Verification Approach

The project will use a mix of:

- Host-side demo testing
- CMake/CTest unit tests
- Telemetry review
- Future Python/emulator-based integration tests

## Requirement Checks

| Requirement | Verification Method |
|---|---|
| Host-side demo builds and runs | Build `mr_robo_demo` and confirm startup output |
| Module separation is preserved | Confirm command, motion, telemetry, and fault logic remain in separate modules |
| `PING` works | Send `PING`, expect `OK PONG` |
| `STATUS` works | Send `STATUS`, expect current state telemetry |
| `MOVE` works | Send move command and confirm position changes |
| `STOP` works | Send `STOP` during motion and confirm movement stops |
| `ESTOP` works | Send `ESTOP` and confirm fault state |
| `CLEAR_FAULT` works | Send `CLEAR_FAULT` after `ESTOP` and confirm fault state clears |
| Invalid commands are rejected | Send malformed command and expect `ERR_UNKNOWN_COMMAND` or `ERR_INVALID_ARGUMENT` |
| Telemetry is generated | Confirm periodic `T` messages |
| Unit tests pass | Run CTest and confirm `4/4` tests passing |

## MVP Pass Criteria

The MVP passes verification when:

- The firmware boots successfully.
- Host commands receive expected responses.
- Simulated position updates during movement.
- Stop, emergency-stop, and fault-clear behavior work.
- Automated tests pass for telemetry, command parsing, motion behavior, and fault handling.

## Current Validation Command

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected result:

```text
4/4 tests passed
```

## Future Verification

Future verification phases should add:

- QEMU firmware boot verification
- FreeRTOS task scheduling checks
- UART command-response tests
- Python CLI integration tests
- Telemetry logging validation
