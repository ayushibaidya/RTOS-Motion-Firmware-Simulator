# Coding Agent Protocols

This file is the entry point for coding agents working on Mr. Robo, the RTOS-based motion-control firmware simulator. Use it to understand the project constraints, implementation order, module boundaries, and where to find deeper documentation.

Note: this file lives in `docs/`, so it primarily documents agent expectations. If these rules should be enforced across the entire repository by AGENTS-aware tools, copy or move this content to a root-level `AGENTS.md`.

## Project Summary

Mr. Robo is a software-only embedded motion-control firmware simulator for a virtual 2-axis stage. The firmware is written in C and is designed to evolve toward an ARM Cortex-M + FreeRTOS + QEMU workflow.

The project currently emphasizes learning, correctness, modularity, and testability over feature speed.

## Documentation Map

- `README.md`: project overview, purpose, planned architecture, and resume keywords.
- `docs/requirements.md`: functional requirements and MVP definition of done.
- `docs/design_spec.md`: firmware task design, command protocol, motion model, telemetry, and fault handling.
- `docs/architecture.md`: high-level component structure and data flow.
- `docs/freertos_task_architecture.md`: FreeRTOS-style task boundaries, wrapper layer, task-layer design, and next integration steps.
- `docs/verification_plan.md`: requirement-to-test verification strategy.
- `docs/test_procedure.md`: manual and automated test procedures.

Before implementing a feature, read the relevant requirement and design sections first.

## Current SDLC Phase

Current phase: Implementation Phase 2 - FreeRTOS-style task architecture.

Completed:

- Requirements
- Design
- Implementation setup
- Telemetry module
- Motion controller module
- Fault manager module
- Command parser module
- Host-side demo loop
- C unit tests for telemetry, motion, command parsing, and fault handling
- Host-side RTOS wrapper for queues, mutexes, and event groups
- Task-style application layer for command, motion, telemetry, and fault flow
- C unit tests for RTOS wrapper and task layer
- Python host-demo smoke test

In progress:

- Refactor `firmware/Core/main.c` to use `firmware/App/tasks/app_tasks.c`
- Documentation updates for the FreeRTOS-style phase
- GitHub Actions CI/CD setup

Planned next:

- Replace duplicate orchestration in `main.c` with task-layer calls
- Add CI to build, run CTest, and run the Python smoke test
- Add QEMU and real FreeRTOS after the host task layer is stable

## Implementation Principles

- Prefer simple, readable C over clever abstractions.
- Keep modules small and single-purpose.
- Preserve embedded-style constraints: bounded buffers, integer math, fixed-width types, and explicit error handling.
- Avoid heap allocation unless a later design document explicitly allows it.
- Avoid floating-point math in firmware modules unless justified by a requirement.
- Keep the current FreeRTOS-style wrapper host-testable until the real FreeRTOS backend is intentionally added.
- Do not claim physical hardware behavior; this project is currently simulation-only.

## Commenting Standard

Implementation comments should explain why code exists, not restate what the code obviously does.

Good comments reference:

- requirement IDs
- safety behavior
- embedded constraints
- non-obvious edge cases
- future RTOS/concurrency implications

Avoid comments like:

```c
/* Set x to zero. */
```

Prefer comments like:

```c
/* REQ-007: Fault state starts clear so stale simulator state cannot block a new run. */
```

## Module Boundaries

### `firmware/App/command/`

Purpose: convert text commands into structured firmware commands.

Owns:

- command type definitions
- parse result definitions
- command argument validation
- conversion from millimeters to milli-millimeters

Must not:

- directly move the robot
- directly set telemetry output
- directly mutate motion state

Primary files:

- `firmware/App/command/command_parser.h`
- `firmware/App/command/command_parser.c`

### `firmware/App/motion/`

Purpose: own simulated X/Y motion state.

Owns:

- current position
- target position
- feedrate
- motion state machine
- position update logic

Important variables/concepts:

- positions use `milli_mm`
- feedrate uses `milli_mm_per_s`
- states include `IDLE`, `MOVING`, `STOPPED`, `DONE`, and `FAULT`

Must not:

- parse raw text commands
- format telemetry strings
- own global fault reasons

Primary files:

- `firmware/App/motion/motion_controller.h`
- `firmware/App/motion/motion_controller.c`

### `firmware/App/telemetry/`

Purpose: format and emit firmware status text.

Owns:

- telemetry status struct
- output writer callback
- status line formatting
- startup/status messages

Important variables/concepts:

- `telemetry_write_fn_t` abstracts UART/QEMU/test output
- positions are formatted from `milli_mm`

Must not:

- decide motion behavior
- parse commands
- activate faults

Primary files:

- `firmware/App/telemetry/telemetry.h`
- `firmware/App/telemetry/telemetry.c`

### `firmware/App/fault/`

Purpose: track safety/fault state.

Owns:

- fault active flag
- current fault reason
- fault reason string conversion
- ESTOP helper

Important fault reasons:

- `FAULT_REASON_NONE`
- `FAULT_REASON_ESTOP`
- `FAULT_REASON_LIMIT_EXCEEDED`
- `FAULT_REASON_INVALID_COMMAND`

Must not:

- calculate motion
- parse full command strings
- emit telemetry directly

Primary files:

- `firmware/App/fault/fault_manager.h`
- `firmware/App/fault/fault_manager.c`

### `firmware/App/pid/`

Purpose: future PID/control-loop logic.

Current status: placeholder.

Do not implement PID until the MVP command parser and runnable main loop are stable.

### `firmware/App/rtos/`

Purpose: provide host-side RTOS-style primitives before real FreeRTOS is linked.

Owns:

- bounded queue behavior
- mutex lock/unlock behavior
- event bit set/clear/get behavior

Must not:

- start a real scheduler
- allocate heap memory
- own motion, fault, or telemetry policy

Primary files:

- `firmware/App/rtos/rtos_port.h`
- `firmware/App/rtos/rtos_port_host.c`

### `firmware/App/tasks/`

Purpose: provide task-style application orchestration.

Owns:

- command queue storage
- task context initialization
- command-task step
- motion-task step
- telemetry-task step
- event bits for `MOVING`, `FAULTED`, and `STOP_REQUESTED`

Must not:

- duplicate parser internals
- duplicate motion math
- duplicate telemetry formatting
- claim to be running a real FreeRTOS scheduler

Primary files:

- `firmware/App/tasks/app_messages.h`
- `firmware/App/tasks/app_tasks.h`
- `firmware/App/tasks/app_tasks.c`

### `firmware/Core/`

Purpose: firmware entry point and system orchestration.

Owns:

- module initialization
- top-level control loop for the non-FreeRTOS MVP
- later FreeRTOS task creation and scheduler startup
- next refactor should call the task layer instead of duplicating command orchestration

Must not:

- contain detailed command parsing internals
- contain detailed motion math
- contain PID internals
- continue growing separate command/motion/fault orchestration once `app_tasks` owns that policy

Primary file:

- `firmware/Core/main.c`

## Unit Conventions

- Position values use milli-millimeters: `1 mm = 1000 milli-mm`.
- Feedrate values use milli-millimeters per second.
- MVP command inputs use integer millimeters only.
- Decimal commands such as `X=10.5` are invalid until decimal parsing is intentionally added.

## Testing Protocol

When adding or changing a module:

1. Compile-check the changed module with `arm-none-eabi-gcc` if possible.
2. Add or update a unit test under `tests/unit/`.
3. Run host-side CMake tests:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Do not fix unrelated tests or unrelated modules during focused implementation.

## Current Build Notes

- Host-side unit tests build with the Mac compiler.
- ARM-oriented compile checks use `arm-none-eabi-gcc`.
- `cmake/arm-none-eabi.cmake` configures CMake for ARM builds.
- The FreeRTOS-style host wrapper and task layer are active.
- Real FreeRTOS scheduler integration and QEMU execution are planned, not yet active.

## Agent Behavior Rules

- Do not generate large implementations unless explicitly asked.
- Prefer guiding the user function-by-function when they are learning.
- Preserve the user's code style unless a change is needed for correctness or consistency.
- Ask before making broad architectural changes.
- Keep changes small, testable, and aligned with the SDLC phase.
- Update documentation when behavior, requirements, or architecture changes.
