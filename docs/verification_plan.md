# Verification Plan

## Purpose

This document defines how the project will prove that the firmware meets the requirements.

## Verification Approach

The project will use a mix of:

- Host-side demo testing
- CMake/CTest unit tests
- Telemetry review
- Python host-demo integration smoke testing
- Python FreeRTOS scheduler-demo smoke testing
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
| Host unit tests pass | Run default CTest and confirm `6/6` tests passing |
| Host demo smoke test passes | Run `python3 tests/hil/test_host_demo.py` |
| CI workflow passes | Push to GitHub and confirm the Actions run builds and tests successfully |
| FreeRTOS dependency is tracked | Confirm `third_party/FreeRTOS-Kernel` is present as a Git submodule |
| FreeRTOS backend remains gated | Confirm default host build uses `USE_FREERTOS=OFF` |
| FreeRTOS backend tests pass | Configure with `-DUSE_FREERTOS=ON` and confirm `7/7` CTest suites passing |
| FreeRTOS backend smoke test passes | Run Python smoke test with `DEMO_PATH=build-freertos/mr_robo_demo` |
| FreeRTOS scheduler starts | Run `motion_freertos_scheduler_demo` through CTest |
| FreeRTOS tasks execute command flow | Run `tests/hil/test_freertos_scheduler_demo.py` |
| Scheduler demo exits for CI | Confirm scheduler smoke test finishes before timeout |

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
- The `USE_FREERTOS=ON` backend builds and passes automated validation.
- The FreeRTOS scheduler demo starts real tasks and exits cleanly for CI.
- The scheduler smoke test confirms boot, movement, fault, recovery, and status behavior.

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

## FreeRTOS Backend Validation Command

```bash
cmake -S . -B build-freertos -G Ninja -DUSE_FREERTOS=ON
cmake --build build-freertos
ctest --test-dir build-freertos --output-on-failure
```

Expected result:

```text
7/7 tests passed
```

## Current Integration Smoke Command

```bash
python3 tests/hil/test_host_demo.py
```

Expected result:

```text
host demo integration test passed
```

## FreeRTOS Scheduler Smoke Command

```bash
DEMO_PATH=build-freertos/motion_freertos_scheduler_demo python3 tests/hil/test_freertos_scheduler_demo.py
```

Expected result:

```text
FreeRTOS scheduler demo integration test passed
```

## CI Verification

The CI workflow should verify:

- repository checkout with submodules
- CMake configure for host and FreeRTOS backend modes
- CMake build for both backend modes
- CTest unit suites for both backend modes
- Python host-demo smoke test for both backend modes
- Python scheduler-demo smoke test for the FreeRTOS backend mode

Workflow file:

```text
.github/workflows/ci.yml
```

## Future Verification

Future verification phases should add:

- QEMU firmware boot verification
- Longer-running FreeRTOS task scheduling checks
- UART command-response tests
- Python CLI integration tests
- Telemetry logging validation
