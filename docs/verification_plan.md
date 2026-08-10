# Verification Plan

## Purpose

This document defines how the project will prove that the firmware meets the requirements.

## Verification Approach

The project will use a mix of:

- Host-side demo testing
- CMake/CTest unit tests
- Telemetry review
- Python host-demo integration smoke testing
- GitHub Actions CI validation
- Future Python/emulator-based integration tests

## Requirement Checks

| Requirement | Verification Method |
|---|---|
| Host-side demo builds and runs | Build `mr_robo_demo` and confirm startup output |
| Module separation is preserved | Confirm command, motion, telemetry, and fault logic remain in separate modules |
| `PING` works | Send `PING`, expect `OK PONG` |
| `STATUS` works | Send `STATUS`, expect current state telemetry |
| `MOVE` works | Send move command and confirm position changes |
| Travel bounds work | Send out-of-bounds move and expect `ERR LIMIT_EXCEEDED` plus fault state |
| `STOP` works | Send `STOP` during motion and confirm movement stops |
| `ESTOP` works | Send `ESTOP` and confirm fault state |
| `CLEAR_FAULT` works | Send `CLEAR_FAULT` after `ESTOP` and confirm fault state clears |
| Invalid commands are rejected | Send malformed command and expect `ERR_UNKNOWN_COMMAND` or `ERR_INVALID_ARGUMENT` |
| Telemetry is generated | Confirm periodic `T` messages |
| RTOS wrapper behaves correctly | Run `rtos_port` CTest suite for queues, mutexes, and event groups |
| Task-layer flow behaves correctly | Run `app_tasks` CTest suite for command queueing, motion, telemetry, and fault recovery |
| Unit tests pass | Run CTest and confirm `6/6` tests passing |
| Host demo smoke test passes | Run `python3 tests/hil/test_host_demo.py` |
| CI workflow passes | Push to GitHub and confirm the Actions run builds and tests successfully |
| FreeRTOS dependency is tracked | Confirm `third_party/FreeRTOS-Kernel` is present as a Git submodule |
| FreeRTOS backend remains gated | Confirm default host build uses `USE_FREERTOS=OFF` |

## MVP Pass Criteria

The MVP passes verification when:

- The firmware boots successfully.
- Host commands receive expected responses.
- Simulated position updates during movement.
- Out-of-bounds movement targets are rejected and latched as `LIMIT_EXCEEDED`.
- Stop, emergency-stop, and fault-clear behavior work.
- Automated tests pass for telemetry, command parsing, motion behavior, fault handling, RTOS wrapper behavior, and app task flow.
- Python host-demo smoke testing passes.
- GitHub Actions CI passes on push or pull request.
- The FreeRTOS kernel dependency is present without breaking the default host build.

## Current Validation Command

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected result:

```text
6/6 tests passed
```

## Current Integration Smoke Command

```bash
python3 tests/hil/test_host_demo.py
```

Expected result:

```text
host demo integration test passed
```

## CI Verification

The CI workflow should verify:

- repository checkout with submodules
- CMake configure
- CMake build
- CTest unit suites
- Python host-demo smoke test

Workflow file:

```text
.github/workflows/ci.yml
```

## Future Verification

Future verification phases should add:

- `USE_FREERTOS=ON` backend build checks
- QEMU firmware boot verification
- Real FreeRTOS task scheduling checks
- UART command-response tests
- Python CLI integration tests
- Telemetry logging validation
