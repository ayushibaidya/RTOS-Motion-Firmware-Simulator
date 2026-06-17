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
- `docs/verification_plan.md`: requirement-to-test verification strategy.
- `docs/test_procedure.md`: manual and automated test procedures.

Before implementing a feature, read the relevant requirement and design sections first.

## Current SDLC Phase

Current phase: Implementation MVP.

Completed:

- Requirements
- Design
- Implementation setup
- Telemetry module
- Motion controller module
- Fault manager module
- Initial unit tests for telemetry and motion

In progress:

- Command parser module

Planned next:

- Wire command parser into `main.c`
- Add unit tests for command parsing
- Build a runnable MVP loop
- Add QEMU and FreeRTOS after the plain C modules are stable

## Implementation Principles

- Prefer simple, readable C over clever abstractions.
- Keep modules small and single-purpose.
- Preserve embedded-style constraints: bounded buffers, integer math, fixed-width types, and explicit error handling.
- Avoid heap allocation unless a later design document explicitly allows it.
- Avoid floating-point math in firmware modules unless justified by a requirement.
- Do not introduce FreeRTOS until core modules can be built and tested as plain C.
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

### `firmware/Core/`

Purpose: firmware entry point and system orchestration.

Owns:

- module initialization
- top-level control loop for the non-FreeRTOS MVP
- later FreeRTOS task creation and scheduler startup

Must not:

- contain detailed command parsing internals
- contain detailed motion math
- contain PID internals

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
- QEMU and FreeRTOS integration are planned, not yet active.

## Agent Behavior Rules

- Do not generate large implementations unless explicitly asked.
- Prefer guiding the user function-by-function when they are learning.
- Preserve the user's code style unless a change is needed for correctness or consistency.
- Ask before making broad architectural changes.
- Keep changes small, testable, and aligned with the SDLC phase.
- Update documentation when behavior, requirements, or architecture changes.
