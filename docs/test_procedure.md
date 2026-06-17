# Test Procedure

## Purpose

This document lists the basic tests used to check the firmware during development. The current MVP is tested as a host-side C demo plus CMake/CTest unit tests. Emulator and Python-based tests are planned for later phases.

## Manual Tests

### Test 1: Firmware Startup

1. Build the project.
2. Run the host-side demo:

```bash
./build/mr_robo_demo
```

3. Confirm startup output appears.
4. Confirm telemetry begins or the firmware accepts commands.

Expected result: firmware is responsive.

### Test 2: Ping Command

1. Send:

```text
PING
```

Expected result:

```text
OK PONG
```

### Test 3: Status Command

1. Send:

```text
STATUS
```

Expected result: firmware reports current state and position.

### Test 4: Move Command

1. Send:

```text
MOVE X=50 Y=20 F=600
```

Expected result: firmware accepts the command and simulated position changes over time.

### Test 5: Stop Command

1. Start a move.
2. Send:

```text
STOP
```

Expected result: motion stops without entering fault state.

### Test 6: Emergency Stop

1. Start a move.
2. Send:

```text
ESTOP
```

Expected result: motion stops and firmware enters `FAULT` state.

### Test 7: Clear Fault

1. Trigger emergency stop.
2. Send:

```text
CLEAR_FAULT
```

Expected result: firmware clears fault state and returns motion state to `IDLE`.

### Test 8: Invalid Command

1. Send:

```text
BANANA
```

Expected result: firmware returns `ERR_UNKNOWN_COMMAND`.

## Automated Tests

Run the current automated test suite with:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Current CTest suites:

- `telemetry`
- `motion_controller`
- `command_parser`
- `fault_manager`

Expected result:

```text
4/4 tests passed
```

Future Python/emulator tests should cover:

- Emulator command-response behavior
- UART-style host communication
- Telemetry logging
- Fault injection
- QEMU firmware boot
